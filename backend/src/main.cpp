#include "db.h"
#include "job_queue.h"
#include "worker_pool.h"
#include <crow.h>
#include <iostream>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

// Define a global CORS middleware to handle browser requests properly
struct CORSMiddleware {
    struct context {};
    void before_handle(crow::request& req, crow::response& res, context& ctx) {
        // Options requests can be intercepted early to avoid database lookups
        if (req.method == "OPTIONS"_method) {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
            res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE");
            res.code = 204;
            res.end();
        }
    }
    void after_handle(crow::request& req, crow::response& res, context& ctx) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE");
    }
};

int main() {
    std::cout << "Initializing Distributed Job Platform Backend..." << std::endl;

    // 1. Fetch Environment Configurations
    const char* db_env = std::getenv("DATABASE_URL");
    std::string db_conn_str = db_env ? db_env : "postgresql://postgres:password@db:5432/jobs_db";

    const char* redis_host_env = std::getenv("REDIS_HOST");
    std::string redis_host = redis_host_env ? redis_host_env : "redis";

    const char* redis_port_env = std::getenv("REDIS_PORT");
    int redis_port = redis_port_env ? std::stoi(redis_port_env) : 6379;

    const char* redis_pass_env = std::getenv("REDIS_PASSWORD");
    std::string redis_pass = redis_pass_env ? redis_pass_env : "";

    const char* workers_env = std::getenv("WORKER_THREADS");
    int num_workers = workers_env ? std::stoi(workers_env) : 3;

    // Read PORT environment variable dynamically (Render injects PORT, e.g. 10000 or 8080)
    const char* port_env = std::getenv("PORT");
    int api_port = (port_env && std::string(port_env).length() > 0) ? std::stoi(port_env) : 8080;

    // Shared pointers for DB, Redis, and WorkerPool wrapped in holder for thread safety
    auto db_pool_holder = std::make_shared<std::shared_ptr<DBConnectionPool>>(nullptr);
    auto redis_queue_holder = std::make_shared<std::shared_ptr<JobQueue>>(nullptr);
    auto worker_pool_holder = std::make_shared<std::shared_ptr<WorkerPool>>(nullptr);

    // Initialize DB, Redis, and Worker Threads asynchronously in background thread
    std::thread init_thread([db_conn_str, redis_host, redis_port, redis_pass, num_workers, db_pool_holder, redis_queue_holder, worker_pool_holder]() {
        std::cout << "[Background Init] Connecting to Database..." << std::endl;
        try {
            *db_pool_holder = DBConnectionPool::create(db_conn_str, 5);
            init_db_schema(*db_pool_holder);
            std::cout << "[Background Init] Database connected & schema ready." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[Background Init Warning] DB error: " << e.what() << std::endl;
        }

        std::cout << "[Background Init] Connecting to Redis..." << std::endl;
        try {
            *redis_queue_holder = std::make_shared<JobQueue>(redis_host, redis_port, redis_pass);
            std::cout << "[Background Init] Redis queue connected." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[Background Init Warning] Redis error: " << e.what() << std::endl;
        }

        if (*db_pool_holder) {
            *worker_pool_holder = std::make_shared<WorkerPool>(*db_pool_holder, *redis_queue_holder, num_workers);
            (*worker_pool_holder)->start();
            std::cout << "[Background Init] Worker pool started with " << num_workers << " threads." << std::endl;
        }
    });
    init_thread.detach();

    // 5. Initialize Crow App with CORS Middleware
    crow::App<CORSMiddleware> app;

    // Root Health Check Route
    CROW_ROUTE(app, "/")
    ([]() {
        crow::json::wvalue res;
        res["status"] = "online";
        res["service"] = "Distributed Job Platform Backend API";
        return crow::response(res);
    });

    // Route to query all jobs (for the main dashboard table)
    CROW_ROUTE(app, "/api/jobs")
    ([db_pool_holder]() {
        auto db_pool = *db_pool_holder;
        if (!db_pool) {
            crow::json::wvalue::list empty_list;
            return crow::response(crow::json::wvalue(empty_list));
        }
        auto jobs = get_all_jobs(db_pool);
        crow::json::wvalue::list job_list;
        for (const auto& job : jobs) {
            crow::json::wvalue j;
            j["id"] = job.id;
            j["type"] = job.type;
            j["label"] = job.label;
            j["priority"] = job.priority;
            j["progress"] = job.progress;
            if (job.payload.empty() || job.payload == "null") {
                j["payload"] = nullptr;
            } else {
                auto p = crow::json::load(job.payload);
                j["payload"] = p ? crow::json::wvalue(p) : crow::json::wvalue(job.payload);
            }
            j["status"] = job.status;
            j["attempts"] = job.attempts;
            if (job.result.empty()) {
                j["result"] = nullptr;
            } else {
                auto r = crow::json::load(job.result);
                j["result"] = r ? crow::json::wvalue(r) : crow::json::wvalue(job.result);
            }
            j["created_at"] = job.created_at;
            j["updated_at"] = job.updated_at;
            job_list.push_back(std::move(j));
        }
        crow::json::wvalue res_json(std::move(job_list));
        return crow::response(res_json);
     });

    // Route to query single job details
    CROW_ROUTE(app, "/api/jobs/<int>")
    ([db_pool_holder](int id) {
        auto db_pool = *db_pool_holder;
        if (!db_pool) {
            crow::json::wvalue err; err["error"] = "Database not connected yet";
            crow::response res(err); res.code = 503; return res;
        }
        try {
            auto job = get_job_by_id(db_pool, id);
            crow::json::wvalue j;
            j["id"] = job.id;
            j["type"] = job.type;
            j["label"] = job.label;
            j["priority"] = job.priority;
            j["progress"] = job.progress;
            if (job.payload.empty() || job.payload == "null") {
                j["payload"] = nullptr;
            } else {
                auto p = crow::json::load(job.payload);
                j["payload"] = p ? crow::json::wvalue(p) : crow::json::wvalue(job.payload);
            }
            j["status"] = job.status;
            j["attempts"] = job.attempts;
            if (job.result.empty()) {
                j["result"] = nullptr;
            } else {
                auto r = crow::json::load(job.result);
                j["result"] = r ? crow::json::wvalue(r) : crow::json::wvalue(job.result);
            }
            j["created_at"] = job.created_at;
            j["updated_at"] = job.updated_at;
            return crow::response(j);
        } catch (const std::exception& e) {
            crow::json::wvalue err; err["error"] = e.what();
            crow::response res(err); res.code = 404; return res;
        }
     });

    // Route to submit a new job
    CROW_ROUTE(app, "/api/jobs")
    .methods("POST"_method)
    ([db_pool_holder, redis_queue_holder](const crow::request& req) {
        auto db_pool = *db_pool_holder;
        auto redis_queue = *redis_queue_holder;
        if (!db_pool || !redis_queue) {
            crow::json::wvalue err; err["error"] = "Services connecting in background, please try again in a moment";
            crow::response res(err); res.code = 503; return res;
        }
        auto body_json = crow::json::load(req.body);
        if (!body_json) {
            crow::json::wvalue err; err["error"] = "Invalid JSON";
            crow::response res(err); res.code = 400; return res;
        }
        if (!body_json.has("type") || body_json["type"].t() != crow::json::type::String) {
            crow::json::wvalue err; err["error"] = "Missing or invalid field 'type'";
            crow::response res(err); res.code = 400; return res;
        }

        std::string type = body_json["type"].s();
        std::string label = body_json.has("label") && body_json["label"].t() == crow::json::type::String
            ? std::string(body_json["label"].s()) : type;
        std::string priority = body_json.has("priority") && body_json["priority"].t() == crow::json::type::String
            ? std::string(body_json["priority"].s()) : "MEDIUM";
        std::string payload_str;

        if (body_json.has("payload")) {
            if (body_json["payload"].t() == crow::json::type::String) {
                payload_str = body_json["payload"].s();
            } else {
                crow::json::wvalue temp(body_json["payload"]);
                payload_str = temp.dump();
            }
        }

        try {
            int job_id = create_job(db_pool, type, label, payload_str, priority);
            redis_queue->enqueue(job_id);
            crow::json::wvalue res_json;
            res_json["id"] = job_id;
            res_json["type"] = type;
            res_json["label"] = label;
            res_json["priority"] = priority;
            res_json["status"] = "QUEUED";
            res_json["progress"] = 0;
            res_json["attempts"] = 0;
            crow::response res(res_json); res.code = 201; return res;
        } catch (const std::exception& e) {
            crow::json::wvalue err; err["error"] = std::string("Failed to enqueue: ") + e.what();
            crow::response res(err); res.code = 500; return res;
        }
     });

    // Route to retry a failed/succeeded job manually
    CROW_ROUTE(app, "/api/jobs/<int>/retry")
    .methods("POST"_method)
    ([db_pool_holder, redis_queue_holder](int id) {
        auto db_pool = *db_pool_holder;
        auto redis_queue = *redis_queue_holder;
        if (!db_pool || !redis_queue) {
            crow::json::wvalue err; err["error"] = "Services connecting in background";
            crow::response res(err); res.code = 503; return res;
        }
        try {
            auto job = get_job_by_id(db_pool, id);
            if (job.status != "FAILED" && job.status != "SUCCESS") {
                crow::json::wvalue err;
                err["error"] = "Only FAILED or SUCCESS jobs can be retried (current status: " + job.status + ")";
                crow::response res(err);
                res.code = 400;
                return res;
            }

            // Set DB status to QUEUED and clear/reset attempts to 0
            update_job_status(db_pool, id, "QUEUED", "Manually triggered retry", 0);

            // Re-enqueue job ID in Redis queue
            redis_queue->enqueue(id);

            crow::json::wvalue res_json;
            res_json["id"] = id;
            res_json["status"] = "QUEUED";
            res_json["attempts"] = 0;
            
            crow::response res(res_json);
            return res;

        } catch (const std::exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            crow::response res(err);
            res.code = 404;
            return res;
        }
     });

    // 6. Start Crow HTTP server on 0.0.0.0:8080 (or PORT env)
    std::cout << "Crow Web Server starting on 0.0.0.0:" << api_port << "..." << std::endl;
    app.bindaddr("0.0.0.0").port(api_port).multithreaded().run();

    // 7. Gracefully shutdown all services once App terminates
    std::cout << "Web server stopped. Cleaning up and joining worker pool threads..." << std::endl;
    worker_pool.stop();
    db_pool->shutdown();
    std::cout << "Graceful shutdown complete." << std::endl;

    return 0;
}
