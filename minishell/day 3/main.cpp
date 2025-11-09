#include "include/shell.h"
#include <iostream>
#include <string>
#include <algorithm>

static std::string stripTrailingAmpersand(const std::string &input, bool &background) {
    // Remove trailing spaces
    size_t end = input.find_last_not_of(" \t\n\r");
    if (end == std::string::npos) {
        background = false;
        return "";
    }
    size_t pos = end;
    // If final non-space char is &, mark background
    if (input[pos] == '&') {
        background = true;
        // return everything up to the & (trim trailing spaces before & too)
        size_t new_end = input.find_last_not_of(" \t\n\r", pos-1);
        if (new_end == std::string::npos) return "";
        return input.substr(0, new_end + 1);
    } else {
        background = false;
        return input.substr(0, end + 1);
    }
}

int main() {
    initShellSignals();

    while (true) {
        checkBackgroundJobs(); // reap before prompt
        displayPrompt();
        std::string input = readInput();
        if (input.empty() && std::cin.eof()) {
            std::cout << "\n";
            break;
        }
        // Detect trailing & for background execution
        bool background = false;
        std::string cleaned = stripTrailingAmpersand(input, background);
        // Parse cleaned input into args
        std::vector<std::string> args = parseInput(cleaned);
        int status = executeCommand(args, background, cleaned);
        if (status == 0) break; // exit requested
    }
    return 0;
}

