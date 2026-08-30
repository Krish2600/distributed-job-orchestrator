#include "worker_pool.h"
#include "worker.h"
#include <iostream>

WorkerPool::WorkerPool(
    std::shared_ptr<DBConnectionPool> db_pool,
    std::shared_ptr<JobQueue> queue,
    size_t num_workers
) : db_pool_(db_pool), queue_(queue), num_workers_(num_workers) {}

WorkerPool::~WorkerPool() {
    stop();
}

void WorkerPool::start() {
    if (started_) return;
    started_ = true;
    stop_flag_ = false;
    
    std::cout << "Starting WorkerPool with " << num_workers_ << " worker threads..." << std::endl;
    workers_.reserve(num_workers_);
    for (size_t i = 0; i < num_workers_; ++i) {
        // Spawn each worker run_worker function in a separate thread.
        workers_.emplace_back(run_worker, static_cast<int>(i + 1), db_pool_, queue_, std::ref(stop_flag_));
    }
}

void WorkerPool::stop() {
    if (!started_) return;
    
    std::cout << "Shutting down WorkerPool. Signaling stop to all threads..." << std::endl;
    stop_flag_ = true; // Set atomic flag to true
    
    for (auto& t : workers_) {
        if (t.joinable()) {
            t.join(); // Wait for thread to finish its current loop iteration and exit
        }
    }
    workers_.clear();
    started_ = false;
    std::cout << "WorkerPool stopped. All worker threads shut down successfully." << std::endl;
}
