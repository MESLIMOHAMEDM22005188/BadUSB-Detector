#include "DisconnectKeyboard.h"
#include <LogisticModel.h>
#include "UsbUtils.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>
#include <map>
#include <vector>
#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>

using namespace std;
using namespace std::chrono;

constexpr int WINDOW_SIZE = 5;
constexpr double THRESHOLD = 0.5;
constexpr int SILENCE_TIMEOUT_SECONDS = 5;

atomic INPUT_BLOCKED{false};
using Clock = steady_clock;

static CFMachPortRef gTap = nullptr;

map<CGKeyCode, Clock::time_point> key_press_starts;
map<CGKeyCode, long long> key_delays;
bool first_key = true;
Clock::time_point last_key_down_time;
Clock::time_point t_last_threat_activity = Clock::now();
string captured_payload;

struct KeyData { long long delay; long long hold; };
vector<KeyData> window_buffer;

void showPopup(const string& message) {
    system("killall osascript > /dev/null 2>&1");
    system("say -v Samantha 'Security Alert.' &");
    string command = "osascript -e 'display alert \"⚠️ MALICIOUS USB DETECTED\" message \"" + message +
                     "\" as critical buttons {\"Locked\"} default button \"Locked\"' &";
    system(command.c_str());
}

void saveForensics() {
    if (captured_payload.empty()) return;
    ofstream out(FORENSIC_LOG_FILE, ios::app);
    if (out.is_open()) {
        out << "\n===== NEW INCIDENT =====\n" << captured_payload << "\n";
        out.close();
    }
    captured_payload.clear();
}

void activateLockdown(const string& source) {
    if (!INPUT_BLOCKED.load()) {
        INPUT_BLOCKED.store(true);
        cout << "\n[!!!] THREAT DETECTED BY " << source << " [!!!]\n";
        saveForensics();
        system("diskutil eject /Volumes/* > /dev/null 2>&1");
        showPopup("Malicious activity detected by " + source + ". The pop-up will be closed after "
            + to_string(SILENCE_TIMEOUT_SECONDS) + " seconds of no activity.");
    }
}

void process_window() {
    if (window_buffer.size() < WINDOW_SIZE) return;

    double sum_delay = 0, sum_hold = 0;
    double min_delay = 1e9, max_delay = 0;
    for (const auto& k : window_buffer) {
        sum_delay += k.delay; sum_hold += k.hold;
        min_delay = min(min_delay, (double)k.delay);
        max_delay = max(max_delay, (double)k.delay);
    }
    double avg_delay = sum_delay / WINDOW_SIZE;
    double avg_hold = sum_hold / WINDOW_SIZE;
    double var_sum = 0;
    for (const auto& k : window_buffer) var_sum += (k.delay - avg_delay) * (k.delay - avg_delay);
    double std_delay = sqrt(var_sum / WINDOW_SIZE);

    double input[5] = {avg_delay, std_delay, min_delay, max_delay, avg_hold};
    double prob = score(input);

    if (prob > THRESHOLD) {
        activateLockdown("AI Algorithm");
    }
    window_buffer.clear();
}

CGEventRef eventCallbackDisconnect(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void*) {

    if (type == kCGEventTapDisabledByTimeout) {
        cout << "[WARNING] Event Tap timed out. Re-enabling..." << endl;
        if (gTap) CGEventTapEnable(gTap, true);
        return event;
    }
    // if (type == kCGEventTapDisabledByUserInterest) {
    //     cout << "[WARNING] Event Tap disabled by user interest. Re-enabling..." << endl;
    //     if (gTap) CGEventTapEnable(gTap, true);
    //     return event;
    // }

    auto now = Clock::now();

    if (INPUT_BLOCKED.load()) {
        t_last_threat_activity = now;

        if (type == kCGEventKeyDown) {
            UniChar chars[4]; UniCharCount len;
            CGEventKeyboardGetUnicodeString(event, 4, &len, chars);
            if (len > 0) {
                 ofstream q(QUARANTINE_LOG_FILE, ios::out | ios::app);
                 if (q.is_open()) {
                    char c = (char)chars[0];
                    if (c == 13) q << "\n"; else q << c;
                 }
            }
        }
        return nullptr;
    }

    if (type != kCGEventKeyDown && type != kCGEventKeyUp) return event;
    if (CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0) return event;

    CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

    if (type == kCGEventKeyDown) {
        UniChar chars[4]; UniCharCount len;
        CGEventKeyboardGetUnicodeString(event, 4, &len, chars);
        if (len > 0) {
            char c = (char)chars[0];
            if (c >= 32 || c == 13) captured_payload += (c==13 ? '\n' : c);
        }

        long long delay = 0;
        if (!first_key) delay = duration_cast<milliseconds>(now - last_key_down_time).count();
        first_key = false;
        last_key_down_time = now;
        key_press_starts[keycode] = now;
        key_delays[keycode] = delay;
    }
    else if (type == kCGEventKeyUp) {
        long long hold = 0, delay = 0;
        if (key_press_starts.count(keycode)) {
            hold = duration_cast<milliseconds>(now - key_press_starts[keycode]).count();
            key_press_starts.erase(keycode);
        }
        if (key_delays.count(keycode)) {
            delay = key_delays[keycode];
            key_delays.erase(keycode);
        }
        window_buffer.push_back({delay, hold});
        if (window_buffer.size() >= WINDOW_SIZE) process_window();
    }

    return event;
}

void monitorUsbLoop() {
    sleep(2);
    int baseline = getUsbDeviceCount();

    while (true) {
        sleep(1);

        if (INPUT_BLOCKED.load()) {
            // Calculate how long it has been quiet
            auto quiet = duration_cast<seconds>(Clock::now() - t_last_threat_activity).count();

            // Just check if the time has expired (No visible countdown)
            if (quiet > SILENCE_TIMEOUT_SECONDS) {
                cout << "\n[INFO] Threat neutralized. Resetting...\n";
                system("killall osascript > /dev/null 2>&1");
                system("say -v Samantha 'System secured.' &");

                saveForensics();

                window_buffer.clear();
                key_press_starts.clear();
                key_delays.clear();
                captured_payload.clear();
                first_key = true;

                baseline = getUsbDeviceCount();
                INPUT_BLOCKED.store(false);
            }
        } else {
            int current = getUsbDeviceCount();
            if (current > baseline) {
                activateLockdown("USB Watchdog");
                baseline = current;
            } else if (current < baseline) {
                baseline = current;
            }
        }
    }
}

int ejectBadUSB() {
    cout << "BadUSB Defense running\n";

    CGEventMask mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) |
                       CGEventMaskBit(kCGEventLeftMouseDown) | CGEventMaskBit(kCGEventLeftMouseUp) |
                       CGEventMaskBit(kCGEventRightMouseDown) | CGEventMaskBit(kCGEventRightMouseUp) |
                       CGEventMaskBit(kCGEventMouseMoved) |
                       CGEventMaskBit(kCGEventLeftMouseDragged) | CGEventMaskBit(kCGEventRightMouseDragged) |
                       CGEventMaskBit(kCGEventScrollWheel) |
                       CGEventMaskBit(kCGEventOtherMouseDown) | CGEventMaskBit(kCGEventOtherMouseUp);

    // Using static_cast<CGEventTapOptions>(0) as per your setup
    CFMachPortRef tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, static_cast<CGEventTapOptions>(0), mask, eventCallbackDisconnect, nullptr);

    if (!tap) { cerr << "Run with sudo\n"; return 1; }

    gTap = tap;

    CFRunLoopSourceRef src = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), src, kCFRunLoopCommonModes);
    CGEventTapEnable(tap, true);

    thread usb(monitorUsbLoop);
    usb.detach();

    CFRunLoopRun();
    return 0;
}