/**
 * OmniPeek Plugin: Developer & Document Engine (plugins/dev-docs/plugin.js)
 * Supports:
 * - Markdown (.md, .markdown) with live GFM rendering, TOC sidebar, and Raw/Render toggle
 * - JSON (.json) with interactive collapsible tree, search, and formatted view
 * - Spreadsheets (.csv, .tsv, .xlsx) with sortable columns, multi-sheet tabs, and instant filter
 * - Source Code & Logs (30+ languages) with syntax coloring, line numbers, word wrap, and copy
 * - PDF (.pdf) via native Chromium PDF engine
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

    // Fast regex-based syntax highlighter for source code blocks
    function highlightCodeLine(line) {
        let html = escapeHtml(line);
        // Comments
        if (/^\s*(\/\/|#|--|;)/.test(line)) {
            return `<span class="hl-comment">${html}</span>`;
        }
        // Strings
        html = html.replace(/(&quot;.*?&quot;|'.*?'|`.*?`)/g, '<span class="hl-str">$1</span>');
        // Keywords
        html = html.replace(/\b(function|return|const|let|var|if|else|for|while|switch|case|break|continue|class|struct|public|private|protected|import|from|export|default|async|await|try|catch|new|delete|void|int|float|double|bool|char|string|auto|namespace|using|include|def|elif|None|True|False|fn|pub|impl|match|mut|package|func|interface|SELECT|FROM|WHERE|INSERT|UPDATE|DELETE|JOIN|GROUP|ORDER|BY|CREATE|TABLE)\b/g, '<span class="hl-kw">$1</span>');
        // Numbers
        html = html.replace(/\b(\d+(\.\d+)?)\b/g, '<span class="hl-num">$1</span>');
        return html;
    }

    // Lightweight GFM Markdown parser with TOC extraction
    function parseMarkdown(mdText) {
        const lines = mdText.replace(/\r\n/g, '\n').split('\n');
        let html = '';
        const toc = [];
        let inCode = false;
        let codeLang = '';
        let codeLines = [];
        let inTable = false;
        let tableRows = [];
        let inList = false;

        const flushTable = () => {
            if (!inTable) return;
            inTable = false;
            if (tableRows.length === 0) return;
            html += '<div class="md-table-wrap"><table class="md-table"><thead><tr>';
            const headers = tableRows[0];
            headers.forEach(h => { html += `<th>${formatInline(h)}</th>`; });
            html += '</tr></thead><tbody>';
            for (let r = 1; r < tableRows.length; r++) {
                const row = tableRows[r];
                if (row.every(c => /^[\s:-]+$/.test(c))) continue;
                html += '<tr>';
                row.forEach(c => { html += `<td>${formatInline(c)}</td>`; });
                html += '</tr>';
            }
            html += '</tbody></table></div>';
            tableRows = [];
        };

        const flushList = () => {
            if (inList) {
                html += '</ul>';
                inList = false;
            }
        };

        const formatInline = (text) => {
            let s = escapeHtml(text);
            s = s.replace(/`([^`]+)`/g, '<code class="md-inline-code">$1</code>');
            s = s.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
            s = s.replace(/\*([^*]+)\*/g, '<em>$1</em>');
            s = s.replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2" target="_blank" rel="noopener">$1</a>');
            return s;
        };

        for (let i = 0; i < lines.length; i++) {
            const raw = lines[i];
            if (raw.trim().startsWith('```')) {
                flushTable();
                flushList();
                if (!inCode) {
                    inCode = true;
                    codeLang = raw.trim().slice(3).trim();
                    codeLines = [];
                } else {
                    inCode = false;
                    const highlighted = codeLines.map(highlightCodeLine).join('\n');
                    html += `<div class="md-code-block"><div class="md-code-lang">${escapeHtml(codeLang || 'code')}</div><pre><code>${highlighted}</code></pre></div>`;
                }
                continue;
            }
            if (inCode) {
                codeLines.push(raw);
                continue;
            }

            if (raw.trim().startsWith('|') && raw.trim().endsWith('|')) {
                flushList();
                inTable = true;
                const cells = raw.trim().slice(1, -1).split('|').map(c => c.trim());
                tableRows.push(cells);
                continue;
            } else {
                flushTable();
            }

            const hMatch = raw.match(/^(#{1,6})\s+(.+)$/);
            if (hMatch) {
                flushList();
                const level = hMatch[1].length;
                const title = hMatch[2].trim();
                const id = 'md-h-' + toc.length;
                toc.push({ level, title, id });
                html += `<h${level} id="${id}" class="md-heading md-h${level}">${formatInline(title)}</h${level}>`;
                continue;
            }

            if (/^(\*{3,}|-{3,}|_{3,})\s*$/.test(raw.trim())) {
                flushList();
                html += '<hr class="md-hr" />';
                continue;
            }

            if (raw.trim().startsWith('>')) {
                flushList();
                html += `<blockquote class="md-quote">${formatInline(raw.trim().replace(/^>\s?/, ''))}</blockquote>`;
                continue;
            }

            const listMatch = raw.match(/^\s*([-*+]|\d+\.)\s+(.+)$/);
            if (listMatch) {
                if (!inList) { html += '<ul class="md-list">'; inList = true; }
                let itemBody = listMatch[2];
                if (itemBody.startsWith('[x] ') || itemBody.startsWith('[X] ')) {
                    html += `<li class="md-task"><input type="checkbox" checked disabled /> ${formatInline(itemBody.slice(4))}</li>`;
                } else if (itemBody.startsWith('[ ] ')) {
                    html += `<li class="md-task"><input type="checkbox" disabled /> ${formatInline(itemBody.slice(4))}</li>`;
                } else {
                    html += `<li>${formatInline(itemBody)}</li>`;
                }
                continue;
            } else {
                flushList();
            }

            if (raw.trim() === '') {
                html += '<div class="md-spacer"></div>';
            } else {
                html += `<p class="md-p">${formatInline(raw)}</p>`;
            }
        }
        flushTable();
        flushList();
        return { html, toc };
    }

    // RFC-4180 CSV / TSV parser
    function parseDelimitedTable(text, delimiter) {
        const rows = [];
        let curRow = [];
        let curVal = '';
        let inQuotes = false;
        const len = Math.min(text.length, 512 * 1024);

        for (let i = 0; i < len; i++) {
            const ch = text[i];
            const next = text[i + 1];
            if (inQuotes) {
                if (ch === '"' && next === '"') {
                    curVal += '"';
                    i++;
                } else if (ch === '"') {
                    inQuotes = false;
                } else {
                    curVal += ch;
                }
            } else {
                if (ch === '"') {
                    inQuotes = true;
                } else if (ch === delimiter) {
                    curRow.push(curVal);
                    curVal = '';
                } else if (ch === '\n' || (ch === '\r' && next === '\n')) {
                    if (ch === '\r') i++;
                    curRow.push(curVal);
                    if (curRow.length > 1 || curRow[0] !== '') rows.push(curRow);
                    curRow = [];
                    curVal = '';
                    if (rows.length >= 2000) break;
                } else if (ch !== '\r') {
                    curVal += ch;
                }
            }
        }
        if (curVal !== '' || curRow.length > 0) {
            curRow.push(curVal);
            rows.push(curRow);
        }
        return rows;
    }

    // Offline .xlsx parser using embedded fflate.min.js + DOMParser
    function parseXlsxBuffer(arrayBuffer) {
        if (typeof fflate === 'undefined') {
            throw new Error('fflate library not loaded');
        }
        const files = fflate.unzipSync(new Uint8Array(arrayBuffer));
        const dec = new TextDecoder('utf-8');
        const parser = new DOMParser();

        // 1. Shared Strings
        const sharedStrings = [];
        if (files['xl/sharedStrings.xml']) {
            const sstXml = parser.parseFromString(dec.decode(files['xl/sharedStrings.xml']), 'application/xml');
            const siNodes = sstXml.getElementsByTagName('si');
            for (let i = 0; i < siNodes.length; i++) {
                const tNodes = siNodes[i].getElementsByTagName('t');
                let s = '';
                for (let j = 0; j < tNodes.length; j++) s += tNodes[j].textContent || '';
                sharedStrings.push(s);
            }
        }

        // 2. Sheet Names
        const sheetNames = [];
        if (files['xl/workbook.xml']) {
            const wbXml = parser.parseFromString(dec.decode(files['xl/workbook.xml']), 'application/xml');
            const sNodes = wbXml.getElementsByTagName('sheet');
            for (let i = 0; i < sNodes.length; i++) {
                sheetNames.push(sNodes[i].getAttribute('name') || `Sheet${i + 1}`);
            }
        }

        // 3. Parse each worksheet
        const sheets = [];
        const sheetKeys = Object.keys(files).filter(k => /^xl\/worksheets\/sheet\d+\.xml$/.test(k)).sort();
        sheetKeys.forEach((key, idx) => {
            const wsXml = parser.parseFromString(dec.decode(files[key]), 'application/xml');
            const rowNodes = wsXml.getElementsByTagName('row');
            const rows = [];
            let maxCol = 0;

            const colRefToIdx = (ref) => {
                let col = 0;
                for (let i = 0; i < ref.length; i++) {
                    const c = ref.charCodeAt(i);
                    if (c >= 65 && c <= 90) col = col * 26 + (c - 64);
                    else break;
                }
                return Math.max(0, col - 1);
            };

            for (let r = 0; r < Math.min(rowNodes.length, 1500); r++) {
                const cNodes = rowNodes[r].getElementsByTagName('c');
                const rowData = [];
                for (let c = 0; c < cNodes.length; c++) {
                    const cell = cNodes[c];
                    const ref = cell.getAttribute('r') || '';
                    const cIdx = ref ? colRefToIdx(ref) : c;
                    maxCol = Math.max(maxCol, cIdx + 1);
                    const t = cell.getAttribute('t');
                    const vNode = cell.getElementsByTagName('v')[0];
                    const isNode = cell.getElementsByTagName('is')[0];
                    let val = '';
                    if (t === 's' && vNode) {
                        const sIdx = parseInt(vNode.textContent || '0', 10);
                        val = sharedStrings[sIdx] !== undefined ? sharedStrings[sIdx] : vNode.textContent;
                    } else if (t === 'inlineStr' && isNode) {
                        val = isNode.textContent || '';
                    } else if (vNode) {
                        val = vNode.textContent || '';
                    }
                    while (rowData.length <= cIdx) rowData.push('');
                    rowData[cIdx] = val;
                }
                rows.push(rowData);
            }
            sheets.push({
                name: sheetNames[idx] || `Sheet ${idx + 1}`,
                rows
            });
        });

        return sheets;
    }

    window.OmniPluginRegistry['dev-docs'] = {
        async mount(container, context, hostApi) {
            container.innerHTML = '';
            const mode = context.previewMode || 'code';

            if (mode === 'pdf') {
                hostApi.setToolbar(`<span class="omni-badge">PDF 原生矢量引擎</span>`);
                container.innerHTML = `<iframe class="dd-pdf-frame" src="${context.fileUrl}"></iframe>`;
                hostApi.setStatus(`PDF 文档 · ${hostApi.formatBytes(context.fileSize)}`);
                return;
            }

            if (mode === 'table-xlsx') {
                await this.mountXlsx(container, context, hostApi);
                return;
            }

            const text = await hostApi.fetchText(context.fileUrl);
            const lineCount = text.split('\n').length;
            hostApi.setStatus(`${lineCount} 行 · ${hostApi.formatBytes(context.fileSize)}${context.truncated ? ' (已安全截取前 512KB)' : ''}`);

            if (mode === 'markdown') {
                this.mountMarkdown(container, context, hostApi, text);
            } else if (mode === 'json') {
                this.mountJson(container, context, hostApi, text);
            } else if (mode === 'table-csv' || mode === 'table-tsv') {
                const delim = (mode === 'table-tsv' || context.ext === '.tsv') ? '\t' : ',';
                const rows = parseDelimitedTable(text, delim);
                this.renderDataGrid(container, hostApi, [{ name: context.fileName, rows }]);
            } else {
                this.mountCode(container, context, hostApi, text);
            }
        },

        mountMarkdown(container, context, hostApi, text) {
            const parsed = parseMarkdown(text);
            hostApi.setToolbar(`
                <button class="omni-btn active" id="dd-md-view">📖 渲染视图</button>
                <button class="omni-btn" id="dd-md-raw">📝 源码视图</button>
                <button class="omni-btn active" id="dd-md-toc-btn">📑 目录 (${parsed.toc.length})</button>
                <button class="omni-btn" id="dd-md-copy">📋 复制</button>
            `);

            const tocHtml = parsed.toc.length > 0
                ? parsed.toc.map(item => `<div class="dd-toc-item dd-toc-l${item.level}" data-target="${item.id}">${escapeHtml(item.title)}</div>`).join('')
                : `<div class="dd-toc-empty">无标题大纲</div>`;

            const wrap = document.createElement('div');
            wrap.className = 'dd-md-layout';
            wrap.innerHTML = `
                <aside class="dd-md-toc" id="dd-md-toc-pane">
                    <div class="dd-toc-header">📑 文档大纲</div>
                    <div class="dd-toc-list">${tocHtml}</div>
                </aside>
                <main class="dd-md-body" id="dd-md-content">${parsed.html}</main>
                <pre class="dd-md-raw-pane hidden" id="dd-md-raw-pane"><code>${escapeHtml(text)}</code></pre>
            `;
            container.appendChild(wrap);

            wrap.querySelectorAll('.dd-toc-item').forEach(el => {
                el.onclick = () => {
                    const target = document.getElementById(el.getAttribute('data-target'));
                    if (target) target.scrollIntoView({ behavior: 'smooth', block: 'start' });
                };
            });

            const btnView = document.getElementById('dd-md-view');
            const btnRaw = document.getElementById('dd-md-raw');
            const btnToc = document.getElementById('dd-md-toc-btn');
            const btnCopy = document.getElementById('dd-md-copy');
            const contentPane = wrap.querySelector('#dd-md-content');
            const rawPane = wrap.querySelector('#dd-md-raw-pane');
            const tocPane = wrap.querySelector('#dd-md-toc-pane');

            if (btnView && btnRaw) {
                btnView.onclick = () => {
                    btnView.classList.add('active'); btnRaw.classList.remove('active');
                    contentPane.classList.remove('hidden'); rawPane.classList.add('hidden');
                };
                btnRaw.onclick = () => {
                    btnRaw.classList.add('active'); btnView.classList.remove('active');
                    rawPane.classList.remove('hidden'); contentPane.classList.add('hidden');
                };
            }
            if (btnToc && tocPane) {
                btnToc.onclick = () => {
                    tocPane.classList.toggle('hidden');
                    btnToc.classList.toggle('active', !tocPane.classList.contains('hidden'));
                };
            }
            if (btnCopy) {
                btnCopy.onclick = () => {
                    navigator.clipboard.writeText(text);
                    btnCopy.textContent = '✅ 已复制';
                    setTimeout(() => { btnCopy.textContent = '📋 复制'; }, 1500);
                };
            }
        },

        mountJson(container, context, hostApi, text) {
            let parsedObj = null;
            let parseErr = null;
            try {
                parsedObj = JSON.parse(text);
            } catch (e) {
                parseErr = e.message;
            }

            if (parseErr) {
                this.mountCode(container, context, hostApi, text);
                return;
            }

            hostApi.setToolbar(`
                <input type="text" class="omni-input" id="dd-json-search" placeholder="🔍 搜索键名或值..." />
                <button class="omni-btn" id="dd-json-expand">➕ 全展开</button>
                <button class="omni-btn" id="dd-json-collapse">➖ 全折叠</button>
                <button class="omni-btn" id="dd-json-copy">📋 复制格式化</button>
            `);

            const buildTreeHtml = (val, keyName, depth, filter) => {
                const keyPrefix = keyName !== null ? `<span class="jt-key">"${escapeHtml(keyName)}"</span>: ` : '';
                if (val === null) return `<div class="jt-leaf">${keyPrefix}<span class="jt-null">null</span></div>`;
                if (typeof val === 'boolean') return `<div class="jt-leaf">${keyPrefix}<span class="jt-bool">${val}</span></div>`;
                if (typeof val === 'number') return `<div class="jt-leaf">${keyPrefix}<span class="jt-num">${val}</span></div>`;
                if (typeof val === 'string') return `<div class="jt-leaf">${keyPrefix}<span class="jt-str">"${escapeHtml(val)}"</span></div>`;

                const isArr = Array.isArray(val);
                const entries = isArr ? val.map((v, idx) => [idx, v]) : Object.entries(val);
                const filteredEntries = filter
                    ? entries.filter(([k, v]) => JSON.stringify({ [k]: v }).toLowerCase().includes(filter))
                    : entries;

                const openAttr = depth < 2 || filter ? 'open' : '';
                const summaryBadge = isArr ? `Array[${entries.length}]` : `Object{${entries.length}}`;
                let childrenHtml = '';
                for (let i = 0; i < Math.min(filteredEntries.length, 500); i++) {
                    const [k, v] = filteredEntries[i];
                    childrenHtml += buildTreeHtml(v, isArr ? null : k, depth + 1, filter);
                }
                return `
                    <details class="jt-node" ${openAttr}>
                        <summary>${keyPrefix}<span class="jt-badge">${summaryBadge}</span></summary>
                        <div class="jt-children">${childrenHtml}</div>
                    </details>
                `;
            };

            const wrap = document.createElement('div');
            wrap.className = 'dd-json-stage';
            wrap.innerHTML = buildTreeHtml(parsedObj, null, 0, '');
            container.appendChild(wrap);

            const searchInput = document.getElementById('dd-json-search');
            const btnExpand = document.getElementById('dd-json-expand');
            const btnCollapse = document.getElementById('dd-json-collapse');
            const btnCopy = document.getElementById('dd-json-copy');

            if (searchInput) {
                searchInput.oninput = (e) => {
                    const q = (e.target.value || '').trim().toLowerCase();
                    wrap.innerHTML = buildTreeHtml(parsedObj, null, 0, q);
                };
            }
            if (btnExpand) btnExpand.onclick = () => wrap.querySelectorAll('details.jt-node').forEach(d => d.open = true);
            if (btnCollapse) btnCollapse.onclick = () => wrap.querySelectorAll('details.jt-node').forEach((d, idx) => { if (idx > 0) d.open = false; });
            if (btnCopy) {
                btnCopy.onclick = () => {
                    navigator.clipboard.writeText(JSON.stringify(parsedObj, null, 2));
                    btnCopy.textContent = '✅ 已复制';
                    setTimeout(() => { btnCopy.textContent = '📋 复制格式化'; }, 1500);
                };
            }
        },

        async mountXlsx(container, context, hostApi) {
            try {
                const buf = await hostApi.fetchBuffer(context.fileUrl);
                const sheets = parseXlsxBuffer(buf);
                this.renderDataGrid(container, hostApi, sheets);
            } catch (e) {
                container.innerHTML = `<div class="dd-empty-msg">无法解析 Excel 工作簿: ${escapeHtml(e.message)}</div>`;
            }
        },

        renderDataGrid(container, hostApi, sheets) {
            if (!sheets || sheets.length === 0) {
                container.innerHTML = `<div class="dd-empty-msg">表格无数据</div>`;
                return;
            }
            let activeSheetIdx = 0;
            let filterText = '';
            let sortCol = -1;
            let sortAsc = true;

            hostApi.setToolbar(`
                <input type="text" class="omni-input" id="dd-grid-search" placeholder="🔍 筛选表格行..." />
                <span class="omni-badge" id="dd-grid-stat">0 行</span>
            `);

            const wrap = document.createElement('div');
            wrap.className = 'dd-grid-layout';
            wrap.innerHTML = `
                <div class="dd-grid-scroll" id="dd-grid-scroll"></div>
                <div class="dd-grid-tabs" id="dd-grid-tabs"></div>
            `;
            container.appendChild(wrap);

            const scrollBox = wrap.querySelector('#dd-grid-scroll');
            const tabsBox = wrap.querySelector('#dd-grid-tabs');
            const statBadge = document.getElementById('dd-grid-stat');

            const renderCurrentSheet = () => {
                const sheet = sheets[activeSheetIdx];
                const rows = sheet.rows || [];
                if (rows.length === 0) {
                    scrollBox.innerHTML = `<div class="dd-empty-msg">当前工作表为空</div>`;
                    if (statBadge) statBadge.textContent = '0 行';
                    return;
                }

                const header = rows[0];
                let bodyRows = rows.slice(1);
                if (filterText) {
                    bodyRows = bodyRows.filter(r => r.some(cell => String(cell || '').toLowerCase().includes(filterText)));
                }
                if (sortCol >= 0) {
                    bodyRows.sort((a, b) => {
                        const va = String(a[sortCol] ?? '');
                        const vb = String(b[sortCol] ?? '');
                        const na = parseFloat(va), nb = parseFloat(vb);
                        if (!isNaN(na) && !isNaN(nb)) return sortAsc ? na - nb : nb - na;
                        return sortAsc ? va.localeCompare(vb) : vb.localeCompare(va);
                    });
                }

                const colCount = Math.max(header.length, ...bodyRows.slice(0, 50).map(r => r.length));
                if (statBadge) statBadge.textContent = `${bodyRows.length} 行 × ${colCount} 列`;
                hostApi.setStatus(`工作表 [${sheet.name}] · ${bodyRows.length} 行 × ${colCount} 列`);

                let tbl = '<table class="dd-data-table"><thead><tr><th class="dd-row-idx">#</th>';
                for (let c = 0; c < colCount; c++) {
                    const arrow = sortCol === c ? (sortAsc ? ' ▲' : ' ▼') : '';
                    const title = header[c] !== undefined && header[c] !== '' ? header[c] : String.fromCharCode(65 + (c % 26));
                    tbl += `<th data-col="${c}" class="dd-sortable-th">${escapeHtml(title)}${arrow}</th>`;
                }
                tbl += '</tr></thead><tbody>';

                const displayRows = bodyRows.slice(0, 500);
                for (let r = 0; r < displayRows.length; r++) {
                    const row = displayRows[r];
                    tbl += `<tr><td class="dd-row-idx">${r + 1}</td>`;
                    for (let c = 0; c < colCount; c++) {
                        tbl += `<td>${escapeHtml(row[c] ?? '')}</td>`;
                    }
                    tbl += '</tr>';
                }
                tbl += '</tbody></table>';
                scrollBox.innerHTML = tbl;

                scrollBox.querySelectorAll('.dd-sortable-th').forEach(th => {
                    th.onclick = () => {
                        const c = parseInt(th.getAttribute('data-col'), 10);
                        if (sortCol === c) sortAsc = !sortAsc;
                        else { sortCol = c; sortAsc = true; }
                        renderCurrentSheet();
                    };
                });
            };

            if (sheets.length > 1) {
                tabsBox.innerHTML = sheets.map((s, idx) =>
                    `<button class="dd-sheet-tab ${idx === activeSheetIdx ? 'active' : ''}" data-idx="${idx}">📊 ${escapeHtml(s.name)}</button>`
                ).join('');
                tabsBox.querySelectorAll('.dd-sheet-tab').forEach(btn => {
                    btn.onclick = () => {
                        activeSheetIdx = parseInt(btn.getAttribute('data-idx'), 10);
                        sortCol = -1;
                        tabsBox.querySelectorAll('.dd-sheet-tab').forEach(b => b.classList.remove('active'));
                        btn.classList.add('active');
                        renderCurrentSheet();
                    };
                });
            } else {
                tabsBox.style.display = 'none';
            }

            const searchInput = document.getElementById('dd-grid-search');
            if (searchInput) {
                searchInput.oninput = (e) => {
                    filterText = (e.target.value || '').trim().toLowerCase();
                    renderCurrentSheet();
                };
            }

            renderCurrentSheet();
        },

        mountCode(container, context, hostApi, text) {
            let wrapLines = true;
            hostApi.setToolbar(`
                <input type="text" class="omni-input" id="dd-code-search" placeholder="🔍 搜索代码行..." />
                <button class="omni-btn active" id="dd-code-wrap">↩️ 自动换行</button>
                <button class="omni-btn" id="dd-code-copy">📋 复制全文</button>
            `);

            const lines = text.replace(/\r\n/g, '\n').split('\n');
            const renderLines = (filterQ) => {
                let html = '';
                if (context.truncated) {
                    html += `<div class="dd-trunc-banner">⚡ 大文件极速保护已生效：当前仅加载前 512 KB 内容（原文件大小 ${hostApi.formatBytes(context.fileSize)}）</div>`;
                }
                html += `<table class="dd-code-table ${wrapLines ? 'wrap-on' : ''}"><tbody>`;
                const maxLines = Math.min(lines.length, 4000);
                for (let i = 0; i < maxLines; i++) {
                    const ln = lines[i];
                    if (filterQ && !ln.toLowerCase().includes(filterQ)) continue;
                    html += `<tr><td class="dd-ln">${i + 1}</td><td class="dd-lc">${highlightCodeLine(ln)}</td></tr>`;
                }
                html += '</tbody></table>';
                return html;
            };

            const wrap = document.createElement('div');
            wrap.className = 'dd-code-stage';
            wrap.innerHTML = renderLines('');
            container.appendChild(wrap);

            const searchInput = document.getElementById('dd-code-search');
            const btnWrap = document.getElementById('dd-code-wrap');
            const btnCopy = document.getElementById('dd-code-copy');

            if (searchInput) {
                searchInput.oninput = (e) => {
                    wrap.innerHTML = renderLines((e.target.value || '').trim().toLowerCase());
                };
            }
            if (btnWrap) {
                btnWrap.onclick = () => {
                    wrapLines = !wrapLines;
                    btnWrap.classList.toggle('active', wrapLines);
                    const tbl = wrap.querySelector('.dd-code-table');
                    if (tbl) tbl.classList.toggle('wrap-on', wrapLines);
                };
            }
            if (btnCopy) {
                btnCopy.onclick = () => {
                    navigator.clipboard.writeText(text);
                    btnCopy.textContent = '✅ 已复制';
                    setTimeout(() => { btnCopy.textContent = '📋 复制全文'; }, 1500);
                };
            }
        },

        unmount() {}
    };
})();
