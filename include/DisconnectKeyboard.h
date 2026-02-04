#ifndef DESIGN_DISCONNECTKEYBOARD_H
#define DESIGN_DISCONNECTKEYBOARD_H

#include <string>
#include <ApplicationServices/ApplicationServices.h>

static const std::string QUARANTINE_LOG_FILE = "/Users/balaticinicolae/Library/CloudStorage/OneDrive-Personal/Documente/Facultate_AC_2022-2026_LICENTA/POLI_AN_4/LICENTA/design/data/badusb_keystrokes_after.txt";
static const std::string FORENSIC_LOG_FILE   = "/Users/balaticinicolae/Library/CloudStorage/OneDrive-Personal/Documente/Facultate_AC_2022-2026_LICENTA/POLI_AN_4/LICENTA/design/data/badusb_keystrokes_before.txt";

void showPopup(const std::string& message);
void saveForensics();
void activateLockdown();
CGEventRef eventCallbackDisconnect(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon);
int ejectBadUSB();
void monitorUsbLoop();

#endif