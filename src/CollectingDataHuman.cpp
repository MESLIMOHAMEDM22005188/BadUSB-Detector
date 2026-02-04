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
int current_label = 0;

using Clock = steady_clock;
using TimePoint = time_point<Clock>;

static TimePoint t_plug_in;
static TimePoint t_last_key_down;
static bool is_first_key = true;

map<CGKeyCode, TimePoint> key_press_starts_human;
struct KeyMetrics {
    long long reaction_time;
    long long inter_char_delay;
};
map<CGKeyCode, KeyMetrics> active_key_metrics;

string getSafeChar(int charCode) {
    string safeChar;
    switch (charCode) {
        case 32: safeChar = "[SPACE]"; break;
        case 13:
        case 10: safeChar = "[ENTER]"; break;
        case 9:  safeChar = "[TAB]"; break;
        case 44: safeChar = "[COMMA]"; break;
        case 27: safeChar = "[ESC]"; break;
        default:
            if (charCode < 32) {
                safeChar = "[CTRL]";
            } else {
                safeChar = string(1, (char)charCode);
            }
            break;
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
        long long reaction = 0;
        long long inter_char = 0;

        if (is_first_key) {
            reaction = duration_cast<milliseconds>(t_now - t_plug_in).count();
            inter_char = 0;
        } else {
            inter_char = duration_cast<milliseconds>(t_now - t_last_key_down).count();
        }

        t_last_key_down = t_now;
        active_key_metrics[keycode] = {reaction, inter_char};
        key_press_starts_human[keycode] = t_now;

        if (is_first_key) cout << "REACTION: " << reaction << "ms" << endl;
        else cout << "DELAY: " << inter_char << "ms" << endl;
    }

    else if (type == kCGEventKeyUp) {
        long long hold_time = 0;
        if (key_press_starts_human.find(keycode) != key_press_starts_human.end()) {
            hold_time = duration_cast<milliseconds>(t_now - key_press_starts_human[keycode]).count();
            key_press_starts_human.erase(keycode);
        }

        long long final_reaction = 0;
        long long final_inter_char = 0;

        if (active_key_metrics.find(keycode) != active_key_metrics.end()) {
            final_reaction = active_key_metrics[keycode].reaction_time;
            final_inter_char = active_key_metrics[keycode].inter_char_delay;
            active_key_metrics.erase(keycode);
        }

        UniChar chars[4];
        UniCharCount len;
        CGEventKeyboardGetUnicodeString(event, 4, &len, chars);
        int charCode = (len > 0) ? static_cast<int>(chars[0]) : 0;
        string safeChar = getSafeChar(charCode);

        if (is_first_key) is_first_key = false;

        if (csvFile_human.is_open()) {
            csvFile_human << safeChar << ","
                    << final_reaction << ","
                    << final_inter_char << ","
                    << hold_time << ","
                    << current_label << "\n";
            csvFile_human.flush();
        }

        cout << "Saved: " << safeChar << " (" << hold_time << "ms hold)" << endl;
    }

    return event;
}

void startDataCollectionHuman() {
    system("mkdir -p data");

    cout << "--- THESIS DATA COLLECTOR ---\n";

    ifstream checkFile(CSV_FILE_HUMAN_DATA);
    bool exists = checkFile.good();
    checkFile.close();

    csvFile_human.open(CSV_FILE_HUMAN_DATA, ios::out | ios::app);
    if (!exists) {
        csvFile_human << "character,reaction_time_ms,inter_char_delay_ms,key_hold_time_ms,label\n";
    }

    cout << "\n[*] PREPARING TO CAPTURE...\n";
    cout << "[*] Saving to: " << CSV_FILE_HUMAN_DATA << "\n";
    cout << "3...\n"; this_thread::sleep_for(chrono::seconds(1));
    cout << "2...\n"; this_thread::sleep_for(chrono::seconds(1));
    cout << "1...\n"; this_thread::sleep_for(chrono::seconds(1));
    cout << "[*] START! (Type now)\n";

    t_plug_in = Clock::now();
    is_first_key = true;

    CGEventMask eventMask = (1 << kCGEventKeyDown) | (1 << kCGEventKeyUp);
    CFMachPortRef eventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, static_cast<CGEventTapOptions>(0), eventMask, eventCallbackHUMAN, NULL);

    if (!eventTap) {
        cerr << "[ERROR] Failed to create Event Tap. Run with sudo!\n";
        exit(1);
    }

    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);

    CFRunLoopRun();
}