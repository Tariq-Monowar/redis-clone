# Build without CMake: `make`
# With CMake: `cmake -S . -B build && cmake --build build`

CXX      ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -Iinclude
LDFLAGS  ?= -pthread

.PHONY: all clean

all: miniredis-server miniredis-cli

miniredis-server: src/main_server.cpp src/kv_store.cpp src/resp.cpp src/server.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

miniredis-cli: tools/miniredis_cli.cpp src/resp.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f miniredis-server miniredis-cli
