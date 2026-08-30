#pragma once

#include "db.h"
#include "job_queue.h"
#include <atomic>
#include <memory>

// Starts the worker loop in a separate thread.
// It will continuously dequeue and process jobs until stop_flag becomes true.
void run_worker(
    int worker_id,
    std::shared_ptr<DBConnectionPool> db_pool,
    std::shared_ptr<JobQueue> queue,
    std::atomic<bool>& stop_flag
);
