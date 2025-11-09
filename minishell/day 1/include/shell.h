#ifndef SHELL_H
#define SHELL_H

#include <string>
#include <vector>

void displayPrompt();
std::string readInput();
std::vector<std::string> parseInput(const std::string &input);
int executeCommand(const std::vector<std::string> &args);

#endif

