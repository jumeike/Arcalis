// shared_memory_app.h
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
#include <iostream>

struct MemcachedMessage {
    enum Type { GET, SET, GET_RESPONSE, SET_RESPONSE };
    Type type;
    std::string key;
    std::string value;
    bool success;
};

struct AppSharedMemory {
    static const size_t BUFFER_SIZE = 1024 * 1024;  
    uint8_t data[BUFFER_SIZE];
    size_t size;     
    bool has_data;   
    sem_t sem;      
    pthread_mutex_t mutex;
    MemcachedMessage msg;
};

class AppSharedMemoryManager {
public:
    static AppSharedMemoryManager& getInstance();
    AppSharedMemory* initSharedMemory();
    void cleanup();

private:
    AppSharedMemoryManager() = default;
    ~AppSharedMemoryManager();
    int shm_fd_;
    AppSharedMemory* shared_mem_ = nullptr;
};
