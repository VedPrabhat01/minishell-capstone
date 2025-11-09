
#include "include/shell.h"
#include <iostream>
#include <string>
#include <cctype>

// Trim trailing whitespace and detect a trailing '&'.
// Returns the trimmed command line (without the trailing '&' if present).
// Sets `background` to true when the command ends with '&'.
static std::string stripTrailingAmpersand(const std::string &input, bool &background) {
    // find last non-space character
    size_t last_non_space = input.find_last_not_of(" \t\r\n");
    if (last_non_space == std::string::npos) {
        background = false;
        return ""; // empty input or only whitespace
    }

    // if the last non-space char is '&', treat command as background
    if (input[last_non_space] == '&') {
        background = true;
        // find last non-space before the ampersand
        if (last_non_space == 0) return ""; // only '&' found
        size_t new_end = input.find_last_not_of(" \t\r\n", last_non_space - 1);
        if (new_end == std::string::npos) return "";
        return input.substr(0, new_end + 1);
    }

    // no ampersand: return trimmed string
    background = false;
    return input.substr(0, last_non_space + 1);
}

int main() {
    // Set up signal handlers for the shell (defined in shell.cpp)
    initShellSignals();

    // Main loop: display prompt, read line, parse background flag, execute
    while (true) {
        // Check for any background jobs that changed state (reap children, print notifications)
        checkBackgroundJobs();

        // Show prompt and flush so prompt appears immediately
        displayPrompt();

        // Read one line from user. readInput() also records history.
        std::string rawLine = readInput();

        // If EOF detected (Ctrl+D), readInput returns empty and cin.eof() is true.
        if (rawLine.empty() && std::cin.eof()) {
            std::cout << "\n"; // nice newline before exit
            break;
        }

        // Accept empty lines (just re-prompt)
        if (rawLine.empty()) {
            continue;
        }

        // Determine whether the user requested background execution ("&")
        bool runInBackground = false;
        std::string cleanedLine = stripTrailingAmpersand(rawLine, runInBackground);

        // Execute the entire line. executeLine handles built-ins, pipes, redirection, jobs.
        // It returns 0 when the built-in 'exit' was issued (so we should quit the shell).
        int result = executeLine(cleanedLine, runInBackground);
        if (result == 0) break;
    }

    return 0;
}

