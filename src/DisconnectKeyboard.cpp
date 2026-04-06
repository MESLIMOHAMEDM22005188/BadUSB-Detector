#include "DisconnectKeyboard.h"
#include "UsbUtils.h"
#include "BadUSBDetector.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>
#include <map>
#include <vector>
#include <iomanip>
#include <cstdio>
#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>
#include "GeminiAnalyzer.h"
#include "ShadowModels.h"

using namespace std;
using namespace std::chrono;

void runNativeModelEvaluation();

constexpr int WINDOW_SIZE = 12;             // dictates the AI analyzes blocks of 15 keys
constexpr double THRESHOLD = 0.75;          // the XGBoost confidence required to trigger a lockdown
constexpr int SILENCE_TIMEOUT_SECONDS = 5;  // safety timer

atomic<bool> INPUT_BLOCKED{false};       // when the AI sets this to true, the macOS Event Tap instantly starts swallowing keystrokes.
atomic<int> ATTACK_SEQUENCE_ID{0};
using Clock = steady_clock;

static CFMachPortRef gTap = nullptr;        // it holds the active connection to the operating system that intercepts hardware events (keyboard, mouse, etc.) before they reach your apps.

map<CGKeyCode, Clock::time_point> key_press_starts;
map<CGKeyCode, Clock::time_point> key_release_ends;
bool first_key = true;
Clock::time_point last_key_release_time;    // a simple timestamp of the moment the previously typed key was released.
Clock::time_point t_last_threat_activity = Clock::now();

string pre_catch_payload;

struct KeyData {
    float flight;
    float inter;
    float hold;
};
vector<KeyData> window_buffer;              // it stores the last 15 keystrokes. When it reaches 15, the AI calculates the Mean and Variance (Jitter) of the whole array to decide if the typing pattern is a human or a machine.

BadUSBDetector* ai_agent = nullptr;         // this holds your active XGBoost Neural Engine in memory

string getCurrentTimestamp() {
    auto t = time(nullptr);
    auto tm = *localtime(&t);
    ostringstream oss;                      // it prints the text into an invisible memory buffer
    oss << put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void showPopup(const string& message) {
    system("killall osascript > /dev/null 2>&1");
    system("say -v Samantha 'Security Alert. Malicious USB Detected.' &");
    string command = "osascript -e 'display alert \" THREAT ISOLATED\" message \"" + message +
                     "\" as critical buttons {\"Locked\"} default button \"Locked\"' &";
    system(command.c_str());
}

bool askUserToViewSecondPopup() {
    string script = R"(osascript -e 'button returned of (display alert "Threat Neutralized" message "The system has been unlocked. Would you like to view the attack description?" buttons {"No", "Yes"} default button "Yes" as informational)')";
    FILE* pipe = popen(script.c_str(), "r");
    if (!pipe) return false;

    char buffer[128];
    string result = "";
    while (!feof(pipe)) { // ??????????
        if (fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);

    return (result.find("Yes") != string::npos);
}

bool askUserToViewGraphsPopup() {
    string script = R"(osascript -e 'button returned of (display alert "Threat Analysis Graphs" message "Would you like to generate and view the live telemetry graphs for this attack?" buttons {"No", "Yes"} default button "Yes" as informational)')";
    FILE* pipe = popen(script.c_str(), "r");
    if (!pipe) return false;

    char buffer[128];
    string result = "";
    while (!feof(pipe)) { // ??????????
        if (fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);

    return (result.find("Yes") != string::npos);
}

void showSecuredPopup() {
    //we use the '&' at the end so it runs in the background and doesn't freeze the program!
    string command = R"(osascript -e 'display alert "✅ System Secured" message "All malicious activity has been completely neutralized. Your input devices have been restored and you are safe to resume your work." as informational buttons {"Understood"} default button "Understood"' &)";
    system(command.c_str());
}

void savePreCatchForensics() { // it records the keys before the BadUSB is being caught
    if (pre_catch_payload.empty()) return;

    ofstream out("badusb_pre_catch.log", ios::app);
    if (out.is_open()) {
        out << "\n[" << getCurrentTimestamp() << "] ===== AI TRIGGERED =====\n";
        out << "Payload executed before detection:\n";
        out << pre_catch_payload << "\n";
        out << "========================================\n";
        out.close();
    }
    pre_catch_payload.clear();
}

double marginToProbability(double margin) {
    return 1.0 / (1.0 + exp(-margin));
}

// it handles everything from isolating the threat and preventing data theft to formatting the UI for the user.
void activateLockdown(float xgb_prob, double rf_prob, double svm_prob, double nn_prob, double log_prob) {
    if (!INPUT_BLOCKED.load()) {
        INPUT_BLOCKED.store(true);

        ATTACK_SEQUENCE_ID++;
        system("pkill -f visualize_live_metrics.py > /dev/null 2>&1 &");
        t_last_threat_activity = Clock::now();

        cout << "\n[!!!] XGBoost TRIGGERED LOCKDOWN [!!!]\n";
        savePreCatchForensics();

        // Push to background so macOS doesn't freeze waiting for weird USBs to eject!
        system("diskutil eject /Volumes/* > /dev/null 2>&1 &");

        ofstream q("badusb_post_catch.log", ios::out | ios::app);
        if (q.is_open()) {
            q << "\n[" << getCurrentTimestamp() << "] ===== BLOCKED PAYLOAD =====\n";
        }

        stringstream ss;
        ss << "Input has been paralyzed!\n\n";
        ss << "LIVE AI JURY CONSENSUS (Threat Confidence):\n";
        ss << fixed << setprecision(1);
        ss << "• XGBoost (Active): " << (xgb_prob * 100.0) << "%\n";
        ss << "• Neural Network: " << (nn_prob * 100.0) << "%\n";
        ss << "• Random Forest: " << (rf_prob * 100.0) << "%\n";
        ss << "• Support Vector Machine: " << (svm_prob * 100.0) << "%\n";
        ss << "• Logistic Regression: " << (log_prob * 100.0) << "%\n\n";
        ss << "System will unlock after " << SILENCE_TIMEOUT_SECONDS << " seconds of silence.";

        showPopup(ss.str());
    }
}

void process_window() {
    if (window_buffer.size() < WINDOW_SIZE) return; // it checks if the window_buffer has collected enough keystrokes (15 keys)

    int malicious_keystroke_count = 0;
    double last_rf_prob = 0, last_svm_prob = 0, last_nn_prob = 0, last_log_prob = 0;
    float last_xgb_prob = 0;

    for (const auto& k : window_buffer) {
        last_xgb_prob = ai_agent->predict(k.flight, k.inter, k.hold);

        double scaled_features[3];
        // mathematically shrinks raw millisecond times into normalized numbers
        // (usually between -3.0 and 3.0) using the exact same StandardScaler constants exported from Python
        scale_features(k.flight, k.inter, k.hold, scaled_features);

        // Map margins to probabilities (0.0 to 1.0) using the Sigmoid Helper
        last_log_prob = marginToProbability(predict_logistic_reg(scaled_features));
        last_svm_prob = marginToProbability(predict_svm(scaled_features));

        // Random Forest outputs an array. We calculate the percentage of trees that voted "Malicious"
        double rf_output[2];
        predict_random_forest(scaled_features, rf_output);
        double total_trees = rf_output[0] + rf_output[1];
        last_rf_prob = (total_trees == 0) ? 0 : (rf_output[1] / total_trees);

        // Neural Network is already a probability
        last_nn_prob = predict_neural_network(scaled_features);

        // Terminal Log (Optional: You can change BAD/OK to percentages here too if you want!)
        cout << "[CONSENSUS] XGB: " << (last_xgb_prob > THRESHOLD ? "BAD" : "SAFE ")
             << " | RF: " << (last_rf_prob > 0.5 ? "BAD" : "SAFE ")
             << " | SVM: " << (last_svm_prob > 0.5 ? "BAD" : "SAFE ")
             << " | NN: " << (last_nn_prob > 0.5 ? "BAD" : "SAFE ")
             << " | LogReg: " << (last_log_prob > 0.5 ? "BAD" : "SAFE ") << "\n";

        if (last_xgb_prob > THRESHOLD) {
            malicious_keystroke_count++;
        }
    }

    //if XGBoost flagged at least 4 of the 15 keystrokes as definitively malicious, the AI drops the hammer.
    if (malicious_keystroke_count >= 4) {
        // Pass the precise math to the visual UI!
        activateLockdown(last_xgb_prob, last_rf_prob, last_svm_prob, last_nn_prob, last_log_prob);
        window_buffer.clear();
    } else {
        window_buffer.erase(window_buffer.begin());
    }
}

CGEventRef eventCallbackDisconnect(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void*) {

    if (type == kCGEventTapDisabledByTimeout) {
        if (gTap) CGEventTapEnable(gTap, true);
        return event;
    }
    if (type != kCGEventKeyDown && type != kCGEventKeyUp) {
        return INPUT_BLOCKED.load() ? nullptr : event;
    }

    if (CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0) {
        return INPUT_BLOCKED.load() ? nullptr : event;
    }

    auto now = Clock::now();
    bool is_blocked = INPUT_BLOCKED.load();
    CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

    if (is_blocked) {
        t_last_threat_activity = now;
    }

    // saves the keys into badusb_post_catch.log (AFTER the lockdown)
    if (type == kCGEventKeyDown) {
        UniChar chars[4]; UniCharCount len;
        CGEventKeyboardGetUnicodeString(event, 4, &len, chars);

        if (len > 0) {
            char c = (char)chars[0];
            if (is_blocked) {
                ofstream q("badusb_post_catch.log", ios::out | ios::app);
                if (q.is_open()) {
                    if (c == 13 || c == 3) q << "\n";
                    else if (c >= 32 && c <= 126) q << c;
                }
            } else {
                if (c == 13 || c == 3) pre_catch_payload += '\n';
                else if (c >= 32 && c <= 126) pre_catch_payload += c;
            }
        }

        key_press_starts[keycode] = now;
        return is_blocked ? nullptr : event;
    }
    else if (type == kCGEventKeyUp) { //saves the metrics into a new .csv file (catch_telemetry.csv)
        float hold = 0, flight = 0, inter = 0;

        if (key_press_starts.count(keycode)) {
            hold = duration_cast<milliseconds>(now - key_press_starts[keycode]).count();
            key_press_starts.erase(keycode);
        }

        if (!first_key) {
            inter = duration_cast<milliseconds>(now - last_key_release_time).count();
            flight = inter - hold;

            if (flight > 2000) {
                if (!is_blocked) {
                    window_buffer.clear();
                    pre_catch_payload.clear();
                }
            }
            else if (flight > 0 && hold > 0) {
                if (is_blocked) {
                    bool is_new = !filesystem::exists("catch_telemetry.csv");
                    ofstream telemetry("catch_telemetry.csv", ios::app);

                    if (telemetry.is_open()) {
                        if (is_new) telemetry << "flight,inter,hold,xgboost_prob\n";
                        float prob = ai_agent->predict(flight, inter, hold);
                        telemetry << flight << "," << inter << "," << hold << "," << prob << "\n";
                    }
                } else {
                    window_buffer.push_back({flight, inter, hold});
                    if (window_buffer.size() >= WINDOW_SIZE) process_window();
                }
            }
        }

        first_key = false;
        last_key_release_time = now;

        return is_blocked ? nullptr : event;
    }

    return event;
}

string getApiKey() {
    ifstream file("api_key.txt");
    string key;
    if (file.is_open()) {
        getline(file, key);
        file.close();

        key.erase(remove(key.begin(), key.end(), '\n'), key.end());
        key.erase(remove(key.begin(), key.end(), '\r'), key.end());
    } else {
        cerr << "[ERROR] Could not find api_key.txt! Make sure it is in your root folder." << endl;
    }
    return key;
}

void clearOldForensicFiles() {
    remove("catch_telemetry.csv");
    // remove("badusb_pre_catch.log");
    // remove("badusb_post_catch.log");
}

void monitorUsbLoop() {
    sleep(2);
    int baseline = getUsbDeviceCount();

    while (true) {
        sleep(1);

        if (INPUT_BLOCKED.load()) {
            auto quiet = duration_cast<seconds>(Clock::now() - t_last_threat_activity).count();

            if (quiet > SILENCE_TIMEOUT_SECONDS) {
                cout << "\n[INFO] Threat Neutralized. Restoring User Control.\n";

                system("killall osascript > /dev/null 2>&1");
                system("say -v Samantha 'System secured.' &");
                savePreCatchForensics();

                // 1. INSTANTLY UNLOCK THE SYSTEM
                INPUT_BLOCKED.store(false);

                // 2. WIPE THE MEMORY IMMEDIATELY
                window_buffer.clear();
                key_press_starts.clear();
                pre_catch_payload.clear();
                first_key = true;
                baseline = getUsbDeviceCount();

                // 3. CAPTURE THE CURRENT ATTACK ID
                int current_attack_id = ATTACK_SEQUENCE_ID.load();

                // 4. PUSH ALL UI & REPORTS INTO A DETACHED THREAD
                thread ui_recovery_thread([current_attack_id]() {

                    // Safety Check 1
                    if (ATTACK_SEQUENCE_ID.load() != current_attack_id) return;

                    // ==========================================
                    // PHASE 1: THE GEMINI ANALYSIS
                    // ==========================================
                    if (askUserToViewSecondPopup()) {

                        if (ATTACK_SEQUENCE_ID.load() != current_attack_id) return;

                        cout << "[INFO] Establishing secure C++ connection to Gemini AI...\n";

                        // THE LOADING POPUP (ONLY ONE BUTTON: "OK")
                        // It will pause the code right here until you click it!
                        string script = R"(osascript -e 'display alert "C++ AI Bridge" message "The Gemini AI analysis is starting. This may take a few seconds..." buttons {"OK"} default button "OK" as informational')";
                        system(script.c_str());

                        if (ATTACK_SEQUENCE_ID.load() != current_attack_id) return;

                        cout << "[INFO] Gemini AI is generating the report. Please wait...\n";
                        string api_key = getApiKey();

                        // Runs synchronously. The main code waits until Gemini is done.
                        GeminiAnalyzer ai_analyst(api_key);
                        ai_analyst.generateThreatReport("badusb_post_catch.log");

                        cout << "[INFO] Gemini AI report completed!\n";

                    } else {
                        cout << "[INFO] Gemini Analysis declined by user.\n";
                    }

                    // ==========================================
                    // PHASE 2: THE TELEMETRY GRAPHS
                    // ==========================================
                    if (ATTACK_SEQUENCE_ID.load() != current_attack_id) return;

                    // This now asks the user ONLY AFTER Gemini has completely finished
                    if (askUserToViewGraphsPopup()) {

                        if (ATTACK_SEQUENCE_ID.load() != current_attack_id) return;

                        cout << "[INFO] Generating Live Telemetry Graphs...\n";
                        int ret = system("/usr/local/bin/python3.12 src/visualize_live_metrics.py");

                        if (ATTACK_SEQUENCE_ID.load() != current_attack_id) return;

                        if (ret == 0) {
                            cout << "[INFO] Graphs generated successfully! Opening folder...\n";
                            system("open images/live_attack_metrics");
                        } else {
                            cout << "[ERROR] Failed to generate graphs. Check Python path or script.\n";
                        }
                    } else {
                        cout << "[INFO] Telemetry Graphs declined by user.\n";
                    }

                    // ==========================================
                    // PHASE 3: SECURED CONFIRMATION & CLEANUP
                    // ==========================================
                    if (ATTACK_SEQUENCE_ID.load() != current_attack_id) return;

                    // Show the final reassurance popup
                    showSecuredPopup();

                    // Finally, clear the old files ONLY if we successfully finished without a new attack
                    if (ATTACK_SEQUENCE_ID.load() == current_attack_id) {
                        clearOldForensicFiles();
                    }
                });

                // Detach the entire recovery process!
                ui_recovery_thread.detach();

            }
        } else {
            int current = getUsbDeviceCount();
            if (current > baseline) {
                baseline = current;
            } else if (current < baseline) {
                baseline = current;
            }
        }
    }
}

int ejectBadUSB() {
    // runNativeModelEvaluation();

    cout << "=======================================\n";
    cout << " XGBOOST BAD-USB DEFENSE INITIALIZING\n";
    cout << "=======================================\n";

    ai_agent = new BadUSBDetector("badusb_xgboost.json");

    CGEventMask mask = kCGEventMaskForAllEvents;

    CFMachPortRef tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, static_cast<CGEventTapOptions>(0), mask, eventCallbackDisconnect, nullptr);

    if (!tap) { cerr << "FATAL: Must run with sudo!\n"; return 1; }

    gTap = tap;

    CFRunLoopSourceRef src = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), src, kCFRunLoopCommonModes);
    CGEventTapEnable(tap, true);

    thread usb(monitorUsbLoop);
    usb.detach();

    CFRunLoopRun();

    delete ai_agent;
    return 0;
}