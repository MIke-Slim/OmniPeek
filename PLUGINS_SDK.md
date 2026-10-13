# OmniPeek (万能预览器) 插件开发与格式扩展规范 (Plugin SDK v1.0)

OmniPeek 采用 **“单 CLSID 微内核宿主 + WebView2 统一画布 + 前后端双轨插件协议”** 架构。
**新增一种预览格式或开发一个全新预览器插件时，无需修改或重新编译核心 C++ COM 宿主 (`OmniPeekExtension.dll`)**，只需在 `plugins/` 目录下创建插件文件夹即可被系统自动发现、注册并路由。

---

## 1. 插件目录结构

每个插件都是 `plugins/<plugin-id>/` 下的一个独立文件夹：

```text
plugins/
└── my-custom-plugin/
    ├── plugin.json     # 插件与支持后缀的声明清单 (必需)
    ├── plugin.js       # 前端渲染插件入口脚本 (必需)
    └── style.css       # 可选：自定义样式或子资源
```

---

## 2. `plugin.json` 清单规范

`OmniPeekExtension.dll`、`OmniPeekWorker.exe` 与 `OmniPeekSettings.exe` 均通过 `src/common/PluginRegistry.h` 在运行时动态扫描 `plugins/*/plugin.json`：

```json
{
  "id": "my-custom-plugin",
  "name": "自定义格式预览引擎",
  "version": "1.0.0",
  "description": "插件功能简述",
  "entry": "plugin.js",
  "previewMode": "custom-mode",
  "thumbnailMode": "doc-card",
  "formats": [
    {
      "ext": ".abc",
      "label": ".abc (自定义数据格式)",
      "category": "5. 扩展自定义格式",
      "previewMode": "custom-mode",
      "thumbnailMode": "doc-card",
      "workerAction": ""
    }
  ]
}
```

### 字段说明：
- **`ext`**：文件扩展名（以 `.` 开头，小写）。
- **`previewMode`**：传递给前端插件的细分渲染模式（例如 `"video"`, `"audio"`, `"markdown"`, `"json"`, `"table-csv"`, `"archive"`, `"font"`, `"cad-3d"`）。
- **`thumbnailMode`**：文件夹缩略图 (`IThumbnailProvider`) 的生成管线策略：
  - `"3d-raster"`：后台 Worker 使用 CPU 软件光栅化引擎生成立体三维光照缩略图。
  - `"media-card"`：生成音视频/多媒体高分波形与元数据缩略图卡。
  - `"doc-card"`：读取文件前 12 行真实内容，使用 GDI 渲染语法着色微缩文档卡 + 语言角标。
  - `"archive-card"`：解析压缩包目录索引头，渲染归档文件数量与前几项列表缩略图卡。
  - `"font-card"`：通过 `AddFontResourceExW(..., FR_PRIVATE, 0)` 临时加载字体，渲染真实的 `Aa 永 123` 字形卡片。
- **`workerAction`**：预览前是否需要后台 `OmniPeekWorker.exe` 进行预处理转换：
  - `""`（空字符串，**纯前端直读模式**）：WebView2 直接通过本地安全流 URL 读取原始文件。
  - `"convert-cad"`：由后台 Worker 将工业 B-Rep 格式（如 `.step`, `.iges`, `.brep`）转换为高速三角网格缓存后传给前端。
  - `"archive-list"`：由后台 Worker 免解压读取压缩包（`.zip`, `.7z`, `.rar`, `.tar`, `.gz`）文件头索引，生成 JSON 目录树缓存传给前端。
  - `"text-head"`：由后台 Worker 对超大文本/代码/日志进行安全头部截取与 UTF-8 编码归一化，防止百兆大文件卡死前端。

---

## 3. 前端插件生命周期接口 (`window.OmniPluginRegistry`)

每个插件的 `plugin.js` 只需向全局 `window.OmniPluginRegistry` 注册一个包含 `mount` 和 `unmount` 方法的对象：

```javascript
window.OmniPluginRegistry = window.OmniPluginRegistry || {};

window.OmniPluginRegistry["my-custom-plugin"] = {
  /**
   * 挂载并渲染文件预览
   * @param {HTMLElement} container - 插件专属 DOM 挂载容器
   * @param {Object} context - 文件上下文信息:
   *   - fileUrl:      可用于 fetch / <video> / <iframe> 的 file:/// URL (若经 Worker 转换则为缓存文件 URL)
   *   - origPath:     原始本地文件绝对路径 (如 "C:/test/demo.abc")
   *   - fileName:     文件名 (如 "demo.abc")
   *   - ext:          小写后缀名 (如 ".abc")
   *   - previewMode:  plugin.json 中声明的 previewMode
   *   - fileSize:     原始文件字节大小
   *   - truncated:    是否因超大文件被安全截断 ("0" 或 "1")
   *   - settings:     全局设置 { volume, autoplay, theme, maxTextPreviewKB }
   * @param {Object} hostApi - 宿主提供的工具 API:
   *   - setToolbar(htmlOrNodes): 在顶部公共导航栏右侧注入插件专属工具按钮
   *   - setStatus(text):         更新底部/顶部状态栏信息
   *   - fetchText(url):          安全读取文本内容
   *   - fetchBuffer(url):        读取二进制 ArrayBuffer
   *   - formatBytes(bytes):      格式化文件大小显示
   */
  async mount(container, context, hostApi) {
    hostApi.setStatus(`正在预览: ${context.fileName}`);
    const text = await hostApi.fetchText(context.fileUrl);
    container.innerHTML = `<div style="padding:16px;">${text}</div>`;
  },

  /**
   * 当用户切换到其他文件或关闭预览窗格时触发（用于释放 WebGL 显存、暂停音视频或清理定时器）
   */
  unmount() {
    // 清理资源
  }
};
```
