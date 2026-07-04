#ifndef KEYSTROKE_COLLECTOR_H
#define KEYSTORE_COLLECTOR_H


#include <string>
#include <fstream>
#include <ApplicationServices/ApplicationServices.h>

enum class CollectionLabel {
    Human = 0,
    BadUSB = 1
};


std::string getSafeChar(int charCode);

int startKeystrokeCollection(CollectionLabel label, const std::string& outputCsvPath, bool waitForNewUsb);


#endif