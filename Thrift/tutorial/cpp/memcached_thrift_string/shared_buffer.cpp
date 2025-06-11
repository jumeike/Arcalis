// shared_buffer.cpp
#include "shared_buffer.h"
#include <iostream>

SharedMemoryManager& SharedMemoryManager::getInstance() {
   static SharedMemoryManager instance;
   return instance;
}

SharedMemory* SharedMemoryManager::initSharedMemory() {
   if (shared_mem_) {
       return shared_mem_; // Already initialized
   }

   // Create shared memory segment
   shm_fd_ = shm_open("/dpdk_rpc_shm", O_CREAT | O_RDWR, 0666);
   if (shm_fd_ == -1) {
       std::cerr << "Failed to create shared memory: " << strerror(errno) << std::endl;
       return nullptr;
   }

   // Set size of shared memory
   if (ftruncate(shm_fd_, sizeof(SharedMemory)) == -1) {
       std::cerr << "Failed to set shared memory size: " << strerror(errno) << std::endl;
       close(shm_fd_);
       return nullptr;
   }

   // Map shared memory
   shared_mem_ = (SharedMemory*)mmap(NULL, 
                                   sizeof(SharedMemory),
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED,
                                   shm_fd_,
                                   0);
   printf("Shared memory mapped at: %p, size: %zu\n",
           (void*)shared_mem_, sizeof(SharedMemory));

   if (shared_mem_ == MAP_FAILED) {
       std::cerr << "Failed to map shared memory: " << strerror(errno) << std::endl;
       close(shm_fd_);
       return nullptr;
   }

   // Initialize semaphores and mutexes
   sem_init(&shared_mem_->rx_buffer.sem, 1, 0);
   sem_init(&shared_mem_->tx_buffer.sem, 1, 0);
   pthread_mutex_init(&shared_mem_->rx_buffer.mutex, NULL);
   pthread_mutex_init(&shared_mem_->tx_buffer.mutex, NULL);

   // Initialize buffer states
   shared_mem_->rx_buffer.has_data = false;
   shared_mem_->rx_buffer.size = 0;
   shared_mem_->tx_buffer.has_data = false;
   shared_mem_->tx_buffer.size = 0;

   return shared_mem_;
}

void SharedMemoryManager::cleanup() {
   if (shared_mem_) {
       // Destroy semaphores and mutexes
       sem_destroy(&shared_mem_->rx_buffer.sem);
       sem_destroy(&shared_mem_->tx_buffer.sem);
       pthread_mutex_destroy(&shared_mem_->rx_buffer.mutex);
       pthread_mutex_destroy(&shared_mem_->tx_buffer.mutex);

       // Unmap shared memory
       munmap(shared_mem_, sizeof(SharedMemory));
       
       // Close and unlink shared memory
       close(shm_fd_);
       shm_unlink("/dpdk_rpc_shm");
       
       shared_mem_ = nullptr;
   }
}

SharedMemoryManager::~SharedMemoryManager() {
   cleanup();
}
