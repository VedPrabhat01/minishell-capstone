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
#include <fcntl.h>

using namespace std;

// history and jobs
static vector<string> command_history;

// Jobs data
static vector<Job> jobs;
static int next_job_id = 1;
static volatile sig_atomic_t child_terminated = 0;

static void sigint_handler_parent(int) {
    cout << "\n";
    displayPrompt();
    cout.flush();
}

static void sigchld_handler(int) {
    child_terminated = 1;
}

void initShellSignals() {
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

void displayPrompt() {
    cout << "minishell> ";
    cout.flush();
}

string readInput() {
    string input;
    if (!getline(cin, input)) {
        return string();
    }
    if (!input.empty()) command_history.push_back(input);
    return input;
}

// parseInput: tokenizes a line by whitespace but honors single/double quotes
vector<string> parseInput(const string &input) {
    vector<string> args;
    string token;
    bool in_single=false, in_double=false;
    for (size_t i=0;i<input.size();++i) {
        char c = input[i];
        if (c == '\'' && !in_double) { in_single = !in_single; continue; }
        if (c == '"' && !in_single) { in_double = !in_double; continue; }
        if (!in_single && !in_double && isspace(static_cast<unsigned char>(c))) {
            if (!token.empty()) { args.push_back(token); token.clear(); }
        } else {
            token.push_back(c);
        }
    }
    if (!token.empty()) args.push_back(token);
    return args;
}

// Split pipeline on '|' characters that are not inside quotes
vector<string> splitPipeline(const string &line) {
    vector<string> parts;
    string cur;
    bool in_single=false, in_double=false;
    for (size_t i=0;i<line.size();++i) {
        char c = line[i];
        if (c == '\'' && !in_double) { in_single = !in_single; cur.push_back(c); continue; }
        if (c == '"' && !in_single) { in_double = !in_double; cur.push_back(c); continue; }
        if (c == '|' && !in_single && !in_double) {
            // push trimmed cur
            size_t a = cur.find_first_not_of(" \t");
            size_t b = cur.find_last_not_of(" \t");
            if (a==string::npos) parts.push_back("");
            else parts.push_back(cur.substr(a, b-a+1));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    size_t a = cur.find_first_not_of(" \t");
    if (a==string::npos) parts.push_back("");
    else {
        size_t b = cur.find_last_not_of(" \t");
        parts.push_back(cur.substr(a, b-a+1));
    }
    return parts;
}

// parse a single command string into tokens and detect redirection tokens (< and >).
// Returns argv tokens (without < > and filenames), and sets infile/outfile (empty if none).
vector<string> parseCommandTokens(const string &cmd, string &infile, string &outfile) {
    vector<string> tokens = parseInput(cmd);
    vector<string> argv;
    for (size_t i=0;i<tokens.size();) {
        if (tokens[i] == "<") {
            if (i+1 < tokens.size()) {
                infile = tokens[i+1];
                i += 2;
            } else {
                // malformed: ignore
                ++i;
            }
        } else if (tokens[i] == ">") {
            if (i+1 < tokens.size()) {
                outfile = tokens[i+1];
                i += 2;
            } else {
                ++i;
            }
        } else {
            argv.push_back(tokens[i]);
            ++i;
        }
    }
    return argv;
}

// Job helpers
static Job* findJobById(int id) {
    for (auto &j : jobs) if (j.id == id) return &j;
    return nullptr;
}
static Job* findJobByPid(pid_t pid) {
    for (auto &j : jobs) if (j.pid == pid) return &j;
    return nullptr;
}
static void addJob(pid_t pid, const string &cmd, JobStatus st=RUNNING) {
    Job j; j.id = next_job_id++; j.pid = pid; j.cmd = cmd; j.status = st;
    jobs.push_back(j);
    cout << "[" << j.id << "] " << j.pid << " started: " << j.cmd << "\n";
}
static void removeJobByPid(pid_t pid) {
    jobs.erase(remove_if(jobs.begin(), jobs.end(), [pid](const Job &j){ return j.pid == pid; }), jobs.end());
}
void printJobs() {
    for (auto &j : jobs) {
        const char* st = (j.status==RUNNING ? "Running" : (j.status==STOPPED ? "Stopped" : "Done"));
        cout << "["<<j.id<<"] "<<j.pid<<" "<<st<<"  "<<j.cmd<<"\n";
    }
}

// reap background jobs - called in main loop when flag set
void checkBackgroundJobs() {
    if (!child_terminated) return;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        Job* j = findJobByPid(pid);
        if (!j) continue;
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            cout << "\n["<<j->id<<"] "<< j->pid << " Done: " << j->cmd << "\n";
            removeJobByPid(pid);
            displayPrompt();
            cout.flush();
        } else if (WIFSTOPPED(status)) {
            j->status = STOPPED;
            cout << "\n["<<j->id<<"] "<< j->pid << " Stopped: " << j->cmd << "\n";
            displayPrompt();
            cout.flush();
        } else if (WIFCONTINUED(status)) {
            j->status = RUNNING;
            cout << "\n["<<j->id<<"] "<< j->pid << " Continued: " << j->cmd << "\n";
            displayPrompt();
            cout.flush();
        }
    }
    child_terminated = 0;
}

// Execute pipeline of commands with optional infile/outfile and background flag
// Returns 1 to continue shell, 0 to exit shell
int executeLine(const string &rawline, bool background) {
    // Built-in quick checks: we need to detect built-ins that apply without forking/piping.
    // For simplicity, check top-level built-ins that are single commands without pipes/redirection
    // But general approach: parse pipeline
    vector<string> pipeline = splitPipeline(rawline);
    if (pipeline.empty()) return 1;

    // If pipeline has single segment, parse tokens and check built-ins where appropriate
    if (pipeline.size() == 1) {
        string infile, outfile;
        vector<string> argv = parseCommandTokens(pipeline[0], infile, outfile);
        if (argv.empty()) return 1;
        string cmd = argv[0];

        // builtins: exit, pwd, cd, history, jobs, fg, bg
        if (cmd == "exit") { cout << "Exiting minishell...\n"; return 0; }
        if (cmd == "pwd") {
            char cwd[4096];
            if (getcwd(cwd, sizeof(cwd))) cout << cwd << "\n"; else perror("pwd");
            return 1;
        }
        if (cmd == "cd") {
            if (argv.size() < 2) cerr << "cd: missing argument\n";
            else if (chdir(argv[1].c_str()) != 0) perror("cd");
            return 1;
        }
        if (cmd == "history") {
            for (size_t i=0;i<command_history.size();++i) cout << i+1 << " " << command_history[i] << "\n";
            return 1;
        }
        if (cmd == "jobs") { printJobs(); return 1; }
        if (cmd == "fg") {
            if (argv.size() < 2) { cerr << "fg: missing job id\n"; return 1; }
            string tok = argv[1]; if (!tok.empty() && tok[0]=='%') tok = tok.substr(1);
            int jid = stoi(tok);
            Job* j = findJobById(jid);
            if (!j) { cerr << "fg: job %" << jid << " not found\n"; return 1; }
            pid_t pid = j->pid;
            if (kill(pid, SIGCONT) < 0) perror("fg: kill(SIGCONT)");
            int status; if (waitpid(pid, &status, WUNTRACED) == -1) perror("waitpid");
            removeJobByPid(pid);
            return 1;
        }
        if (cmd == "bg") {
            if (argv.size() < 2) { cerr << "bg: missing job id\n"; return 1; }
            string tok = argv[1]; if (!tok.empty() && tok[0]=='%') tok = tok.substr(1);
            int jid = stoi(tok);
            Job* j = findJobById(jid);
            if (!j) { cerr << "bg: job %" << jid << " not found\n"; return 1; }
            if (kill(j->pid, SIGCONT) < 0) perror("bg: kill(SIGCONT)");
            j->status = RUNNING;
            cout << "["<<j->id<<"] "<<j->pid<<" continued in background\n";
            return 1;
        }

        // Not a builtin -> handle single external command possibly with redirection
        pid_t pid = fork();
        if (pid < 0) { perror("fork failed"); return 1; }
        if (pid == 0) {
            // Child: restore default signals
            signal(SIGINT, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);
            // Handle input redirection
            if (!infile.empty()) {
                int fd = open(infile.c_str(), O_RDONLY);
                if (fd < 0) { perror("open infile"); _exit(EXIT_FAILURE); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            // Handle output redirection
            if (!outfile.empty()) {
                int fd = open(outfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror("open outfile"); _exit(EXIT_FAILURE); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            // Prepare argv char* array
            vector<char*> argv_c;
            for (auto &s : argv) argv_c.push_back(const_cast<char*>(s.c_str()));
            argv_c.push_back(nullptr);
            execvp(argv_c[0], argv_c.data());
            perror("execvp");
            _exit(EXIT_FAILURE);
        } else {
            if (background) {
                addJob(pid, rawline, RUNNING);
            } else {
                int status; if (waitpid(pid, &status, WUNTRACED) == -1) perror("waitpid");
                if (WIFSTOPPED(status)) addJob(pid, rawline, STOPPED);
            }
            return 1;
        }
    }

    // pipeline length > 1: we must create pipes and fork children for each stage.
    size_t n = pipeline.size();
    vector<int> pipefds(2*(n-1));
    for (size_t i=0;i<n-1;i++) {
        if (pipe(pipefds.data()+2*i) == -1) {
            perror("pipe");
            return 1;
        }
    }

    vector<pid_t> pids;
    string global_infile, global_outfile; // only first/last commands should set these
    for (size_t i=0;i<n;i++) {
        string infile, outfile;
        vector<string> argv = parseCommandTokens(pipeline[i], infile, outfile);
        if (i==0) global_infile = infile;
        if (i==n-1) global_outfile = outfile;

        if (argv.empty()) {
            // empty command in pipeline: skip or error
            cerr << "Error: empty command in pipeline\n";
            // close fds
            for (int fd : pipefds) if (fd>0) close(fd);
            return 1;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            // close fds
            for (int fd : pipefds) if (fd>0) close(fd);
            return 1;
        }

        if (pid == 0) {
            // Child
            signal(SIGINT, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);

            // Setup stdin
            if (i==0) {
                if (!global_infile.empty()) {
                    int fd = open(global_infile.c_str(), O_RDONLY);
                    if (fd < 0) { perror("open infile"); _exit(EXIT_FAILURE); }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
            } else {
                // read from previous pipe
                int read_fd = pipefds[2*(i-1)];
                dup2(read_fd, STDIN_FILENO);
            }

            // Setup stdout
            if (i==n-1) {
                if (!global_outfile.empty()) {
                    int fd = open(global_outfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0) { perror("open outfile"); _exit(EXIT_FAILURE); }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
            } else {
                // write to next pipe
                int write_fd = pipefds[2*i + 1];
                dup2(write_fd, STDOUT_FILENO);
            }

            // Close all pipe fds in child
            for (int fd : pipefds) if (fd>2) close(fd);

            // Prepare argv
            vector<char*> argv_c;
            for (auto &s : argv) argv_c.push_back(const_cast<char*>(s.c_str()));
            argv_c.push_back(nullptr);
            execvp(argv_c[0], argv_c.data());
            perror("execvp");
            _exit(EXIT_FAILURE);
        } else {
            // Parent: record pid
            pids.push_back(pid);
        }
    }

    // Parent: close pipe fds
    for (int fd : pipefds) if (fd>0) close(fd);

    // If background: add last child's pid as job (track the entire pipeline using last pid)
    if (background) {
        // record last pid and rawline
        pid_t lastpid = pids.back();
        addJob(lastpid, rawline, RUNNING);
        // do not wait
    } else {
        // Wait for all pids (foreground). Use waitpid in loop; handle stop -> add job.
        for (pid_t pid : pids) {
            int status;
            if (waitpid(pid, &status, WUNTRACED) == -1) {
                perror("waitpid");
            } else {
                if (WIFSTOPPED(status)) {
                    addJob(pid, rawline, STOPPED);
                }
            }
        }
    }
    return 1;
}

