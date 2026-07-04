#include "../include/KeystrokeCollector.h"
#include "../include/UsbUtils.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <map>
#include <ApplicationServices/ApplicationServices.h>

#include <unistd.h>

using namespace std;
using namespace std::chrono;
using Clock = steady_clock;
using TimePoint = time_point<Clock>;


namespace {

    struct PendingKeyData {
        TimePoint start_time;
        long long flight_time_ms;
        long long inter_char_delay_ms;


        constexpr int FLUSH_EVERY_N_LINES = 20;

        ofstream g_csvFile;
        CollectionLabel g_label;
        TimePoint g_last_key_down;
        TimePoint g_last_key_up;
        bool g_is_first_key = true;
        int g_lines_since_flush = 0;

        map<CGKeyCode, PendingKeyData> g_pending_keys;
        
void writeCsvHeaderIfNeeded() {
    if (g_csvFile.tellp() == 0) {
        g_csvFile << "character,flight_time_ms,inter_char_delay_ms,key_hold_time_ms,label\n";
    }
}
 
void resetSessionState() {
    g_last_key_down = TimePoint();
    g_last_key_up = TimePoint();
    g_is_first_key = true;
    g_lines_since_flush = 0;
    g_pending_keys.clear();

}

CGEventRef eventCallback(CGEventTapProxy /*proxy*/, CGEventType type, CGEventRef event, void* /*refcon*/) {
    if (type != kCGEventKeyDown && type != kCGEventKeyUp) {
        return event;
    }
    if (CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0) {
        return event;
    }

    TimePoint t_now = Clock::now();
    CGKeyCode keycode = static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
    
    if (type == kCGEventKeyDown) {
        long long flight_time = 0;
        long long inter_char_delay = 0;
 
        if (!g_is_first_key) {
            flight_time = duration_cast<milliseconds>(t_now - g_last_key_up).count();
            inter_char_delay = duration_cast<milliseconds>(t_now - g_last_key_down).count();
        }

        g_last_key_down = t_now;
        g_pending_keys[keycode] = {t_now, flight_time, inter_char_delay}
    }

    else {
        g_last_key_up = t_now;

        auto it = g_pending_keys.find(keycode);
        if (it == g_pending_keys.end()) {
            return event; // key-up sans key-down correspondant capturé (rare, on ignore)
        }
 
        PendingKeyData data = it->second;
        long long hold_time_ms = duration_cast<milliseconds>(t_now - data.start_time).count();
 
        UniChar chars[4];
        UniCharCount len = 0;
        CGEventKeyboardGetUnicodeString(event, 4, &len, chars);
        int charCode = (len > 0) ? static_cast<int>(chars[0]) : 0;
        string safeChar = getSafeChar(charCode);
 
        if (g_is_first_key) {
            g_is_first_key = false;
        }
 
        if (g_csvFile.is_open()) {
            g_csvFile << safeChar << ","
                      << data.flight_time_ms << ","
                      << data.inter_char_delay_ms << ","
                      << hold_time_ms << ","
                      << static_cast<int>(g_label) << "\n";
 
            if (++g_lines_since_flush >= FLUSH_EVERY_N_LINES) {
                g_csvFile.flush();
                g_lines_since_flush = 0;
            }
        }
 
        cout << "[" << (g_label == CollectionLabel::Human ? "HUMAN" : "BADUSB") << "] "
             << safeChar << " | flight=" << data.flight_time_ms
             << "ms inter=" << data.inter_char_delay_ms
             << "ms hold=" << hold_time_ms << "ms" << endl;
 
        g_pending_keys.erase(it);
    }
 
    return event;
}
 
void waitForNewUsbDevice() {
    int baseline = getUsbDeviceCount();
    cout << "[*] Baseline: " << baseline << " devices connected.\n";
    cout << "[*] Waiting for new USB device...\n";
 
    while (getUsbDeviceCount() <= baseline) {
        usleep(100000); // 100ms
    }
    cout << "[*] New device detected! Starting capture...\n";
}
 
void runCountdown() {
    cout << "\n[*] PREPARING TO CAPTURE...\n";
    cout << "3...\n"; this_thread::sleep_for(seconds(1));
    cout << "2...\n"; this_thread::sleep_for(seconds(1));
    cout << "1...\n"; this_thread::sleep_for(seconds(1));
    cout << "[*] START! (Type naturally now)\n";


    
string getSafeChar(int charCode) {
    if (charCode < 32 || charCode == 127) {
        switch (charCode) {
            case 13:
            case 10: return "[ENTER]";
            case 9:  return "[TAB]";
            case 27: return "[ESC]";
            case 8:  return "[BACKSPACE]";
            default: return "[CTRL]";
        }
    }
    if (charCode <= 126) {
        switch (charCode) {
            case 32: return "[SPACE]";
            case 44: return "[COMMA]";
            case 34: return "[QUOTE]";
            case 47: return "/";
            case 92: return "\\";
            default: return string(1, static_cast<char>(charCode));
        }
    }
    if (charCode >= 128 && charCode <= 255) {
        if (charCode == 167) {
            return "\xC2\xA7"; // '§' encodé en UTF-8
        }
        char utf8[3] = {0};
        utf8[0] = static_cast<char>(0xC0 | (charCode >> 6));
        utf8[1] = static_cast<char>(0x80 | (charCode & 0x3F));
        return string(utf8);
    }
    return "[UNICODE]";
}
 
int startKeystrokeCollection(CollectionLabel label, const string& outputCsvPath, bool waitForNewUsb) {
    g_label = label;
    resetSessionState();
 
    cout << "--- KEYSTROKE COLLECTOR (" << (label == CollectionLabel::Human ? "HUMAN" : "BADUSB") << ") ---\n";
    cout << "[*] Saving to: " << outputCsvPath << "\n";
 
    if (waitForNewUsb) {
        waitForNewUsbDevice();
    } else {
        runCountdown();
    }
 
    g_csvFile.open(outputCsvPath, ios::out | ios::app);
    if (!g_csvFile.is_open()) {
        cerr << "[ERROR] Could not open output file: " << outputCsvPath << "\n";
        return 1;
    }
    writeCsvHeaderIfNeeded();
 
    CGEventMask eventMask = (1 << kCGEventKeyDown) | (1 << kCGEventKeyUp) | (1 << kCGEventFlagsChanged);
    CFMachPortRef eventTap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionDefault,
        eventMask,
        eventCallback,
        nullptr
    );
 
    if (!eventTap) {
        cerr << "[ERROR] Failed to create event tap! Run with sudo and check Accessibility permissions.\n";
        return 1;
    }
 
    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);
 
    cout << "[*] Capturing... (Ctrl+C to stop)\n";
    CFRunLoopRun();
 
    g_csvFile.close();
    return 0;
}

