#include "GeminiAnalyzer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdlib>

using json = nlohmann::json;
using namespace std;
namespace fs = std::filesystem;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string getUniqueReportName() {
    auto t = time(nullptr);
    auto tm = *localtime(&t);
    ostringstream oss;
    oss << "Threat_Report_" << put_time(&tm, "%Y-%m-%d_%H-%M-%S") << ".txt";
    return oss.str();
}

void showLoadingAnimation(std::atomic<bool>& is_done) {
    const int width = 40;
    int progress = 0;

    while (!is_done) {
        std::cout << "\r[INFO] Gemini AI Analyzing Payload [";
        int pos = progress % width;
        for (int i = 0; i < width; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << std::flush;

        progress++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\r[INFO] Gemini AI Analyzing Payload [========================================] COMPLETE!   \n";
}

GeminiAnalyzer::GeminiAnalyzer(const string& key) : api_key(key) {}

void GeminiAnalyzer::generateThreatReport(const string& log_filepath) {
    cout << "\n[INFO] Establishing Secure Connection to Google Cloud...\n";

    ifstream log_file(log_filepath);
    string payload;
    if (log_file.is_open()) {
        ostringstream ss;
        ss << log_file.rdbuf();
        payload = ss.str();
    } else {
        payload = "No payload captured or file missing.";
    }

    string prompt = "You are an elite Cybersecurity Forensic Analyst. My C++ endpoint detection system just intercepted a BadUSB attack on a macOS machine. Here is the raw payload:\n\n" +
                payload +
                "\n\nProvide a concise threat report explaining what this payload is trying to do. "
                "IMPORTANT: Use only plain text. Do not use Markdown formatting, do not use asterisks for bolding, and do not use hashtags for headers. "
                "Format: 1. Threat Summary 2. Step-by-step breakdown 3. Potential Impact.";

    json request_json = {
        {"contents", {{
            {"parts", {{ {"text", prompt} }}}
        }}}
    };

    string json_string = request_json.dump();
    string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" + api_key;

    CURL* curl = curl_easy_init();
    string readBuffer;
    string analysis = "";

    if (curl) {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_string.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

        // system("osascript -e 'display dialog \"Analyzing Malicious Payload...\\n\\nPlease wait while the forensic threat report is generated.\" buttons {} with icon note' &");
        std::atomic<bool> is_done(false);
        std::thread loader_thread(showLoadingAnimation, std::ref(is_done));

        CURLcode res = curl_easy_perform(curl);

        is_done = true;
        loader_thread.join();
        system("killall osascript > /dev/null 2>&1");

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            analysis = "SYSTEM DIAGNOSTIC: NETWORK FAILURE\n\n";
            analysis += "The C++ EDR agent successfully blocked the BadUSB attack, but could not reach the Gemini Cloud for analysis.\n";
            analysis += "CURL Error: " + string(curl_easy_strerror(res)) + "\n";
            analysis += "Please check your internet connection and try again.";
        } else {
            try {
                json response_json = json::parse(readBuffer);

                if (response_json.contains("candidates")) {
                    analysis = response_json["candidates"][0]["content"]["parts"][0]["text"];
                } else if (response_json.contains("error")) {
                    analysis = "SYSTEM DIAGNOSTIC: API REJECTION\n\n";
                    analysis += "Google Cloud rejected the analysis request.\n";
                    analysis += "API Message: " + response_json["error"]["message"].get<string>() + "\n";
                    analysis += "Status Code: " + response_json["error"]["status"].get<string>() + "\n";
                } else {
                    analysis = "SYSTEM DIAGNOSTIC: UNKNOWN FORMAT\n\n";
                    analysis += "Received an unrecognized response from the AI servers.\n";
                    analysis += "Raw Dump:\n" + readBuffer;
                }
            } catch (const exception& e) {
                analysis = "SYSTEM DIAGNOSTIC: PARSING EXCEPTION\n\n";
                analysis += "The C++ JSON parser crashed while reading the server response.\n";
                analysis += "Exception Details: " + string(e.what()) + "\n";
            }
        }
    } else {
        analysis = "SYSTEM DIAGNOSTIC: INITIALIZATION ERROR\n\nFailed to initialize libcurl. The network subsystem is down.";
    }

    string local_folder = "Threat_Reports";
    if (!fs::exists(local_folder)) {
        fs::create_directory(local_folder);
    }

    string file_name = getUniqueReportName();
    string local_report_path = local_folder + "/" + file_name;

    ofstream local_report(local_report_path);
    if (local_report.is_open()) {
        local_report << analysis << "\n";
        local_report.close();

        const char* home_dir = getenv("HOME");
        string desktop_report_path = local_report_path;

        if (home_dir) {
            string desktop_folder = string(home_dir) + "/Desktop/Threat_Reports";

            if (!fs::exists(desktop_folder)) {
                fs::create_directory(desktop_folder);
            }

            desktop_report_path = desktop_folder + "/" + file_name;
            ofstream desktop_report(desktop_report_path);
            if (desktop_report.is_open()) {
                desktop_report << analysis << "\n";
                desktop_report.close();
            }
        }

        string command = "open " + desktop_report_path;
        system(command.c_str());
        cout << "[SUCCESS] Diagnostic/Threat Report saved to Desktop: " << desktop_report_path << "\n\n";

        system("osascript -e 'display alert \"Report Created!\" message \"The AI Threat Intelligence Report has been generated and saved to the \\\"Threat_Reports\\\" folder on your Desktop.\" as informational' &");

    } else {
        cerr << "[FATAL ERROR] Could not write to disk!\n";
        system("osascript -e 'display alert \"Critical Error\" message \"The agent blocked the attack but could not save the report to your hard drive due to a permissions error.\" as critical' &");
    }
}