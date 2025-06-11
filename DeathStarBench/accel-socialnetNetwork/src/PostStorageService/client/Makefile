# Makefile for PostStorage Client Test

CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra -O2 -pthread
INCLUDES = -I. -I../include
LIBS = -lthrift -lpthread

# Thrift generated files
THRIFT_GEN = ../../../gen-cpp/PostStorageService.cpp \
             ../../../gen-cpp/social_network_types.cpp \
             ../../../gen-cpp/social_network_constants.cpp

SOURCES = post_storage_client_test.cpp $(THRIFT_GEN)
TARGET = post_storage_client_test

.PHONY: all clean debug release

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

debug: CXXFLAGS += -g -DDEBUG_LOGGING
debug: $(TARGET)

release: CXXFLAGS += -O3 -DNDEBUG
release: $(TARGET)

clean:
	rm -f $(TARGET)

# Test targets
test-basic:
	./$(TARGET) -t 2 -o 100 -v

test-load:
	./$(TARGET) -t 4 -o 500

test-stress:
	./$(TARGET) -t 8 -o 1000

help:
	@echo "Available targets:"
	@echo "  all         - Build client"
	@echo "  debug       - Build with debug flags"
	@echo "  release     - Build optimized"
	@echo "  clean       - Remove executable"
	@echo "  test-basic  - Basic test"
	@echo "  test-load   - Load test"
	@echo "  test-stress - Stress test"
