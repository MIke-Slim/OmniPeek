/**
 * OmniPeek Plugin: Media Player Studio (plugins/media-player/plugin.js)
 * Supports Video (.mp4, .mkv, .webm, .mov, .avi, .m4v, .3gp, .ogv),
 * Audio (.mp3, .flac, .wav, .aac, .ogg, .m4a, .opus) with 64-band WebAudio Spectrum,
 * and Vector/Modern Images (.svg, .webp, .gif, .png, .jpg).
 */
(function () {
    window.OmniPluginRegistry = window.OmniPluginRegistry || {};

    let activeMediaEl = null;
    let audioCtx = null;
    let animFrameId = null;

    function stopActiveMedia() {
        if (animFrameId) {
            cancelAnimationFrame(animFrameId);
            animFrameId = null;
        }
        if (activeMediaEl) {
            try {
                activeMediaEl.pause();
                activeMediaEl.removeAttribute('src');
                activeMediaEl.load();
            } catch (e) {}
            activeMediaEl = null;
        }
        if (audioCtx) {
            try { audioCtx.close(); } catch (e) {}
            audioCtx = null;
        }
    }

    function getSavedVolume(defaultVol) {
        const saved = localStorage.getItem('omnipeek_media_volume');
        if (saved !== null) {
            const v = parseFloat(saved);
            if (!isNaN(v) && v >= 0 && v <= 1) return v;
        }
        return Math.max(0, Math.min(1, (defaultVol || 60) / 100));
    }

    function formatDuration(sec) {
        if (!isFinite(sec) || sec < 0) return '00:00';
        const m = Math.floor(sec / 60);
        const s = Math.floor(sec % 60);
        const h = Math.floor(m / 60);
        if (h > 0) {
            return `${h}:${String(m % 60).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
        }
        return `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
    }

    window.OmniPluginRegistry['media-player'] = {
        mount(container, context, hostApi) {
            stopActiveMedia();
            container.innerHTML = '';

            const mode = context.previewMode || 'video';
            const fileUrl = context.fileUrl;
            const initVol = getSavedVolume(context.settings.volume);
            const autoplay = context.settings.autoplay !== false;

            if (mode === 'image') {
                this.mountImage(container, context, hostApi, fileUrl);
            } else if (mode === 'audio') {
                this.mountAudio(container, context, hostApi, fileUrl, initVol, autoplay);
            } else {
                this.mountVideo(container, context, hostApi, fileUrl, initVol, autoplay);
            }
        },

        mountVideo(container, context, hostApi, fileUrl, initVol, autoplay) {
            hostApi.setToolbar(`
                <button class="omni-btn" id="mp-step-back" title="后退一帧 (1/30s)">⏮ -1帧</button>
                <button class="omni-btn" id="mp-step-fwd" title="前进一帧 (1/30s)">+1帧 ⏭</button>
                <select class="omni-select" id="mp-speed" title="播放倍速">
                    <option value="0.5">0.5x</option>
                    <option value="1" selected>1.0x</option>
                    <option value="1.25">1.25x</option>
                    <option value="1.5">1.5x</option>
                    <option value="2">2.0x</option>
                </select>
                <button class="omni-btn active" id="mp-loop" title="循环播放">🔁 循环</button>
                <button class="omni-btn" id="mp-snap" title="截取当前视频帧">📷 截图</button>
            `);

            const wrap = document.createElement('div');
            wrap.className = 'mp-video-stage';
            wrap.innerHTML = `
                <video id="mp-video-el" controls loop playsinline preload="metadata"></video>
                <div class="mp-hud-bar" id="mp-video-hud">
                    <span id="mp-vid-res">分辨率: 检测中...</span>
                    <span>•</span>
                    <span id="mp-vid-dur">时长: --:--</span>
                    <span>•</span>
                    <span>大小: ${hostApi.formatBytes(context.fileSize)}</span>
                </div>
            `;
            container.appendChild(wrap);

            const video = wrap.querySelector('#mp-video-el');
            activeMediaEl = video;
            video.volume = initVol;
            video.src = fileUrl;

            video.addEventListener('volumechange', () => {
                localStorage.setItem('omnipeek_media_volume', String(video.volume));
            });

            video.addEventListener('loadedmetadata', () => {
                const resEl = wrap.querySelector('#mp-vid-res');
                const durEl = wrap.querySelector('#mp-vid-dur');
                if (resEl) resEl.textContent = `分辨率: ${video.videoWidth} × ${video.videoHeight}`;
                if (durEl) durEl.textContent = `时长: ${formatDuration(video.duration)}`;
                hostApi.setStatus(`${video.videoWidth}×${video.videoHeight} · ${formatDuration(video.duration)}`);
                if (autoplay) {
                    video.play().catch(() => {});
                }
            });

            const btnBack = document.getElementById('mp-step-back');
            const btnFwd = document.getElementById('mp-step-fwd');
            const selSpeed = document.getElementById('mp-speed');
            const btnLoop = document.getElementById('mp-loop');
            const btnSnap = document.getElementById('mp-snap');

            if (btnBack) btnBack.onclick = () => { video.pause(); video.currentTime = Math.max(0, video.currentTime - 1 / 30); };
            if (btnFwd) btnFwd.onclick = () => { video.pause(); video.currentTime = Math.min(video.duration || 0, video.currentTime + 1 / 30); };
            if (selSpeed) selSpeed.onchange = (e) => { video.playbackRate = parseFloat(e.target.value) || 1.0; };
            if (btnLoop) btnLoop.onclick = () => {
                video.loop = !video.loop;
                btnLoop.classList.toggle('active', video.loop);
            };
            if (btnSnap) btnSnap.onclick = () => {
                if (!video.videoWidth) return;
                const c = document.createElement('canvas');
                c.width = video.videoWidth;
                c.height = video.videoHeight;
                c.getContext('2d').drawImage(video, 0, 0, c.width, c.height);
                const a = document.createElement('a');
                a.href = c.toDataURL('image/png');
                a.download = `${context.fileName}_frame_${Math.round(video.currentTime * 100) / 100}s.png`;
                a.click();
            };
        },

        mountAudio(container, context, hostApi, fileUrl, initVol, autoplay) {
            let vizMode = 'bars'; // 'bars' | 'wave'
            hostApi.setToolbar(`
                <button class="omni-btn active" id="mp-viz-bars">📊 频谱柱</button>
                <button class="omni-btn" id="mp-viz-wave">〰️ 示波波形</button>
                <select class="omni-select" id="mp-audio-speed" title="播放倍速">
                    <option value="0.75">0.75x</option>
                    <option value="1" selected>1.0x</option>
                    <option value="1.25">1.25x</option>
                    <option value="1.5">1.5x</option>
                    <option value="2">2.0x</option>
                </select>
                <button class="omni-btn active" id="mp-audio-loop">🔁 循环</button>
            `);

            const wrap = document.createElement('div');
            wrap.className = 'mp-audio-stage';
            wrap.innerHTML = `
                <div class="mp-audio-card">
                    <div class="mp-audio-header">
                        <div class="mp-audio-disc" id="mp-disc">♪</div>
                        <div class="mp-audio-meta">
                            <div class="mp-audio-title">${context.fileName}</div>
                            <div class="mp-audio-sub" id="mp-audio-sub">格式: ${context.ext.toUpperCase().replace('.', '')} · 大小: ${hostApi.formatBytes(context.fileSize)}</div>
                        </div>
                    </div>
                    <canvas id="mp-spectrum-canvas" width="640" height="200"></canvas>
                    <audio id="mp-audio-el" controls loop preload="metadata"></audio>
                </div>
            `;
            container.appendChild(wrap);

            const audio = wrap.querySelector('#mp-audio-el');
            const canvas = wrap.querySelector('#mp-spectrum-canvas');
            const disc = wrap.querySelector('#mp-disc');
            activeMediaEl = audio;
            audio.volume = initVol;
            audio.src = fileUrl;

            audio.addEventListener('volumechange', () => {
                localStorage.setItem('omnipeek_media_volume', String(audio.volume));
            });

            audio.addEventListener('play', () => { if (disc) disc.classList.add('spinning'); });
            audio.addEventListener('pause', () => { if (disc) disc.classList.remove('spinning'); });

            audio.addEventListener('loadedmetadata', () => {
                const sub = wrap.querySelector('#mp-audio-sub');
                if (sub) {
                    sub.textContent = `格式: ${context.ext.toUpperCase().replace('.', '')} · 时长: ${formatDuration(audio.duration)} · 大小: ${hostApi.formatBytes(context.fileSize)}`;
                }
                hostApi.setStatus(`音频时长: ${formatDuration(audio.duration)}`);
                if (autoplay) {
                    audio.play().catch(() => {});
                }
            });

            // Setup WebAudio Analyzer
            try {
                const AudioContextClass = window.AudioContext || window.webkitAudioContext;
                audioCtx = new AudioContextClass();
                const srcNode = audioCtx.createMediaElementSource(audio);
                const analyser = audioCtx.createAnalyser();
                analyser.fftSize = 256;
                srcNode.connect(analyser);
                analyser.connect(audioCtx.destination);

                const bufferLength = analyser.frequencyBinCount;
                const dataArray = new Uint8Array(bufferLength);
                const ctx2d = canvas.getContext('2d');

                const renderSpectrum = () => {
                    animFrameId = requestAnimationFrame(renderSpectrum);
                    const W = canvas.width;
                    const H = canvas.height;
                    ctx2d.clearRect(0, 0, W, H);

                    if (vizMode === 'bars') {
                        analyser.getByteFrequencyData(dataArray);
                        const bars = 64;
                        const barWidth = (W / bars) - 3;
                        for (let i = 0; i < bars; i++) {
                            const val = dataArray[i] || 4;
                            const barHeight = Math.max(4, (val / 255) * (H - 20));
                            const x = i * (barWidth + 3);
                            const y = H - barHeight;
                            const grad = ctx2d.createLinearGradient(0, y, 0, H);
                            grad.addColorStop(0, '#38bdf8');
                            grad.addColorStop(0.5, '#818cf8');
                            grad.addColorStop(1, '#c084fc');
                            ctx2d.fillStyle = grad;
                            ctx2d.fillRect(x, y, barWidth, barHeight);
                        }
                    } else {
                        analyser.getByteTimeDomainData(dataArray);
                        ctx2d.lineWidth = 2.5;
                        ctx2d.strokeStyle = '#38bdf8';
                        ctx2d.beginPath();
                        const sliceWidth = W / bufferLength;
                        let x = 0;
                        for (let i = 0; i < bufferLength; i++) {
                            const v = dataArray[i] / 128.0;
                            const y = (v * H) / 2;
                            if (i === 0) ctx2d.moveTo(x, y);
                            else ctx2d.lineTo(x, y);
                            x += sliceWidth;
                        }
                        ctx2d.lineTo(W, H / 2);
                        ctx2d.stroke();
                    }
                };
                renderSpectrum();
            } catch (e) {
                console.warn('WebAudio visualizer fallback:', e);
            }

            const btnBars = document.getElementById('mp-viz-bars');
            const btnWave = document.getElementById('mp-viz-wave');
            const selSpeed = document.getElementById('mp-audio-speed');
            const btnLoop = document.getElementById('mp-audio-loop');

            if (btnBars && btnWave) {
                btnBars.onclick = () => { vizMode = 'bars'; btnBars.classList.add('active'); btnWave.classList.remove('active'); };
                btnWave.onclick = () => { vizMode = 'wave'; btnWave.classList.add('active'); btnBars.classList.remove('active'); };
            }
            if (selSpeed) selSpeed.onchange = (e) => { audio.playbackRate = parseFloat(e.target.value) || 1.0; };
            if (btnLoop) btnLoop.onclick = () => {
                audio.loop = !audio.loop;
                btnLoop.classList.toggle('active', audio.loop);
            };
        },

        mountImage(container, context, hostApi, fileUrl) {
            let scale = 1.0;
            let checker = true;
            hostApi.setToolbar(`
                <button class="omni-btn" id="mp-img-zoom-out">➖ 缩小</button>
                <span class="omni-badge" id="mp-img-zoom-val">100%</span>
                <button class="omni-btn" id="mp-img-zoom-in">➕ 放大</button>
                <button class="omni-btn" id="mp-img-reset">适配</button>
                <button class="omni-btn active" id="mp-img-checker">🏁 透明棋盘</button>
            `);

            const wrap = document.createElement('div');
            wrap.className = 'mp-image-stage checker-bg';
            wrap.innerHTML = `<img id="mp-img-el" src="${fileUrl}" alt="${context.fileName}" />`;
            container.appendChild(wrap);

            const img = wrap.querySelector('#mp-img-el');
            const zoomVal = document.getElementById('mp-img-zoom-val');
            const updateZoom = (newScale) => {
                scale = Math.max(0.1, Math.min(8.0, newScale));
                img.style.transform = `scale(${scale})`;
                if (zoomVal) zoomVal.textContent = `${Math.round(scale * 100)}%`;
            };

            img.onload = () => {
                hostApi.setStatus(`尺寸: ${img.naturalWidth} × ${img.naturalHeight} px · ${hostApi.formatBytes(context.fileSize)}`);
            };

            wrap.addEventListener('wheel', (e) => {
                e.preventDefault();
                updateZoom(scale * (e.deltaY < 0 ? 1.15 : 0.87));
            }, { passive: false });

            const btnOut = document.getElementById('mp-img-zoom-out');
            const btnIn = document.getElementById('mp-img-zoom-in');
            const btnReset = document.getElementById('mp-img-reset');
            const btnCheck = document.getElementById('mp-img-checker');
            if (btnOut) btnOut.onclick = () => updateZoom(scale * 0.8);
            if (btnIn) btnIn.onclick = () => updateZoom(scale * 1.25);
            if (btnReset) btnReset.onclick = () => updateZoom(1.0);
            if (btnCheck) btnCheck.onclick = () => {
                checker = !checker;
                wrap.classList.toggle('checker-bg', checker);
                btnCheck.classList.toggle('active', checker);
            };
        },

        unmount() {
            stopActiveMedia();
        }
    };
})();
