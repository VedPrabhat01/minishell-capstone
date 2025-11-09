// main.cpp (Day 5)
#include "include/shell.h"
#include <iostream>
#include <string>

// Trim trailing whitespace and detect a trailing '&'.
// Returns the trimmed command line (without the trailing '&' if present).
static std::string stripTrailingAmpersand(const std::string &input, bool &background) {
    size_t last_non_space = input.find_last_not_of(" \t\r\n");
    if (last_non_space == std::string::npos) {
        background = false;
        return "";
    }
    if (input[last_non_space] == '&') {
        background = true;
        if (last_non_space == 0) return "";
        size_t new_end = input.find_last_not_of(" \t\r\n", last_non_space - 1);
        if (new_end == std::string::npos) return "";
        return input.substr(0, new_end + 1);
    }
    background = false;
    return input.substr(0, last_non_space + 1);
}

int main() {
    // Set up signal handlers and load history
    initShellSignals();
    loadHistory();

    // main loop
    while (true) {
        checkBackgroundJobs();
        displayPrompt();
        std::string rawLine = readInput();

        if (rawLine.empty() && std::cin.eof()) {
            std::cout << "\n";
            break;
        }
        if (rawLine.empty()) continue;

        bool runInBackground = false;
        std::string cleaned = stripTrailingAmpersand(rawLine, runInBackground);

        int status = executeLine(cleaned, runInBackground);
        if (status == 0) break; // executeLine saved history already when exit invoked
    }

    // Ensure history saved if user killed shell by EOF or other means
    saveHistory();
    return 0;
}

