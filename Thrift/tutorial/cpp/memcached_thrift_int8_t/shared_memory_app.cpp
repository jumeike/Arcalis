// shared_memory_app.cpp
#include "shared_memory_app.h"

AppSharedMemoryManager& AppSharedMemoryManager::getInstance() {
    static AppSharedMemoryManager instance;
    return instance;
}

AppSharedMemory* AppSharedMemoryManager::initSharedMemory() {
    if (shared_mem_) {
        return shared_mem_;
    }

    shm_fd_ = shm_open("/memcached_rpc_shm", O_CREAT | O_RDWR, 0666);
    if (shm_fd_ == -1) {
        std::cerr << "Failed to create shared memory: " << strerror(errno) << std::endl;
        return nullptr;
    }

    if (ftruncate(shm_fd_, sizeof(AppSharedMemory)) == -1) {
        std::cerr << "Failed to set shared memory size: " << strerror(errno) << std::endl;
        close(shm_fd_);
        return nullptr;
    }

    shared_mem_ = (AppSharedMemory*)mmap(NULL, 
                                        sizeof(AppSharedMemory),
                                        PROT_READ | PROT_WRITE,
                                        MAP_SHARED,
                                        shm_fd_,
                                        0);
    if (shared_mem_ == MAP_FAILED) {
        std::cerr << "Failed to map shared memory: " << strerror(errno) << std::endl;
        close(shm_fd_);
        return nullptr;
    }

    // Initialize synchronization primitives
    sem_init(&shared_mem_->sem, 1, 0);
    pthread_mutex_init(&shared_mem_->mutex, NULL);

    // Initialize state
    shared_mem_->has_data = false;
    shared_mem_->size = 0;

    return shared_mem_;
}

void AppSharedMemoryManager::cleanup() {
    if (shared_mem_) {
        sem_destroy(&shared_mem_->sem);
        pthread_mutex_destroy(&shared_mem_->mutex);
        munmap(shared_mem_, sizeof(AppSharedMemory));
        close(shm_fd_);
        shm_unlink("/memcached_rpc_shm");
        shared_mem_ = nullptr;
    }
}

AppSharedMemoryManager::~AppSharedMemoryManager() {
    cleanup();
}
