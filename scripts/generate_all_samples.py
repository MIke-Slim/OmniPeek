import os
import sys
import shutil
import subprocess
import struct
import zlib
import tarfile
import gzip
import zipfile
from pathlib import Path
from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont
import openpyxl
from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
from reportlab.lib.pagesizes import letter
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib import colors

ROOT_DIR = Path(__file__).resolve().parent.parent
SAMPLE_DIR = ROOT_DIR / "sample_files"
MODELPEEK_MODELS_DIR = Path(r"C:\Users\20858\Desktop\ModelPeek_开发\sample_models")
FFMPEG_EXE = r"C:\Users\20858\AppData\Roaming\Python\Python310\site-packages\imageio_ffmpeg\binaries\ffmpeg-win-x86_64-v7.1.exe"

DIR_CAD = SAMPLE_DIR / "1_CAD_3D"
DIR_MEDIA = SAMPLE_DIR / "2_Media_Audio_Video"
DIR_DOCS = SAMPLE_DIR / "3_Dev_Docs_Tables"
DIR_ARCHIVE = SAMPLE_DIR / "4_Archives_Fonts"

for d in [DIR_CAD, DIR_MEDIA, DIR_DOCS, DIR_ARCHIVE]:
    d.mkdir(parents=True, exist_ok=True)

print("=== Generating OmniPeek All-Format Sample Files ===")

# -------------------------------------------------------------
# 1. CAD & 3D (18 formats)
# -------------------------------------------------------------
cad_models_map = {
    ".step": ["sample_bracket.step", "test_flange.step"],
    ".stp": ["as1_pe_203.stp"],
    ".iges": ["sample_bracket.iges"],
    ".igs": ["curve_geom_plate.igs", "sample_bracket.igs"],
    ".brep": ["sample_bracket.brep"],
    ".brp": ["sample_bracket.brp"],
    ".dxf": ["architectural_floorplan.dxf", "mechanical_layout.dxf"],
    ".stl": ["hex_nut_m8.stl", "demo_crystal.stl", "slotted_disk.stl"],
    ".obj": ["crystal_mineral.obj", "tree.obj"],
    ".glb": ["duck.glb", "DamagedHelmet.glb"],
    ".gltf": ["box_embedded.gltf"],
    ".3mf": ["cube_gears.3mf"],
    ".fbx": ["nurbs.fbx", "stanford_bunny.fbx"],
    ".ply": ["dolphins.ply", "torus_mesh.ply"],
    ".pcd": ["lidar_scan.pcd", "spiral_galaxy_scan.pcd"],
    ".gcode": ["calibration_cube.gcode", "benchy.gcode"],
    ".dae": ["elf.dae"],
    ".3ds": ["portalgun.3ds"]
}

print("\n[1/4] Copying / Generating CAD & 3D samples (18 formats)...")
for ext, files in cad_models_map.items():
    for fname in files:
        src = MODELPEEK_MODELS_DIR / fname
        if not src.exists():
            src = SAMPLE_DIR / fname
        dst = DIR_CAD / fname
        if src.exists():
            shutil.copy2(src, dst)
            print(f"  + {ext}: {fname} ({dst.stat().st_size} bytes)")
        else:
            print(f"  ! Missing source for {fname}")

# -------------------------------------------------------------
# 2. Media: Audio, Video & Multimedia (18 formats)
# -------------------------------------------------------------
print("\n[2/4] Generating Video, Audio & Image samples (18 formats)...")

# Videos (8)
video_targets = [
    ("sample_video.mp4", [FFMPEG_EXE, "-f", "lavfi", "-i", "testsrc=duration=3:size=320x240:rate=30", "-f", "lavfi", "-i", "sine=frequency=440:duration=3", "-c:v", "libx264", "-c:a", "aac", "-pix_fmt", "yuv420p", "-y"]),
    ("sample_video.mkv", [FFMPEG_EXE, "-f", "lavfi", "-i", "testsrc=duration=3:size=320x240:rate=30", "-f", "lavfi", "-i", "sine=frequency=440:duration=3", "-c:v", "libx264", "-c:a", "aac", "-pix_fmt", "yuv420p", "-y"]),
    ("sample_video.webm", [FFMPEG_EXE, "-f", "lavfi", "-i", "testsrc=duration=3:size=320x240:rate=30", "-f", "lavfi", "-i", "sine=frequency=440:duration=3", "-c:v", "libvpx", "-c:a", "libvorbis", "-pix_fmt", "yuv420p", "-y"]),
    ("sample_video.mov", [FFMPEG_EXE, "-f", "lavfi", "-i", "testsrc=duration=3:size=320x240:rate=30", "-f", "lavfi", "-i", "sine=frequency=440:duration=3", "-c:v", "libx264", "-c:a", "aac", "-pix_fmt", "yuv420p", "-y"]),
    ("sample_video.avi", [FFMPEG_EXE, "-f", "lavfi", "-i", "testsrc=duration=3:size=320x240:rate=30", "-f", "lavfi", "-i", "sine=frequency=440:duration=3", "-c:v", "mpeg4", "-c:a", "mp3", "-y"]),
    ("sample_video.m4v", [FFMPEG_EXE, "-f", "lavfi", "-i", "testsrc=duration=3:size=320x240:rate=30", "-f", "lavfi", "-i", "sine=frequency=440:duration=3", "-c:v", "libx264", "-c:a", "aac", "-pix_fmt", "yuv420p", "-y"]),
    ("sample_video.3gp", [FFMPEG_EXE, "-f", "lavfi", "-i", "testsrc=duration=3:size=176x144:rate=15", "-f", "lavfi", "-i", "sine=frequency=440:duration=3", "-c:v", "libx264", "-c:a", "aac", "-pix_fmt", "yuv420p", "-y"]),
    ("sample_video.ogv", [FFMPEG_EXE, "-f", "lavfi", "-i", "testsrc=duration=3:size=320x240:rate=30", "-f", "lavfi", "-i", "sine=frequency=440:duration=3", "-c:v", "libtheora", "-c:a", "libvorbis", "-pix_fmt", "yuv420p", "-y"]),
]

for name, cmd in video_targets:
    dst = DIR_MEDIA / name
    full_cmd = cmd + [str(dst)]
    res = subprocess.run(full_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ext = Path(name).suffix
    print(f"  + {ext}: {name} ({dst.stat().st_size if dst.exists() else 0} bytes)")

# Audios (7)
audio_targets = [
    ("sample_audio.mp3", [FFMPEG_EXE, "-f", "lavfi", "-i", "sine=frequency=440:duration=3", "-c:a", "libmp3lame", "-y"]),
    ("sample_audio.flac", [FFMPEG_EXE, "-f", "lavfi", "-i", "sine=frequency=440:duration=3", "-c:a", "flac", "-y"]),
    ("sample_audio.wav", [FFMPEG_EXE, "-f", "lavfi", "-i", "sine=frequency=440:duration=3", "-c:a", "pcm_s16le", "-y"]),
    ("sample_audio.aac", [FFMPEG_EXE, "-f", "lavfi", "-i", "sine=frequency=440:duration=3", "-c:a", "aac", "-y"]),
    ("sample_audio.ogg", [FFMPEG_EXE, "-f", "lavfi", "-i", "sine=frequency=440:duration=3", "-c:a", "libvorbis", "-y"]),
    ("sample_audio.m4a", [FFMPEG_EXE, "-f", "lavfi", "-i", "sine=frequency=440:duration=3", "-c:a", "aac", "-y"]),
    ("sample_audio.opus", [FFMPEG_EXE, "-f", "lavfi", "-i", "sine=frequency=440:duration=3", "-c:a", "libopus", "-y"]),
]

for name, cmd in audio_targets:
    dst = DIR_MEDIA / name
    full_cmd = cmd + [str(dst)]
    subprocess.run(full_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ext = Path(name).suffix
    print(f"  + {ext}: {name} ({dst.stat().st_size if dst.exists() else 0} bytes)")

# SVG
svg_path = DIR_MEDIA / "sample_vector.svg"
with open(svg_path, "w", encoding="utf-8") as f:
    f.write('''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 400 300" width="400" height="300">
  <defs>
    <linearGradient id="bgGrad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#0f172a" />
      <stop offset="50%" stop-color="#1e293b" />
      <stop offset="100%" stop-color="#0284c7" />
    </linearGradient>
    <linearGradient id="badgeGrad" x1="0%" y1="0%" x2="100%" y2="0%">
      <stop offset="0%" stop-color="#38bdf8" />
      <stop offset="100%" stop-color="#818cf8" />
    </linearGradient>
  </defs>
  <rect width="400" height="300" rx="16" fill="url(#bgGrad)" />
  <circle cx="200" cy="120" r="54" fill="#1e293b" stroke="url(#badgeGrad)" stroke-width="4" />
  <polygon points="190,95 225,120 190,145" fill="#38bdf8" />
  <text x="200" y="210" text-anchor="middle" font-family="Segoe UI, sans-serif" font-weight="700" font-size="20" fill="#f8fafc">OmniPeek Vector Sample</text>
  <text x="200" y="238" text-anchor="middle" font-family="Segoe UI, sans-serif" font-size="13" fill="#94a3b8">Interactive SVG Graphics · 400 × 300</text>
</svg>''')
print(f"  + .svg: sample_vector.svg ({svg_path.stat().st_size} bytes)")

# WebP
webp_path = DIR_MEDIA / "sample_photo.webp"
img = Image.new("RGBA", (320, 240), color=(15, 23, 42, 255))
draw = ImageDraw.Draw(img)
for i in range(240):
    r = int(15 + (i / 240.0) * 40)
    g = int(23 + (i / 240.0) * 80)
    b = int(42 + (i / 240.0) * 140)
    draw.line([(0, i), (320, i)], fill=(r, g, b))
draw.rectangle([40, 40, 280, 200], outline=(56, 189, 248), width=3)
draw.text((70, 110), "OmniPeek WebP Sample", fill=(255, 255, 255))
img.save(webp_path, "WEBP", quality=90)
print(f"  + .webp: sample_photo.webp ({webp_path.stat().st_size} bytes)")

# GIF
gif_path = DIR_MEDIA / "sample_animation.gif"
frames = []
for f_idx in range(12):
    frame = Image.new("RGBA", (240, 240), color=(15, 23, 42, 255))
    d = ImageDraw.Draw(frame)
    angle = (f_idx / 12.0) * 360
    import math
    cx = 120 + int(40 * math.cos(math.radians(angle)))
    cy = 120 + int(40 * math.sin(math.radians(angle)))
    d.ellipse([cx - 25, cy - 25, cx + 25, cy + 25], fill=(56, 189, 248), outline=(255, 255, 255), width=2)
    d.text((50, 195), f"Frame {f_idx + 1}/12 (GIF)", fill=(203, 213, 225))
    frames.append(frame)
frames[0].save(gif_path, save_all=True, append_images=frames[1:], duration=100, loop=0)
print(f"  + .gif: sample_animation.gif ({gif_path.stat().st_size} bytes)")

# -------------------------------------------------------------
# 3. Developer, Docs, Spreadsheets & Code (27 formats)
# -------------------------------------------------------------
print("\n[3/4] Generating Dev, Docs & Spreadsheets samples (27 formats)...")

# Markdown & Docs
with open(DIR_DOCS / "sample_document.md", "w", encoding="utf-8") as f:
    f.write('''# OmniPeek 架构概览与技术白皮书

OmniPeek 是一款专为 Windows 资源管理器量身打造的**万能快速预览扩展套件**。

## 核心特性
- **毫秒级极速响应**：基于 WebView2 预热实例与独立窗口隔离。
- **全格式多模态覆盖**：涵盖 72+ 种工业 CAD、音视频、代码、表格与字体格式。
- **架构完全解耦**：采用现代微内核设计，所有格式引擎均插件化。

## 性能测试指标
| 格式分类 | 平均加载时延 | 内存占用 (RAM) | 渲染后端 |
| :--- | :--- | :--- | :--- |
| **3D / CAD** | 42 ms | ~45 MB | Three.js + WebGL 2.0 |
| **Media 音视频** | 18 ms | ~28 MB | HTML5 Video & Audio Core |
| **Dev / Docs** | 12 ms | ~15 MB | Monaco / Prism / Tabulator |
| **Archives 归档** | 8 ms | ~10 MB | Worker C++ Header Scanner |

> 提示：按下 Windows `Alt+P` 快捷键即可在资源管理器右侧开启本窗口实时预览！
''')
print("  + .md: sample_document.md")

with open(DIR_DOCS / "sample_notes.markdown", "w", encoding="utf-8") as f:
    f.write('''# 工程研发会议纪要 (Engineering Notes)

- **会议主题**：OmniPeek 扩展兼容性与格式库扩充讨论
- **参会人员**：系统架构师、前端渲染小组、COM 外壳工程师
- **重点决议**：
  1. 保证所有插件在加载时具备故障自愈与异步兜底机制。
  2. 针对高分辨率屏幕（4K / 200% DPI）优化缩略图渲染品质。
''')
print("  + .markdown: sample_notes.markdown")

# PDF
pdf_path = DIR_DOCS / "sample_report.pdf"
doc = SimpleDocTemplate(str(pdf_path), pagesize=letter)
styles = getSampleStyleSheet()
story = []
title_style = ParagraphStyle(
    'TitleStyle',
    parent=styles['Heading1'],
    fontSize=22,
    textColor=colors.HexColor("#0f172a"),
    spaceAfter=14
)
story.append(Paragraph("OmniPeek Universal Preview Specification", title_style))
story.append(Paragraph("A Unified Windows Shell Preview and Thumbnail Solution for 72+ Formats.", styles['Normal']))
story.append(Spacer(1, 14))

table_data = [
    ["Module", "Technology", "Status", "Max File Size"],
    ["CAD 3D", "Three.js / WebGL", "Active", "500 MB"],
    ["Media Player", "HTML5 Native", "Active", "4 GB"],
    ["Docs & Code", "Prism / Tabulator", "Active", "100 MB"],
    ["Archive & Font", "Worker C++ / FontFace", "Active", "2 GB"]
]
t = Table(table_data, colWidths=[110, 130, 90, 110])
t.setStyle(TableStyle([
    ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor("#0284c7")),
    ('TEXTCOLOR', (0, 0), (-1, 0), colors.whitesmoke),
    ('ALIGN', (0, 0), (-1, -1), 'CENTER'),
    ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
    ('FONTSIZE', (0, 0), (-1, -1), 10),
    ('BOTTOMPADDING', (0, 0), (-1, 0), 8),
    ('BACKGROUND', (0, 1), (-1, -1), colors.HexColor("#f8fafc")),
    ('GRID', (0, 0), (-1, -1), 1, colors.HexColor("#cbd5e1")),
]))
story.append(t)
doc.build(story)
print(f"  + .pdf: sample_report.pdf ({pdf_path.stat().st_size} bytes)")

# Structured Config
with open(DIR_DOCS / "sample_config.json", "w", encoding="utf-8") as f:
    f.write('''{
  "$schema": "https://json.schemastore.org/omnipeek-config.json",
  "name": "OmniPeek Universal Previewer",
  "version": "1.0.0",
  "engine": {
    "core": "WebView2",
    "hardwareAcceleration": true,
    "maxCacheSizeMB": 2048
  },
  "modules": [
    { "id": "cad-3d", "enabled": true, "formats": 18 },
    { "id": "media-player", "enabled": true, "formats": 18 },
    { "id": "dev-docs", "enabled": true, "formats": 27 },
    { "id": "archive-font", "enabled": true, "formats": 9 }
  ],
  "preferences": {
    "theme": "dark",
    "autoPlayMedia": true,
    "defaultVolume": 65
  }
}''')
print("  + .json: sample_config.json")

with open(DIR_DOCS / "sample_pipeline.yaml", "w", encoding="utf-8") as f:
    f.write('''name: OmniPeek Build and Release Matrix

on:
  push:
    branches: [ main, release/* ]
  pull_request:
    branches: [ main ]

jobs:
  build-win64:
    runs-on: windows-latest
    steps:
      - name: Checkout Source Code
        uses: actions/checkout@v4
      - name: Compile COM Extension and Worker
        run: pwsh -File scripts/build.ps1
      - name: Run E2E Test Suite
        run: ./tests/test_preview_com.exe
''')
print("  + .yaml: sample_pipeline.yaml")

with open(DIR_DOCS / "sample_docker.yml", "w", encoding="utf-8") as f:
    f.write('''version: '3.8'

services:
  omnipeek-backend:
    image: omnipeek/worker-engine:latest
    container_name: omnipeek_worker
    restart: always
    environment:
      - OMNIPEEK_ENV=production
      - THUMBNAIL_DPI=192
    ports:
      - "8080:8080"
    volumes:
      - ./cache:/app/cache
''')
print("  + .yml: sample_docker.yml")

with open(DIR_DOCS / "sample_settings.toml", "w", encoding="utf-8") as f:
    f.write('''[package]
name = "omnipeek-worker"
version = "1.0.0"
edition = "2021"
authors = ["OmniPeek Team <dev@omnipeek.local>"]
description = "Fast multi-modal thumbnail and preview extraction service"

[dependencies]
windows = { version = "0.52", features = ["Win32_UI_WindowsAndMessaging", "Win32_Graphics_Gdi"] }
serde = { version = "1.0", features = ["derive"] }
serde_json = "1.0"

[profile.release]
opt-level = 3
lto = true
codegen-units = 1
''')
print("  + .toml: sample_settings.toml")

with open(DIR_DOCS / "sample_schema.xml", "w", encoding="utf-8") as f:
    f.write('''<?xml version="1.0" encoding="UTF-8"?>
<OmniPeekRegistry version="1.0">
  <Extensions total="72">
    <Category name="CAD_3D" count="18">
      <Format ext=".step" handler="cad-3d" />
      <Format ext=".stl" handler="cad-3d" />
      <Format ext=".glb" handler="cad-3d" />
    </Category>
    <Category name="Media" count="18">
      <Format ext=".mp4" handler="media-player" />
      <Format ext=".wav" handler="media-player" />
    </Category>
    <Category name="DevDocs" count="27">
      <Format ext=".md" handler="dev-docs" />
      <Format ext=".json" handler="dev-docs" />
      <Format ext=".xlsx" handler="dev-docs" />
    </Category>
    <Category name="ArchiveFont" count="9">
      <Format ext=".zip" handler="archive-font" />
      <Format ext=".ttf" handler="archive-font" />
    </Category>
  </Extensions>
</OmniPeekRegistry>''')
print("  + .xml: sample_schema.xml")

with open(DIR_DOCS / "sample_config.ini", "w", encoding="utf-8") as f:
    f.write('''[General]
Theme=dark
Language=zh-CN
HighDpiEnabled=1

[Media]
Volume=65
Autoplay=1
HardwareAcceleration=1

[Docs]
MaxTextPreviewKB=512
TabSize=4
ShowLineNumbers=1
WordWrap=1

[Thumbnails]
RasterResolution=256
PreserveAspectRatio=1
''')
print("  + .ini: sample_config.ini")

# Tables
with open(DIR_DOCS / "sample_data.csv", "w", encoding="utf-8") as f:
    f.write('''Item,Category,Monthly Sales,Growth Rate,Inventory,Status
Industrial Servo Motor,Robotics,142000,18.4%,350,In Stock
Planetary Gearbox M12,Mechanical,89500,12.1%,120,Low Stock
Stepper Motor Driver 24V,Electronics,67200,24.8%,840,In Stock
High-Precision Optical Linear Scale,Sensors,231500,8.5%,45,Backordered
Aluminum Extrusion Profile 4040,Structure,45000,5.2%,1500,In Stock
''')
print("  + .csv: sample_data.csv")

with open(DIR_DOCS / "sample_metrics.tsv", "w", encoding="utf-8") as f:
    f.write('''MetricID\tBenchmarkName\tLatencyMS\tThroughputRPS\tCPUPercent\tMemoryMB
M-101\tSTEP_Assembly_Tessellation\t38.4\t1520\t24.5\t68.2
M-102\tH264_Hardware_Decode_Init\t14.2\t3890\t12.8\t32.5
M-103\tMonaco_Syntax_Highlighting\t8.1\t8200\t5.4\t18.0
M-104\tZIP_Central_Directory_Scan\t2.6\t24500\t3.1\t6.4
M-105\tFont_Glyph_Metrics_Parse\t6.3\t11400\t4.8\t14.2
''')
print("  + .tsv: sample_metrics.tsv")

# XLSX
wb = openpyxl.Workbook()
ws = wb.active
ws.title = "OmniPeek Metrics"
headers = ["ID", "Format", "Category", "Preview Mode", "Avg Load Time (ms)", "Status"]
ws.append(headers)

rows = [
    [1, ".step", "CAD 3D", "cad-3d", 38.5, "Verified"],
    [2, ".mp4", "Media", "media", 16.2, "Verified"],
    [3, ".md", "Docs", "markdown", 9.4, "Verified"],
    [4, ".xlsx", "Docs", "table-xlsx", 22.1, "Verified"],
    [5, ".zip", "Archive", "archive", 4.8, "Verified"],
    [6, ".ttf", "Font", "font", 12.0, "Verified"]
]
for r in rows:
    ws.append(r)

header_fill = PatternFill(start_color="0284C7", end_color="0284C7", fill_type="solid")
header_font = Font(name="Segoe UI", size=11, bold=True, color="FFFFFF")
for cell in ws[1]:
    cell.fill = header_fill
    cell.font = header_font
    cell.alignment = Alignment(horizontal="center")

for col in ws.columns:
    max_len = max(len(str(cell.value or '')) for cell in col)
    col_letter = openpyxl.utils.get_column_letter(col[0].column)
    ws.column_dimensions[col_letter].width = max(max_len + 4, 12)

wb.save(DIR_DOCS / "sample_workbook.xlsx")
print("  + .xlsx: sample_workbook.xlsx")

# Source Code & Scripts (15 formats)
with open(DIR_DOCS / "sample_script.py", "w", encoding="utf-8") as f:
    f.write('''#!/usr/bin/env python3
"""OmniPeek Core Architecture Python Module."""
from dataclasses import dataclass
from typing import List, Optional

@dataclass
class PreviewRequest:
    file_path: str
    extension: str
    target_dpi: int = 96
    enable_gpu: bool = True

class OmniPeekEngine:
    """Universal Shell Preview Dispatcher."""
    def __init__(self, engine_name: str = "OmniPeek") -> None:
        self.engine_name = engine_name
        self.supported_extensions: List[str] = [".step", ".mp4", ".md", ".zip"]

    def dispatch(self, req: PreviewRequest) -> bool:
        print(f"[{self.engine_name}] Dispatching preview for {req.file_path} (DPI={req.target_dpi})")
        return True

if __name__ == "__main__":
    engine = OmniPeekEngine()
    req = PreviewRequest(file_path="sample.step", extension=".step")
    engine.dispatch(req)
''')
print("  + .py: sample_script.py")

with open(DIR_DOCS / "sample_algorithm.cpp", "w", encoding="utf-8") as f:
    f.write('''#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

template <typename T>
class MatrixBuffer {
private:
    std::vector<T> m_data;
    size_t m_rows, m_cols;
public:
    MatrixBuffer(size_t rows, size_t cols) : m_rows(rows), m_cols(cols), m_data(rows * cols) {}
    T& at(size_t r, size_t c) { return m_data[r * m_cols + c]; }
    size_t size() const { return m_data.size(); }
};

int main() {
    std::cout << "OmniPeek High-Performance Native Engine Initialized.\\n";
    MatrixBuffer<float> mat(4, 4);
    mat.at(0, 0) = 1.0f;
    return 0;
}
''')
print("  + .cpp: sample_algorithm.cpp")

with open(DIR_DOCS / "sample_driver.c", "w", encoding="utf-8") as f:
    f.write('''#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t payload_size;
} OmniHeader;

int validate_header(const OmniHeader* hdr) {
    if (!hdr) return 0;
    return (hdr->magic == 0x5045454B) && (hdr->version >= 1);
}

int main(void) {
    OmniHeader hdr = { .magic = 0x5045454B, .version = 1, .payload_size = 1024 };
    if (validate_header(&hdr)) {
        printf("OmniHeader validation succeeded!\\n");
    }
    return 0;
}
''')
print("  + .c: sample_driver.c")

with open(DIR_DOCS / "sample_header.h", "w", encoding="utf-8") as f:
    f.write('''#pragma once
#ifndef OMNIPEEK_CORE_API_H
#define OMNIPEEK_CORE_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OMNI_SUCCESS = 0,
    OMNI_ERROR_FILE_NOT_FOUND = -1,
    OMNI_ERROR_UNSUPPORTED_FORMAT = -2
} OmniResult;

OmniResult OmniPeek_InitEngine(void);
OmniResult OmniPeek_RenderPreview(const char* filePath, void* hwndTarget);
void OmniPeek_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // OMNIPEEK_CORE_API_H
''')
print("  + .h: sample_header.h")

with open(DIR_DOCS / "sample_app.js", "w", encoding="utf-8") as f:
    f.write('''/**
 * OmniPeek Front-end Interactive Controller
 */
export class OmniViewController {
    constructor(container) {
        this.container = container;
        this.activePlugin = null;
    }

    async loadPreview(fileUrl, ext) {
        console.log(`[OmniPeek] Loading ${fileUrl} (${ext})...`);
        const plugin = window.OmniPluginRegistry ? window.OmniPluginRegistry[ext] : null;
        if (plugin && typeof plugin.mount === 'function') {
            await plugin.mount(this.container, { fileUrl, ext });
        }
    }
}
''')
print("  + .js: sample_app.js")

with open(DIR_DOCS / "sample_types.ts", "w", encoding="utf-8") as f:
    f.write('''export interface FormatDescriptor {
    extension: string;
    category: 'cad' | 'media' | 'docs' | 'archive';
    label: string;
    workerAction?: string;
    thumbnailMode: string;
}

export interface PreviewContext<T = unknown> {
    filePath: string;
    fileUrl: string;
    fileSize: number;
    extraMetadata?: T;
}

export type PreviewMountFn = (container: HTMLElement, ctx: PreviewContext) => Promise<void>;
''')
print("  + .ts: sample_types.ts")

with open(DIR_DOCS / "sample_service.rs", "w", encoding="utf-8") as f:
    f.write('''use std::path::Path;

pub enum PluginCategory {
    Cad3D,
    MediaPlayer,
    DevDocs,
    ArchiveFont,
}

pub struct FormatRegistry {
    pub name: String,
    pub category: PluginCategory,
}

impl FormatRegistry {
    pub fn new(name: &str, category: PluginCategory) -> Self {
        Self { name: name.to_string(), category }
    }
}

fn main() {
    let reg = FormatRegistry::new("OmniPeek Fast Rust Service", PluginCategory::Cad3D);
    println!("Initialized: {}", reg.name);
}
''')
print("  + .rs: sample_service.rs")

with open(DIR_DOCS / "sample_server.go", "w", encoding="utf-8") as f:
    f.write('''package main

import (
	"fmt"
	"net/http"
	"time"
)

func handlePreview(w http.ResponseWriter, r *http.Request) {
	fmt.Fprintf(w, "OmniPeek Go Microservice: Status OK (Time: %s)", time.Now().Format(time.RFC3339))
}

func main() {
	http.HandleFunc("/api/health", handlePreview)
	fmt.Println("OmniPeek Go Service listening on port 8080...")
}
''')
print("  + .go: sample_server.go")

with open(DIR_DOCS / "sample_application.java", "w", encoding="utf-8") as f:
    f.write('''package com.omnipeek.core;

import java.util.List;
import java.util.stream.Collectors;

public record PluginDescriptor(String id, String name, List<String> extensions) {
    public static void main(String[] args) {
        var plugin = new PluginDescriptor("cad-3d", "Industrial CAD", List.of(".step", ".stl", ".glb"));
        System.out.println("Registered Plugin: " + plugin.name() + " for " + plugin.extensions());
    }
}
''')
print("  + .java: sample_application.java")

with open(DIR_DOCS / "sample_program.cs", "w", encoding="utf-8") as f:
    f.write('''using System;
using System.Collections.Generic;
using System.Linq;

namespace OmniPeek.Core
{
    public record FormatInfo(string Extension, string Category, bool Supported);

    public class Program
    {
        public static void Main()
        {
            var formats = new List<FormatInfo>
            {
                new(".step", "CAD", true),
                new(".mp4", "Media", true),
                new(".md", "Docs", true)
            };

            var supported = formats.Where(f => f.Supported).ToList();
            Console.WriteLine($"OmniPeek C# Runner: {supported.Count} formats active.");
        }
    }
}
''')
print("  + .cs: sample_program.cs")

with open(DIR_DOCS / "sample_queries.sql", "w", encoding="utf-8") as f:
    f.write('''-- OmniPeek Universal Format & Telemetry Schema
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
''')
print("  + .sql: sample_queries.sql")

with open(DIR_DOCS / "sample_deploy.ps1", "w", encoding="utf-8") as f:
    f.write('''<#
.SYNOPSIS
    OmniPeek Windows Shell Extension Automation Script
.DESCRIPTION
    Compiles, registers COM DLL, and validates Explorer Preview Handler.
#>
param(
    [string]$TargetConfig = "Release",
    [switch]$RegisterCOM = $true
)

Write-Host ">>> OmniPeek Deployment Automation for Windows Shell <<<" -ForegroundColor Cyan
$DllPath = Join-Path $PSScriptRoot "..\dist\OmniPeekExtension.dll"

if (Test-Path $DllPath) {
    Write-Host "Registering COM Extension: $DllPath" -ForegroundColor Green
    Start-Process "regsvr32.exe" -ArgumentList "/s `"$DllPath`"" -Wait -NoNewWindow
    Write-Host "Registration Completed successfully!" -ForegroundColor Green
}
''')
print("  + .ps1: sample_deploy.ps1")

with open(DIR_DOCS / "sample_script.sh", "w", encoding="utf-8") as f:
    f.write('''#!/usr/bin/env bash
# OmniPeek Build & Sanity Check Script
set -euo pipefail

echo "========================================="
echo " OmniPeek Linux / WSL Test Suite         "
echo "========================================="

FORMAT_COUNT=72
echo "[INFO] Verifying total registered format specifications: ${FORMAT_COUNT}"

for cat in "CAD_3D" "MEDIA" "DEV_DOCS" "ARCHIVE_FONT"; do
    echo "  -> Checking category pipeline: ${cat}... [PASS]"
done

echo "[SUCCESS] All pipelines validated!"
''')
print("  + .sh: sample_script.sh")

with open(DIR_DOCS / "sample_service.log", "w", encoding="utf-8") as f:
    f.write('''2026-10-13 16:30:01.102 [INFO]  OmniPeek Shell Extension loaded into explorer.exe (PID=4988)
2026-10-13 16:30:01.108 [INFO]  PluginRegistry initialized: 4 plugins, 72 formats registered
2026-10-13 16:30:02.450 [DEBUG] Shell IPreviewHandler::SetWindow(hwnd=0x000F042A) received
2026-10-13 16:30:02.455 [DEBUG] Shell IPreviewHandler::SetRect([0, 0, 840, 920]) applied
2026-10-13 16:30:02.460 [INFO]  Shell IPreviewHandler::DoPreview for "sample_bracket.step"
2026-10-13 16:30:02.498 [INFO]  WebView2 NavigationCompleted: URL="http://omnipeek.local/index.html" hr=0x00000000
2026-10-13 16:30:02.512 [INFO]  HostApi: postMessage previewReady received from Viewer
2026-10-13 16:30:05.120 [INFO]  Shell IPreviewHandler::Unload() called cleanly
''')
print("  + .log: sample_service.log")

with open(DIR_DOCS / "sample_readme.txt", "w", encoding="utf-8") as f:
    f.write('''OmniPeek Universal Windows Shell Previewer & Thumbnail Generator
========================================================================

Welcome to OmniPeek!

This directory contains test samples for the Dev/Docs & Text plugin:
- Code files (.py, .cpp, .c, .h, .js, .ts, .rs, .go, .java, .cs, .sql, .ps1, .sh)
- Documents & Notes (.md, .markdown, .pdf, .txt, .log)
- Structured Configs (.json, .yaml, .yml, .toml, .xml, .ini)
- Tabular Spreadsheets (.csv, .tsv, .xlsx)

Press Alt+P in File Explorer to open the Preview Pane and enjoy instantaneous viewing!
''')
print("  + .txt: sample_readme.txt")

# -------------------------------------------------------------
# 4. Archives & Fonts (9 formats)
# -------------------------------------------------------------
print("\n[4/4] Generating Archives & Fonts samples (9 formats)...")

# ZIP
zip_path = DIR_ARCHIVE / "sample_pack.zip"
with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
    zf.writestr("welcome.txt", "Welcome to OmniPeek ZIP Explorer!")
    zf.writestr("configs/app_config.json", '{"name": "OmniPeek", "status": "active"}')
    zf.writestr("dataset/table.csv", "id,name,value\n1,alpha,100\n2,beta,200\n")
    zf.writestr("docs/license.md", "# MIT License\nCopyright (c) 2026 OmniPeek")
print(f"  + .zip: sample_pack.zip ({zip_path.stat().st_size} bytes)")

# TAR
tar_path = DIR_ARCHIVE / "sample_bundle.tar"
with tarfile.open(tar_path, "w") as tf:
    for name, content in [
        ("manifest.json", b'{"archive": "sample_bundle.tar", "version": 1}'),
        ("readme.txt", b"Standard POSIX TAR archive preview test."),
        ("source/main.c", b"#include <stdio.h>\nint main() { return 0; }\n")
    ]:
        ti = tarfile.TarInfo(name=name)
        ti.size = len(content)
        ti.mtime = 1700000000
        import io
        tf.addfile(ti, io.BytesIO(content))
print(f"  + .tar: sample_bundle.tar ({tar_path.stat().st_size} bytes)")

# GZ
gz_path = DIR_ARCHIVE / "sample_data.gz"
with open(gz_path, "wb") as f_out, gzip.GzipFile(filename="sample_raw_data.csv", mode="wb", fileobj=f_out, mtime=1700000000) as gf:
    gf.write(b"timestamp,sensor_a,sensor_b\n2026-10-13T16:00:00,42.5,98.2\n2026-10-13T16:01:00,43.1,97.8\n")
print(f"  + .gz: sample_data.gz ({gz_path.stat().st_size} bytes)")

# 7Z (using Windows built-in bsdtar)
z7_path = DIR_ARCHIVE / "sample_archive.7z"
tar_cmd = ["tar", "-acf", str(z7_path), "sample_document.md", "sample_config.json"]
subprocess.run(tar_cmd, cwd=str(DIR_DOCS), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print(f"  + .7z: sample_archive.7z ({z7_path.stat().st_size if z7_path.exists() else 0} bytes)")

# RAR (Standard RAR4 binary structure)
def create_rar4(filepath, entries):
    data = bytearray(b'Rar!\x1a\x07\x00')
    main_head = bytearray(struct.pack('<HBHH', 0x0000, 0x73, 0x0000, 7))
    crc = zlib.crc32(main_head[2:]) & 0xffff
    struct.pack_into('<H', main_head, 0, crc)
    data.extend(main_head)
    
    for name, content in entries:
        raw_name = name.encode('utf-8')
        name_len = len(raw_name)
        packed_size = len(content)
        unp_size = len(content)
        head_size = 7 + 4 + 4 + 1 + 4 + 4 + 1 + 1 + 2 + 4 + name_len
        file_crc = zlib.crc32(content) & 0xffffffff
        
        file_block = bytearray(struct.pack(
            '<HBHHIIBIIBBHI',
            0,
            0x74,
            0x8000,
            head_size,
            packed_size,
            unp_size,
            0,
            file_crc,
            0x4c210000,
            20,
            0x30,
            name_len,
            0x20
        ) + raw_name)
        crc = zlib.crc32(file_block[2:]) & 0xffff
        struct.pack_into('<H', file_block, 0, crc)
        data.extend(file_block)
        data.extend(b'\x00' + content)
        
    end_head = bytearray(struct.pack('<HBHH', 0x0000, 0x7b, 0x0000, 7))
    crc = zlib.crc32(end_head[2:]) & 0xffff
    struct.pack_into('<H', end_head, 0, crc)
    data.extend(end_head)
    
    with open(filepath, 'wb') as f:
        f.write(data)

rar_path = DIR_ARCHIVE / "sample_backup.rar"
create_rar4(rar_path, [
    ('readme_backup.txt', b'OmniPeek RAR Archive Format Preview Test'),
    ('database/schema.sql', b'CREATE TABLE users (id INT, name TEXT);')
])
print(f"  + .rar: sample_backup.rar ({rar_path.stat().st_size} bytes)")

# Fonts: TTF, OTF, WOFF, WOFF2
ttf_src = Path(r"C:\Windows\Fonts\arial.ttf")
otf_src = Path(r"C:\Windows\Fonts\Noto Sans SC (TrueType).otf")
if not otf_src.exists():
    otf_src = Path(r"C:\Windows\Fonts\SourceHanSansJP-Bold.otf")

ttf_dst = DIR_ARCHIVE / "sample_font.ttf"
otf_dst = DIR_ARCHIVE / "sample_font.otf"
woff_dst = DIR_ARCHIVE / "sample_font.woff"
woff2_dst = DIR_ARCHIVE / "sample_font.woff2"

if ttf_src.exists():
    shutil.copy2(ttf_src, ttf_dst)
    print(f"  + .ttf: sample_font.ttf ({ttf_dst.stat().st_size} bytes)")
    
    font = TTFont(str(ttf_src))
    font.flavor = 'woff'
    font.save(str(woff_dst))
    print(f"  + .woff: sample_font.woff ({woff_dst.stat().st_size} bytes)")
    
    font.flavor = 'woff2'
    font.save(str(woff2_dst))
    print(f"  + .woff2: sample_font.woff2 ({woff2_dst.stat().st_size} bytes)")

if otf_src.exists():
    shutil.copy2(otf_src, otf_dst)
    print(f"  + .otf: sample_font.otf ({otf_dst.stat().st_size} bytes)")

print("\n=== All Sample Files Generated Successfully! ===")
