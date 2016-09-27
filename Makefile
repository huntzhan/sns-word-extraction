# CXX		= clang++
CXX		= g++-4.8
# CXXFLAGS	= -std=c++11 -O0 -g -Wall
CXXFLAGS	= -std=c++11 -O2 -Wall


all: solution


solution: solution.cc
	$(CXX) $(CXXFLAGS) solution.cc -o solution

clean:
	rm -f ./solution
	rm -rf *.dSYM
	rm -f output.txt

.PHONY: all clean
