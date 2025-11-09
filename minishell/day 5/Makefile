CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude
SRC = src/shell.cpp main.cpp
OUT = minishell

all: $(OUT)

$(OUT): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(OUT)

clean:
	rm -f $(OUT)

