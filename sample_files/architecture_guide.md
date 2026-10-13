# OmniPeek 万能预览器示例文档

欢迎使用 **OmniPeek**！本文件演示了 `dev-docs` 插件的实时 GFM Markdown 渲染能力。

## 1. 核心特性对比

| 插件标识 | 负责类别 | 渲染内核 |
| :--- | :--- | :--- |
| `cad-3d` | 工业 CAD 与三维网格 | Three.js WebGL + Python OCCT |
| `media-player` | 音视频与矢量图 | HTML5 Media + WebAudio 64段频谱 |
| `dev-docs` | 代码、Markdown、JSON、Excel | 原生 AST 树 + 语法高亮 + 多表引擎 |
| `archive-font` | 压缩包与字库 | 零解压目录树 + FontFace 瀑布流 |

## 2. 示例代码片段

```cpp
bool OmniPeek::PluginRegistry::FindFormatByExtension(const std::wstring& ext, FormatDescriptor& out) {
    // 动态匹配 plugins/*/plugin.json 注册的预览器
    return true;
}
```
