# OmniPeek Contributors & Acknowledgments

OmniPeek (万能预览器) is built upon and inspired by pioneering open-source projects in the Windows Shell extension ecosystem. We express our deepest gratitude to the following authors and contributors:

---

## 🌟 Core Contributors & Upstream Architects

### 1. **Stagnating** ([@Stagnating](https://github.com/Stagnating))
- **Project**: [webview-video-preview-handler](https://github.com/Stagnating/webview-video-preview-handler)
- **Contribution & Impact**:
  - Pioneered the modern C++ `IPreviewHandler` integration with Microsoft Edge WebView2 for multimedia playback in the Windows File Explorer preview pane.
  - Provided the foundational inspiration and architecture for OmniPeek's video/audio streaming core, hardware-accelerated playback pipeline, and WebView2 preview handler concepts.

### 2. **MIke-Slim** ([@MIke-Slim](https://github.com/MIke-Slim))
- **Project**: [ModelPeek](https://github.com/MIke-Slim/ModelPeek)
- **Contribution & Impact**:
  - Designed and developed the industrial 3D CAD/STEP/IGES/STL/GLB preview extension and software rasterization thumbnail pipeline.
  - Designed the unified microkernel architecture (`PluginRegistry`), decoupling format engines into zero-recompilation dynamic plugins, and orchestrating full-coverage multi-modal support for 72+ formats.

---

## 🤝 Open Source Ecosystem & Third-Party Credits

We also gratefully acknowledge the following open-source libraries that make OmniPeek possible:
- **Microsoft WebView2** — High-performance modern web rendering engine for Windows Shell
- **Three.js** — 3D WebGL runtime for industrial CAD, mesh, and point cloud visualization
- **OpenCASCADE Technology (OCCT)** — B-Rep boundary representation and parametric CAD kernel
- **FFmpeg** (`imageio-ffmpeg`) — Multi-format media processing and sample generation
- **FontTools** & **Brotli** — OpenType/TrueType/WOFF/WOFF2 font analysis and compilation
- **ReportLab** & **OpenPyXL** — Document and spreadsheet parsing and generation
