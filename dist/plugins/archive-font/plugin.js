/**
 * OmniPeek Plugin: Archive Explorer & Font Waterfall (plugins/archive-font/plugin.js)
 * Supports:
 * - Archives (.zip, .7z, .rar, .tar, .gz) via zero-extraction Worker header manifest
 * - Fonts (.ttf, .otf, .woff, .woff2) via dynamic FontFace API waterfall & glyph inspector
 */
(function () {
    window.OmniPluginRegistry = window.OmniPluginRegistry || {};

    function escapeHtml(str) {
        return String(str || '')
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;');
    }

    window.OmniPluginRegistry['archive-font'] = {
        async mount(container, context, hostApi) {
            container.innerHTML = '';
            const mode = context.previewMode || (['.ttf', '.otf', '.woff', '.woff2'].includes(context.ext) ? 'font' : 'archive');

            if (mode === 'font') {
                await this.mountFont(container, context, hostApi);
            } else {
                await this.mountArchive(container, context, hostApi);
            }
        },

        async mountArchive(container, context, hostApi) {
            let manifest = null;
            try {
                const jsonText = await hostApi.fetchText(context.fileUrl);
                manifest = JSON.parse(jsonText);
            } catch (e) {
                container.innerHTML = `<div class="dd-empty-msg">无法读取归档包目录索引: ${escapeHtml(e.message)}</div>`;
                return;
            }

            const entries = manifest.entries || [];
            const totalUncomp = manifest.totalUncompressed || 0;
            const totalComp = manifest.totalCompressed || manifest.archiveSize || context.fileSize || 0;
            const ratio = totalUncomp > 0 ? Math.min(100, Math.round((totalComp / totalUncomp) * 100)) : 100;

            let filterQ = '';
            let sortKey = 'name'; // 'name' | 'size' | 'packed'
            let sortAsc = true;

            hostApi.setToolbar(`
                <input type="text" class="omni-input" id="af-arch-search" placeholder="🔍 搜索压缩包内文件..." />
                <span class="omni-badge">${entries.length} 项 · 压缩比 ${ratio}%</span>
            `);
            hostApi.setStatus(`${manifest.format || 'ARCHIVE'} 归档 · 共 ${entries.length} 项 · 解压后 ${hostApi.formatBytes(totalUncomp)}`);

            const wrap = document.createElement('div');
            wrap.className = 'af-archive-stage';
            wrap.innerHTML = `
                <div class="af-summary-cards">
                    <div class="af-stat-card">
                        <div class="af-stat-label">归档格式</div>
                        <div class="af-stat-val">${escapeHtml(manifest.format || context.ext.toUpperCase())}</div>
                    </div>
                    <div class="af-stat-card">
                        <div class="af-stat-label">文件与目录数</div>
                        <div class="af-stat-val">${entries.length} 项</div>
                    </div>
                    <div class="af-stat-card">
                        <div class="af-stat-label">原始总大小</div>
                        <div class="af-stat-val">${hostApi.formatBytes(totalUncomp)}</div>
                    </div>
                    <div class="af-stat-card">
                        <div class="af-stat-label">压缩后体积 (${ratio}%)</div>
                        <div class="af-stat-val">${hostApi.formatBytes(totalComp)}</div>
                    </div>
                </div>
                <div class="af-list-scroll" id="af-list-scroll"></div>
            `;
            container.appendChild(wrap);

            const listScroll = wrap.querySelector('#af-list-scroll');

            const renderEntries = () => {
                let list = entries.slice();
                if (filterQ) {
                    list = list.filter(e => (e.name || '').toLowerCase().includes(filterQ));
                }
                list.sort((a, b) => {
                    if (a.isDir !== b.isDir) return a.isDir ? -1 : 1;
                    if (sortKey === 'size') return sortAsc ? (a.size - b.size) : (b.size - a.size);
                    if (sortKey === 'packed') return sortAsc ? (a.packed - b.packed) : (b.packed - a.packed);
                    return sortAsc ? (a.name || '').localeCompare(b.name || '') : (b.name || '').localeCompare(a.name || '');
                });

                let html = `
                    <table class="dd-data-table">
                        <thead>
                            <tr>
                                <th class="dd-sortable-th" data-sort="name">路径与文件名 ${sortKey === 'name' ? (sortAsc ? '▲' : '▼') : ''}</th>
                                <th class="dd-sortable-th" data-sort="size" style="width:120px">原始大小 ${sortKey === 'size' ? (sortAsc ? '▲' : '▼') : ''}</th>
                                <th class="dd-sortable-th" data-sort="packed" style="width:120px">压缩大小 ${sortKey === 'packed' ? (sortAsc ? '▲' : '▼') : ''}</th>
                            </tr>
                        </thead>
                        <tbody>
                `;
                for (let i = 0; i < Math.min(list.length, 1000); i++) {
                    const item = list[i];
                    const icon = item.isDir ? '📁' : '📄';
                    html += `
                        <tr>
                            <td><span style="margin-right:6px">${icon}</span>${escapeHtml(item.name)}</td>
                            <td style="font-family:Consolas,monospace">${item.isDir ? '--' : hostApi.formatBytes(item.size)}</td>
                            <td style="font-family:Consolas,monospace">${item.isDir ? '--' : hostApi.formatBytes(item.packed)}</td>
                        </tr>
                    `;
                }
                html += '</tbody></table>';
                listScroll.innerHTML = html;

                listScroll.querySelectorAll('.dd-sortable-th').forEach(th => {
                    th.onclick = () => {
                        const k = th.getAttribute('data-sort');
                        if (sortKey === k) sortAsc = !sortAsc;
                        else { sortKey = k; sortAsc = true; }
                        renderEntries();
                    };
                });
            };

            const searchInput = document.getElementById('af-arch-search');
            if (searchInput) {
                searchInput.oninput = (e) => {
                    filterQ = (e.target.value || '').trim().toLowerCase();
                    renderEntries();
                };
            }

            renderEntries();
        },

        async mountFont(container, context, hostApi) {
            const fontFamilyName = 'OmniPreviewFont_' + Date.now();
            let loadedOk = false;
            try {
                const fontFace = new FontFace(fontFamilyName, `url("${context.fileUrl}")`);
                const loadedFace = await fontFace.load();
                document.fonts.add(loadedFace);
                loadedOk = true;
            } catch (e) {
                console.warn('FontFace load fallback:', e);
            }

            const defaultText = '天地玄黄 宇宙洪荒 · The quick brown fox jumps over the lazy dog 0123456789';
            hostApi.setToolbar(`
                <input type="text" class="omni-input" id="af-font-input" value="${defaultText}" style="width:240px" placeholder="输入自定义预览文字..." />
                <button class="omni-btn" id="af-preset-cn">🇨🇳 中文</button>
                <button class="omni-btn" id="af-preset-en">🔤 英文</button>
                <button class="omni-btn" id="af-preset-code">💻 代码符号</button>
            `);
            hostApi.setStatus(`字体预览 · ${context.fileName} (${hostApi.formatBytes(context.fileSize)})`);

            const sizes = [14, 18, 24, 32, 44, 60];
            const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()_+-=[]{}|;:\'",.<>?/永字八法山水日月风云雷电春夏秋冬';

            const wrap = document.createElement('div');
            wrap.className = 'af-font-stage';
            const fontStyle = loadedOk ? `font-family: "${fontFamilyName}", sans-serif;` : '';

            wrap.innerHTML = `
                <div class="af-font-hero" style="${fontStyle}">
                    <div class="af-font-big-glyph">Aa 永 01</div>
                    <div class="af-font-sub">字库文件: ${escapeHtml(context.fileName)} · 格式: ${context.ext.toUpperCase().replace('.', '')}</div>
                </div>
                <div class="af-waterfall" id="af-waterfall">
                    ${sizes.map(sz => `
                        <div class="af-wf-row">
                            <span class="af-wf-size">${sz}px</span>
                            <div class="af-wf-text" style="${fontStyle} font-size:${sz}px">${escapeHtml(defaultText)}</div>
                        </div>
                    `).join('')}
                </div>
                <div class="af-glyph-section-title">字符集网格预览 (${chars.length} 个常用字符)</div>
                <div class="af-glyph-grid" style="${fontStyle}">
                    ${Array.from(chars).map(ch => {
                        const hex = ch.codePointAt(0).toString(16).toUpperCase().padStart(4, '0');
                        return `<div class="af-glyph-cell" title="U+${hex}">
                            <span class="af-g-char">${escapeHtml(ch)}</span>
                            <span class="af-g-code">U+${hex}</span>
                        </div>`;
                    }).join('')}
                </div>
            `;
            container.appendChild(wrap);

            const updateTexts = (txt) => {
                wrap.querySelectorAll('.af-wf-text').forEach(el => {
                    el.textContent = txt || defaultText;
                });
            };

            const inputEl = document.getElementById('af-font-input');
            const btnCn = document.getElementById('af-preset-cn');
            const btnEn = document.getElementById('af-preset-en');
            const btnCode = document.getElementById('af-preset-code');

            if (inputEl) inputEl.oninput = (e) => updateTexts(e.target.value);
            if (btnCn) btnCn.onclick = () => { const t = '落霞与孤鹜齐飞，秋水共长天一色。0123456789'; if (inputEl) inputEl.value = t; updateTexts(t); };
            if (btnEn) btnEn.onclick = () => { const t = 'Sphinx of black quartz, judge my vow! 0123456789'; if (inputEl) inputEl.value = t; updateTexts(t); };
            if (btnCode) btnCode.onclick = () => { const t = 'const fn = (a, b) => a !== b && a >= 0 ? [1, 2, 3] : { ok: true };'; if (inputEl) inputEl.value = t; updateTexts(t); };
        },

        unmount() {}
    };
})();
