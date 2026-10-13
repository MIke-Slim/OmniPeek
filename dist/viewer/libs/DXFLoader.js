/**
 * Standalone Lightweight DXF Loader for Three.js
 * Parses ASCII DXF files into Three.js line segments, polylines, circles, arcs, and 3D faces.
 */

(function () {
    // AutoCAD Standard 256 ACI Color Palette Map (Key 1-7 and fallbacks)
    const ACI_COLORS = {
        1: 0xff0000, // Red
        2: 0xffff00, // Yellow
        3: 0x00ff00, // Green
        4: 0x00ffff, // Cyan
        5: 0x0000ff, // Blue
        6: 0xff00ff, // Magenta
        7: 0xffffff, // White
        8: 0x808080, // Dark Gray
        9: 0xc0c0c0  // Light Gray
    };

    function getAciColor(index) {
        if (!index || index <= 0) return 0x00d2ff; // Default CAD Cyan
        if (ACI_COLORS[index]) return ACI_COLORS[index];
        // Generate pseudo RGB from index
        const r = (index * 67) % 256;
        const g = (index * 131) % 256;
        const b = (index * 197) % 256;
        return (r << 16) | (g << 8) | b;
    }

    class DXFLoader extends THREE.Loader {
        constructor(manager) {
            super(manager);
        }

        load(url, onLoad, onProgress, onError) {
            const scope = this;
            const loader = new THREE.FileLoader(scope.manager);
            loader.setPath(scope.path);
            loader.setResponseType('text');
            loader.setRequestHeader(scope.requestHeader);
            loader.setWithCredentials(scope.withCredentials);
            loader.load(url, function (text) {
                try {
                    onLoad(scope.parse(text));
                } catch (e) {
                    if (onError) onError(e);
                    else console.error(e);
                    scope.manager.itemError(url);
                }
            }, onProgress, onError);
        }

        parse(text) {
            const lines = text.split(/\r\n|\r|\n/);
            const linePositions = [];
            const lineColors = [];
            const meshPositions = [];
            const meshColors = [];

            let i = 0;
            const numLines = lines.length;

            let inEntities = false;
            let currentEntity = null;
            let entityData = {};

            function flushEntity() {
                if (!currentEntity) return;

                const col = getAciColor(entityData.color || 4);
                const r = ((col >> 16) & 255) / 255;
                const g = ((col >> 8) & 255) / 255;
                const b = (col & 255) / 255;

                switch (currentEntity) {
                    case 'LINE': {
                        const x1 = entityData.x1 || 0, y1 = entityData.y1 || 0, z1 = entityData.z1 || 0;
                        const x2 = entityData.x2 || 0, y2 = entityData.y2 || 0, z2 = entityData.z2 || 0;
                        linePositions.push(x1, y1, z1, x2, y2, z2);
                        lineColors.push(r, g, b, r, g, b);
                        break;
                    }
                    case 'CIRCLE': {
                        const cx = entityData.cx || 0, cy = entityData.cy || 0, cz = entityData.cz || 0;
                        const radius = entityData.radius || 0;
                        if (radius > 0) {
                            const segs = 48;
                            for (let s = 0; s < segs; s++) {
                                const th1 = (s / segs) * Math.PI * 2;
                                const th2 = ((s + 1) / segs) * Math.PI * 2;
                                linePositions.push(
                                    cx + Math.cos(th1) * radius, cy + Math.sin(th1) * radius, cz,
                                    cx + Math.cos(th2) * radius, cy + Math.sin(th2) * radius, cz
                                );
                                lineColors.push(r, g, b, r, g, b);
                            }
                        }
                        break;
                    }
                    case 'ARC': {
                        const cx = entityData.cx || 0, cy = entityData.cy || 0, cz = entityData.cz || 0;
                        const radius = entityData.radius || 0;
                        let aStart = ((entityData.startAngle || 0) * Math.PI) / 180;
                        let aEnd = ((entityData.endAngle || 360) * Math.PI) / 180;
                        if (aEnd < aStart) aEnd += Math.PI * 2;

                        if (radius > 0) {
                            const segs = 32;
                            for (let s = 0; s < segs; s++) {
                                const th1 = aStart + (s / segs) * (aEnd - aStart);
                                const th2 = aStart + ((s + 1) / segs) * (aEnd - aStart);
                                linePositions.push(
                                    cx + Math.cos(th1) * radius, cy + Math.sin(th1) * radius, cz,
                                    cx + Math.cos(th2) * radius, cy + Math.sin(th2) * radius, cz
                                );
                                lineColors.push(r, g, b, r, g, b);
                            }
                        }
                        break;
                    }
                    case 'LWPOLYLINE': {
                        const pts = entityData.points || [];
                        const isClosed = (entityData.flag & 1) === 1;
                        for (let p = 0; p < pts.length - 1; p++) {
                            linePositions.push(pts[p].x, pts[p].y, pts[p].z || 0, pts[p + 1].x, pts[p + 1].y, pts[p + 1].z || 0);
                            lineColors.push(r, g, b, r, g, b);
                        }
                        if (isClosed && pts.length > 2) {
                            const pFirst = pts[0];
                            const pLast = pts[pts.length - 1];
                            linePositions.push(pLast.x, pLast.y, pLast.z || 0, pFirst.x, pFirst.y, pFirst.z || 0);
                            lineColors.push(r, g, b, r, g, b);
                        }
                        break;
                    }
                    case '3DFACE':
                    case 'SOLID': {
                        const v1 = { x: entityData.x1 || 0, y: entityData.y1 || 0, z: entityData.z1 || 0 };
                        const v2 = { x: entityData.x2 || 0, y: entityData.y2 || 0, z: entityData.z2 || 0 };
                        const v3 = { x: entityData.x3 || 0, y: entityData.y3 || 0, z: entityData.z3 || 0 };
                        const v4 = { x: entityData.x4 || v3.x, y: entityData.y4 || v3.y, z: entityData.z4 || v3.z };

                        // Triangle 1: v1, v2, v3
                        meshPositions.push(v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, v3.x, v3.y, v3.z);
                        meshColors.push(r, g, b, r, g, b, r, g, b);

                        // Triangle 2 (if quad): v1, v3, v4
                        if (v4.x !== v3.x || v4.y !== v3.y || v4.z !== v3.z) {
                            meshPositions.push(v1.x, v1.y, v1.z, v3.x, v3.y, v3.z, v4.x, v4.y, v4.z);
                            meshColors.push(r, g, b, r, g, b, r, g, b);
                        }
                        break;
                    }
                }
                currentEntity = null;
                entityData = {};
            }

            while (i < numLines - 1) {
                const code = parseInt(lines[i].trim(), 10);
                const val = lines[i + 1].trim();
                i += 2;

                if (code === 0) {
                    if (val === 'SECTION') {
                        // next lines will tell which section
                    } else if (val === 'ENDSEC') {
                        inEntities = false;
                        flushEntity();
                    } else if (val === 'ENTITIES') {
                        inEntities = true;
                    } else if (inEntities) {
                        flushEntity();
                        currentEntity = val;
                        entityData = { points: [] };
                    }
                } else if (code === 2 && val === 'ENTITIES') {
                    inEntities = true;
                } else if (inEntities && currentEntity) {
                    // Collect entity parameters
                    switch (code) {
                        case 8: entityData.layer = val; break;
                        case 62: entityData.color = parseInt(val, 10); break;
                        case 70: entityData.flag = parseInt(val, 10); break;

                        // Point 1
                        case 10:
                            if (currentEntity === 'LWPOLYLINE') {
                                entityData.points.push({ x: parseFloat(val), y: 0, z: 0 });
                            } else {
                                entityData.x1 = parseFloat(val);
                                entityData.cx = parseFloat(val);
                            }
                            break;
                        case 20:
                            if (currentEntity === 'LWPOLYLINE') {
                                if (entityData.points.length > 0) {
                                    entityData.points[entityData.points.length - 1].y = parseFloat(val);
                                }
                            } else {
                                entityData.y1 = parseFloat(val);
                                entityData.cy = parseFloat(val);
                            }
                            break;
                        case 30:
                            if (currentEntity === 'LWPOLYLINE') {
                                if (entityData.points.length > 0) {
                                    entityData.points[entityData.points.length - 1].z = parseFloat(val);
                                }
                            } else {
                                entityData.z1 = parseFloat(val);
                                entityData.cz = parseFloat(val);
                            }
                            break;

                        // Point 2
                        case 11: entityData.x2 = parseFloat(val); break;
                        case 21: entityData.y2 = parseFloat(val); break;
                        case 31: entityData.z2 = parseFloat(val); break;

                        // Point 3
                        case 12: entityData.x3 = parseFloat(val); break;
                        case 22: entityData.y3 = parseFloat(val); break;
                        case 32: entityData.z3 = parseFloat(val); break;

                        // Point 4
                        case 13: entityData.x4 = parseFloat(val); break;
                        case 23: entityData.y4 = parseFloat(val); break;
                        case 33: entityData.z4 = parseFloat(val); break;

                        // Dimensions / Angles
                        case 40: entityData.radius = parseFloat(val); break;
                        case 50: entityData.startAngle = parseFloat(val); break;
                        case 51: entityData.endAngle = parseFloat(val); break;
                    }
                }
            }
            flushEntity();

            const group = new THREE.Group();
            group.name = 'DXF Model';

            // 1. Build Line Geometry
            if (linePositions.length > 0) {
                const lineGeom = new THREE.BufferGeometry();
                lineGeom.setAttribute('position', new THREE.Float32BufferAttribute(linePositions, 3));
                lineGeom.setAttribute('color', new THREE.Float32BufferAttribute(lineColors, 3));
                const lineMat = new THREE.LineBasicMaterial({ vertexColors: true, linewidth: 1.5 });
                const linesMesh = new THREE.LineSegments(lineGeom, lineMat);
                linesMesh.name = 'DXF 线条与轮廓';
                group.add(linesMesh);
            }

            // 2. Build Mesh Geometry (3DFACE / SOLID)
            if (meshPositions.length > 0) {
                const meshGeom = new THREE.BufferGeometry();
                meshGeom.setAttribute('position', new THREE.Float32BufferAttribute(meshPositions, 3));
                meshGeom.setAttribute('color', new THREE.Float32BufferAttribute(meshColors, 3));
                meshGeom.computeVertexNormals();
                const meshMat = new THREE.MeshStandardMaterial({
                    vertexColors: true,
                    side: THREE.DoubleSide,
                    roughness: 0.4,
                    metalness: 0.1
                });
                const facesMesh = new THREE.Mesh(meshGeom, meshMat);
                facesMesh.name = 'DXF 三维实体面';
                group.add(facesMesh);
            }

            return group;
        }
    }

    THREE.DXFLoader = DXFLoader;
})();
