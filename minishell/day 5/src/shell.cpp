// src/shell.cpp
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
#include <fstream>
#include <iomanip>
#include <ctime>
#include <pwd.h>

using namespace std;

static vector<string> command_history;

// Jobs data
static vector<Job> jobs;
static int next_job_id = 1;
static volatile sig_atomic_t child_terminated = 0;

// Path to history file
static string history_file_path();

// ------------------ Utility: history file path ------------------
static string history_file_path() {
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (!home) return ".minishell_history"; // fallback local
    string p = string(home) + "/.minishell_history";
    return p;
}

// ------------------ Signal handlers ------------------
static void sigint_handler_parent(int) {
    // Don't terminate shell on Ctrl+C; just print newline and prompt will reappear.
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

// ------------------ Prompt / input ------------------
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

// ------------------ Tokenizers / parsers (from Day 4) ------------------
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

vector<string> splitPipeline(const string &line) {
    vector<string> parts;
    string cur;
    bool in_single=false, in_double=false;
    for (size_t i=0;i<line.size();++i) {
        char c = line[i];
        if (c == '\'' && !in_double) { in_single = !in_single; cur.push_back(c); continue; }
        if (c == '"' && !in_single) { in_double = !in_double; cur.push_back(c); continue; }
        if (c == '|' && !in_single && !in_double) {
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

vector<string> parseCommandTokens(const string &cmd, string &infile, string &outfile) {
    vector<string> tokens = parseInput(cmd);
    vector<string> argv;
    for (size_t i=0;i<tokens.size();) {
        if (tokens[i] == "<") {
            if (i+1 < tokens.size()) { infile = tokens[i+1]; i+=2; }
            else ++i;
        } else if (tokens[i] == ">") {
            if (i+1 < tokens.size()) { outfile = tokens[i+1]; i+=2; }
            else ++i;
        } else {
            argv.push_back(tokens[i]);
            ++i;
        }
    }
    return argv;
}

// ------------------ Jobs helpers ------------------
void addJob(pid_t pid, const string &cmd, JobStatus status) {
    Job j;
    j.id = next_job_id++;
    j.pid = pid;
    j.cmd = cmd;
    j.status = status;
    j.started = time(nullptr);
    jobs.push_back(j);
    cout << "[" << j.id << "] " << j.pid << " started: " << j.cmd << "\n";
}

void removeJobByPid(pid_t pid) {
    jobs.erase(remove_if(jobs.begin(), jobs.end(), [pid](const Job &j){ return j.pid == pid; }), jobs.end());
}

Job* findJobById(int id) {
    for (auto &j : jobs) if (j.id == id) return &j;
    return nullptr;
}

Job* findJobByPid(pid_t pid) {
    for (auto &j : jobs) if (j.pid == pid) return &j;
    return nullptr;
}

void printJobs() {
    cout << left << setw(6) << "JobID" << setw(10) << "PID" << setw(10) << "Status" << "Command\n";
    for (auto &j : jobs) {
        string st = (j.status==RUNNING ? "Running" : (j.status==STOPPED ? "Stopped" : "Done"));
        char tb[64];
        struct tm *tm = localtime(&j.started);
        strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", tm);
        cout << "[" << j.id << "]    " << setw(8) << j.pid << setw(10) << st << j.cmd << "  (since " << tb << ")\n";
    }
}

// ------------------ Background children reaper ------------------
void checkBackgroundJobs() {
    if (!child_terminated) return;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        Job* j = findJobByPid(pid);
        if (!j) continue;
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            j->status = DONE;
            cout << "\n[" << j->id << "] " << j->pid << " Done: " << j->cmd << "\n";
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
    child_terminated = 0;
}

// ------------------ History persistence ------------------
void loadHistory() {
    string path = history_file_path();
    ifstream in(path);
    if (!in.is_open()) return;
    string line;
    while (getline(in, line)) {
        if (!line.empty()) command_history.push_back(line);
    }
    in.close();
}

void saveHistory() {
    string path = history_file_path();
    ofstream out(path, ios::app); // append new commands
    if (!out.is_open()) return;
    // Append only new commands that are not already in file: easiest approach - write all (small risk of duplicates).
    // For simplicity and safety, write only last N commands (e.g., 1000).
    size_t N = command_history.size();
    // We'll write last up to 1000 commands
    size_t start = (N > 1000 ? N - 1000 : 0);
    for (size_t i = start; i < N; ++i) {
        out << command_history[i] << "\n";
    }
    out.close();
}

// ------------------ Built-in: kill ------------------
// Supports:
//  kill <pid>
//  kill -9 <pid>
//  kill %<jobid>
//  kill -9 %<jobid>
int builtin_kill(const vector<string>& argv) {
    if (argv.size() < 2) {
        cerr << "kill: missing operand\n";
        return 1;
    }
    int sig = SIGTERM;
    size_t idx = 1;
    if (argv[1].rfind("-", 0) == 0 && argv.size() >= 3) { // starts with '-' and has another arg
        // parse numeric signal like -9
        try {
            sig = stoi(argv[1].substr(1));
            idx = 2;
        } catch (...) {
            cerr << "kill: invalid signal spec\n";
            return 1;
        }
    }
    if (idx >= argv.size()) {
        cerr << "kill: missing pid or job\n";
        return 1;
    }
    string target = argv[idx];
    pid_t target_pid = 0;

    if (!target.empty() && target[0] == '%') {
        // job id
        try {
            int jid = stoi(target.substr(1));
            Job* j = findJobById(jid);
            if (!j) { cerr << "kill: job %" << jid << " not found\n"; return 1; }
            target_pid = j->pid;
        } catch (...) {
            cerr << "kill: invalid job id\n";
            return 1;
        }
    } else {
        // pid
        try {
            target_pid = static_cast<pid_t>(stoi(target));
        } catch (...) {
            cerr << "kill: invalid pid\n";
            return 1;
        }
    }

    if (kill(target_pid, sig) < 0) {
        perror("kill");
        return 1;
    }
    return 1;
}

// ------------------ Built-in: disown %jobid ------------------
int builtin_disown(const vector<string>& argv) {
    if (argv.size() < 2) {
        cerr << "disown: missing job id\n";
        return 1;
    }
    string tok = argv[1];
    if (!tok.empty() && tok[0] == '%') tok = tok.substr(1);
    int jid = 0;
    try {
        jid = stoi(tok);
    } catch (...) {
        cerr << "disown: invalid job id\n";
        return 1;
    }
    Job* j = findJobById(jid);
    if (!j) {
        cerr << "disown: job %" << jid << " not found\n";
        return 1;
    }
    // Remove job from tracking; do not kill it
    removeJobByPid(j->pid);
    cout << "disowned job %" << jid << "\n";
    return 1;
}

// ------------------ executeLine (pipes/redir/jobs) ------------------
// This reuses the Day-4 implementation with minor integration for builtins kill/disown/history.
int executeLine(const string &rawline, bool background) {
    // Save command to history (already appended by readInput, but ensure)
    if (!rawline.empty()) command_history.push_back(rawline);

    vector<string> pipeline = splitPipeline(rawline);
    if (pipeline.empty()) return 1;

    // Single-segment fast-path to handle built-ins
    if (pipeline.size() == 1) {
        string infile, outfile;
        vector<string> argv = parseCommandTokens(pipeline[0], infile, outfile);
        if (argv.empty()) return 1;
        string cmd = argv[0];

        // builtins: exit, pwd, cd, history, jobs, fg, bg, kill, disown
        if (cmd == "exit") { 
            // before exiting, save history
            saveHistory();
            cout << "Exiting minishell...\n"; 
            return 0; 
        }
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
            string tok = argv[1];
            if (!tok.empty() && tok[0] == '%') tok = tok.substr(1);
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
            string tok = argv[1];
            if (!tok.empty() && tok[0] == '%') tok = tok.substr(1);
            int jid = stoi(tok);
            Job* j = findJobById(jid);
            if (!j) { cerr << "bg: job %" << jid << " not found\n"; return 1; }
            if (kill(j->pid, SIGCONT) < 0) perror("bg: kill(SIGCONT)");
            j->status = RUNNING;
            cout << "["<<j->id<<"] "<<j->pid<<" continued in background\n";
            return 1;
        }
        if (cmd == "kill") {
            return builtin_kill(argv);
        }
        if (cmd == "disown") {
            return builtin_disown(argv);
        }

        // Not builtin: fork and run single command (with redirection)
        pid_t pid = fork();
        if (pid < 0) { perror("fork failed"); return 1; }
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);
            if (!infile.empty()) {
                int fd = open(infile.c_str(), O_RDONLY);
                if (fd < 0) { perror("open infile"); _exit(EXIT_FAILURE); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (!outfile.empty()) {
                int fd = open(outfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror("open outfile"); _exit(EXIT_FAILURE); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            vector<char*> argv_c;
            for (auto &s : argv) argv_c.push_back(const_cast<char*>(s.c_str()));
            argv_c.push_back(nullptr);
            execvp(argv_c[0], argv_c.data());
            perror("execvp");
            _exit(EXIT_FAILURE);
        } else {
            if (background) addJob(pid, rawline, RUNNING);
            else {
                int status;
                if (waitpid(pid, &status, WUNTRACED) == -1) perror("waitpid");
                if (WIFSTOPPED(status)) addJob(pid, rawline, STOPPED);
            }
            return 1;
        }
    }

    // Multi-stage pipeline: same as Day 4 treatment
    size_t n = pipeline.size();
    vector<int> pipefds(2*(n-1));
    for (size_t i=0;i<n-1;i++) {
        if (pipe(pipefds.data()+2*i) == -1) { perror("pipe"); return 1; }
    }

    vector<pid_t> pids;
    string global_infile, global_outfile;
    for (size_t i=0;i<n;i++) {
        string infile, outfile;
        vector<string> argv = parseCommandTokens(pipeline[i], infile, outfile);
        if (i==0) global_infile = infile;
        if (i==n-1) global_outfile = outfile;

        if (argv.empty()) { cerr << "Error: empty command in pipeline\n"; for (int fd : pipefds) if (fd>0) close(fd); return 1; }

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); for (int fd : pipefds) if (fd>0) close(fd); return 1; }

        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);
            if (i==0) {
                if (!global_infile.empty()) {
                    int fd = open(global_infile.c_str(), O_RDONLY);
                    if (fd < 0) { perror("open infile"); _exit(EXIT_FAILURE); }
                    dup2(fd, STDIN_FILENO); close(fd);
                }
            } else {
                int read_fd = pipefds[2*(i-1)];
                dup2(read_fd, STDIN_FILENO);
            }

            if (i==n-1) {
                if (!global_outfile.empty()) {
                    int fd = open(global_outfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0) { perror("open outfile"); _exit(EXIT_FAILURE); }
                    dup2(fd, STDOUT_FILENO); close(fd);
                }
            } else {
                int write_fd = pipefds[2*i + 1];
                dup2(write_fd, STDOUT_FILENO);
            }

            for (int fd : pipefds) if (fd>2) close(fd);

            vector<char*> argv_c;
            for (auto &s : argv) argv_c.push_back(const_cast<char*>(s.c_str()));
            argv_c.push_back(nullptr);
            execvp(argv_c[0], argv_c.data());
            perror("execvp");
            _exit(EXIT_FAILURE);
        } else {
            pids.push_back(pid);
        }
    }

    for (int fd : pipefds) if (fd>0) close(fd);

    if (background) {
        pid_t lastpid = pids.back();
        addJob(lastpid, rawline, RUNNING);
    } else {
        for (pid_t pid : pids) {
            int status;
            if (waitpid(pid, &status, WUNTRACED) == -1) perror("waitpid");
            else if (WIFSTOPPED(status)) addJob(pid, rawline, STOPPED);
        }
    }
    return 1;
}

