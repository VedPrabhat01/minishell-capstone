#include "../include/shell.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <signal.h>
#include <cstdlib>

using namespace std;

// Simple in-memory history (Day 2 helper)
static vector<string> command_history;

void displayPrompt() {
    cout << "minishell> ";
    cout.flush();
}

string readInput() {
    string input;
    if (!getline(cin, input)) {
        // EOF (Ctrl+D)
        return string();
    }
    // store into history if non-empty
    if (!input.empty()) command_history.push_back(input);
    return input;
}

// parse words with support for quoted args (single and double)
vector<string> parseInput(const string &input) {
    vector<string> args;
    string token;
    bool in_single = false, in_double = false;
    token.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c == '\'' && !in_double) {
            in_single = !in_single;
            continue;
        } else if (c == '"' && !in_single) {
            in_double = !in_double;
            continue;
        }

        if (!in_single && !in_double && isspace(static_cast<unsigned char>(c))) {
            if (!token.empty()) {
                args.push_back(token);
                token.clear();
            }
        } else {
            token.push_back(c);
        }
    }
    if (!token.empty()) args.push_back(token);
    return args;
}

void sigint_handler_parent(int signo) {
    // ignore — prevents shell from exiting when user hits Ctrl+C
    cout << "\n"; // print newline so prompt prints on next line nicely
    displayPrompt();
}

void initShellSignals() {
    // Parent shell ignores SIGINT; child will restore default
    struct sigaction sa;
    sa.sa_handler = sigint_handler_parent;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, nullptr);
}

int executeCommand(const vector<string> &args) {
    if (args.empty()) return 1;

    string cmd = args[0];

    // built-in: exit
    if (cmd == "exit") {
        cout << "Exiting minishell...\n";
        return 0;
    }

    // built-in: pwd
    if (cmd == "pwd") {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            cout << cwd << endl;
        } else {
            perror("pwd");
        }
        return 1;
    }

    // built-in: cd
    if (cmd == "cd") {
        if (args.size() < 2) {
            cerr << "cd: missing argument\n";
        } else if (chdir(args[1].c_str()) != 0) {
            perror("cd");
        }
        return 1;
    }

    // built-in: history
    if (cmd == "history") {
        for (size_t i = 0; i < command_history.size(); ++i) {
            cout << i + 1 << " " << command_history[i] << "\n";
        }
        return 1;
    }

    // External command execution
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return 1;
    }

    if (pid == 0) {
        // Child: restore default signal handlers so Ctrl+C affects child
        signal(SIGINT, SIG_DFL);

        // Prepare argv array for execvp
        vector<char*> argv;
        for (const auto &a : args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        // If execvp returns, it failed
        perror("execvp");
        _exit(EXIT_FAILURE);
    } else {
        // Parent: wait for child to finish
        int status = 0;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
        }
        return 1;
    }
}

