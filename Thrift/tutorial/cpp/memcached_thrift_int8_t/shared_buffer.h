// shared_buffer.h
#pragma once

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>
#include <algorithm>

struct SharedBuffer {
   static const size_t BUFFER_SIZE = 1 * 1024 * 1024;  // 512MB
   alignas(64) uint8_t data[BUFFER_SIZE];
   size_t size;      // Amount of valid data
   bool has_data;    // Flag to indicate data availability
   sem_t sem;        // Synchronization
   pthread_mutex_t mutex;  // Protect access
};

struct SharedMemory {
   SharedBuffer rx_buffer;  // For DPDK -> RPC
   SharedBuffer tx_buffer;  // For RPC -> DPDK
};

class SharedMemoryManager {
public:
   static SharedMemoryManager& getInstance();
   SharedMemory* initSharedMemory();
   void cleanup();

private:
   SharedMemoryManager() = default;
   ~SharedMemoryManager();
   int shm_fd_;
   SharedMemory* shared_mem_ = nullptr;
};
