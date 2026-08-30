#pragma once

#include <pqxx/pqxx>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <string>
#include <vector>

// Struct representing a Job database record
struct Job {
    int id;
    std::string type;
    std::string label;       // Human-readable display name (especially for CUSTOM jobs)
    std::string payload;     // Stringified JSON
    std::string status;
    std::string priority;    // HIGH, MEDIUM, LOW
    int progress;            // 0-100
    int attempts;
    std::string result;
    std::string created_at;
    std::string updated_at;
};

// Thread-safe connection pool for PostgreSQL using libpqxx
class DBConnectionPool : public std::enable_shared_from_this<DBConnectionPool> {
public:
    static std::shared_ptr<DBConnectionPool> create(const std::string& conn_str, size_t pool_size);

    DBConnectionPool(const std::string& conn_str, size_t pool_size);
    ~DBConnectionPool();

    DBConnectionPool(const DBConnectionPool&) = delete;
    DBConnectionPool& operator=(const DBConnectionPool&) = delete;

    std::shared_ptr<pqxx::connection> get_connection();
    void shutdown();

private:
    void init_pool();

    std::string conn_str_;
    size_t pool_size_;
    std::queue<std::shared_ptr<pqxx::connection>> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool shutdown_ = false;
};

// Database Operations
void init_db_schema(std::shared_ptr<DBConnectionPool> pool);
int create_job(std::shared_ptr<DBConnectionPool> pool, const std::string& type, const std::string& label, const std::string& payload, const std::string& priority);
std::vector<Job> get_all_jobs(std::shared_ptr<DBConnectionPool> pool);
Job get_job_by_id(std::shared_ptr<DBConnectionPool> pool, int id);
void update_job_status(std::shared_ptr<DBConnectionPool> pool, int id, const std::string& status, const std::string& result, int attempts);
void update_job_progress(std::shared_ptr<DBConnectionPool> pool, int id, int progress);
void increment_job_attempts(std::shared_ptr<DBConnectionPool> pool, int id, const std::string& status);
