#pragma once

#include <hiredis/hiredis.h>
#include <string>
#include <mutex>
#include <stdexcept>

// Thread-safe Redis wrapper using hiredis
class JobQueue {
public:
    JobQueue(const std::string& host, int port, const std::string& password = "");
    ~JobQueue();

    // Disable copying/assignment
    JobQueue(const JobQueue&) = delete;
    JobQueue& operator=(const JobQueue&) = delete;

    // Enqueue a job_id (thread-safe, protects the inner redisContext with a mutex)
    void enqueue(int job_id);

    // Creates a new independent redisContext. Each worker thread MUST call this
    // to obtain its own connection because hiredis contexts are not thread-safe
    // and BRPOP blocks the connection.
    redisContext* create_independent_context() const;

private:
    // Ensures the primary client connection is alive (reconnects if needed)
    void ensure_connected();

    std::string host_;
    int port_;
    std::string password_;
    redisContext* ctx_ = nullptr;
    std::mutex mutex_;
};
