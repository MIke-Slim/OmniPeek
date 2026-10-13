# OmniPeek 架构概览与技术白皮书

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
