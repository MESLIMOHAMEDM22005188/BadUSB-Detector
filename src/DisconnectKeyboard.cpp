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
constexpr int WINDOW_SIZE = 15;             // dictates the AI analyzes blocks of 15 keys
constexpr double THRESHOLD = 0.75;          // the XGBoost confidence required to trigger a lockdown
constexpr int SILENCE_TIMEOUT_SECONDS = 10;  // safety timer
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
    system("killall osascript > /dev/null 2>&1"); // Închide cu forța orice alt pop-up macOS deschis anterior, trimițând eventualele erori în neant (> /dev/null).
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
    while (!feof(pipe)) {
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
    while (!feof(pipe)) {
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

// Modele precum Support Vector Machine (SVM) sau Regresia Logistică nu scot la ieșire „90% malițios”,
// ci un număr real (ex: 2.5 sau -1.2). Această funcție transformă acel număr într-o probabilitate ușor de înțeles de către om,
// între 0.0 și 1.0 (adică 0% - 100%).
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
        // Demontează/ejectează forțat și instantaneu toate memoriile USB conectate la Mac
        // Rularea se face în fundal (&) pentru a nu îngheța programul C++ cât timp macOS se chinuie să demonteze partițiile
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

// se creeaza probabilitatile
void process_window() {
    if (window_buffer.size() < WINDOW_SIZE) return; // it checks if the window_buffer has collected enough keystrokes (15 keys)

    int malicious_keystroke_count = 0;
    double last_rf_prob = 0, last_svm_prob = 0, last_nn_prob = 0, last_log_prob = 0;
    float last_xgb_prob = 0;

    // Intră într-o buclă care ia pe rând fiecare dintre cele 8 taste din buffer (k)
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
        // Un algoritm Random Forest este compus din zeci/sute de "arbori de decizie" independenți.
        // Funcția ta se uită câți arbori au votat că tasta e "Umană" (rf_output[0]) și câți au votat "BadUSB"
        // (rf_output[1]). Probabilitatea finală este procentul de arbori care au votat pentru atac.
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
    if (malicious_keystroke_count >= 3) {
        // Pass the precise math to the visual UI!
        activateLockdown(last_xgb_prob, last_rf_prob, last_svm_prob, last_nn_prob, last_log_prob);
        window_buffer.clear();
    } else {
        // Dacă nu s-a detectat niciun atac, nu golim toată fereastra. Ștergem doar cea mai veche tastă din buffer (erase(begin())).
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
            } else { // salvare in pre_catch_payload
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
            // Când tasta se ridică, funcția scade din momentul curent (now) momentul apăsării (stocat anterior). Apoi, curăță tasta din dicționar.
            hold = duration_cast<milliseconds>(now - key_press_starts[keycode]).count();
            key_press_starts.erase(keycode);
        }

        if (!first_key) {
            inter = duration_cast<milliseconds>(now - last_key_release_time).count();
            flight = inter - hold;
            // Dacă nu suntem la prima tastă tastată, se calculează Inter-key Time (distanța de la ridicarea tastei anterioare)
            // și Flight Time (timpul de zbor între taste)

            if (flight > 2000) {
                //Dacă o persoană face o pauză mai mare de 2 secunde între taste (2000 milisecunde), sistemul curăță buffer-ul glisant.
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

                INPUT_BLOCKED.store(false);

                window_buffer.clear();
                key_press_starts.clear();
                pre_catch_payload.clear();
                first_key = true;
                baseline = getUsbDeviceCount();

                int current_attack_id = ATTACK_SEQUENCE_ID.load();
                thread ui_recovery_thread([current_attack_id]() {

                    if (ATTACK_SEQUENCE_ID.load() != current_attack_id) return;
                    if (askUserToViewSecondPopup()) {
                        if (ATTACK_SEQUENCE_ID.load() != current_attack_id) return;

                        cout << "[INFO] Consolidating forensic data for Gemini AI analysis...\n";

                        string combinedFile = "full_threat_analysis.log";
                        ofstream outfile(combinedFile);

                        ifstream pre("badusb_pre_catch.log");
                        if (pre.is_open()) {
                            outfile << "--- SECVENȚA DE PRE-DETECȚIE (TRIGGER) ---\n";
                            outfile << pre.rdbuf() << "\n\n";
                            pre.close();
                        }

                        ifstream post("badusb_post_catch.log");
                        if (post.is_open()) {
                            outfile << "--- SECVENȚA DE POST-DETECȚIE (INPUTUL ATACATORULUI ÎN BLOCARE) ---\n";
                            outfile << post.rdbuf() << "\n";
                            post.close();
                        }
                        outfile.close();

                        cout << "[INFO] Establishing secure C++ connection to Gemini AI...\n";
                        string api_key = getApiKey();
                        GeminiAnalyzer ai_analyst(api_key);

                        ai_analyst.generateThreatReport(combinedFile);
                        remove(combinedFile.c_str());

                        cout << "[INFO] Gemini AI report completed!\n";
                    } else {
                        cout << "[INFO] Gemini Analysis declined by user.\n";
                    }

                    if (ATTACK_SEQUENCE_ID.load() != current_attack_id) return;
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

                    if (ATTACK_SEQUENCE_ID.load() != current_attack_id) return;
                    showSecuredPopup();
                    if (ATTACK_SEQUENCE_ID.load() == current_attack_id) {
                        clearOldForensicFiles();
                    }
                });
                ui_recovery_thread.detach();

            }
        } else {
            // Aceasta este ramura else a condiției if (INPUT_BLOCKED.load()). Dacă sistemul NU este sub atac (merge normal),
            // bucla pur și simplu numără câte dispozitive USB sunt în calculator. Dacă bagi sau scoți un stick USB normal,
            // actualizează baseline-ul (noul normal), pregătindu-se pentru momentul în care AI-ul va declanșa o alarmă.
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

    // kCGHeadInsertEventTap (Foarte important): Acest parametru spune sistemului de operare să pună sonda ta în capul listei de interceptori.
    // eventCallbackDisconnect: Este "adresa" funcției tale. Îi spune sistemului: "Ori de câte ori apare o tastă, trimite-o mai întâi la această funcție a mea".
    // kCGSessionEventTap: Indică faptul că monitorizezi evenimentele la nivelul sesiunii grafice a utilizatorului curent.
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