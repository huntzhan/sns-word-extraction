CXX		= clang++
CXXFLAGS	= -std=c++11 -O2 -ggdb -Wall


all: solution


solution: solution.cc
	$(CXX) $(CXXFLAGS) solution.cc -o solution

clean:
	rm -f ./solution
	rm -rf *.dSYM
	rm -f output.txt

.PHONY: all clean
