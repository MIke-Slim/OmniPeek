/**
 * OmniPeek Universal Frontend Plugin Host & Router (src/viewer/viewer.js)
 * Dynamically loads plugins from ../plugins/<plugin-id>/<entry> and manages
 * lifecycle (mount / unmount) and hostApi services.
 */
(function () {
    window.OmniPluginRegistry = window.OmniPluginRegistry || {};

    const loadedScripts = {};
    let activePluginId = null;

    function toFileUrl(winPath) {
        if (!winPath) return '';
        if (/^file:\/\//i.test(winPath) || /^https?:\/\//i.test(winPath)) {
            return winPath;
        }
        const normalized = winPath.replace(/\\/g, '/');
        const encoded = normalized.split('/').map(seg => {
            if (/^[A-Za-z]:$/.test(seg)) return seg;
            return encodeURIComponent(seg);
        }).join('/');
        return encoded.startsWith('/') ? `file://${encoded}` : `file:///${encoded}`;
    }

    function extractFileName(winPath) {
        if (!winPath) return '未命名文件';
        const parts = winPath.replace(/\\/g, '/').split('/');
        return parts[parts.length - 1] || winPath;
    }

    function formatBytes(bytes) {
        const b = Number(bytes) || 0;
        if (b <= 0) return '0 B';
        const units = ['B', 'KB', 'MB', 'GB', 'TB'];
        const i = Math.min(units.length - 1, Math.floor(Math.log(b) / Math.log(1024)));
        const val = b / Math.pow(1024, i);
        return `${i === 0 ? val : val.toFixed(i >= 2 ? 2 : 1)} ${units[i]}`;
    }

    function fetchText(url) {
        return new Promise((resolve, reject) => {
            fetch(url)
                .then(r => r.text())
                .then(resolve)
                .catch(() => {
                    const xhr = new XMLHttpRequest();
                    xhr.open('GET', url, true);
                    xhr.responseType = 'text';
                    xhr.onload = () => resolve(xhr.responseText || '');
                    xhr.onerror = () => reject(new Error('无法读取本地文本文件'));
                    xhr.send();
                });
        });
    }

    function fetchBuffer(url) {
        return new Promise((resolve, reject) => {
            fetch(url)
                .then(r => r.arrayBuffer())
                .then(resolve)
                .catch(() => {
                    const xhr = new XMLHttpRequest();
                    xhr.open('GET', url, true);
                    xhr.responseType = 'arraybuffer';
                    xhr.onload = () => resolve(xhr.response);
                    xhr.onerror = () => reject(new Error('无法读取本地二进制文件'));
                    xhr.send();
                });
        });
    }

    function ensurePluginScript(pluginId, entryFile) {
        if (window.OmniPluginRegistry[pluginId]) {
            return Promise.resolve(window.OmniPluginRegistry[pluginId]);
        }
        let cleanEntry = (entryFile || 'plugin.js').replace(/\\/g, '/').replace(/^\/+/, '');
        let scriptUrl;
        if (cleanEntry.startsWith('../plugins/')) {
            scriptUrl = cleanEntry;
        } else if (cleanEntry.startsWith('plugins/')) {
            scriptUrl = `../${cleanEntry}`;
        } else {
            scriptUrl = `../plugins/${encodeURIComponent(pluginId)}/${cleanEntry}`;
        }
        if (loadedScripts[scriptUrl]) {
            return loadedScripts[scriptUrl];
        }
        loadedScripts[scriptUrl] = new Promise((resolve, reject) => {
            const s = document.createElement('script');
            s.src = scriptUrl;
            s.async = true;
            s.onload = () => {
                const mod = window.OmniPluginRegistry[pluginId];
                if (mod) resolve(mod);
                else reject(new Error(`插件 [${pluginId}] 未注册到 window.OmniPluginRegistry`));
            };
            s.onerror = () => reject(new Error(`无法加载插件脚本: ${scriptUrl}`));
            document.head.appendChild(s);
        });
        return loadedScripts[scriptUrl];
    }

    const hostApi = {
        setToolbar(html) {
            const slot = document.getElementById('omni-toolbar-slot');
            if (slot) slot.innerHTML = html || '';
        },
        setStatus(text) {
            const st = document.getElementById('omni-status');
            if (st) st.textContent = text ? `· ${text}` : '';
        },
        showToast(msg, durationMs = 2200) {
            const t = document.getElementById('toast');
            if (!t) return;
            t.textContent = msg;
            t.style.display = 'block';
            clearTimeout(t._timer);
            t._timer = setTimeout(() => { t.style.display = 'none'; }, durationMs);
        },
        showLoading(msg) {
            const ov = document.getElementById('loader-overlay');
            const tx = document.getElementById('loader-text');
            if (tx && msg) tx.textContent = msg;
            if (ov) ov.style.display = 'flex';
        },
        hideLoading() {
            const ov = document.getElementById('loader-overlay');
            if (ov) ov.style.display = 'none';
        },
        formatBytes,
        fetchText,
        fetchBuffer,
        toFileUrl
    };

    async function bootPreviewFromUrl() {
        const params = new URLSearchParams(window.location.search);
        const filePath = params.get('file') || '';
        const origPath = params.get('orig') || filePath;
        const ext = (params.get('ext') || '').toLowerCase();
        const pluginId = params.get('plugin') || 'cad-3d';
        const entryFile = params.get('entry') || 'plugin.js';
        const previewMode = params.get('mode') || '3d';
        const fileSize = parseInt(params.get('size') || '0', 10);
        const truncated = params.get('truncated') === '1';
        const vol = parseInt(params.get('vol') || '60', 10);
        const autoplay = params.get('autoplay') !== '0';
        const theme = params.get('theme') || 'theme-dark';

        if (!filePath) return;

        // Unmount previous plugin if any
        window.OmniHost.unload();

        const cadStage = document.getElementById('cad-stage');
        const universalStage = document.getElementById('universal-stage');
        const pluginContainer = document.getElementById('plugin-stage');

        const fileName = extractFileName(origPath);
        const context = {
            filePath,
            origPath,
            fileName,
            fileUrl: toFileUrl(filePath),
            origUrl: toFileUrl(origPath),
            ext,
            pluginId,
            previewMode,
            fileSize,
            truncated,
            settings: {
                volume: isNaN(vol) ? 60 : vol,
                autoplay,
                theme
            }
        };

        // Update universal header metadata
        const badgeEl = document.getElementById('omni-badge');
        const nameEl = document.getElementById('omni-filename');
        const sizeEl = document.getElementById('omni-size');
        if (badgeEl) badgeEl.textContent = (ext.replace('.', '') || pluginId).toUpperCase();
        if (nameEl) {
            nameEl.textContent = fileName;
            nameEl.title = origPath;
        }
        if (sizeEl) sizeEl.textContent = fileSize > 0 ? formatBytes(fileSize) : '';
        hostApi.setToolbar('');
        hostApi.setStatus('');

        // Switch stage visibility based on pluginId
        if (pluginId === 'cad-3d') {
            if (universalStage) universalStage.style.display = 'none';
            if (cadStage) cadStage.style.display = 'block';
        } else {
            if (cadStage) cadStage.style.display = 'none';
            if (universalStage) universalStage.style.display = 'flex';
        }

        try {
            const plugin = await ensurePluginScript(pluginId, entryFile);
            activePluginId = pluginId;
            await plugin.mount(pluginContainer, context, hostApi);
        } catch (err) {
            console.error('[OmniHost] Plugin load error:', err);
            if (universalStage) universalStage.style.display = 'flex';
            if (cadStage) cadStage.style.display = 'none';
            if (pluginContainer) {
                pluginContainer.innerHTML = `<div class="dd-empty-msg">⚠️ 插件加载失败: ${err.message}</div>`;
            }
        }
    }

    window.OmniHost = {
        boot: bootPreviewFromUrl,
        unload() {
            if (activePluginId && window.OmniPluginRegistry[activePluginId]) {
                try {
                    const mod = window.OmniPluginRegistry[activePluginId];
                    if (typeof mod.unmount === 'function') {
                        mod.unmount();
                    }
                } catch (e) {
                    console.warn('[OmniHost] Unmount warning:', e);
                }
            }
            activePluginId = null;
        },
        hostApi
    };

    window.addEventListener('DOMContentLoaded', () => {
        bootPreviewFromUrl();
    });
})();
