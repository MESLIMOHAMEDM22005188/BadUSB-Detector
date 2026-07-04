#include "DisconnectKeyboard.h"
#include "KeystrokeCollector.h"
#include <iostream>
#include <string>

using namespace std;

void printUsage(const char* progName) {
    cout << "Usage:\n"
         << "  " << progName << "                     
         << "  " << progName << " --collect-human
         << "  " << progName << " --collect-badusb   
}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        return ejectBadUSB();
    }

    string arg = argv[1];

    if (arg == "--collect-human") {
        return startKeystrokeCollection(CollectionLabel::Human, "../data/human_data.csv", /*waitForNewUsb=*/false);
    }
    if (arg == "--collect-badusb") {
        return startKeystrokeCollection(CollectionLabel::BadUSB, "../data/badusb_data.csv", /*waitForNewUsb=*/true);
    }
    if (arg == "--help" || arg == "-h") {
        printUsage(argv[0]);
        return 0;
    }

    cerr << "[ERROR] Unknown argument: " << arg << "\n";
    printUsage(argv[0]);
    return 1;
}