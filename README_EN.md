# OmniPeek 🚀

<div align="center">

![Platform](https://img.shields.io/badge/Platform-Windows_10_%2F_11_x64-0078D4?logo=windows&logoColor=white)
![Formats](https://img.shields.io/badge/Supported_Formats-72+-10b981?logo=files)
![Architecture](https://img.shields.io/badge/Architecture-Microkernel_%2B_Plugins-8b5cf6)
![License](https://img.shields.io/badge/License-MIT-blue)
![Built_With](https://img.shields.io/badge/Powered_by-WebView2_%2B_Three.js_%2B_C%2B%2B-orange)

**The Universal Windows File Explorer Preview Handler (`IPreviewHandler`) & Multi-Modal Thumbnail Engine (`IThumbnailProvider`) for 72+ Formats**

[Quick Start](#-quick-start) • [Supported Formats (72+)](#-supported-formats-inventory-72-formats) • [Plugin SDK](PLUGINS_SDK.md) • [Contributors & Acknowledgments](#-contributors--acknowledgments) • [中文说明](README.md)

</div>

---

## 💡 Origin & Vision

In Windows File Explorer, the native Preview Pane (`Alt + P`) only supports a handful of file formats. Users working across industrial design, multimedia, programming, and document formats are often forced to install heavy, disparate preview applications.

OmniPeek integrates and expands upon the concepts of two pioneering open-source projects:
- **[Stagnating/webview-video-preview-handler](https://github.com/Stagnating/webview-video-preview-handler)**: The groundbreaking preview handler bringing Microsoft Edge WebView2 into Windows Explorer for seamless, hardware-accelerated video/audio playback.
- **[MIke-Slim/ModelPeek](https://github.com/MIke-Slim/ModelPeek)**: The pioneer in industrial 3D CAD/STEP/IGES/STL/GLB previewing and CPU soft-rasterization thumbnails for Windows Shell.

OmniPeek unifies and elevates both into a **Microkernel + Dynamic Plugin Protocol (`PluginRegistry`)** architecture. Formats and renderers are completely decoupled from the C++ core DLL, allowing developers to add new formats dynamically with zero C++ recompilation.

---

## ✨ Key Features

1. **Seamless Windows Explorer Integration**:
   - **Preview Pane (`Alt + P`)**: Native 64-bit COM `IPreviewHandler` powered by WebView2, offering instantaneous (<15ms) switching without blank flicker.
   - **Multi-Modal Thumbnails (`IThumbnailProvider`)**: Built-in 3D soft-rasterizer and Win32 GDI card generator providing isometric 3D models, font specimen cards, archive tables, and code thumbnails.
2. **Four Built-in Plugin Packs Covering 72+ Formats**:
   - 🧊 **`cad-3d` (3D & Industrial CAD, 18 formats)**: `.step`, `.stp`, `.iges`, `.igs`, `.brep`, `.brp`, `.dxf`, `.stl`, `.obj`, `.glb`, `.gltf`, `.3mf`, `.fbx`, `.ply`, `.pcd`, `.gcode`, `.dae`, `.3ds`
   - 🎬 **`media-player` (Audio, Video & Multimedia, 18 formats)**: `.mp4`, `.mkv`, `.webm`, `.mov`, `.avi`, `.m4v`, `.3gp`, `.ogv`, `.mp3`, `.flac`, `.wav`, `.aac`, `.ogg`, `.m4a`, `.opus`, `.svg`, `.webp`, `.gif` (with 64-band WebAudio dynamic spectrum analyzer, frame stepping, snapshot capture)
   - 💻 **`dev-docs` (Developer, Docs & Spreadsheets, 27 formats)**: `.md`, `.markdown`, `.pdf`, `.json`, `.yaml`, `.yml`, `.toml`, `.xml`, `.ini`, `.csv`, `.tsv`, `.xlsx`, `.py`, `.cpp`, `.c`, `.h`, `.js`, `.ts`, `.rs`, `.go`, `.java`, `.cs`, `.sql`, `.ps1`, `.sh`, `.log`, `.txt`
   - 📦 **`archive-font` (Archives & Fonts, 9 formats)**: `.zip`, `.7z`, `.rar`, `.tar`, `.gz`, `.ttf`, `.otf`, `.woff`, `.woff2`
3. **Extensible Plugin Ecosystem**:
   - Formats are defined declaratively in `plugins/*/plugin.json`. Adding a new format requires no recompilation of the COM DLL.
4. **Zero Risk & Non-Intrusive**:
   - Never changes default double-click associations. Automatically backs up previous handlers and restores them cleanly upon uninstall.

---

## ⚡ Quick Start

### 1. Installation
Run `dist/install.bat` as Administrator.

### 2. Previewing
Open File Explorer, press `Alt + P` to toggle the Preview Pane, and click any file in the [`sample_files`](sample_files/) directory.

### 3. Settings Control Center
Launch `dist/OmniPeekSettings.exe` to manage plugins, configure default media volume, autoplay, or check system health.

---

## 🤝 Contributors & Acknowledgments

* **[@Stagnating](https://github.com/Stagnating)**
  * Author of [webview-video-preview-handler](https://github.com/Stagnating/webview-video-preview-handler).
  * Provided core inspiration and architecture for Windows Explorer WebView2-based media preview handlers.
* **[@MIke-Slim](https://github.com/MIke-Slim)**
  * Author of [ModelPeek](https://github.com/MIke-Slim/ModelPeek).
  * Designed the industrial 3D CAD preview pipeline, software rasterizer, and OmniPeek's dynamic plugin microkernel architecture.

---

## 📄 License

MIT License. See [LICENSE](LICENSE) for details.
