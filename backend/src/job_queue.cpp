#include "job_queue.h"
#include <iostream>

JobQueue::JobQueue(const std::string& host, int port, const std::string& password)
    : host_(host), port_(port), password_(password) {
    // Attempt initial connection. If it fails, ensure_connected will throw/reconnect
    try {
        ensure_connected();
    } catch (const std::exception& e) {
        std::cerr << "Warning: Initial Redis connection failed: " << e.what() 
                  << ". Reconnection will be attempted during enqueue." << std::endl;
    }
}

JobQueue::~JobQueue() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ctx_) {
        redisFree(ctx_);
    }
}

void JobQueue::ensure_connected() {
    if (ctx_ && !ctx_->err) {
        return;
    }
    if (ctx_) {
        redisFree(ctx_);
        ctx_ = nullptr;
    }
    ctx_ = redisConnect(host_.c_str(), port_);
    if (ctx_ == nullptr || ctx_->err) {
        std::string err_str = ctx_ ? ctx_->errstr : "unknown error";
        if (ctx_) {
            redisFree(ctx_);
            ctx_ = nullptr;
        }
        throw std::runtime_error("Redis connection failed: " + err_str);
    }
    if (!password_.empty()) {
        redisReply* auth_reply = (redisReply*)redisCommand(ctx_, "AUTH %s", password_.c_str());
        if (auth_reply && auth_reply->type == REDIS_REPLY_ERROR) {
            freeReplyObject(auth_reply);
            auth_reply = (redisReply*)redisCommand(ctx_, "AUTH default %s", password_.c_str());
        }
        if (auth_reply) freeReplyObject(auth_reply);
    }
    std::cout << "Connected to Redis at " << host_ << ":" << port_ << std::endl;
}

void JobQueue::enqueue(int job_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    ensure_connected();
    
    std::string job_id_str = std::to_string(job_id);
    redisReply* reply = (redisReply*)redisCommand(ctx_, "LPUSH job_queue %s", job_id_str.c_str());
    
    if (reply == nullptr) {
        std::cerr << "Redis LPUSH failed (connection lost). Reconnecting..." << std::endl;
        redisFree(ctx_);
        ctx_ = nullptr;
        
        ensure_connected();
        reply = (redisReply*)redisCommand(ctx_, "LPUSH job_queue %s", job_id_str.c_str());
    }
    
    if (reply != nullptr) {
        freeReplyObject(reply);
    } else {
        throw std::runtime_error("Redis LPUSH failed after reconnection attempt");
    }
}

redisContext* JobQueue::create_independent_context() const {
    redisContext* c = redisConnect(host_.c_str(), port_);
    if (c == nullptr || c->err) {
        std::string err_str = c ? c->errstr : "unknown error";
        if (c) redisFree(c);
        std::cerr << "Failed to create independent Redis context: " << err_str << std::endl;
        return nullptr;
    }
    if (!password_.empty()) {
        redisReply* auth_reply = (redisReply*)redisCommand(c, "AUTH %s", password_.c_str());
        if (auth_reply && auth_reply->type == REDIS_REPLY_ERROR) {
            freeReplyObject(auth_reply);
            auth_reply = (redisReply*)redisCommand(c, "AUTH default %s", password_.c_str());
        }
        if (auth_reply) freeReplyObject(auth_reply);
    }
    return c;
}
