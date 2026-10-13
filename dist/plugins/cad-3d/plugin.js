/**
 * ModelPeek 3D Previewer Engine
 * High-performance, lightweight WebGL viewer for CAD & 3D models.
 */

const I18N = {
    zh: {
        mode_shaded_edges: "渲染+边线",
        mode_shaded: "纯实体",
        mode_wireframe: "线框",
        view_iso: "等轴测",
        view_top: "顶视",
        view_front: "前视",
        view_right: "右视",
        fit: "适配",
        dimensions: "📐 标注",
        measure: "📏 测量",
        section: "✂️ 剖切",
        tree: "🌲 部件",
        theme_dark: "🌌 深色科技",
        theme_blueprint: "📐 工业蓝图",
        theme_studio: "💡 摄影白底",
        theme_grid: "🏁 透明棋盘",
        dims_label: "外形尺寸:",
        verts_label: "顶点数量:",
        faces_label: "三角面数:",
        measure_title: "📏 测距:",
        measure_prompt: "点击模型拾取两点",
        measure_clear: "清除",
        measure_close: "关闭",
        section_title: "✂️ 剖切:",
        section_invert: "反向",
        section_close: "关闭",
        tree_title: "🌲 零部件结构树",
        tree_all: "👁️ 显示全部",
        tree_close: "✕",
        tree_search_placeholder: "🔍 搜索零部件名称...",
        tree_status: "点击部件选中高亮，点眼睛切换可见性",
        loading: "正在解析模型...",
        waiting: "等待选择模型...",
        toast_dim_on: "已开启三维包围盒尺寸标注",
        toast_dim_off: "已关闭三维包围盒尺寸标注"
    },
    en: {
        mode_shaded_edges: "Shaded+Edges",
        mode_shaded: "Shaded",
        mode_wireframe: "Wireframe",
        view_iso: "Isometric",
        view_top: "Top",
        view_front: "Front",
        view_right: "Right",
        fit: "Fit",
        dimensions: "📐 BBox",
        measure: "📏 Measure",
        section: "✂️ Section",
        tree: "🌲 Parts",
        theme_dark: "🌌 Dark Tech",
        theme_blueprint: "📐 Blueprint",
        theme_studio: "💡 Studio White",
        theme_grid: "🏁 Transparent",
        dims_label: "Dimensions:",
        verts_label: "Vertices:",
        faces_label: "Triangles:",
        measure_title: "📏 Measure:",
        measure_prompt: "Click 2 points on model",
        measure_clear: "Clear",
        measure_close: "Close",
        section_title: "✂️ Section:",
        section_invert: "Invert",
        section_close: "Close",
        tree_title: "🌲 Assembly Tree",
        tree_all: "👁️ Show All",
        tree_close: "✕",
        tree_search_placeholder: "🔍 Search parts...",
        tree_status: "Click to select & highlight, eye to toggle visibility",
        loading: "Loading 3D model...",
        waiting: "Select a 3D model to view...",
        toast_dim_on: "3D Bounding box dimensions enabled",
        toast_dim_off: "3D Bounding box dimensions disabled"
    }
};

class ModelPeekViewer {
    constructor(containerId) {
        this.container = document.getElementById(containerId);
        this.scene = null;
        this.camera = null;
        this.renderer = null;
        this.controls = null;
        this.currentModel = null;
        this.edgeLines = null;
        this.gridHelper = null;
        this.axesHelper = null;
        this.renderMode = 'shaded_edges'; // 'shaded', 'shaded_edges', 'wireframe'
        this.modelBBox = null;
        this.modelCenter = new THREE.Vector3();
        this.modelRadius = 100;

        // Measurement & Section Tools
        this.isMeasuring = false;
        this.measurePoints = [];
        this.measureMarkers = [];
        this.measureLine = null;
        this.raycaster = new THREE.Raycaster();
        this.mouse = new THREE.Vector2();

        this.isSectioning = false;
        this.sectionAxis = 'x';
        this.sectionInvert = false;
        this.sectionSliderVal = 50;
        this.clipPlane = new THREE.Plane(new THREE.Vector3(1, 0, 0), 0);

        // Assembly Model Tree & Themes
        this.modelComponents = [];
        this.selectedComponentId = null;
        this.highlightHelper = null;
        this.treeSearchFilter = '';
        this.currentTheme = 'theme-dark';
        this.ambientLight = null;

        // Bounding Box 3D Dimensions & Localization
        this.showDimensions = false;
        this.dimensionGroup = null;
        this.lang = 'zh';

        this.init();
        this.initI18n();
        this.setupEventListeners();
        this.checkUrlParameters();
    }

    init() {
        const width = this.container.clientWidth || window.innerWidth;
        const height = this.container.clientHeight || window.innerHeight;

        // 1. Scene
        this.scene = new THREE.Scene();
        this.scene.background = null; // transparent to allow CSS radial gradient

        // 2. Camera
        this.camera = new THREE.PerspectiveCamera(45, width / height, 0.1, 100000000);
        this.camera.position.set(150, 150, 200);

        // 3. Renderer
        this.renderer = new THREE.WebGLRenderer({
            antialias: true,
            alpha: true,
            preserveDrawingBuffer: true,
            powerPreference: "high-performance",
            logarithmicDepthBuffer: true
        });
        this.renderer.setSize(width, height);
        this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
        this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
        this.renderer.toneMappingExposure = 1.1;
        this.renderer.localClippingEnabled = true;
        this.container.appendChild(this.renderer.domElement);

        // 4. OrbitControls
        this.controls = new THREE.OrbitControls(this.camera, this.renderer.domElement);
        this.controls.enableDamping = true;
        this.controls.dampingFactor = 0.08;
        this.controls.screenSpacePanning = true;
        this.controls.zoomSpeed = 1.2;
        this.controls.maxDistance = 1000000000;
        this.controls.minDistance = 0.0001;

        // 5. Lighting Setup (Professional CAD Studio Lights)
        const ambientLight = new THREE.AmbientLight(0xffffff, 0.65);
        this.ambientLight = ambientLight;
        this.scene.add(ambientLight);

        const keyLight = new THREE.DirectionalLight(0xffffff, 0.75);
        keyLight.position.set(1, 1.5, 1).normalize();
        this.scene.add(keyLight);

        const fillLight = new THREE.DirectionalLight(0xd8e2ec, 0.45);
        fillLight.position.set(-1, 0.8, -1).normalize();
        this.scene.add(fillLight);

        const backLight = new THREE.DirectionalLight(0x88c0d0, 0.35);
        backLight.position.set(0, -1, 0).normalize();
        this.scene.add(backLight);

        // 6. Helpers
        this.createEnvironmentHelpers(200);

        // 7. Animation Loop
        const animate = () => {
            requestAnimationFrame(animate);
            this.controls.update();
            this.renderer.render(this.scene, this.camera);
        };
        animate();

        window.addEventListener('resize', () => this.onWindowResize());
    }

    createEnvironmentHelpers(size) {
        if (this.gridHelper) this.scene.remove(this.gridHelper);
        if (this.axesHelper) this.scene.remove(this.axesHelper);

        // Subtle CAD Ground Grid
        this.gridHelper = new THREE.GridHelper(size * 2, 20, 0x4c566a, 0x3b4252);
        this.gridHelper.position.y = 0;
        this.scene.add(this.gridHelper);

        // RGB Coordinate axes helper
        this.axesHelper = new THREE.AxesHelper(size * 0.4);
        this.axesHelper.renderOrder = 1;
        this.scene.add(this.axesHelper);
    }

    onWindowResize() {
        const width = this.container.clientWidth || window.innerWidth;
        const height = this.container.clientHeight || window.innerHeight;
        this.camera.aspect = width / height;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(width, height);
    }

    // Load 3D model from URL / local file path
    loadModel(filePath, origDisplayPath) {
        this._origDisplayName = origDisplayPath ? origDisplayPath.split(/[\/\\]/).pop() : null;
        let fileUrl = filePath;
        if (!fileUrl.startsWith('file://') && !fileUrl.startsWith('http://') && !fileUrl.startsWith('https://')) {
            const normalized = fileUrl.replace(/\\/g, '/');
            const encoded = normalized.split('/').map(seg => /^[a-zA-Z]:$/.test(seg) ? seg : encodeURIComponent(seg)).join('/');
            fileUrl = encoded.startsWith('/') ? 'file://' + encoded : 'file:///' + encoded;
        }

        const fileName = filePath.split(/[\/\\]/).pop();
        this.showLoader(`加载中: ${fileName}...`);
        const ext = fileName.split('.').pop().toLowerCase();

        const onProgress = (xhr) => {
            if (xhr.lengthComputable) {
                const percent = Math.round((xhr.loaded / xhr.total) * 100);
                this.updateLoaderText(`加载中: ${percent}%`);
            }
        };

        const onError = (error) => {
            console.error("加载模型失败:", error);
            this.hideLoader();
            this.showToast("模型加载失败，请检查文件格式或损坏状态");
        };

        try {
            switch (ext) {
                case 'stl':
                    new THREE.STLLoader().load(fileUrl, (geometry) => {
                        const material = this.createDefaultMaterial();
                        const mesh = new THREE.Mesh(geometry, material);
                        this.setModel(mesh, filePath);
                    }, onProgress, onError);
                    break;

                case 'obj':
                    new THREE.OBJLoader().load(fileUrl, (object) => {
                        this.setModel(object, filePath);
                    }, onProgress, onError);
                    break;

                case 'glb':
                case 'gltf':
                    new THREE.GLTFLoader().load(fileUrl, (gltf) => {
                        this.setModel(gltf.scene, filePath);
                    }, onProgress, onError);
                    break;

                case '3mf':
                    new THREE.ThreeMFLoader().load(fileUrl, (object) => {
                        this.setModel(object, filePath);
                    }, onProgress, onError);
                    break;

                case 'fbx':
                    new THREE.FBXLoader().load(fileUrl, (object) => {
                        this.setModel(object, filePath);
                    }, onProgress, onError);
                    break;

                case 'ply':
                    new THREE.PLYLoader().load(fileUrl, (geometry) => {
                        let material;
                        if (geometry.hasAttribute('color')) {
                            material = new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.45, metalness: 0.2, side: THREE.DoubleSide });
                        } else {
                            material = this.createDefaultMaterial();
                        }
                        const mesh = new THREE.Mesh(geometry, material);
                        this.setModel(mesh, filePath);
                    }, onProgress, onError);
                    break;

                case 'gcode':
                    new THREE.GCodeLoader().load(fileUrl, (object) => {
                        this.setModel(object, filePath);
                    }, onProgress, onError);
                    break;

                case 'dae':
                    new THREE.ColladaLoader().load(fileUrl, (collada) => {
                        this.setModel(collada.scene, filePath);
                    }, onProgress, onError);
                    break;

                case '3ds':
                    new THREE.TDSLoader().load(fileUrl, (object) => {
                        this.setModel(object, filePath);
                    }, onProgress, onError);
                    break;

                case 'dxf':
                    new THREE.DXFLoader().load(fileUrl, (object) => {
                        this.setModel(object, filePath);
                    }, onProgress, onError);
                    break;

                case 'pcd':
                    new THREE.PCDLoader().load(fileUrl, (points) => {
                        if (points.material) {
                            points.material.size = Math.max(1, (this.modelRadius || 100) * 0.005);
                        }
                        this.setModel(points, filePath);
                    }, onProgress, onError);
                    break;

                default:
                    this.hideLoader();
                    this.showToast(`暂不支持直接在视口解析 .${ext}，尝试由 Worker 转换后预览`);
                    break;
            }
        } catch (err) {
            onError(err);
        }
    }

    // Set model into scene and calculate stats
    setModel(object, filePath) {
        if (this.currentModel) {
            this.scene.remove(this.currentModel);
            this.currentModel = null;
        }

        this.currentModel = object;
        this.scene.add(this.currentModel);

        // Normalize materials & gather statistics
        let vertexCount = 0;
        let faceCount = 0;

        this.currentModel.traverse((child) => {
            if (child.isMesh) {
                if (!child.material || child.material.type === 'MeshBasicMaterial') {
                    child.material = this.createDefaultMaterial();
                } else {
                    child.material.side = THREE.DoubleSide;
                }

                if (child.geometry) {
                    if (child.geometry.attributes && child.geometry.attributes.position) {
                        vertexCount += child.geometry.attributes.position.count;
                    }
                    if (child.geometry.index) {
                        faceCount += child.geometry.index.count / 3;
                    } else if (child.geometry.attributes && child.geometry.attributes.position) {
                        faceCount += child.geometry.attributes.position.count / 3;
                    }
                }
            } else if (child.isLine || child.isLineSegments) {
                if (child.material) {
                    if (child.material.color && (child.material.color.getHex() === 0x3300ff || child.material.color.getHex() === 0x000000)) {
                        child.material.color.setHex(0x00d2ff);
                    }
                    if (child.material.linewidth !== undefined) {
                        child.material.linewidth = 2;
                    }
                } else {
                    child.material = new THREE.LineBasicMaterial({ color: 0x00d2ff, linewidth: 2 });
                }
                if (child.geometry && child.geometry.attributes && child.geometry.attributes.position) {
                    vertexCount += child.geometry.attributes.position.count;
                }
            } else if (child.isPoints) {
                if (child.geometry && child.geometry.attributes && child.geometry.attributes.position) {
                    vertexCount += child.geometry.attributes.position.count;
                }
            }
        });

        // Compute Bounding Box
        const bbox = new THREE.Box3().setFromObject(this.currentModel);
        this.modelBBox = bbox;
        const size = new THREE.Vector3();
        bbox.getSize(size);
        bbox.getCenter(this.modelCenter);
        this.modelRadius = Math.max(size.x, size.y, size.z) || 100;

        // Position ground grid just below the model
        this.createEnvironmentHelpers(this.modelRadius);
        if (this.gridHelper) {
            this.gridHelper.position.y = bbox.min.y - (size.y * 0.01);
        }

        // Dynamically configure camera clipping and OrbitControls distance based on model radius
        const r = Math.max(this.modelRadius, 0.01);
        this.controls.minDistance = Math.max(r * 0.0005, 0.001);
        this.controls.maxDistance = Math.max(r * 100, 100000000);
        this.camera.near = Math.max(r * 0.0005, 0.01);
        this.camera.far = Math.max(r * 200, 200000000);
        this.camera.updateProjectionMatrix();

        // Generate Edge Lines for CAD look
        this.generateEdgeLines();

        // Collect model components for Assembly Tree
        this.modelComponents = [];
        this.selectedComponentId = null;
        this.clearHighlight();
        let partIndex = 1;
        this.currentModel.traverse((child) => {
            if (child.isMesh || child.isLine || child.isLineSegments || child.isPoints) {
                let pFaces = 0;
                let pVerts = 0;
                if (child.geometry) {
                    if (child.geometry.attributes && child.geometry.attributes.position) {
                        pVerts = child.geometry.attributes.position.count;
                    }
                    if (child.geometry.index) {
                        pFaces = child.geometry.index.count / 3;
                    } else if (child.geometry.attributes && child.geometry.attributes.position && child.isMesh) {
                        pFaces = child.geometry.attributes.position.count / 3;
                    }
                }
                const rawName = (child.name && child.name.trim() !== '') ? child.name : (child.isPoints ? `点云 #${partIndex++}` : `部件 #${partIndex++}`);
                const comp = {
                    id: this.modelComponents.length,
                    name: rawName,
                    object: child,
                    faceCount: Math.round(pFaces),
                    vertexCount: Math.round(pVerts),
                    type: child.isMesh ? 'mesh' : (child.isPoints ? 'points' : 'line'),
                    visible: true
                };
                child.userData.componentId = comp.id;
                this.modelComponents.push(comp);
            }
        });
        this.buildModelTreeUI();

        // Fit Camera View
        this.fitView();

        // Bounding Box Dimensions
        if (this.showDimensions) {
            this.buildDimensionAnnotations();
        }

        // Update UI Panel
        const fileName = filePath ? filePath.split(/[\/\\]/).pop() : "3D Model";
        this.updateInfoPanel({
            name: fileName,
            sizeX: size.x.toFixed(2),
            sizeY: size.y.toFixed(2),
            sizeZ: size.z.toFixed(2),
            vertices: Math.round(vertexCount).toLocaleString(),
            faces: Math.round(faceCount).toLocaleString()
        });

        this.hideLoader();
    }

    createDefaultMaterial() {
        return new THREE.MeshStandardMaterial({
            color: 0x90a4ae,
            metalness: 0.25,
            roughness: 0.45,
            side: THREE.DoubleSide
        });
    }

    generateEdgeLines() {
        if (this.edgeLines) {
            this.scene.remove(this.edgeLines);
            this.edgeLines = null;
        }

        const edgeGroup = new THREE.Group();
        this.currentModel.traverse((child) => {
            if (child.isMesh && child.geometry) {
                const edgesGeometry = new THREE.EdgesGeometry(child.geometry, 28);
                const line = new THREE.LineSegments(
                    edgesGeometry,
                    new THREE.LineBasicMaterial({ color: 0x2e3440, linewidth: 1 })
                );
                line.matrix = child.matrixWorld;
                line.matrixAutoUpdate = false;
                line.userData.sourceObject = child;
                edgeGroup.add(line);
            }
        });

        this.edgeLines = edgeGroup;
        if (this.renderMode === 'shaded_edges') {
            this.scene.add(this.edgeLines);
        }
    }

    // Set Render Mode: 'shaded', 'shaded_edges', 'wireframe'
    setRenderMode(mode) {
        this.renderMode = mode;
        if (!this.currentModel) return;

        if (this.edgeLines) {
            if (mode === 'shaded_edges') {
                this.scene.add(this.edgeLines);
            } else {
                this.scene.remove(this.edgeLines);
            }
        }

        this.currentModel.traverse((child) => {
            if (child.isMesh && child.material) {
                child.material.wireframe = (mode === 'wireframe');
            }
        });
    }

    // Preset Views
    setView(viewType) {
        if (!this.modelBBox) return;

        const center = this.modelCenter;
        const dist = (this.modelRadius || 100) * 1.8;

        switch (viewType) {
            case 'iso':
                this.camera.position.set(center.x + dist * 0.7, center.y + dist * 0.6, center.z + dist * 0.7);
                break;
            case 'top':
                this.camera.position.set(center.x, center.y + dist * 1.5, center.z + 0.0001);
                break;
            case 'front':
                this.camera.position.set(center.x, center.y, center.z + dist * 1.5);
                break;
            case 'right':
                this.camera.position.set(center.x + dist * 1.5, center.y, center.z);
                break;
        }

        this.controls.target.copy(center);
        this.controls.update();
    }

    fitView() {
        if (!this.modelBBox) return;

        const center = this.modelCenter;
        const maxDim = this.modelRadius || 100;

        // Ensure distance limits accommodate this model scale dynamically
        this.controls.minDistance = Math.max(maxDim * 0.0005, 0.001);
        this.controls.maxDistance = Math.max(maxDim * 100, 100000000);
        this.camera.near = Math.max(maxDim * 0.0005, 0.01);
        this.camera.far = Math.max(maxDim * 200, 200000000);
        this.camera.updateProjectionMatrix();

        const fov = this.camera.fov * (Math.PI / 180);
        let cameraZ = Math.abs(maxDim / 2 / Math.tan(fov / 2)) * 1.8;

        this.camera.position.set(center.x + cameraZ * 0.7, center.y + cameraZ * 0.6, center.z + cameraZ * 0.7);
        this.camera.lookAt(center);
        this.controls.target.copy(center);
        this.controls.update();
    }

    setupEventListeners() {
        // Render mode buttons
        document.querySelectorAll('[data-render-mode]').forEach(btn => {
            btn.addEventListener('click', (e) => {
                document.querySelectorAll('[data-render-mode]').forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                this.setRenderMode(btn.dataset.renderMode);
            });
        });

        // View preset buttons
        document.querySelectorAll('[data-view]').forEach(btn => {
            btn.addEventListener('click', () => {
                this.setView(btn.dataset.view);
            });
        });

        // Fit view button
        const fitBtn = document.getElementById('btn-fit');
        if (fitBtn) {
            fitBtn.addEventListener('click', () => this.fitView());
        }

        // Bounding Box Dimensions button
        const dimBtn = document.getElementById('btn-dimensions');
        if (dimBtn) {
            dimBtn.addEventListener('click', () => {
                this.showDimensions = !this.showDimensions;
                dimBtn.classList.toggle('active', this.showDimensions);
                this.buildDimensionAnnotations();
                const t = I18N[this.lang || 'zh'];
                this.showToast(this.showDimensions ? t.toast_dim_on : t.toast_dim_off);
            });
        }

        // Measurement button
        const measureBtn = document.getElementById('btn-measure');
        if (measureBtn) {
            measureBtn.addEventListener('click', () => this.toggleMeasure());
        }
        const measureClearBtn = document.getElementById('btn-measure-clear');
        if (measureClearBtn) {
            measureClearBtn.addEventListener('click', () => this.clearMeasure());
        }
        const measureCloseBtn = document.getElementById('btn-measure-close');
        if (measureCloseBtn) {
            measureCloseBtn.addEventListener('click', () => this.toggleMeasure(false));
        }

        // Section button & controls
        const sectionBtn = document.getElementById('btn-section');
        if (sectionBtn) {
            sectionBtn.addEventListener('click', () => this.toggleSection());
        }
        document.querySelectorAll('#section-bar [data-axis]').forEach(b => {
            b.addEventListener('click', () => this.setSectionAxis(b.dataset.axis));
        });
        const secSlider = document.getElementById('sec-slider');
        if (secSlider) {
            secSlider.addEventListener('input', (e) => {
                this.sectionSliderVal = parseFloat(e.target.value);
                this.updateSectionPlane();
            });
        }
        const secInvert = document.getElementById('sec-invert');
        if (secInvert) {
            secInvert.addEventListener('click', () => {
                this.sectionInvert = !this.sectionInvert;
                secInvert.classList.toggle('active', this.sectionInvert);
                this.updateSectionPlane();
            });
        }
        const secCloseBtn = document.getElementById('btn-section-close');
        if (secCloseBtn) {
            secCloseBtn.addEventListener('click', () => this.toggleSection(false));
        }

        // Model Tree Drawer toggle & actions
        const treeBtn = document.getElementById('btn-tree');
        const treeDrawer = document.getElementById('tree-drawer');
        if (treeBtn && treeDrawer) {
            treeBtn.addEventListener('click', () => {
                treeDrawer.classList.toggle('hidden');
                treeBtn.classList.toggle('active', !treeDrawer.classList.contains('hidden'));
            });
        }
        const treeCloseBtn = document.getElementById('btn-tree-close');
        if (treeCloseBtn && treeDrawer) {
            treeCloseBtn.addEventListener('click', () => {
                treeDrawer.classList.add('hidden');
                if (treeBtn) treeBtn.classList.remove('active');
            });
        }
        const treeResetBtn = document.getElementById('btn-tree-isolate-reset');
        if (treeResetBtn) {
            treeResetBtn.addEventListener('click', () => this.isolateReset());
        }
        const treeSearch = document.getElementById('tree-search-input');
        if (treeSearch) {
            treeSearch.addEventListener('input', (e) => {
                this.treeSearchFilter = e.target.value.trim();
                this.buildModelTreeUI();
            });
        }

        // Theme selector
        const themeSelector = document.getElementById('theme-selector');
        if (themeSelector) {
            themeSelector.addEventListener('change', (e) => {
                this.setTheme(e.target.value);
            });
        }

        // Canvas click for measuring
        this.renderer.domElement.addEventListener('pointerdown', (e) => this.onCanvasClick(e));

        // Drag & Drop local file support
        window.addEventListener('dragover', (e) => e.preventDefault());
        window.addEventListener('drop', (e) => {
            e.preventDefault();
            if (e.dataTransfer.files.length > 0) {
                const file = e.dataTransfer.files[0];
                const url = URL.createObjectURL(file);
                this.loadModel(url, file.name);
            }
        });

        // WebView2 host message listener
        if (window.chrome && window.chrome.webview) {
            window.chrome.webview.addEventListener('message', (event) => {
                const data = event.data;
                if (typeof data === 'string') {
                    this.loadModel(data);
                } else if (data && data.action === 'load') {
                    this.loadModel(data.path);
                }
            });
        }
    }

    toggleMeasure(enable) {
        this.isMeasuring = (enable !== undefined) ? enable : !this.isMeasuring;
        const bar = document.getElementById('measure-bar');
        const btn = document.getElementById('btn-measure');
        if (this.isMeasuring) {
            bar.classList.add('active');
            btn.classList.add('active');
            this.clearMeasure();
        } else {
            bar.classList.remove('active');
            btn.classList.remove('active');
            this.clearMeasure();
        }
    }

    clearMeasure() {
        this.measurePoints = [];
        this.measureMarkers.forEach(m => this.scene.remove(m));
        this.measureMarkers = [];
        if (this.measureLine) {
            this.scene.remove(this.measureLine);
            this.measureLine = null;
        }
        const res = document.getElementById('measure-result');
        if (res) res.innerText = "点击模型拾取两点";
    }

    onCanvasClick(event) {
        if (!this.isMeasuring || !this.currentModel) return;

        const rect = this.renderer.domElement.getBoundingClientRect();
        this.mouse.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
        this.mouse.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;

        this.raycaster.setFromCamera(this.mouse, this.camera);
        const intersects = this.raycaster.intersectObject(this.currentModel, true);

        if (intersects.length > 0) {
            const pt = intersects[0].point;
            this.addMeasurePoint(pt);
        }
    }

    addMeasurePoint(pt) {
        if (this.measurePoints.length >= 2) {
            this.clearMeasure();
        }

        this.measurePoints.push(pt);

        const markerGeom = new THREE.SphereGeometry(this.modelRadius * 0.015, 16, 16);
        const markerMat = new THREE.MeshBasicMaterial({ color: 0xebcb8b });
        const marker = new THREE.Mesh(markerGeom, markerMat);
        marker.position.copy(pt);
        this.scene.add(marker);
        this.measureMarkers.push(marker);

        if (this.measurePoints.length === 2) {
            const p1 = this.measurePoints[0];
            const p2 = this.measurePoints[1];
            const dist = p1.distanceTo(p2);
            const dx = Math.abs(p2.x - p1.x);
            const dy = Math.abs(p2.y - p1.y);
            const dz = Math.abs(p2.z - p1.z);

            const lineGeom = new THREE.BufferGeometry().setFromPoints([p1, p2]);
            const lineMat = new THREE.LineBasicMaterial({ color: 0xa3be8c, linewidth: 2 });
            this.measureLine = new THREE.Line(lineGeom, lineMat);
            this.scene.add(this.measureLine);

            const res = document.getElementById('measure-result');
            if (res) {
                res.innerText = `${dist.toFixed(2)} mm (ΔX: ${dx.toFixed(2)}, ΔY: ${dy.toFixed(2)}, ΔZ: ${dz.toFixed(2)})`;
            }
        } else {
            const res = document.getElementById('measure-result');
            if (res) res.innerText = "已拾取第 1 点，请点击第 2 点...";
        }
    }

    toggleSection(enable) {
        this.isSectioning = (enable !== undefined) ? enable : !this.isSectioning;
        const bar = document.getElementById('section-bar');
        const btn = document.getElementById('btn-section');
        if (this.isSectioning) {
            bar.classList.add('active');
            btn.classList.add('active');
            this.updateSectionPlane();
        } else {
            bar.classList.remove('active');
            btn.classList.remove('active');
            this.disableSectionPlane();
        }
    }

    setSectionAxis(axis) {
        this.sectionAxis = axis;
        document.querySelectorAll('#section-bar [data-axis]').forEach(b => b.classList.remove('active'));
        const activeBtn = document.querySelector(`#section-bar [data-axis="${axis}"]`);
        if (activeBtn) activeBtn.classList.add('active');
        this.updateSectionPlane();
    }

    updateSectionPlane() {
        if (!this.modelBBox || !this.isSectioning) return;

        let normal = new THREE.Vector3();
        let minVal = 0, maxVal = 0;
        if (this.sectionAxis === 'x') {
            normal.set(this.sectionInvert ? -1 : 1, 0, 0);
            minVal = this.modelBBox.min.x;
            maxVal = this.modelBBox.max.x;
        } else if (this.sectionAxis === 'y') {
            normal.set(0, this.sectionInvert ? -1 : 1, 0);
            minVal = this.modelBBox.min.y;
            maxVal = this.modelBBox.max.y;
        } else if (this.sectionAxis === 'z') {
            normal.set(0, 0, this.sectionInvert ? -1 : 1);
            minVal = this.modelBBox.min.z;
            maxVal = this.modelBBox.max.z;
        }

        const t = this.sectionSliderVal / 100.0;
        const currentCoord = minVal + (maxVal - minVal) * t;

        const constant = -(normal.x * (this.sectionAxis === 'x' ? currentCoord : 0) +
                           normal.y * (this.sectionAxis === 'y' ? currentCoord : 0) +
                           normal.z * (this.sectionAxis === 'z' ? currentCoord : 0));

        this.clipPlane.normal.copy(normal);
        this.clipPlane.constant = constant;

        this.applyClippingPlanes([this.clipPlane]);
    }

    disableSectionPlane() {
        this.applyClippingPlanes([]);
    }

    applyClippingPlanes(planes) {
        if (!this.currentModel) return;
        this.currentModel.traverse((child) => {
            if (child.isMesh && child.material) {
                child.material.clippingPlanes = planes;
                child.material.clipShadows = true;
                child.material.needsUpdate = true;
            }
        });
    }

    checkUrlParameters() {
        const params = new URLSearchParams(window.location.search);
        const file = params.get('file');
        const orig = params.get('orig');
        if (file) {
            this.loadModel(file, orig);
        } else {
            // Display empty welcome guide
            this.hideLoader();
            this.updateInfoPanel({
                name: "ModelPeek 就绪",
                sizeX: "--",
                sizeY: "--",
                sizeZ: "--",
                vertices: "--",
                faces: "--"
            });
        }
    }

    showLoader(text) {
        const overlay = document.getElementById('loader-overlay');
        const label = document.getElementById('loader-text');
        if (overlay) overlay.style.display = 'flex';
        if (label) label.innerText = text;
    }

    updateLoaderText(text) {
        const label = document.getElementById('loader-text');
        if (label) label.innerText = text;
    }

    hideLoader() {
        const overlay = document.getElementById('loader-overlay');
        if (overlay) overlay.style.display = 'none';
    }

    showToast(msg) {
        const toast = document.getElementById('toast');
        if (toast) {
            toast.innerText = msg;
            toast.style.display = 'block';
            setTimeout(() => { toast.style.display = 'none'; }, 3500);
        }
    }

    buildModelTreeUI() {
        const countSpan = document.getElementById('tree-count');
        if (countSpan) countSpan.textContent = this.modelComponents.length;

        const listEl = document.getElementById('tree-list');
        if (!listEl) return;
        listEl.innerHTML = '';

        if (this.modelComponents.length === 0) {
            listEl.innerHTML = '<div style="padding:10px;text-align:center;color:#6c7a89;font-size:11px;">无独立子部件</div>';
            return;
        }

        const filterText = (this.treeSearchFilter || '').toLowerCase();

        this.modelComponents.forEach((comp) => {
            if (filterText && !comp.name.toLowerCase().includes(filterText)) {
                return;
            }

            const item = document.createElement('div');
            item.className = 'tree-item' + (this.selectedComponentId === comp.id ? ' active' : '') + (!comp.visible ? ' hidden-part' : '');
            item.dataset.id = comp.id;

            const eye = document.createElement('span');
            eye.className = 'tree-eye';
            eye.textContent = comp.visible ? '👁️' : '🚫';
            eye.title = comp.visible ? '隐藏该部件' : '显示该部件';
            eye.addEventListener('click', (e) => {
                e.stopPropagation();
                this.toggleComponentVisibility(comp.id);
            });

            const name = document.createElement('span');
            name.className = 'tree-item-name';
            name.textContent = comp.name;
            name.title = comp.name;

            const badge = document.createElement('span');
            badge.className = 'tree-badge';
            badge.textContent = comp.faceCount > 0 ? `${comp.faceCount}面` : `${comp.vertexCount}点`;

            item.appendChild(eye);
            item.appendChild(name);
            item.appendChild(badge);

            item.addEventListener('click', () => {
                this.selectComponent(comp.id);
            });

            listEl.appendChild(item);
        });
    }

    toggleComponentVisibility(id) {
        const comp = this.modelComponents.find(c => c.id === id);
        if (!comp) return;

        comp.visible = !comp.visible;
        comp.object.visible = comp.visible;

        if (this.edgeLines) {
            this.edgeLines.traverse((edgeChild) => {
                if (edgeChild.userData && edgeChild.userData.sourceObject === comp.object) {
                    edgeChild.visible = comp.visible;
                }
            });
        }

        this.buildModelTreeUI();
    }

    selectComponent(id) {
        if (this.selectedComponentId === id) {
            this.selectedComponentId = null;
            this.clearHighlight();
            const statusEl = document.getElementById('tree-status');
            if (statusEl) statusEl.innerHTML = '<span>点击部件选中高亮，点眼睛切换可见性</span>';
            this.buildModelTreeUI();
            return;
        }

        this.selectedComponentId = id;
        const comp = this.modelComponents.find(c => c.id === id);
        if (!comp) return;

        this.highlightComponent(comp.object);

        const statusEl = document.getElementById('tree-status');
        if (statusEl) {
            const compBBox = new THREE.Box3().setFromObject(comp.object);
            const compSize = new THREE.Vector3();
            compBBox.getSize(compSize);
            statusEl.innerHTML = `<span>选中: <b>${comp.name}</b> (${compSize.x.toFixed(1)}×${compSize.y.toFixed(1)}×${compSize.z.toFixed(1)}mm)</span>`;
        }

        this.buildModelTreeUI();
    }

    highlightComponent(object) {
        this.clearHighlight();
        if (!object) return;

        const box = new THREE.BoxHelper(object, 0x00e5ff);
        box.material.depthTest = false;
        box.material.transparent = true;
        box.material.opacity = 0.85;
        this.scene.add(box);
        this.highlightHelper = box;
    }

    clearHighlight() {
        if (this.highlightHelper) {
            this.scene.remove(this.highlightHelper);
            if (this.highlightHelper.geometry) this.highlightHelper.geometry.dispose();
            this.highlightHelper = null;
        }
    }

    isolateReset() {
        this.modelComponents.forEach(comp => {
            comp.visible = true;
            comp.object.visible = true;
        });
        if (this.edgeLines) {
            this.edgeLines.traverse(e => e.visible = true);
        }
        this.selectedComponentId = null;
        this.clearHighlight();
        const statusEl = document.getElementById('tree-status');
        if (statusEl) statusEl.innerHTML = '<span>已恢复所有零部件显示</span>';
        this.buildModelTreeUI();
    }

    setTheme(themeName) {
        this.currentTheme = themeName;
        document.body.className = themeName;
        if (themeName === 'theme-studio') {
            if (this.gridHelper && this.gridHelper.material) this.gridHelper.material.color.setHex(0xaaaaaa);
            if (this.ambientLight) this.ambientLight.intensity = 0.9;
        } else if (themeName === 'theme-blueprint') {
            if (this.gridHelper && this.gridHelper.material) this.gridHelper.material.color.setHex(0x3a608d);
            if (this.ambientLight) this.ambientLight.intensity = 0.65;
        } else {
            if (this.gridHelper && this.gridHelper.material) this.gridHelper.material.color.setHex(0x444b58);
            if (this.ambientLight) this.ambientLight.intensity = 0.65;
        }
    }

    updateInfoPanel(stats) {
        document.getElementById('stat-name').innerText = stats.name;
        document.getElementById('stat-dims').innerText = `${stats.sizeX} × ${stats.sizeY} × ${stats.sizeZ} mm`;
        document.getElementById('stat-vertices').innerText = stats.vertices;
        document.getElementById('stat-faces').innerText = stats.faces;
    }

    buildDimensionAnnotations() {
        if (this.dimensionGroup) {
            this.scene.remove(this.dimensionGroup);
            this.dimensionGroup.traverse(child => {
                if (child.geometry) child.geometry.dispose();
                if (child.material) {
                    if (child.material.map) child.material.map.dispose();
                    child.material.dispose();
                }
            });
            this.dimensionGroup = null;
        }

        if (!this.showDimensions || !this.currentModel || !this.modelBBox) {
            return;
        }

        const box = this.modelBBox;
        const min = box.min;
        const max = box.max;
        const size = new THREE.Vector3();
        box.getSize(size);
        if (size.x === 0 && size.y === 0 && size.z === 0) return;

        const group = new THREE.Group();
        const maxDim = Math.max(size.x, size.y, size.z);
        const offset = Math.max(maxDim * 0.08, 2);
        const tickLen = Math.max(maxDim * 0.03, 1);

        // 1. Subtle bounding box wireframe
        const boxGeom = new THREE.BoxGeometry(size.x, size.y, size.z);
        const boxCenter = new THREE.Vector3();
        box.getCenter(boxCenter);
        const wireMat = new THREE.MeshBasicMaterial({
            color: 0x00e5ff,
            wireframe: true,
            transparent: true,
            opacity: 0.35,
            depthTest: false
        });
        const wireMesh = new THREE.Mesh(boxGeom, wireMat);
        wireMesh.position.copy(boxCenter);
        group.add(wireMesh);

        // 2. Dimension lines & extension ticks (X, Y, Z)
        const lineMat = new THREE.LineBasicMaterial({
            color: 0x00e5ff,
            linewidth: 2,
            depthTest: false,
            transparent: true,
            opacity: 0.95
        });

        const linePositions = [];

        // --- X Dimension (Front Bottom) ---
        linePositions.push(
            min.x, min.y, max.z + offset,
            max.x, min.y, max.z + offset
        );
        linePositions.push(
            min.x, min.y, max.z,
            min.x, min.y, max.z + offset + tickLen,
            max.x, min.y, max.z,
            max.x, min.y, max.z + offset + tickLen
        );

        // --- Y Dimension (Front Left) ---
        linePositions.push(
            min.x - offset, min.y, max.z,
            min.x - offset, max.y, max.z
        );
        linePositions.push(
            min.x, min.y, max.z,
            min.x - offset - tickLen, min.y, max.z,
            min.x, max.y, max.z,
            min.x - offset - tickLen, max.y, max.z
        );

        // --- Z Dimension (Bottom Left) ---
        linePositions.push(
            min.x - offset, min.y, min.z,
            min.x - offset, min.y, max.z
        );
        linePositions.push(
            min.x, min.y, min.z,
            min.x - offset - tickLen, min.y, min.z,
            min.x, min.y, max.z,
            min.x - offset - tickLen, min.y, max.z
        );

        const lineGeom = new THREE.BufferGeometry();
        lineGeom.setAttribute('position', new THREE.Float32BufferAttribute(linePositions, 3));
        const dimLines = new THREE.LineSegments(lineGeom, lineMat);
        dimLines.renderOrder = 998;
        group.add(dimLines);

        // 3. Text Sprites for X, Y, Z
        const spriteScale = maxDim * 0.22;

        const labelX = this.createLabelSprite(`X: ${size.x.toFixed(1)} mm`, spriteScale);
        labelX.position.set(boxCenter.x, min.y, max.z + offset + tickLen * 1.5);
        group.add(labelX);

        const labelY = this.createLabelSprite(`Y: ${size.y.toFixed(1)} mm`, spriteScale);
        labelY.position.set(min.x - offset - tickLen * 1.5, boxCenter.y, max.z);
        group.add(labelY);

        const labelZ = this.createLabelSprite(`Z: ${size.z.toFixed(1)} mm`, spriteScale);
        labelZ.position.set(min.x - offset - tickLen * 1.5, min.y, boxCenter.z);
        group.add(labelZ);

        this.dimensionGroup = group;
        this.scene.add(this.dimensionGroup);
    }

    createLabelSprite(text, scale) {
        const canvas = document.createElement('canvas');
        canvas.width = 512;
        canvas.height = 128;
        const ctx = canvas.getContext('2d');

        // Draw pill / rounded rect capsule
        ctx.fillStyle = 'rgba(15, 23, 42, 0.88)';
        ctx.strokeStyle = '#00e5ff';
        ctx.lineWidth = 6;

        const x = 8, y = 8, w = 496, h = 112, r = 24;
        ctx.beginPath();
        ctx.moveTo(x + r, y);
        ctx.arcTo(x + w, y, x + w, y + h, r);
        ctx.arcTo(x + w, y + h, x, y + h, r);
        ctx.arcTo(x, y + h, x, y, r);
        ctx.arcTo(x, y, x + w, y, r);
        ctx.closePath();
        ctx.fill();
        ctx.stroke();

        // Draw Text
        ctx.fillStyle = '#ffffff';
        ctx.font = 'bold 44px -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(text, 256, 64);

        const texture = new THREE.CanvasTexture(canvas);
        texture.minFilter = THREE.LinearFilter;
        texture.wrapS = THREE.ClampToEdgeWrapping;
        texture.wrapT = THREE.ClampToEdgeWrapping;

        const mat = new THREE.SpriteMaterial({
            map: texture,
            depthTest: false,
            transparent: true
        });

        const sprite = new THREE.Sprite(mat);
        sprite.renderOrder = 999;
        sprite.scale.set(scale, scale * (128 / 512), 1);
        return sprite;
    }

    initI18n() {
        const urlParams = new URLSearchParams(window.location.search);
        let lang = urlParams.get('lang');
        if (!lang) {
            const navLang = (navigator.language || navigator.userLanguage || 'zh').toLowerCase();
            lang = navLang.startsWith('zh') ? 'zh' : 'en';
        }
        this.setLanguage(lang);

        const langSelect = document.getElementById('lang-selector');
        if (langSelect) {
            langSelect.value = this.lang;
            langSelect.addEventListener('change', (e) => {
                this.setLanguage(e.target.value);
            });
        }
    }

    setLanguage(lang) {
        if (!I18N[lang]) lang = 'zh';
        this.lang = lang;
        const t = I18N[lang];

        const setText = (id, text) => {
            const el = document.getElementById(id);
            if (el) el.innerText = text;
        };

        setText('btn-mode-shaded-edges', t.mode_shaded_edges);
        setText('btn-mode-shaded', t.mode_shaded);
        setText('btn-mode-wireframe', t.mode_wireframe);
        setText('btn-view-iso', t.view_iso);
        setText('btn-view-top', t.view_top);
        setText('btn-view-front', t.view_front);
        setText('btn-view-right', t.view_right);
        setText('btn-fit', t.fit);
        setText('btn-dimensions', t.dimensions);
        setText('btn-measure', t.measure);
        setText('btn-section', t.section);

        const measureTitle = document.querySelector('#measure-bar .tool-title');
        if (measureTitle) measureTitle.innerText = t.measure_title;
        setText('btn-measure-clear', t.measure_clear);
        setText('btn-measure-close', t.measure_close);

        const secTitle = document.querySelector('#section-bar .tool-title');
        if (secTitle) secTitle.innerText = t.section_title;
        setText('sec-invert', t.section_invert);
        setText('btn-section-close', t.section_close);

        const treeTitle = document.querySelector('.tree-title');
        if (treeTitle) treeTitle.innerText = t.tree_title;
        setText('btn-tree-isolate-reset', t.tree_all);
        const searchInput = document.getElementById('tree-search-input');
        if (searchInput) searchInput.placeholder = t.tree_search_placeholder;
        const treeStatus = document.getElementById('tree-status');
        if (treeStatus && !this.selectedComponentId) treeStatus.innerHTML = `<span>${t.tree_status}</span>`;

        const statLabels = document.querySelectorAll('#info-panel .stat-label');
        if (statLabels.length >= 3) {
            statLabels[0].innerText = t.dims_label;
            statLabels[1].innerText = t.verts_label;
            statLabels[2].innerText = t.faces_label;
        }

        const loaderText = document.getElementById('loader-text');
        if (loaderText && (loaderText.innerText.includes('解析') || loaderText.innerText.includes('Loading'))) {
            loaderText.innerText = t.loading;
        }

        const statName = document.getElementById('stat-name');
        if (statName && (statName.innerText === '等待选择模型...' || statName.innerText === 'Select a 3D model to view...')) {
            statName.innerText = t.waiting;
        }

        const langSelect = document.getElementById('lang-selector');
        if (langSelect && langSelect.value !== lang) {
            langSelect.value = lang;
        }
    }
}

window.OmniPluginRegistry = window.OmniPluginRegistry || {};
window.OmniPluginRegistry['cad-3d'] = {
    mount(container, context, hostApi) {
        const cadStage = document.getElementById('cad-stage');
        const universalStage = document.getElementById('universal-stage');
        if (cadStage) cadStage.style.display = 'block';
        if (universalStage) universalStage.style.display = 'none';
        if (!window.viewer) {
            window.viewer = new ModelPeekViewer('canvas-container');
        } else {
            window.viewer.onWindowResize();
            window.viewer.loadModel(context.filePath, context.origPath || context.fileName);
        }
    },
    unmount() {
        const cadStage = document.getElementById('cad-stage');
        if (cadStage) cadStage.style.display = 'none';
    }
};
