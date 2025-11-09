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
int executeCommand(const std::vector<std::string> &args, bool background, const std::string &rawline);
void initShellSignals();
void checkBackgroundJobs();     // reap and update jobs
void printJobs();

#endif // SHELL_H

