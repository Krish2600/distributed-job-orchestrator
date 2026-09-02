#include "db.h"
#include <iostream>
#include <stdexcept>

std::shared_ptr<DBConnectionPool> DBConnectionPool::create(const std::string& conn_str, size_t pool_size) {
    auto pool = std::make_shared<DBConnectionPool>(conn_str, pool_size);
    pool->init_pool();
    return pool;
}

DBConnectionPool::DBConnectionPool(const std::string& conn_str, size_t pool_size)
    : conn_str_(conn_str), pool_size_(pool_size) {}

DBConnectionPool::~DBConnectionPool() { shutdown(); }

void DBConnectionPool::init_pool() {
    std::unique_lock<std::mutex> lock(mutex_);
    for (size_t i = 0; i < pool_size_; ++i) {
        try {
            pool_.push(std::make_shared<pqxx::connection>(conn_str_));
        } catch (const std::exception& e) {
            std::cerr << "DB connect warning on connection " << i << ": " << e.what() << std::endl;
            break;
        }
    }
    std::cout << "DB pool initialized with " << pool_.size() << " connections." << std::endl;
}

std::shared_ptr<pqxx::connection> DBConnectionPool::get_connection() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (pool_.empty() && !shutdown_) {
        try {
            return std::make_shared<pqxx::connection>(conn_str_);
        } catch (const std::exception& e) {
            std::cerr << "Dynamic DB connection attempt failed: " << e.what() << std::endl;
            return nullptr;
        }
    }
    if (shutdown_ || pool_.empty()) return nullptr;
    auto conn = pool_.front();
    pool_.pop();
    auto pool_ptr = shared_from_this();
    return std::shared_ptr<pqxx::connection>(conn.get(), [pool_ptr, conn](pqxx::connection*) {
        std::unique_lock<std::mutex> lock(pool_ptr->mutex_);
        if (!pool_ptr->shutdown_) {
            pool_ptr->pool_.push(conn);
            pool_ptr->cv_.notify_one();
        }
    });
}

void DBConnectionPool::shutdown() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (shutdown_) return;
    shutdown_ = true;
    while (!pool_.empty()) pool_.pop();
    cv_.notify_all();
}

void init_db_schema(std::shared_ptr<DBConnectionPool> pool) {
    auto conn = pool->get_connection();
    if (!conn) return;
    pqxx::work tx(*conn);
    tx.exec(
        "CREATE TABLE IF NOT EXISTS jobs ("
        "  id SERIAL PRIMARY KEY,"
        "  type VARCHAR(50) NOT NULL,"
        "  label VARCHAR(100),"
        "  payload JSONB,"
        "  status VARCHAR(20) DEFAULT 'QUEUED',"
        "  priority VARCHAR(10) DEFAULT 'MEDIUM',"
        "  progress INT DEFAULT 0,"
        "  attempts INT DEFAULT 0,"
        "  result TEXT,"
        "  created_at TIMESTAMP DEFAULT NOW(),"
        "  updated_at TIMESTAMP DEFAULT NOW()"
        ")"
    );
    // Add new columns to existing tables (idempotent migration)
    try { tx.exec("ALTER TABLE jobs ADD COLUMN IF NOT EXISTS label VARCHAR(100)"); } catch (...) {}
    try { tx.exec("ALTER TABLE jobs ADD COLUMN IF NOT EXISTS priority VARCHAR(10) DEFAULT 'MEDIUM'"); } catch (...) {}
    try { tx.exec("ALTER TABLE jobs ADD COLUMN IF NOT EXISTS progress INT DEFAULT 0"); } catch (...) {}
    tx.exec("CREATE INDEX IF NOT EXISTS idx_jobs_status ON jobs(status)");
    tx.exec("CREATE INDEX IF NOT EXISTS idx_jobs_priority ON jobs(priority)");
    tx.exec("CREATE INDEX IF NOT EXISTS idx_jobs_created_at ON jobs(created_at DESC)");
    tx.commit();
    std::cout << "Database schema verified." << std::endl;
}

int create_job(std::shared_ptr<DBConnectionPool> pool, const std::string& type, const std::string& label, const std::string& payload, const std::string& priority) {
    auto conn = pool->get_connection();
    if (!conn) throw std::runtime_error("No DB connection available");
    pqxx::work tx(*conn);
    pqxx::result res;
    std::string lbl = label.empty() ? type : label;
    if (payload.empty() || payload == "null") {
        res = tx.exec_params(
            "INSERT INTO jobs (type, label, payload, status, priority, progress, attempts) VALUES ($1, $2, NULL, 'QUEUED', $3, 0, 0) RETURNING id",
            type, lbl, priority
        );
    } else {
        res = tx.exec_params(
            "INSERT INTO jobs (type, label, payload, status, priority, progress, attempts) VALUES ($1, $2, $3::jsonb, 'QUEUED', $4, 0, 0) RETURNING id",
            type, lbl, payload, priority
        );
    }
    tx.commit();
    return res[0][0].as<int>();
}

std::vector<Job> get_all_jobs(std::shared_ptr<DBConnectionPool> pool) {
    auto conn = pool->get_connection();
    if (!conn) return {};
    pqxx::nontransaction tx(*conn);
    pqxx::result res = tx.exec(
        "SELECT id, type, COALESCE(label, type), payload::text, status, "
        "COALESCE(priority, 'MEDIUM'), COALESCE(progress, 0), attempts, result, "
        "to_char(created_at, 'YYYY-MM-DD HH24:MI:SS'), to_char(updated_at, 'YYYY-MM-DD HH24:MI:SS') "
        "FROM jobs ORDER BY "
        "CASE COALESCE(priority,'MEDIUM') WHEN 'HIGH' THEN 1 WHEN 'MEDIUM' THEN 2 ELSE 3 END, "
        "id DESC"
    );
    std::vector<Job> jobs;
    for (auto row : res) {
        jobs.push_back({
            row[0].as<int>(),
            row[1].as<std::string>(),
            row[2].as<std::string>(),
            row[3].is_null() ? "" : row[3].as<std::string>(),
            row[4].as<std::string>(),
            row[5].as<std::string>(),
            row[6].as<int>(),
            row[7].as<int>(),
            row[8].is_null() ? "" : row[8].as<std::string>(),
            row[9].as<std::string>(),
            row[10].as<std::string>()
        });
    }
    return jobs;
}

Job get_job_by_id(std::shared_ptr<DBConnectionPool> pool, int id) {
    auto conn = pool->get_connection();
    if (!conn) throw std::runtime_error("No DB connection available");
    pqxx::nontransaction tx(*conn);
    pqxx::result res = tx.exec_params(
        "SELECT id, type, COALESCE(label, type), payload::text, status, "
        "COALESCE(priority, 'MEDIUM'), COALESCE(progress, 0), attempts, result, "
        "to_char(created_at, 'YYYY-MM-DD HH24:MI:SS'), to_char(updated_at, 'YYYY-MM-DD HH24:MI:SS') "
        "FROM jobs WHERE id = $1", id
    );
    if (res.empty()) throw std::runtime_error("Job not found");
    auto row = res[0];
    return {
        row[0].as<int>(), row[1].as<std::string>(), row[2].as<std::string>(),
        row[3].is_null() ? "" : row[3].as<std::string>(),
        row[4].as<std::string>(), row[5].as<std::string>(),
        row[6].as<int>(), row[7].as<int>(),
        row[8].is_null() ? "" : row[8].as<std::string>(),
        row[9].as<std::string>(), row[10].as<std::string>()
    };
}

void update_job_status(std::shared_ptr<DBConnectionPool> pool, int id, const std::string& status, const std::string& result, int attempts) {
    auto conn = pool->get_connection();
    if (!conn) throw std::runtime_error("No DB connection available");
    pqxx::work tx(*conn);
    int prog = (status == "SUCCESS") ? 100 : (status == "QUEUED" ? 0 : -1);
    if (prog >= 0) {
        tx.exec_params(
            "UPDATE jobs SET status=$1, result=$2, attempts=$3, progress=$4, updated_at=NOW() WHERE id=$5",
            status, result, attempts, prog, id
        );
    } else {
        tx.exec_params(
            "UPDATE jobs SET status=$1, result=$2, attempts=$3, updated_at=NOW() WHERE id=$4",
            status, result, attempts, id
        );
    }
    tx.commit();
}

void update_job_progress(std::shared_ptr<DBConnectionPool> pool, int id, int progress) {
    auto conn = pool->get_connection();
    if (!conn) return;
    pqxx::work tx(*conn);
    tx.exec_params("UPDATE jobs SET progress=$1, updated_at=NOW() WHERE id=$2", progress, id);
    tx.commit();
}

void increment_job_attempts(std::shared_ptr<DBConnectionPool> pool, int id, const std::string& status) {
    auto conn = pool->get_connection();
    if (!conn) throw std::runtime_error("No DB connection available");
    pqxx::work tx(*conn);
    tx.exec_params(
        "UPDATE jobs SET status=$1, attempts=attempts+1, progress=0, updated_at=NOW() WHERE id=$2",
        status, id
    );
    tx.commit();
}

int delete_completed_jobs(std::shared_ptr<DBConnectionPool> pool) {
    auto conn = pool->get_connection();
    if (!conn) throw std::runtime_error("No DB connection available");
    pqxx::work tx(*conn);
    pqxx::result res = tx.exec("DELETE FROM jobs WHERE status IN ('SUCCESS', 'FAILED') RETURNING id");
    tx.commit();
    return static_cast<int>(res.size());
}

int delete_all_jobs(std::shared_ptr<DBConnectionPool> pool) {
    auto conn = pool->get_connection();
    if (!conn) throw std::runtime_error("No DB connection available");
    pqxx::work tx(*conn);
    pqxx::result res = tx.exec("DELETE FROM jobs RETURNING id");
    try {
        tx.exec("ALTER SEQUENCE jobs_id_seq RESTART WITH 1");
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not restart sequence jobs_id_seq: " << e.what() << std::endl;
    }
    tx.commit();
    return static_cast<int>(res.size());
}
