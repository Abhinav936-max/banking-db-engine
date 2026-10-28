CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pedantic
INCLUDES = -Iinclude

all: minidb

minidb: src/main.cpp src/database.cpp src/wal.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

run: minidb
	./minidb

clean:
	rm -f minidb wal.log

.PHONY: all run clean
