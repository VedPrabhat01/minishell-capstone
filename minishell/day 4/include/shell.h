#ifndef SHELL_H
#define SHELL_H

#include <string>
#include <vector>
#include <sys/types.h>

enum JobStatus { RUNNING, STOPPED, DONE };

struct Job {
    int id;
    pid_t pid;
    std::string cmd;
    JobStatus status;
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

#endif // SHELL_H

