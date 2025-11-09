#include "include/shell.h"
#include <iostream>

int main() {
    initShellSignals();

    while (true) {
        displayPrompt();
        std::string input = readInput();
        if (input.empty() && std::cin.eof()) {
            // EOF: exit shell
            std::cout << "\n";
            break;
        }
        std::vector<std::string> args = parseInput(input);
        int status = executeCommand(args);
        if (status == 0) break; // exit requested
    }

    return 0;
}

