#pragma once

#include "db.h"
#include "job_queue.h"
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

// Manages a set of N worker threads processing tasks from the queue
class WorkerPool {
public:
    WorkerPool(
        std::shared_ptr<DBConnectionPool> db_pool,
        std::shared_ptr<JobQueue> queue,
        size_t num_workers
    );
    ~WorkerPool();

    // Disable copying
    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    // Spawns the worker threads
    void start();

    // Signifies shutdown and joins all spawned threads
    void stop();

private:
    std::shared_ptr<DBConnectionPool> db_pool_;
    std::shared_ptr<JobQueue> queue_;
    size_t num_workers_;
    std::vector<std::thread> workers_;
    std::atomic<bool> stop_flag_{false};
    bool started_ = false;
};
