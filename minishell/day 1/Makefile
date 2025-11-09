CXX = g++
CXXFLAGS = -std=c++17 -Wall
INCLUDES = -Iinclude
SRC = src/shell.cpp main.cpp
OUT = minishell

all: $(OUT)

$(OUT): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) $(INCLUDES) -o $(OUT)

clean:
	rm -f $(OUT)

