CC = gcc
CXX = g++
CXXFLAGS = -std=c++17 -g -Wall

INCLUDES = -I/usr/include -I/usr/local/include -I. \
	-I$(HOME)/NMAcc-DPDK/tutorial/cpp/memcached_thrift/memcached \
    -I$(HOME)/NMAcc-DPDK/tutorial/cpp/memcached_thrift/memcached/include
LDFLAGS = -L/usr/local/lib \
          -L$(HOME)/NMAcc-DPDK/tutorial/cpp/memcached_thrift/memcached

# Libraries
THRIFT_LIBS = -lthrift
MEMCACHED_LIBS = -levent -lpthread -lm #-lmemcached
FSTACK_LIBS = #-lfstack
DPDK_LIBS = $(shell pkg-config --libs libdpdk)
DPDK_FLAGS := $(shell pkg-config --cflags libdpdk)

MEMCACHED_BASE = $(HOME)/NMAcc-DPDK/tutorial/cpp/memcached_thrift/memcached

# Source files
THRIFT_GEN = gen-cpp/MemcachedService.cpp
CLIENT_SRC = MemcachedClient.cpp
SERVER_SRC = MemcachedServer.cpp
DPDK_SERVER_SRC = MemcachedDPDKServer.cpp
THRIFT_DPDK_SERVER_SRC = ThriftMemcachedDPDKServer.cpp
DPDK_HANDLER_SRC = DPDKHandler.cpp
SHARED_BUFFER_SRC = shared_buffer.cpp
MEMCACHED_SRCS = \
    $(MEMCACHED_BASE)/memcached.c \
    $(MEMCACHED_BASE)/items.c \
    $(MEMCACHED_BASE)/slabs.c \
    $(MEMCACHED_BASE)/hash.c \
    $(MEMCACHED_BASE)/jenkins_hash.c \
    $(MEMCACHED_BASE)/murmur3_hash.c \
    $(MEMCACHED_BASE)/storage.c \
    $(MEMCACHED_BASE)/assoc.c \
    $(MEMCACHED_BASE)/thread.c \
    $(MEMCACHED_BASE)/util.c \
    $(MEMCACHED_BASE)/logger.c \
    $(MEMCACHED_BASE)/bipbuffer.c \
    $(MEMCACHED_BASE)/crawler.c \
    $(MEMCACHED_BASE)/itoa_ljust.c \
    $(MEMCACHED_BASE)/slab_automove.c \
    $(MEMCACHED_BASE)/stats_prefix.c \
    $(MEMCACHED_BASE)/base64.c \
    $(MEMCACHED_BASE)/cache.c \
    $(MEMCACHED_BASE)/crc32c.c \
    $(MEMCACHED_BASE)/extstore.c \
    $(MEMCACHED_BASE)/authfile.c \
    $(MEMCACHED_BASE)/restart.c \
    $(MEMCACHED_BASE)/proto_bin.c \
    $(MEMCACHED_BASE)/proto_text.c \
    $(MEMCACHED_BASE)/slab_automove_extstore.c

CFLAGS += -DNO_MEMCACHED_MAIN -g
CXXFLAGS += -DNO_MEMCACHED_MAIN

# Object files
THRIFT_OBJ = $(THRIFT_GEN:.cpp=.o)
CLIENT_OBJ = $(CLIENT_SRC:.cpp=.o)
SERVER_OBJ = $(SERVER_SRC:.cpp=.o)
DPDK_SERVER_OBJ = $(DPDK_SERVER_SRC:.cpp=.o)
THRIFT_DPDK_SERVER_OBJ = $(THRIFT_DPDK_SERVER_SRC:.cpp=.o)
DPDK_HANDLER_OBJ = $(DPDK_HANDLER_SRC:.cpp=.o)
SHARED_BUFFER_OBJ = $(SHARED_BUFFER_SRC:.cpp=.o)
MEMCACHED_OBJS = $(MEMCACHED_SRCS:.c=.o)

# Add memcached compile flags
MEMCACHED_FLAGS = -DHAVE_CONFIG_H -DNDEBUG

# Targets
all: memcached_client memcached_server memcached_dpdk_server thrift_memcached_dpdk_server

memcached_client: $(CLIENT_OBJ) $(THRIFT_OBJ)
	$(CXX) $^ -o $@ $(THRIFT_LIBS)

memcached_server: $(SERVER_OBJ) $(THRIFT_OBJ) $(MEMCACHED_OBJS)
	$(CXX) $^ -o $@ $(THRIFT_LIBS) $(FSTACK_LIBS) $(MEMCACHED_LIBS)

memcached_dpdk_server: $(DPDK_SERVER_OBJ) $(MEMCACHED_OBJS)
	$(CXX) $^ -o $@ $(LD_FLAGS) $(THRIFT_LIBS) $(MEMCACHED_LIBS) $(DPDK_LIBS) 

thrift_memcached_dpdk_server: $(THRIFT_DPDK_SERVER_OBJ) $(DPDK_HANDLER_OBJ) $(SHARED_BUFFER_OBJ) $(MEMCACHED_OBJS)
	$(CXX) $^ -o $@ $(LD_FLAGS) $(THRIFT_LIBS) $(MEMCACHED_LIBS) $(DPDK_LIBS)

# Build memcached objects
%.o: %.c
	$(CC) $(CFLAGS) $(MEMCACHED_FLAGS) $(INCLUDES) -c $< -o $@

# Generic rules for object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DPDK_FLAGS) -c $< -o $@

# Generate Thrift code
thrift: memcached_benchmark.thrift
	thrift --gen cpp $<

# Clean
clean:
	rm -f thrift_memcached_dpdk_server memcached_dpdk_server memcached_client memcached_server *.o gen-cpp/*.o
	rm -f *~

# Deep clean (includes generated files)
distclean: clean
	rm -rf gen-cpp

# Dependencies
$(CLIENT_OBJ) $(SERVER_OBJ) $(THRIFT_OBJ): gen-cpp/MemcachedService.h

.PHONY: all clean distclean thrift
