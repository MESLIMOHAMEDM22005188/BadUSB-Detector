#include "../include/CollectingDataBadUSB.h"
#include <iostream>
#include <chrono>
#include <ApplicationServices/ApplicationServices.h>
#include <fstream>

using namespace std;
using namespace std::chrono;

ofstream csvFile;
const string CSV_FILE_BADUSB_DATA = "../data/badusb_data.csv";

using Clock = steady_clock;
using TimePoint = time_point<Clock>;

static TimePoint t_last_key_down;
static TimePoint t_last_key_up;
static bool is_first_key = true;

void setConnectionTime() {
    t_last_key_down = TimePoint();
    t_last_key_up = TimePoint();
    is_first_key = true;

    csvFile.open(CSV_FILE_BADUSB_DATA, ios::out | ios::app);
    if (csvFile.tellp() == 0) {
        csvFile << "character,flight_time_ms,inter_char_delay_ms,key_hold_time_ms,label\n";
    }
}

void measureAndRecordMetrics(int charCode, int eventType) {
    TimePoint t_now = Clock::now();
    static TimePoint t_current_press_start;

    static long long stored_inter_char_delay = 0;
    static long long stored_flight_time = 0;

    if (eventType == 1) {
        t_current_press_start = t_now;

        if (!is_first_key) {
            stored_inter_char_delay = chrono::duration_cast<milliseconds>(t_now - t_last_key_down).count();
            cout << "RHYTHM DELAY: " << stored_inter_char_delay << "ms" << endl;

            stored_flight_time = chrono::duration_cast<milliseconds>(t_now - t_last_key_up).count();
            cout << "FLIGHT TIME:  " << stored_flight_time << "ms" << endl;
        } else {
            stored_inter_char_delay = 0;
            stored_flight_time = 0;
        }

        t_last_key_down = t_now;

    }
    else if (eventType == 0) {
        long long key_hold_time_ms = chrono::duration_cast<milliseconds>(t_now - t_current_press_start).count();
        cout << "[METRIC] HOLD DURATION: " << key_hold_time_ms << "ms" << endl;

        t_last_key_up = t_now;

        if (is_first_key) {
            is_first_key = false;
        }

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
                case 92: safeChar = "\\"; break;
                default: safeChar = string(1, (char)charCode); break;
            }
        }
        else if (charCode >= 128 && charCode <= 255) {
            char utf8[3] = {0};
            utf8[0] = static_cast<char>(0xC0 | (charCode >> 6));
            utf8[1] = static_cast<char>(0x80 | (charCode & 0x3F));
            safeChar = string(utf8);
        }
        else {
            safeChar = "[UNICODE]";
        }

        if (csvFile.is_open()) {
            csvFile << safeChar << ","
                    << stored_flight_time << ","
                    << stored_inter_char_delay << ","
                    << key_hold_time_ms << ","
                    << "1" << "\n";
            csvFile.flush();
        }
    }
}

CGEventRef eventCallbackBADUSB(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    if (type != kCGEventKeyDown && type != kCGEventKeyUp) {
        return event;
    }

    UniChar chars[4];
    UniCharCount actualLength;
    CGEventKeyboardGetUnicodeString(event, 4, &actualLength, chars);
    int charCode = (actualLength > 0) ? static_cast<int>(chars[0]) : 0;

    if (type == kCGEventKeyDown && actualLength > 0) {
        char c = static_cast<char>(chars[0]);
        cout << c << flush;
    }

    if (type == kCGEventKeyDown) {
        measureAndRecordMetrics(charCode, 1);
    } else {
        measureAndRecordMetrics(charCode, 0);
    }

    return event;
}

int startDataCollectionBadUSB(){
    cout << "\"CAPTURING INPUTS\" Mode" << endl;

    int baseline = getUsbDeviceCount();
    cout << "[*] Baseline: " << baseline << " devices." << endl;

    while (true) {
        if (getUsbDeviceCount() > baseline) {
            cout << "\nNew device detected! Starting capture..." << endl;
            setConnectionTime();
            break;
        }
        usleep(100000);
    }

    if (!csvFile.is_open()) {
        csvFile.open(CSV_FILE_BADUSB_DATA, ios::out | ios::trunc);
    }

    CGEventMask eventMask = (1 << kCGEventKeyDown) | (1 << kCGEventKeyUp) | (1 << kCGEventFlagsChanged);
    CFMachPortRef eventTap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionDefault,
        eventMask,
        eventCallbackBADUSB,
        nullptr
    );

    if (!eventTap) {
        cerr << "ERROR: Failed to create event tap! Check permissions." << endl;
        return 1;
    }

    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);

    cout << "Capturing..." << endl;
    cout << "Saving to: " << CSV_FILE_BADUSB_DATA << endl;

    CFRunLoopRun();
    return 0;
}