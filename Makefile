run: all
	./main
all:
	c++ -std=c++11 src/main.cpp -o main -I./include

