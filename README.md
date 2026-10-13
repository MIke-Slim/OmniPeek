# OmniPeek (万能预览器) 🚀

<div align="center">

![Platform](https://img.shields.io/badge/Platform-Windows_10_%2F_11_x64-0078D4?logo=windows&logoColor=white)
![Formats](https://img.shields.io/badge/Supported_Formats-72+-10b981?logo=files)
![Architecture](https://img.shields.io/badge/Architecture-Microkernel_%2B_Plugins-8b5cf6)
![License](https://img.shields.io/badge/License-MIT-blue)
![Built_With](https://img.shields.io/badge/Powered_by-WebView2_%2B_Three.js_%2B_C%2B%2B-orange)

**为 Windows 资源管理器量身打造的万能原生文件预览 (`IPreviewHandler`) 与多模态缩略图 (`IThumbnailProvider`) 扩展引擎**

[快速开始](#-快速安装与体验) • [支持格式列表 (72+)](#-支持格式一览72-种格式) • [插件开发规范](PLUGINS_SDK.md) • [贡献者与鸣谢](#-鸣谢与贡献者-acknowledgments) • [English README](README_EN.md)

</div>

---

## 💡 项目起源与愿景

在 Windows 资源管理器中，默认的预览窗格（`Alt + P`）支持的格式极为有限，各专业领域的预览工具往往互相割裂。

OmniPeek 吸收了开源社区两大先驱项目的卓越精髓：
- **[Stagnating/webview-video-preview-handler](https://github.com/Stagnating/webview-video-preview-handler)**：开创了在 Windows 资源管理器预览窗格中利用 WebView2 实现流媒体与音视频无感回放的现代化架构。
- **[MIke-Slim/ModelPeek](https://github.com/MIke-Slim/ModelPeek)**：开创了工业三维 CAD/STEP/IGES/STL/GLB 文件的 Windows Shell 原生交互与 CPU 软光栅缩略图管线。

OmniPeek 在两者的基础上完成全面重构与升华，提出**「微内核 + 动态插件协议 (`PluginRegistry`)」**架构——将所有格式引擎完全解耦为独立插件。**任何开发者在不重新编译 C++ 底层 DLL 的情况下，只需编写简单的 JSON 配置与 JS 渲染逻辑，即可将任意自定义格式接入 Windows 资源管理器！**

---

## ✨ 核心特性

1. **原生 Windows 资源管理器无缝集成**：
   - **极速预览窗格 (`Alt + P`)**：基于原生 64 位 COM 接口 `IPreviewHandler` 与 WebView2 独立隔离渲染，文件间即点即看，毫秒级响应，告别卡顿白屏。
   - **多模态高清缩略图 (`IThumbnailProvider`)**：内置 C++ 软光栅与 Win32 GDI 高清卡片生成器，支持 3D 实体等轴测图、真实字体字阶卡、压缩包信息卡与代码/表格缩略图。
2. **四大内置全能插件包（覆盖 72+ 种常用格式）**：
   - 🧊 **`cad-3d` (3D 与工业 CAD 引擎，18 种格式)**：`.step`, `.stp`, `.iges`, `.igs`, `.brep`, `.brp`, `.dxf`, `.stl`, `.obj`, `.glb`, `.gltf`, `.3mf`, `.fbx`, `.ply`, `.pcd`, `.gcode`, `.dae`, `.3ds` —— 360° 无死角旋转、两点距离测量、BBox 尺寸标注、X/Y/Z 动态切片剖切、装配结构树查看。
   - 🎬 **`media-player` (音视频与多媒体工作室，18 种格式)**：`.mp4`, `.mkv`, `.webm`, `.mov`, `.avi`, `.m4v`, `.3gp`, `.ogv`, `.mp3`, `.flac`, `.wav`, `.aac`, `.ogg`, `.m4a`, `.opus`, `.svg`, `.webp`, `.gif` —— 硬件加速播放、倍速调节、单帧步进 (`±1/30s`)、一键捕获高清截图、WebAudio 64 段实时动感音频频谱与动态示波器。
   - 💻 **`dev-docs` (开发者与文档表格引擎，27 种格式)**：`.md`, `.markdown`, `.pdf`, `.json`, `.yaml`, `.yml`, `.toml`, `.xml`, `.ini`, `.csv`, `.tsv`, `.xlsx`, `.py`, `.cpp`, `.c`, `.h`, `.js`, `.ts`, `.rs`, `.go`, `.java`, `.cs`, `.sql`, `.ps1`, `.sh`, `.log`, `.txt` —— Markdown 自动提取大纲、JSON 交互折叠树、Excel/CSV 表格排序筛选、30+ 语言语法高亮与超大文本截断保护。
   - 📦 **`archive-font` (压缩包与字体引擎，9 种格式)**：`.zip`, `.7z`, `.rar`, `.tar`, `.gz`, `.ttf`, `.otf`, `.woff`, `.woff2` —— 零解压快速解析内部文件清单与压缩比检索、字体字阶瀑布流 (14px~60px) 与全字符网格展示。
3. **无限扩展：开放的动态插件体系**：
   - 格式映射完全由 `plugins/*/plugin.json` 声明驱动。
   - 新增格式只需在插件包中添加解析器，打开控制中心一键同步，免去重编 DLL 的繁琐流程。详见 [PLUGINS_SDK.md](PLUGINS_SDK.md)。
4. **系统级安全性与无损还原**：
   - 严格仅注册 Explorer 预览与缩略图接口，**绝不篡改双击默认打开方式**。
   - 注册时自动将系统原有关联备份至 `OmniPeekBackup`，卸载时 100% 无损还原。

---

## 📊 支持格式一览（72 种格式）

| 分类 | 格式扩展名 | 交互预览特性 | 缩略图生成方案 |
| :--- | :--- | :--- | :--- |
| **3D / 工业 CAD** (18) | `.step` `.stp` `.iges` `.igs` `.brep` `.brp` `.dxf` `.stl` `.obj` `.glb` `.gltf` `.3mf` `.fbx` `.ply` `.pcd` `.gcode` `.dae` `.3ds` | 交互式 WebGL 3D 场景、材质线框、网格剖切、标尺测距 | C++ 3D 软光栅渲染等轴测实体彩色位图 |
| **音视频与多媒体** (18) | `.mp4` `.mkv` `.webm` `.mov` `.avi` `.m4v` `.3gp` `.ogv`<br>`.mp3` `.flac` `.wav` `.aac` `.ogg` `.m4a` `.opus`<br>`.svg` `.webp` `.gif` | 原生硬件解码播放、倍速调节、视频逐帧步进与快照截取、WebAudio 实时 64 段动感频谱 | Win32 GDI 高清多媒体信息卡片 + 矢量波形 |
| **代码/文档/表格** (27) | `.md` `.markdown` `.pdf`<br>`.json` `.yaml` `.yml` `.toml` `.xml` `.ini`<br>`.csv` `.tsv` `.xlsx`<br>`.py` `.cpp` `.c` `.h` `.js` `.ts` `.rs` `.go` `.java` `.cs` `.sql` `.ps1` `.sh` `.log` `.txt` | GFM Markdown 大纲渲染、JSON 树形折叠、Excel/CSV 交互网格与排序、Prism 语法高亮 | 语法特征卡片缩略图 |
| **压缩包与字体** (9) | `.zip` `.7z` `.rar` `.tar` `.gz`<br>`.ttf` `.otf` `.woff` `.woff2` | 零解压快速读取内部清单与搜索过滤、字体瀑布流字阶测试与字符集全量网格 | 真实字体字形缩略图 / 压缩包概览卡片 |

---

## ⚡ 快速安装与体验

### 1. 一键安装
1. 从 Releases 下载预编译发布包（或直接使用本仓库内的 `dist` 目录）。
2. 右键以管理员身份运行 `dist/install.bat` 即可完成 COM 注册与插件载入。

### 2. 开始体验
1. 打开 Windows 文件资源管理器（`Win + E`）。
2. 按下快捷键 **`Alt + P`** 展开右侧预览窗格。
3. 浏览本仓库中的 [`sample_files`](sample_files/) 目录，单击任意文件即享秒级流畅预览！

### 3. 可视化控制中心
运行 `dist/OmniPeekSettings.exe`，可随时开启/禁用指定插件、自定义默认音量与自动播放策略、一键清理缓存或查看系统运行状态。

### 4. 一键卸载
如需卸载，右键以管理员身份运行 `dist/uninstall.bat`，即可一键注销并完整还原系统原本的所有文件预览关联。

---

## 🛠️ 从源码构建

本项目采用精简无依赖的纯原生编译方案：
- **开发环境要求**：Windows 10 / 11 64-bit、PowerShell 5.1+
- **构建命令**：
  ```powershell
  pwsh -File scripts/build.ps1
  ```
- 构建脚本会自动编译 `OmniPeekExtension.dll`、`OmniPeekWorker.exe` 与 `OmniPeekSettings.exe` 并完整打包输出至 `dist/`。

---

## 🤝 鸣谢与贡献者 (Acknowledgments)

OmniPeek 的诞生离不开开源社区优秀项目的启发与技术铺垫，在此致以最崇高的敬意：

* **[@Stagnating](https://github.com/Stagnating)**
  * 开源项目：[webview-video-preview-handler](https://github.com/Stagnating/webview-video-preview-handler)
  * **贡献与影响**：开创了在 Windows Explorer 预览窗格中基于 WebView2 实现视频音频原生回放的技术先河。OmniPeek 的音视频流式处理管线与外壳微内核架构深受其启发与影响。
* **[@MIke-Slim](https://github.com/MIke-Slim)**
  * 开源项目：[ModelPeek](https://github.com/MIke-Slim/ModelPeek)
  * **贡献与影响**：开源了 Windows 工业三维 CAD/STEP/STL 模型预览与软光栅缩略图管线，并主持设计了 OmniPeek 的解耦式微内核与 72+ 格式动态插件体系。

同时也感谢以下优秀的开源基础设施与工具库：
* [Microsoft Edge WebView2](https://developer.microsoft.com/en-us/microsoft-edge/webview2/)
* [Three.js](https://threejs.org/)
* [OpenCASCADE Technology](https://dev.opencascade.org/)
* [FFmpeg](https://ffmpeg.org/)

---

## 📄 开源许可证

本项目基于 [MIT License](LICENSE) 协议开源。
