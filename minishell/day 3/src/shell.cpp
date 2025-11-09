#include "../include/shell.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <signal.h>
#include <cstdlib>
#include <algorithm>
#include <atomic>

using namespace std;

static vector<string> command_history;

// Jobs
static vector<Job> jobs;
static int next_job_id = 1;

// Flag set in SIGCHLD handler to indicate we should reap children
static volatile sig_atomic_t child_terminated = 0;

void displayPrompt() {
    cout << "minishell> ";
    cout.flush();
}

string readInput() {
    string input;
    if (!getline(cin, input)) {
        return string(); // EOF
    }
    if (!input.empty()) command_history.push_back(input);
    return input;
}

// basic tokenization (keeps this simple — supports quotes if present from earlier days)
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
    // Do nothing major: we just print newline so prompt returns on next line
    cout << "\n";
    displayPrompt();
    // flush stdout so prompt shows
    cout.flush();
}

void sigchld_handler(int signo) {
    // Set flag; do minimal work here (avoid non-async-safe operations)
    child_terminated = 1;
}

void initShellSignals() {
    // Ignore SIGINT in the parent shell; child processes will have default.
    struct sigaction sa_int;
    sa_int.sa_handler = sigint_handler_parent;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, nullptr);

    struct sigaction sa_chld;
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, nullptr);
}

static Job* findJobById(int jobid) {
    for (auto &j : jobs) {
        if (j.id == jobid) return &j;
    }
    return nullptr;
}

static Job* findJobByPid(pid_t pid) {
    for (auto &j : jobs) {
        if (j.pid == pid) return &j;
    }
    return nullptr;
}

void addJob(pid_t pid, const string &cmd, JobStatus status=RUNNING) {
    Job j;
    j.id = next_job_id++;
    j.pid = pid;
    j.cmd = cmd;
    j.status = status;
    jobs.push_back(j);
    cout << "[" << j.id << "] " << j.pid << " started: " << j.cmd << "\n";
}

void removeJobByPid(pid_t pid) {
    jobs.erase(remove_if(jobs.begin(), jobs.end(), [pid](const Job &j){ return j.pid == pid; }), jobs.end());
}

void checkBackgroundJobs() {
    if (!child_terminated) return;
    // Reap all finished children
    int status;
    pid_t pid;
    // Use loop with WNOHANG
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        Job* j = findJobByPid(pid);
        if (!j) {
            // Not a job we tracked (maybe foreground one or already removed)
            continue;
        }
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            j->status = DONE;
            cout << "\n[" << j->id << "] " << j->pid << " Done: " << j->cmd << "\n";
            // remove job
            removeJobByPid(pid);
            displayPrompt();
            cout.flush();
        } else if (WIFSTOPPED(status)) {
            j->status = STOPPED;
            cout << "\n[" << j->id << "] " << j->pid << " Stopped: " << j->cmd << "\n";
            displayPrompt();
            cout.flush();
        } else if (WIFCONTINUED(status)) {
            j->status = RUNNING;
            cout << "\n[" << j->id << "] " << j->pid << " Continued: " << j->cmd << "\n";
            displayPrompt();
            cout.flush();
        }
    }
    // Reset flag
    child_terminated = 0;
}

void printJobs() {
    for (const auto &j : jobs) {
        string st = (j.status == RUNNING ? "Running" : (j.status == STOPPED ? "Stopped" : "Done"));
        cout << "[" << j.id << "] " << j.pid << " " << st << "  " << j.cmd << "\n";
    }
}

int executeCommand(const vector<string> &args, bool background, const string &rawline) {
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

    // built-in: jobs
    if (cmd == "jobs") {
        printJobs();
        return 1;
    }

    // built-in: fg %jobid  (or fg jobid)
    if (cmd == "fg") {
        if (args.size() < 2) {
            cerr << "fg: missing job id\n";
            return 1;
        }
        string tok = args[1];
        if (tok.size() > 0 && tok[0] == '%') tok = tok.substr(1);
        int jid = stoi(tok);
        Job* j = findJobById(jid);
        if (!j) {
            cerr << "fg: job %" << jid << " not found\n";
            return 1;
        }
        // bring to foreground
        pid_t pid = j->pid;
        // send SIGCONT to the job (in case it's stopped)
        if (kill(pid, SIGCONT) < 0) {
            perror("fg: kill(SIGCONT)");
            return 1;
        }
        // Wait for it
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
        }
        // Remove job after it finishes
        removeJobByPid(pid);
        return 1;
    }

    // built-in: bg %jobid
    if (cmd == "bg") {
        if (args.size() < 2) {
            cerr << "bg: missing job id\n";
            return 1;
        }
        string tok = args[1];
        if (tok.size() > 0 && tok[0] == '%') tok = tok.substr(1);
        int jid = stoi(tok);
        Job* j = findJobById(jid);
        if (!j) {
            cerr << "bg: job %" << jid << " not found\n";
            return 1;
        }
        pid_t pid = j->pid;
        if (kill(pid, SIGCONT) < 0) {
            perror("bg: kill(SIGCONT)");
            return 1;
        }
        j->status = RUNNING;
        cout << "[" << j->id << "] " << j->pid << " continued in background\n";
        return 1;
    }

    // External commands
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return 1;
    }

    if (pid == 0) {
        // Child: restore default signal handlers so Ctrl+C affects child
        signal(SIGINT, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

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
        if (background) {
            // add to jobs list and do not wait
            addJob(pid, rawline, RUNNING);
            // Do not wait
        } else {
            // Foreground: wait until child completes (or stops)
            int status = 0;
            if (waitpid(pid, &status, WUNTRACED) == -1) {
                perror("waitpid");
            } else {
                // If child was stopped (Ctrl+Z, etc), add to jobs as stopped
                if (WIFSTOPPED(status)) {
                    addJob(pid, rawline, STOPPED);
                }
            }
        }
    }
    return 1;
}

