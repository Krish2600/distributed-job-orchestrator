-- Database schema initialization for the background job processing platform

CREATE TABLE IF NOT EXISTS jobs (
    id SERIAL PRIMARY KEY,
    type VARCHAR(50) NOT NULL,
    label VARCHAR(100),                        -- Human-readable name (for CUSTOM jobs)
    payload JSONB,
    status VARCHAR(20) DEFAULT 'QUEUED',       -- QUEUED, RUNNING, SUCCESS, FAILED
    priority VARCHAR(10) DEFAULT 'MEDIUM',     -- HIGH, MEDIUM, LOW
    progress INT DEFAULT 0,                    -- 0–100 percent
    attempts INT DEFAULT 0,
    result TEXT,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Index on status and created_at to optimize sorting and dashboard lookups
CREATE INDEX IF NOT EXISTS idx_jobs_status ON jobs(status);
CREATE INDEX IF NOT EXISTS idx_jobs_created_at ON jobs(created_at DESC);
