#include "worker.h"
#include <crow/json.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <cmath>

void process_job(int worker_id, int job_id, std::shared_ptr<DBConnectionPool> db_pool, std::shared_ptr<JobQueue> queue);

// Helper: sleep in increments and report progress to the DB
void simulate_work(std::shared_ptr<DBConnectionPool> db_pool, int job_id, int total_ms, int steps = 4) {
    int step_ms = total_ms / steps;
    for (int i = 1; i <= steps; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(step_ms));
        int pct = (int)std::round((100.0 * i) / steps);
        update_job_progress(db_pool, job_id, pct);
    }
}

void run_worker(int worker_id, std::shared_ptr<DBConnectionPool> db_pool,
                std::shared_ptr<JobQueue> queue, std::atomic<bool>& stop_flag) {
    std::cout << "[Worker " << worker_id << "] Thread started." << std::endl;
    redisContext* redis_ctx = nullptr;

    while (!stop_flag) {
        if (redis_ctx == nullptr || redis_ctx->err) {
            if (redis_ctx) { redisFree(redis_ctx); redis_ctx = nullptr; }
            redis_ctx = queue->create_independent_context();
            if (redis_ctx == nullptr) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
        }

        redisReply* reply = (redisReply*)redisCommand(redis_ctx, "BRPOP job_queue 1");
        if (reply == nullptr) {
            redisFree(redis_ctx); redis_ctx = nullptr; continue;
        }
        if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 2) {
            int job_id = std::stoi(reply->element[1]->str);
            freeReplyObject(reply);
            try { process_job(worker_id, job_id, db_pool, queue); }
            catch (const std::exception& e) {
                std::cerr << "[Worker " << worker_id << "] Exception on job " << job_id << ": " << e.what() << std::endl;
            }
        } else { freeReplyObject(reply); }
    }

    if (redis_ctx) redisFree(redis_ctx);
    std::cout << "[Worker " << worker_id << "] Thread exiting." << std::endl;
}

void process_job(int worker_id, int job_id,
                 std::shared_ptr<DBConnectionPool> db_pool, std::shared_ptr<JobQueue> queue) {
    std::cout << "[Worker " << worker_id << "] Dequeued Job " << job_id << std::endl;

    try { increment_job_attempts(db_pool, job_id, "RUNNING"); }
    catch (const std::exception& e) {
        std::cerr << "[Worker " << worker_id << "] Failed to set RUNNING for job " << job_id << ": " << e.what() << std::endl;
        return;
    }

    Job job;
    try { job = get_job_by_id(db_pool, job_id); }
    catch (const std::exception& e) {
        std::cerr << "[Worker " << worker_id << "] Cannot read job " << job_id << ": " << e.what() << std::endl;
        return;
    }

    std::cout << "[Worker " << worker_id << "] Processing [" << job.type << "] \"" << job.label
              << "\" priority=" << job.priority << " attempt=" << job.attempts << std::endl;

    std::string result_str;
    bool should_fail = false;
    std::string fail_reason;

    // Parse payload once
    crow::json::rvalue payload_json;
    bool has_payload = !job.payload.empty() && job.payload != "null";
    if (has_payload) payload_json = crow::json::load(job.payload);

    try {
        // ── Job Type Dispatch ─────────────────────────────────────────────
        if (job.type == "IMAGE_RESIZE") {
            simulate_work(db_pool, job_id, 2000);
            crow::json::wvalue r;
            r["output"] = "/storage/resized_" + std::to_string(job_id) + ".jpg";
            r["width"] = has_payload && payload_json && payload_json.has("width") ? (int)payload_json["width"].i() : 1280;
            r["height"] = has_payload && payload_json && payload_json.has("height") ? (int)payload_json["height"].i() : 720;
            r["format"] = "JPEG";
            r["size_kb"] = 248;
            result_str = r.dump();

        } else if (job.type == "PDF_GENERATE") {
            simulate_work(db_pool, job_id, 3000);
            crow::json::wvalue r;
            r["file"] = "/storage/report_" + std::to_string(job_id) + ".pdf";
            r["pages"] = 14;
            r["size_kb"] = 2048;
            r["template"] = has_payload && payload_json && payload_json.has("template") ? std::string(payload_json["template"].s()) : "default";
            result_str = r.dump();

        } else if (job.type == "EMAIL_SEND") {
            simulate_work(db_pool, job_id, 1000);
            crow::json::wvalue r;
            std::string to = (has_payload && payload_json && payload_json.has("to")) ? std::string(payload_json["to"].s()) : "user@example.com";
            r["recipient"] = to;
            r["msg_id"] = "msg-" + std::to_string(job_id);
            r["status"] = "delivered";
            r["smtp_code"] = 250;
            result_str = r.dump();

        } else if (job.type == "DATA_EXPORT") {
            simulate_work(db_pool, job_id, 4000);
            crow::json::wvalue r;
            r["file"] = "/exports/data_" + std::to_string(job_id) + ".csv";
            r["rows_exported"] = 15420;
            r["size_mb"] = 12;
            r["format"] = has_payload && payload_json && payload_json.has("format") ? std::string(payload_json["format"].s()) : "CSV";
            result_str = r.dump();

        } else if (job.type == "VIDEO_TRANSCODE") {
            simulate_work(db_pool, job_id, 6000, 6);
            crow::json::wvalue r;
            r["output"] = "/storage/video_" + std::to_string(job_id) + ".mp4";
            r["codec"] = "H.264";
            r["resolution"] = has_payload && payload_json && payload_json.has("resolution") ? std::string(payload_json["resolution"].s()) : "1080p";
            r["duration_s"] = 142;
            r["size_mb"] = 420;
            result_str = r.dump();

        } else if (job.type == "DB_BACKUP") {
            simulate_work(db_pool, job_id, 5000);
            crow::json::wvalue r;
            r["snapshot"] = "backup_" + std::to_string(job_id) + ".sql.gz";
            r["tables"] = 38;
            r["size_mb"] = 156;
            r["checksum"] = "sha256:a3b4c5d6e7f8";
            result_str = r.dump();

        } else if (job.type == "CUSTOM") {
            // User-defined duration from payload (default 3s, max 15s)
            int duration_ms = 3000;
            if (has_payload && payload_json && payload_json.has("duration_ms")) {
                duration_ms = std::min((int)payload_json["duration_ms"].i(), 15000);
            }
            int steps = std::max(2, duration_ms / 1000);
            simulate_work(db_pool, job_id, duration_ms, steps);
            crow::json::wvalue r;
            r["label"] = job.label;
            r["duration_ms"] = duration_ms;
            r["status"] = "completed";
            if (has_payload && payload_json && payload_json.has("output")) {
                r["output"] = std::string(payload_json["output"].s());
            }
            result_str = r.dump();

        } else {
            throw std::invalid_argument("Unknown job type: " + job.type);
        }

        // ── Fault injection from payload ──────────────────────────────────
        if (has_payload && payload_json) {
            if (payload_json.has("fail") && payload_json["fail"].t() == crow::json::type::True) {
                should_fail = true;
                fail_reason = "Simulated permanent failure (payload: fail=true)";
            } else if (payload_json.has("fail_attempts")) {
                int fa = (int)payload_json["fail_attempts"].i();
                if (job.attempts <= fa) {
                    should_fail = true;
                    fail_reason = "Simulated transient failure — attempt " + std::to_string(job.attempts) + "/" + std::to_string(fa);
                }
            }
        }

        if (should_fail) throw std::runtime_error(fail_reason);

        // ── Success ───────────────────────────────────────────────────────
        std::cout << "[Worker " << worker_id << "] Job " << job_id << " SUCCESS." << std::endl;
        update_job_status(db_pool, job_id, "SUCCESS", result_str, job.attempts);

    } catch (const std::exception& e) {
        std::cerr << "[Worker " << worker_id << "] Job " << job_id << " FAILED: " << e.what() << std::endl;
        if (job.attempts < 3) {
            std::cout << "[Worker " << worker_id << "] Re-enqueuing job " << job_id << " (attempt " << job.attempts << "/3)" << std::endl;
            update_job_status(db_pool, job_id, "QUEUED", std::string("Error: ") + e.what(), job.attempts);
            queue->enqueue(job_id);
        } else {
            std::cout << "[Worker " << worker_id << "] Job " << job_id << " permanently FAILED after 3 attempts." << std::endl;
            update_job_status(db_pool, job_id, "FAILED", std::string("Failed (3 attempts): ") + e.what(), job.attempts);
        }
    }
}
