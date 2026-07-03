#include <ApplicationServices/ApplicationServices.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <map>
#include <thread>

using namespace std;
using namespace std::chrono;

const string CSV_FILE_HUMAN_DATA = "../data/human_data.csv";

ofstream csvFile_human;
const int LABEL_HUMAN = 0;

using Clock = steady_clock;
using TimePoint = time_point<Clock>;

static TimePoint t_last_global_down;
static TimePoint t_last_global_up;
static bool is_first_key = true;

struct PendingKeyData {
    TimePoint start_time;
    long long flight_time;
    long long dd_time;
};
map<CGKeyCode, PendingKeyData> pending_keys;

string getSafeChar(int charCode) {
    string safeChar;

    if (charCode < 32 || charCode == 127) {
        switch (charCode) {
            case 13:
            case 10: safeChar = "[ENTER]"; break;
            case 9:  safeChar = "[TAB]"; break;
            case 27: safeChar = "[ESC]"; break;
            case 8:  safeChar = "[BACKSPACE]"; break;
            default: safeChar = "[CTRL]"; break;
        }
    }
    else if (charCode <= 126) {
        switch (charCode) {
            case 32: safeChar = "[SPACE]"; break;
            case 44: safeChar = "[COMMA]"; break;
            case 34: safeChar = "[QUOTE]"; break;
            case 47: safeChar = "/"; break;
            case 92: safeChar = "\\"; break;
            default: safeChar = string(1, (char)charCode); break;
        }
    }
    else if (charCode >= 128 && charCode <= 255) {
        if (charCode == 167) {
             safeChar = "§";
        } else {
            char utf8[3] = {0};
            utf8[0] = static_cast<char>(0xC0 | (charCode >> 6));
            utf8[1] = static_cast<char>(0x80 | (charCode & 0x3F));
            safeChar = string(utf8);
        }
    }
    else {
        safeChar = "[UNICODE]";
    }
    return safeChar;
}

CGEventRef eventCallbackHUMAN(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    if (CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0) {
        return event;
    }

    TimePoint t_now = Clock::now();
    CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

    if (type == kCGEventKeyDown) {
        long long flight_time = 0;
        long long dd_time = 0;

        if (!is_first_key) {
            flight_time = duration_cast<milliseconds>(t_now - t_last_global_up).count();
            dd_time = duration_cast<milliseconds>(t_now - t_last_global_down).count();
        }
        t_last_global_down = t_now;
        pending_keys[keycode] = {t_now, flight_time, dd_time};
        cout << "[DOWN] Flight: " << flight_time << "ms | DD: " << dd_time << "ms" << endl;
    }

    else if (type == kCGEventKeyUp) {
        t_last_global_up = t_now;
        if (pending_keys.find(keycode) != pending_keys.end()) {
            PendingKeyData data = pending_keys[keycode];
            long long hold_time = duration_cast<milliseconds>(t_now - data.start_time).count();
            UniChar chars[4];
            UniCharCount len;
            CGEventKeyboardGetUnicodeString(event, 4, &len, chars);
            int charCode = (len > 0) ? static_cast<int>(chars[0]) : 0;
            string safeChar = getSafeChar(charCode);
            if (is_first_key) is_first_key = false;
            if (csvFile_human.is_open()) {
                csvFile_human << safeChar << ","
                        << data.flight_time << ","
                        << data.dd_time << ","
                        << hold_time << ","
                        << LABEL_HUMAN << "\n";
                csvFile_human.flush();
            }
            cout << "Saved: " << safeChar << " (Hold: " << hold_time << "ms)" << endl;
            pending_keys.erase(keycode);
        }
    }

    return event;
}

void startDataCollectionHuman() {
    system("mkdir -p ../data");

    cout << "--- HUMAN DATA COLLECTOR ---\n";

    csvFile_human.open(CSV_FILE_HUMAN_DATA, ios::out | ios::app);

    if (csvFile_human.tellp() == 0) {
        csvFile_human << "character,flight_time_ms,inter_char_delay_ms,key_hold_time_ms,label\n";
    }

    cout << "\n[*] PREPARING TO CAPTURE...\n";
    cout << "[*] Saving to: " << CSV_FILE_HUMAN_DATA << "\n";
    cout << "3...\n"; this_thread::sleep_for(chrono::seconds(1));
    cout << "2...\n"; this_thread::sleep_for(chrono::seconds(1));
    cout << "1...\n"; this_thread::sleep_for(chrono::seconds(1));
    cout << "[*] START! (Type naturally now)\n";

    t_last_global_down = TimePoint();
    t_last_global_up = TimePoint();
    is_first_key = true;
    pending_keys.clear();

    CGEventMask eventMask = (1 << kCGEventKeyDown) | (1 << kCGEventKeyUp) | (1 << kCGEventFlagsChanged);
    CFMachPortRef eventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, static_cast<CGEventTapOptions>(0),
        eventMask, eventCallbackHUMAN, NULL);

    if (!eventTap) {
        cerr << "Failed to create Event Tap. Run with sudo!\n";
        exit(1);
    }

    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);

    CFRunLoopRun();
}