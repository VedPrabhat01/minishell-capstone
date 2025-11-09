#include "../include/shell.h"
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <vector>
#include <sys/wait.h>
#include <cstring>

using namespace std;

void displayPrompt() {
    cout << "minishell> ";
}

string readInput() {
    string input;
    getline(cin, input);
    return input;
}

vector<string> parseInput(const string &input) {
    vector<string> args;
    stringstream ss(input);
    string token;
    while (ss >> token) {
        args.push_back(token);
    }
    return args;
}

int executeCommand(const vector<string> &args) {
    if (args.empty()) return 1;

    string cmd = args[0];

    // Built-in: exit
    if (cmd == "exit") {
        cout << "Exiting minishell...\n";
        return 0;
    }

    // Built-in: pwd
    if (cmd == "pwd") {
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        cout << cwd << endl;
        return 1;
    }

    // Built-in: cd
    if (cmd == "cd") {
        if (args.size() < 2) {
            cerr << "cd: missing argument\n";
        } else if (chdir(args[1].c_str()) != 0) {
            perror("cd");
        }
        return 1;
    }

    // External commands
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        vector<char*> argv;
        for (auto &arg : args)
            argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        perror("execvp failed");
        exit(1);
    } else if (pid > 0) {
        // Parent waits for child
        waitpid(pid, nullptr, 0);
    } else {
        perror("fork failed");
    }

    return 1;
}

