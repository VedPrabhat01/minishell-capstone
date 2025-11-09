#ifndef SHELL_H
#define SHELL_H

#include <string>
#include <vector>
#include <sys/types.h>
#include <ctime>

enum JobStatus { RUNNING, STOPPED, DONE };

struct Job {
    int id;
    pid_t pid;
    std::string cmd;
    JobStatus status;
    std::time_t started;
};

void displayPrompt();
std::string readInput();
std::vector<std::string> parseInput(const std::string &input);

// Splits a cleaned input line into pipeline segments (trimmed)
std::vector<std::string> splitPipeline(const std::string &line);

// Parse a single command string into argv vector, and extract optional < and > files.
// Returns argv, sets infile/outfile strings (empty if none).
std::vector<std::string> parseCommandTokens(const std::string &cmd, std::string &infile, std::string &outfile);

// Execute a full command line: supports pipes, redirection, background flag, returns 0 to exit
int executeLine(const std::string &rawline, bool background);

// Signals & job management
void initShellSignals();
void checkBackgroundJobs();
void printJobs();
void saveHistory();
void loadHistory();
void addJob(pid_t pid, const std::string &cmd, JobStatus status = RUNNING);
void removeJobByPid(pid_t pid);
Job* findJobById(int id);
Job* findJobByPid(pid_t pid);

// Builtins specifically added this day
int builtin_kill(const std::vector<std::string>& argv);
int builtin_disown(const std::vector<std::string>& argv);

#endif // SHELL_H

