#include "include/shell.h"
#include <iostream>
using namespace std;

int main() {
    while (true) {
        displayPrompt();
        string input = readInput();
        if (cin.eof()) break; // handle Ctrl+D
        vector<string> args = parseInput(input);
        int status = executeCommand(args);
        if (status == 0) break; // exit shell
    }
    return 0;
}

