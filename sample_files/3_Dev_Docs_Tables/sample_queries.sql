-- OmniPeek Universal Format & Telemetry Schema
CREATE TABLE IF NOT EXISTS tbl_format_registry (
    format_id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_extension VARCHAR(16) NOT NULL UNIQUE,
    category_id VARCHAR(32) NOT NULL,
    engine_name VARCHAR(64) NOT NULL,
    default_enabled BOOLEAN DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_format_ext ON tbl_format_registry(file_extension);

INSERT INTO tbl_format_registry (file_extension, category_id, engine_name)
VALUES
    ('.step', 'cad-3d', 'Three.js / OpenCASCADE'),
    ('.mp4', 'media-player', 'WebView2 Media Core'),
    ('.md', 'dev-docs', 'Monaco / Markdown-IT'),
    ('.zip', 'archive-font', 'Worker Fast Header Scanner');

SELECT category_id, COUNT(*) AS total_formats
FROM tbl_format_registry
GROUP BY category_id;
