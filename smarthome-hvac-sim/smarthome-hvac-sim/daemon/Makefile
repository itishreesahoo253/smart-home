CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -pthread
INC      := -Iinclude -I../driver

all: smarthome_daemon test_controller

smarthome_daemon: src/main.cpp src/sources.cpp $(wildcard include/*.hpp)
	$(CXX) $(CXXFLAGS) $(INC) src/main.cpp src/sources.cpp -o $@

test_controller: tests/test_controller.cpp include/hvac_controller.hpp
	$(CXX) $(CXXFLAGS) $(INC) tests/test_controller.cpp -o $@

test: test_controller
	./test_controller

clean:
	rm -f smarthome_daemon test_controller
