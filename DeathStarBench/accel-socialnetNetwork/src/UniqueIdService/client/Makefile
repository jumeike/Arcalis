# Makefile for UniqueID Client Test

CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra -O2 -pthread
INCLUDES = -I. -I../include
LIBS = -lthrift -lpthread

# Thrift generated files (assuming they're in ../gen-cpp)
THRIFT_GEN = ../../../gen-cpp/UniqueIdService.cpp \
             ../../../gen-cpp/social_network_types.cpp \
             ../../../gen-cpp/social_network_constants.cpp

SOURCES = uid_client_test.cpp $(THRIFT_GEN)
TARGET = uid_client_test

.PHONY: all clean debug release

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

release: CXXFLAGS += -O3 -DNDEBUG
release: $(TARGET)

clean:
	rm -f $(TARGET)

# Usage examples (make these available as targets for convenience)
test-single:
	./$(TARGET) -t 1 -r 1000 -v

test-load:
	./$(TARGET) -t 8 -r 5000 -o results.csv

test-stress:
	./$(TARGET) -t 16 -r 10000 -o stress_results.csv

help:
	@echo "Available targets:"
	@echo "  all       - Build the client test program"
	@echo "  debug     - Build with debug flags"
	@echo "  release   - Build with optimization"
	@echo "  clean     - Remove built files"
	@echo "  test-single - Run single-threaded test"
	@echo "  test-load   - Run multi-threaded load test"
	@echo "  test-stress - Run stress test"
	@echo "  help      - Show this help"
