#pragma once
#include <windows.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

struct Vec3 {
    float x, y, z;
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const {
        return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x };
    }
    Vec3 normalize() const {
        float l = std::sqrt(x * x + y * y + z * z);
        return l > 1e-6f ? (*this) * (1.0f / l) : *this;
    }
};

struct Triangle {
    Vec3 v[3];
    Vec3 normal;
};

// Robust STL parser supporting both ASCII and Binary STL with wide filepath
bool LoadSTL(const std::wstring& wpath, std::vector<Triangle>& tris, Vec3& minB, Vec3& maxB) {
    tris.clear();
    minB = {1e9f, 1e9f, 1e9f};
    maxB = {-1e9f, -1e9f, -1e9f};

    FILE* f = _wfopen(wpath.c_str(), L"rb");
    if (!f) return false;

    unsigned char bom[3] = {0}; long startOff = 0;
    if (fread(bom, 1, 3, f) == 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) startOff = 3;
    fseek(f, startOff, SEEK_SET);
    char header[6] = {0};
    fread(header, 1, 5, f);
    bool isAscii = (std::string(header).substr(0, 5) == "solid");

    if (isAscii) {
        fseek(f, 0, SEEK_SET);
        char buf[256];
        bool foundFacet = false;
        for (int i = 0; i < 20 && fgets(buf, sizeof(buf), f); ++i) {
            if (strstr(buf, "facet")) {
                foundFacet = true;
                break;
            }
        }
        if (!foundFacet) isAscii = false;
    }

    if (isAscii) {
        fseek(f, 0, SEEK_SET);
        char word[64];
        Triangle curTri;
        int vIdx = 0;

        while (fscanf(f, "%63s", word) == 1) {
            if (strcmp(word, "facet") == 0) {
                char dummy[64];
                fscanf(f, "%63s %f %f %f", dummy, &curTri.normal.x, &curTri.normal.y, &curTri.normal.z);
                vIdx = 0;
            } else if (strcmp(word, "vertex") == 0) {
                if (vIdx < 3) {
                    fscanf(f, "%f %f %f", &curTri.v[vIdx].x, &curTri.v[vIdx].y, &curTri.v[vIdx].z);
                    minB.x = std::min(minB.x, curTri.v[vIdx].x);
                    minB.y = std::min(minB.y, curTri.v[vIdx].y);
                    minB.z = std::min(minB.z, curTri.v[vIdx].z);
                    maxB.x = std::max(maxB.x, curTri.v[vIdx].x);
                    maxB.y = std::max(maxB.y, curTri.v[vIdx].y);
                    maxB.z = std::max(maxB.z, curTri.v[vIdx].z);
                    vIdx++;
                }
            } else if (strcmp(word, "endfacet") == 0) {
                if (curTri.normal.dot(curTri.normal) < 1e-4f) {
                    Vec3 edge1 = curTri.v[1] - curTri.v[0];
                    Vec3 edge2 = curTri.v[2] - curTri.v[0];
                    curTri.normal = edge1.cross(edge2).normalize();
                } else {
                    curTri.normal = curTri.normal.normalize();
                }
                tris.push_back(curTri);
            }
        }
        fclose(f);
        return !tris.empty();
    } else {
        fseek(f, 80, SEEK_SET);
        uint32_t count = 0;
        fread(&count, 4, 1, f);
        if (count == 0 || count > 10000000) {
            fclose(f);
            return false;
        }

        tris.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            float n[3], v[9];
            uint16_t attr;
            fread(n, 4, 3, f);
            fread(v, 4, 9, f);
            fread(&attr, 2, 1, f);

            tris[i].normal = {n[0], n[1], n[2]};
            for (int j = 0; j < 3; ++j) {
                tris[i].v[j] = {v[j * 3], v[j * 3 + 1], v[j * 3 + 2]};
                minB.x = std::min(minB.x, tris[i].v[j].x);
                minB.y = std::min(minB.y, tris[i].v[j].y);
                minB.z = std::min(minB.z, tris[i].v[j].z);
                maxB.x = std::max(maxB.x, tris[i].v[j].x);
                maxB.y = std::max(maxB.y, tris[i].v[j].y);
                maxB.z = std::max(maxB.z, tris[i].v[j].z);
            }

            if (tris[i].normal.dot(tris[i].normal) < 1e-4f) {
                Vec3 edge1 = tris[i].v[1] - tris[i].v[0];
                Vec3 edge2 = tris[i].v[2] - tris[i].v[0];
                tris[i].normal = edge1.cross(edge2).normalize();
            } else {
                tris[i].normal = tris[i].normal.normalize();
            }
        }
        fclose(f);
        return true;
    }
}

// Wavefront OBJ parser with wide filepath
bool LoadOBJ(const std::wstring& wpath, std::vector<Triangle>& tris, Vec3& minB, Vec3& maxB) {
    tris.clear();
    minB = {1e9f, 1e9f, 1e9f};
    maxB = {-1e9f, -1e9f, -1e9f};

    FILE* f = _wfopen(wpath.c_str(), L"r");
    if (!f) return false;

    std::vector<Vec3> verts;
    char line[1024];

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'v' && line[1] == ' ') {
            Vec3 pt;
            if (sscanf(line + 2, "%f %f %f", &pt.x, &pt.y, &pt.z) == 3) {
                verts.push_back(pt);
                minB.x = std::min(minB.x, pt.x);
                minB.y = std::min(minB.y, pt.y);
                minB.z = std::min(minB.z, pt.z);
                maxB.x = std::max(maxB.x, pt.x);
                maxB.y = std::max(maxB.y, pt.y);
                maxB.z = std::max(maxB.z, pt.z);
            }
        } else if (line[0] == 'f' && line[1] == ' ') {
            std::vector<int> faceIndices;
            char* token = strtok(line + 2, " \t\r\n");
            while (token) {
                int idx = 0;
                sscanf(token, "%d", &idx);
                if (idx > 0) idx -= 1;
                else if (idx < 0) idx = (int)verts.size() + idx;
                faceIndices.push_back(idx);
                token = strtok(NULL, " \t\r\n");
            }

            for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                int i0 = faceIndices[0];
                int i1 = faceIndices[i];
                int i2 = faceIndices[i + 1];

                if (i0 >= 0 && i0 < (int)verts.size() &&
                    i1 >= 0 && i1 < (int)verts.size() &&
                    i2 >= 0 && i2 < (int)verts.size()) {
                    Triangle tri;
                    tri.v[0] = verts[i0];
                    tri.v[1] = verts[i1];
                    tri.v[2] = verts[i2];
                    Vec3 e1 = tri.v[1] - tri.v[0];
                    Vec3 e2 = tri.v[2] - tri.v[0];
                    tri.normal = e1.cross(e2).normalize();
                    tris.push_back(tri);
                }
            }
        }
    }
    fclose(f);
    return !tris.empty();
}

// Stanford PLY parser (ASCII & basic Little Endian)
bool LoadPLY(const std::wstring& wpath, std::vector<Triangle>& tris, Vec3& minB, Vec3& maxB) {
    tris.clear();
    minB = {1e9f, 1e9f, 1e9f};
    maxB = {-1e9f, -1e9f, -1e9f};

    FILE* f = _wfopen(wpath.c_str(), L"r");
    if (!f) return false;

    char line[256];
    if (!fgets(line, sizeof(line), f) || strncmp(line, "ply", 3) != 0) {
        fclose(f);
        return false;
    }

    int numVerts = 0;
    int numFaces = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "element vertex", 14) == 0) {
            sscanf(line + 14, "%d", &numVerts);
        } else if (strncmp(line, "element face", 12) == 0) {
            sscanf(line + 12, "%d", &numFaces);
        } else if (strncmp(line, "end_header", 10) == 0) {
            break;
        }
    }

    if (numVerts <= 0) {
        fclose(f);
        return false;
    }

    std::vector<Vec3> verts;
    verts.reserve(numVerts);

    for (int i = 0; i < numVerts; ++i) {
        Vec3 pt;
        if (fscanf(f, "%f %f %f", &pt.x, &pt.y, &pt.z) == 3) {
            char rest[256];
            fgets(rest, sizeof(rest), f);
            verts.push_back(pt);
            minB.x = std::min(minB.x, pt.x);
            minB.y = std::min(minB.y, pt.y);
            minB.z = std::min(minB.z, pt.z);
            maxB.x = std::max(maxB.x, pt.x);
            maxB.y = std::max(maxB.y, pt.y);
            maxB.z = std::max(maxB.z, pt.z);
        }
    }

    for (int i = 0; i < numFaces; ++i) {
        int n = 0;
        if (fscanf(f, "%d", &n) == 1 && n >= 3) {
            std::vector<int> idx(n);
            for (int j = 0; j < n; ++j) {
                fscanf(f, "%d", &idx[j]);
            }
            char rest[256];
            fgets(rest, sizeof(rest), f);

            for (int j = 1; j + 1 < n; ++j) {
                int i0 = idx[0], i1 = idx[j], i2 = idx[j + 1];
                if (i0 < (int)verts.size() && i1 < (int)verts.size() && i2 < (int)verts.size()) {
                    Triangle tri;
                    tri.v[0] = verts[i0];
                    tri.v[1] = verts[i1];
                    tri.v[2] = verts[i2];
                    Vec3 e1 = tri.v[1] - tri.v[0];
                    Vec3 e2 = tri.v[2] - tri.v[0];
                    tri.normal = e1.cross(e2).normalize();
                    tris.push_back(tri);
                }
            }
        }
    }
    fclose(f);
    return !tris.empty();
}

// Point Cloud Data (.pcd) parser (ASCII & Binary)
bool LoadPCD(const std::wstring& wpath, std::vector<Triangle>& tris, Vec3& minB, Vec3& maxB) {
    tris.clear();
    minB = {1e9f, 1e9f, 1e9f};
    maxB = {-1e9f, -1e9f, -1e9f};

    FILE* f = _wfopen(wpath.c_str(), L"rb");
    if (!f) return false;

    char line[512];
    int numPoints = 0;
    std::string dataType = "ascii";

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "POINTS", 6) == 0) {
            sscanf(line + 6, "%d", &numPoints);
        } else if (strncmp(line, "DATA", 4) == 0) {
            char dt[64] = {0};
            sscanf(line + 4, "%63s", dt);
            dataType = dt;
            break;
        }
    }

    if (numPoints <= 0) {
        fclose(f);
        return false;
    }

    std::vector<Vec3> points;
    points.reserve(std::min(numPoints, 35000));

    if (dataType == "ascii") {
        int stride = (numPoints > 20000) ? (numPoints / 15000 + 1) : 1;
        int count = 0;
        float x, y, z;
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "%f %f %f", &x, &y, &z) >= 3) {
                if ((count++ % stride) == 0) {
                    points.push_back({x, y, z});
                    minB.x = std::min(minB.x, x);
                    minB.y = std::min(minB.y, y);
                    minB.z = std::min(minB.z, z);
                    maxB.x = std::max(maxB.x, x);
                    maxB.y = std::max(maxB.y, y);
                    maxB.z = std::max(maxB.z, z);
                }
            }
        }
    } else {
        int stride = (numPoints > 20000) ? (numPoints / 15000 + 1) : 1;
        for (int i = 0; i < numPoints; ++i) {
            float pt[3];
            if (fread(pt, sizeof(float), 3, f) != 3) break;
            if ((i % stride) == 0) {
                points.push_back({pt[0], pt[1], pt[2]});
                minB.x = std::min(minB.x, pt[0]);
                minB.y = std::min(minB.y, pt[1]);
                minB.z = std::min(minB.z, pt[2]);
                maxB.x = std::max(maxB.x, pt[0]);
                maxB.y = std::max(maxB.y, pt[1]);
                maxB.z = std::max(maxB.z, pt[2]);
            }
        }
    }
    fclose(f);

    if (points.empty()) return false;

    float maxDim = std::max({maxB.x - minB.x, maxB.y - minB.y, maxB.z - minB.z});
    if (maxDim < 1e-4f) maxDim = 1.0f;
    float r = std::max(maxDim * 0.007f, 0.03f);

    tris.reserve(points.size() * 4);
    for (const auto& pt : points) {
        Vec3 v0 = pt + Vec3{0, r, 0};
        Vec3 v1 = pt + Vec3{r * 0.94f, -r * 0.33f, 0};
        Vec3 v2 = pt + Vec3{-r * 0.47f, -r * 0.33f, r * 0.81f};
        Vec3 v3 = pt + Vec3{-r * 0.47f, -r * 0.33f, -r * 0.81f};

        auto addTetraFace = [&](const Vec3& a, const Vec3& b, const Vec3& c) {
            Triangle t;
            t.v[0] = a; t.v[1] = b; t.v[2] = c;
            t.normal = (b - a).cross(c - a).normalize();
            tris.push_back(t);
        };
        addTetraFace(v0, v1, v2);
        addTetraFace(v0, v2, v3);
        addTetraFace(v0, v3, v1);
        addTetraFace(v1, v3, v2);
    }
    return !tris.empty();
}

struct LineSeg { Vec3 p1, p2; };

static void RibbonizeSegments(const std::vector<LineSeg>& segs, std::vector<Triangle>& tris, const Vec3& minB, const Vec3& maxB) {
    float maxDim = std::max({maxB.x - minB.x, maxB.y - minB.y, maxB.z - minB.z});
    if (maxDim < 1e-4f) maxDim = 1.0f;
    float r = std::max(maxDim * 0.0035f, 0.02f);

    for (const auto& s : segs) {
        Vec3 delta = s.p2 - s.p1;
        float len = std::sqrt(delta.dot(delta));
        if (len < 1e-5f) continue;
        Vec3 dir = delta * (1.0f / len);
        Vec3 ref = (std::abs(dir.x) < 0.9f && std::abs(dir.y) < 0.9f) ? Vec3{0, 0, 1} : Vec3{0, 1, 0};
        Vec3 u1 = dir.cross(ref).normalize() * r;
        Vec3 u2 = dir.cross(u1).normalize() * r;

        // Quad 1 (along u1)
        Vec3 q1a = s.p1 - u1, q1b = s.p1 + u1, q1c = s.p2 + u1, q1d = s.p2 - u1;
        Vec3 n1 = u2.normalize();
        tris.push_back({ {q1a, q1b, q1c}, n1 });
        tris.push_back({ {q1a, q1c, q1d}, n1 });

        // Quad 2 (along u2)
        Vec3 q2a = s.p1 - u2, q2b = s.p1 + u2, q2c = s.p2 + u2, q2d = s.p2 - u2;
        Vec3 n2 = u1.normalize();
        tris.push_back({ {q2a, q2b, q2c}, n2 });
        tris.push_back({ {q2a, q2c, q2d}, n2 });
    }
}

// AutoCAD DXF parser (LINE, 3DFACE, SOLID, CIRCLE, ARC, LWPOLYLINE)
bool LoadDXF(const std::wstring& wpath, std::vector<Triangle>& tris, Vec3& minB, Vec3& maxB) {
    tris.clear();
    minB = {1e9f, 1e9f, 1e9f};
    maxB = {-1e9f, -1e9f, -1e9f};

    FILE* f = _wfopen(wpath.c_str(), L"r");
    if (!f) return false;

    std::vector<LineSeg> segs;
    std::vector<Triangle> rawFaces;

    char lineCode[256];
    char lineVal[256];

    auto trim = [](char* s) -> std::string {
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
        std::string res = s;
        while (!res.empty() && (res.back() == ' ' || res.back() == '\t' || res.back() == '\r' || res.back() == '\n')) {
            res.pop_back();
        }
        return res;
    };

    std::string currentEntity = "";
    Vec3 p1 = {0, 0, 0}, p2 = {0, 0, 0}, p3 = {0, 0, 0}, p4 = {0, 0, 0};
    float radius = 0.0f, startAngle = 0.0f, endAngle = 360.0f;
    std::vector<Vec3> polyVerts;
    int polyClosed = 0;
    float elevation = 0.0f;
    bool inEntities = false;

    auto finishEntity = [&]() {
        if (currentEntity == "LINE") {
            segs.push_back({p1, p2});
            minB.x = std::min({minB.x, p1.x, p2.x});
            minB.y = std::min({minB.y, p1.y, p2.y});
            minB.z = std::min({minB.z, p1.z, p2.z});
            maxB.x = std::max({maxB.x, p1.x, p2.x});
            maxB.y = std::max({maxB.y, p1.y, p2.y});
            maxB.z = std::max({maxB.z, p1.z, p2.z});
        } else if (currentEntity == "3DFACE" || currentEntity == "SOLID") {
            Triangle t1, t2;
            t1.v[0] = p1; t1.v[1] = p2; t1.v[2] = p3;
            t1.normal = (p2 - p1).cross(p3 - p1).normalize();
            rawFaces.push_back(t1);
            if (p4.x != p3.x || p4.y != p3.y || p4.z != p3.z) {
                t2.v[0] = p1; t2.v[1] = p3; t2.v[2] = p4;
                t2.normal = (p3 - p1).cross(p4 - p1).normalize();
                rawFaces.push_back(t2);
            }
            minB.x = std::min({minB.x, p1.x, p2.x, p3.x, p4.x});
            minB.y = std::min({minB.y, p1.y, p2.y, p3.y, p4.y});
            minB.z = std::min({minB.z, p1.z, p2.z, p3.z, p4.z});
            maxB.x = std::max({maxB.x, p1.x, p2.x, p3.x, p4.x});
            maxB.y = std::max({maxB.y, p1.y, p2.y, p3.y, p4.y});
            maxB.z = std::max({maxB.z, p1.z, p2.z, p3.z, p4.z});
        } else if (currentEntity == "CIRCLE" && radius > 1e-4f) {
            const int S = 24;
            Vec3 prev = { p1.x + radius, p1.y, p1.z };
            for (int s = 1; s <= S; ++s) {
                float a = (float)(s * 2.0 * 3.14159265 / S);
                Vec3 cur = { p1.x + radius * std::cos(a), p1.y + radius * std::sin(a), p1.z };
                segs.push_back({prev, cur});
                minB.x = std::min(minB.x, cur.x); minB.y = std::min(minB.y, cur.y); minB.z = std::min(minB.z, cur.z);
                maxB.x = std::max(maxB.x, cur.x); maxB.y = std::max(maxB.y, cur.y); maxB.z = std::max(maxB.z, cur.z);
                prev = cur;
            }
        } else if (currentEntity == "ARC" && radius > 1e-4f) {
            const int S = 16;
            float a1 = startAngle * 3.14159265f / 180.0f;
            float a2 = endAngle * 3.14159265f / 180.0f;
            if (a2 < a1) a2 += 2.0f * 3.14159265f;
            Vec3 prev = { p1.x + radius * std::cos(a1), p1.y + radius * std::sin(a1), p1.z };
            for (int s = 1; s <= S; ++s) {
                float a = a1 + (a2 - a1) * ((float)s / S);
                Vec3 cur = { p1.x + radius * std::cos(a), p1.y + radius * std::sin(a), p1.z };
                segs.push_back({prev, cur});
                minB.x = std::min(minB.x, cur.x); minB.y = std::min(minB.y, cur.y); minB.z = std::min(minB.z, cur.z);
                maxB.x = std::max(maxB.x, cur.x); maxB.y = std::max(maxB.y, cur.y); maxB.z = std::max(maxB.z, cur.z);
                prev = cur;
            }
        } else if (currentEntity == "LWPOLYLINE" && polyVerts.size() >= 2) {
            for (size_t i = 0; i + 1 < polyVerts.size(); ++i) {
                segs.push_back({polyVerts[i], polyVerts[i + 1]});
                minB.x = std::min(minB.x, polyVerts[i].x); minB.y = std::min(minB.y, polyVerts[i].y); minB.z = std::min(minB.z, polyVerts[i].z);
                maxB.x = std::max(maxB.x, polyVerts[i].x); maxB.y = std::max(maxB.y, polyVerts[i].y); maxB.z = std::max(maxB.z, polyVerts[i].z);
            }
            if (polyClosed) {
                segs.push_back({polyVerts.back(), polyVerts.front()});
            }
        }
    };

    while (fgets(lineCode, sizeof(lineCode), f) && fgets(lineVal, sizeof(lineVal), f)) {
        int code = atoi(lineCode);
        std::string val = trim(lineVal);

        if (code == 2 && val == "ENTITIES") {
            inEntities = true;
            continue;
        }
        if (code == 0 && val == "ENDSEC") {
            if (inEntities) {
                finishEntity();
                break;
            }
        }

        if (code == 0) {
            finishEntity();
            currentEntity = val;
            p1 = p2 = p3 = p4 = {0, 0, 0};
            radius = startAngle = 0.0f;
            endAngle = 360.0f;
            polyVerts.clear();
            polyClosed = 0;
            elevation = 0.0f;
            continue;
        }

        if (!inEntities) continue;

        switch (code) {
            case 10: p1.x = (float)atof(val.c_str()); if (currentEntity == "LWPOLYLINE") polyVerts.push_back({p1.x, 0, elevation}); break;
            case 20: p1.y = (float)atof(val.c_str()); if (currentEntity == "LWPOLYLINE" && !polyVerts.empty()) polyVerts.back().y = p1.y; break;
            case 30: p1.z = (float)atof(val.c_str()); break;
            case 11: p2.x = (float)atof(val.c_str()); break;
            case 21: p2.y = (float)atof(val.c_str()); break;
            case 31: p2.z = (float)atof(val.c_str()); break;
            case 12: p3.x = (float)atof(val.c_str()); break;
            case 22: p3.y = (float)atof(val.c_str()); break;
            case 32: p3.z = (float)atof(val.c_str()); break;
            case 13: p4.x = (float)atof(val.c_str()); break;
            case 23: p4.y = (float)atof(val.c_str()); break;
            case 33: p4.z = (float)atof(val.c_str()); break;
            case 38: elevation = (float)atof(val.c_str()); break;
            case 40: radius = (float)atof(val.c_str()); break;
            case 50: startAngle = (float)atof(val.c_str()); break;
            case 51: endAngle = (float)atof(val.c_str()); break;
            case 70: if (currentEntity == "LWPOLYLINE") polyClosed = atoi(val.c_str()) & 1; break;
        }
    }
    finishEntity();
    fclose(f);

    if (segs.empty() && rawFaces.empty()) return false;

    tris.insert(tris.end(), rawFaces.begin(), rawFaces.end());
    if (!segs.empty()) {
        RibbonizeSegments(segs, tris, minB, maxB);
    }

    return !tris.empty();
}

bool LoadGCode(const std::wstring& wpath, std::vector<Triangle>& tris, Vec3& minB, Vec3& maxB) {
    tris.clear();
    minB = {1e9f, 1e9f, 1e9f};
    maxB = {-1e9f, -1e9f, -1e9f};

    FILE* f = _wfopen(wpath.c_str(), L"r");
    if (!f) return false;

    std::vector<LineSeg> segs;
    float curX = 0, curY = 0, curZ = 0;
    bool hasPos = false;
    char line[512];

    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';' || *p == '(' || *p == '\0' || *p == '\r' || *p == '\n') continue;

        char* semi = strchr(p, ';');
        if (semi) *semi = '\0';

        if ((p[0] == 'G' || p[0] == 'g') && (p[1] == '0' || p[1] == '1')) {
            float nx = curX, ny = curY, nz = curZ;
            bool moved = false;
            char* token = strtok(p + 2, " \t\r\n");
            while (token) {
                if (token[0] == 'X' || token[0] == 'x') { nx = (float)atof(token + 1); moved = true; }
                else if (token[0] == 'Y' || token[0] == 'y') { ny = (float)atof(token + 1); moved = true; }
                else if (token[0] == 'Z' || token[0] == 'z') { nz = (float)atof(token + 1); moved = true; }
                token = strtok(NULL, " \t\r\n");
            }
            if (moved) {
                if (hasPos) {
                    float dx = nx - curX, dy = ny - curY, dz = nz - curZ;
                    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                    if (dist > 0.05f) {
                        segs.push_back({ {curX, curY, curZ}, {nx, ny, nz} });
                        minB.x = std::min({minB.x, curX, nx});
                        minB.y = std::min({minB.y, curY, ny});
                        minB.z = std::min({minB.z, curZ, nz});
                        maxB.x = std::max({maxB.x, curX, nx});
                        maxB.y = std::max({maxB.y, curY, ny});
                        maxB.z = std::max({maxB.z, curZ, nz});
                    }
                }
                hasPos = true;
                curX = nx; curY = ny; curZ = nz;
            }
        }
    }
    fclose(f);

    if (segs.empty()) return false;
    RibbonizeSegments(segs, tris, minB, maxB);
    return !tris.empty();
}

bool Load3DS(const std::wstring& wpath, std::vector<Triangle>& tris, Vec3& minB, Vec3& maxB) {
    tris.clear();
    minB = {1e9f, 1e9f, 1e9f};
    maxB = {-1e9f, -1e9f, -1e9f};

    FILE* f = _wfopen(wpath.c_str(), L"rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize < 16) { fclose(f); return false; }

    std::vector<uint8_t> data(fsize);
    if (fread(data.data(), 1, fsize, f) != (size_t)fsize) { fclose(f); return false; }
    fclose(f);

    struct ChunkParser {
        const uint8_t* pData;
        size_t totalLen;
        std::vector<Triangle>& outTris;
        Vec3& outMin;
        Vec3& outMax;

        void parse(size_t offset, size_t end) {
            std::vector<Vec3> curVerts;
            while (offset + 6 <= end && offset + 6 <= totalLen) {
                uint16_t cid = *(const uint16_t*)(pData + offset);
                uint32_t clen = *(const uint32_t*)(pData + offset + 2);
                if (clen < 6) break;
                size_t c_end = std::min(offset + clen, end);

                if (cid == 0x4D4D || cid == 0x3D3D || cid == 0x4100) {
                    parse(offset + 6, c_end);
                } else if (cid == 0x4000) {
                    size_t p = offset + 6;
                    while (p < c_end && pData[p] != 0) p++;
                    p++; // skip null
                    parse(p, c_end);
                } else if (cid == 0x4110) {
                    if (offset + 8 <= c_end) {
                        uint16_t nv = *(const uint16_t*)(pData + offset + 6);
                        curVerts.clear();
                        curVerts.reserve(nv);
                        const float* pf = (const float*)(pData + offset + 8);
                        for (uint16_t i = 0; i < nv && (offset + 8 + (i+1)*12 <= c_end); ++i) {
                            curVerts.push_back({ pf[i*3], pf[i*3+1], pf[i*3+2] });
                        }
                    }
                } else if (cid == 0x4120) {
                    if (offset + 8 <= c_end && !curVerts.empty()) {
                        uint16_t nf = *(const uint16_t*)(pData + offset + 6);
                        const uint16_t* pFace = (const uint16_t*)(pData + offset + 8);
                        for (uint16_t i = 0; i < nf && (offset + 8 + (i+1)*8 <= c_end); ++i) {
                            uint16_t a = pFace[i*4];
                            uint16_t b = pFace[i*4+1];
                            uint16_t c = pFace[i*4+2];
                            if (a < curVerts.size() && b < curVerts.size() && c < curVerts.size()) {
                                Vec3 v1 = curVerts[a], v2 = curVerts[b], v3 = curVerts[c];
                                Vec3 norm = (v2 - v1).cross(v3 - v1).normalize();
                                outTris.push_back({ {v1, v2, v3}, norm });
                                outMin.x = std::min({outMin.x, v1.x, v2.x, v3.x});
                                outMin.y = std::min({outMin.y, v1.y, v2.y, v3.y});
                                outMin.z = std::min({outMin.z, v1.z, v2.z, v3.z});
                                outMax.x = std::max({outMax.x, v1.x, v2.x, v3.x});
                                outMax.y = std::max({outMax.y, v1.y, v2.y, v3.y});
                                outMax.z = std::max({outMax.z, v1.z, v2.z, v3.z});
                            }
                        }
                    }
                }
                offset += clen;
            }
        }
    };

    ChunkParser parser{ data.data(), (size_t)fsize, tris, minB, maxB };
    parser.parse(0, (size_t)fsize);

    return !tris.empty();
}

bool LoadSTEP(const std::wstring& wpath, std::vector<Triangle>& tris, Vec3& minB, Vec3& maxB) {
    tris.clear();
    minB = {1e9f, 1e9f, 1e9f};
    maxB = {-1e9f, -1e9f, -1e9f};

    FILE* f = _wfopen(wpath.c_str(), L"rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0 || fsize > 150 * 1024 * 1024) { fclose(f); return false; }

    std::string content(fsize, '\0');
    fread(&content[0], 1, fsize, f);
    fclose(f);

    std::unordered_map<int, Vec3> points;
    std::unordered_map<int, int> vertex_to_point;
    std::unordered_map<int, Vec3> vertices;
    std::unordered_map<int, std::pair<int, int>> edge_curves;
    std::unordered_map<int, std::pair<int, bool>> oriented_edges;
    std::unordered_map<int, std::vector<int>> edge_loops_map;
    std::vector<LineSeg> segs;
    std::vector<Triangle> rawFaces;

    auto updateBBox = [&](const Vec3& pt) {
        minB.x = std::min(minB.x, pt.x); minB.y = std::min(minB.y, pt.y); minB.z = std::min(minB.z, pt.z);
        maxB.x = std::max(maxB.x, pt.x); maxB.y = std::max(maxB.y, pt.y); maxB.z = std::max(maxB.z, pt.z);
    };

    size_t pos = 0;
    while (pos < content.size()) {
        size_t semiPos = content.find(';', pos);
        if (semiPos == std::string::npos) break;

        std::string stmt = content.substr(pos, semiPos - pos);
        pos = semiPos + 1;

        size_t hashPos = stmt.find('#');
        if (hashPos == std::string::npos) continue;

        size_t eqPos = stmt.find('=', hashPos);
        if (eqPos == std::string::npos) continue;

        std::string idStr = stmt.substr(hashPos + 1, eqPos - (hashPos + 1));
        while (!idStr.empty() && (idStr.front() == ' ' || idStr.front() == '\t' || idStr.front() == '\r' || idStr.front() == '\n')) idStr.erase(0, 1);
        while (!idStr.empty() && (idStr.back() == ' ' || idStr.back() == '\t' || idStr.back() == '\r' || idStr.back() == '\n')) idStr.pop_back();

        int id = 0;
        try { id = std::stoi(idStr); } catch (...) { continue; }

        size_t parenOpen = stmt.find('(', eqPos);
        if (parenOpen == std::string::npos) continue;

        std::string type = stmt.substr(eqPos + 1, parenOpen - (eqPos + 1));
        while (!type.empty() && (type.front() == ' ' || type.front() == '\t' || type.front() == '\r' || type.front() == '\n')) type.erase(0, 1);
        while (!type.empty() && (type.back() == ' ' || type.back() == '\t' || type.back() == '\r' || type.back() == '\n')) type.pop_back();

        if (type == "CARTESIAN_POINT") {
            size_t innerOpen = stmt.find('(', parenOpen + 1);
            size_t innerClose = stmt.find(')', innerOpen != std::string::npos ? innerOpen : parenOpen);
            if (innerOpen != std::string::npos && innerClose != std::string::npos) {
                std::string coordsStr = stmt.substr(innerOpen + 1, innerClose - (innerOpen + 1));
                std::stringstream ss(coordsStr);
                std::string part;
                std::vector<float> vals;
                while (std::getline(ss, part, ',')) {
                    try { vals.push_back((float)std::stod(part)); } catch (...) {}
                }
                if (vals.size() == 3) {
                    points[id] = { vals[0], vals[1], vals[2] };
                }
            }
        } else if (type == "VERTEX_POINT") {
            size_t pHash = stmt.find('#', parenOpen);
            if (pHash != std::string::npos) {
                try {
                    int ptId = std::stoi(stmt.substr(pHash + 1));
                    vertex_to_point[id] = ptId;
                } catch (...) {}
            }
        } else if (type == "EDGE_CURVE") {
            std::vector<int> refs;
            size_t cur = parenOpen;
            while ((cur = stmt.find('#', cur)) != std::string::npos) {
                try { refs.push_back(std::stoi(stmt.substr(cur + 1))); } catch (...) {}
                cur++;
            }
            if (refs.size() >= 2) {
                edge_curves[id] = { refs[0], refs[1] };
            }
        } else if (type == "ORIENTED_EDGE") {
            size_t eidHash = stmt.rfind('#');
            if (eidHash != std::string::npos) {
                try {
                    int eid = std::stoi(stmt.substr(eidHash + 1));
                    bool sense = (stmt.find(".T.") != std::string::npos);
                    oriented_edges[id] = { eid, sense };
                } catch (...) {}
            }
        } else if (type == "POLY_LOOP") {
            std::vector<Vec3> loopPts;
            size_t cur = parenOpen;
            while ((cur = stmt.find('#', cur)) != std::string::npos) {
                try {
                    int pid = std::stoi(stmt.substr(cur + 1));
                    if (points.count(pid)) loopPts.push_back(points[pid]);
                } catch (...) {}
                cur++;
            }
            if (loopPts.size() >= 3) {
                Vec3 p0 = loopPts[0];
                for (size_t i = 1; i + 1 < loopPts.size(); ++i) {
                    Vec3 p1 = loopPts[i], p2 = loopPts[i + 1];
                    Vec3 norm = (p1 - p0).cross(p2 - p0).normalize();
                    rawFaces.push_back({ {p0, p1, p2}, norm });
                }
            }
        } else if (type == "EDGE_LOOP") {
            std::vector<int> oeList;
            size_t cur = parenOpen;
            while ((cur = stmt.find('#', cur)) != std::string::npos) {
                try { oeList.push_back(std::stoi(stmt.substr(cur + 1))); } catch (...) {}
                cur++;
            }
            if (!oeList.empty()) {
                edge_loops_map[id] = oeList;
            }
        }
    }

    // Resolve vertices
    for (const auto& [vid, ptId] : vertex_to_point) {
        if (points.count(ptId)) {
            vertices[vid] = points[ptId];
            updateBBox(points[ptId]);
        }
    }

    // Resolve edge curves into line segments
    for (const auto& [eid, ec] : edge_curves) {
        if (vertices.count(ec.first) && vertices.count(ec.second)) {
            segs.push_back({ vertices[ec.first], vertices[ec.second] });
        }
    }

    // Resolve edge loops into faces
    for (const auto& [lid, oeList] : edge_loops_map) {
        std::vector<Vec3> loopPts;
        for (int oeId : oeList) {
            if (oriented_edges.count(oeId)) {
                auto [eid, sense] = oriented_edges[oeId];
                if (edge_curves.count(eid)) {
                    auto [v1, v2] = edge_curves[eid];
                    int vId = sense ? v1 : v2;
                    if (vertices.count(vId)) {
                        loopPts.push_back(vertices[vId]);
                    }
                }
            }
        }
        if (loopPts.size() >= 3) {
            Vec3 p0 = loopPts[0];
            for (size_t i = 1; i + 1 < loopPts.size(); ++i) {
                Vec3 p1 = loopPts[i], p2 = loopPts[i + 1];
                Vec3 norm = (p1 - p0).cross(p2 - p0).normalize();
                rawFaces.push_back({ {p0, p1, p2}, norm });
            }
        }
    }

    if (rawFaces.empty() && segs.empty()) return false;

    tris.insert(tris.end(), rawFaces.begin(), rawFaces.end());
    if (tris.size() < 12 && !segs.empty()) {
        RibbonizeSegments(segs, tris, minB, maxB);
    }

    return !tris.empty();
}

bool LoadIGES(const std::wstring& wpath, std::vector<Triangle>& tris, Vec3& minB, Vec3& maxB) {
    tris.clear();
    minB = {1e9f, 1e9f, 1e9f};
    maxB = {-1e9f, -1e9f, -1e9f};

    FILE* f = _wfopen(wpath.c_str(), L"r");
    if (!f) return false;

    std::string paramText;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len >= 73 && line[72] == 'P') {
            line[64] = '\0';
            paramText += line;
        }
    }
    fclose(f);

    if (paramText.empty()) return false;

    std::vector<LineSeg> segs;
    auto updateBBox = [&](const Vec3& pt) {
        minB.x = std::min(minB.x, pt.x); minB.y = std::min(minB.y, pt.y); minB.z = std::min(minB.z, pt.z);
        maxB.x = std::max(maxB.x, pt.x); maxB.y = std::max(maxB.y, pt.y); maxB.z = std::max(maxB.z, pt.z);
    };

    std::stringstream ss(paramText);
    std::string stmt;
    while (std::getline(ss, stmt, ';')) {
        std::stringstream ssStmt(stmt);
        std::string part;
        std::vector<std::string> parts;
        while (std::getline(ssStmt, part, ',')) {
            while (!part.empty() && (part.front() == ' ' || part.front() == '\t')) part.erase(0, 1);
            while (!part.empty() && (part.back() == ' ' || part.back() == '\t')) part.pop_back();
            if (!part.empty()) parts.push_back(part);
        }
        if (parts.empty()) continue;

        int entityType = 0;
        try { entityType = std::stoi(parts[0]); } catch (...) { continue; }

        if (entityType == 110 && parts.size() >= 7) { // Line
            try {
                Vec3 p1 = { (float)std::stod(parts[1]), (float)std::stod(parts[2]), (float)std::stod(parts[3]) };
                Vec3 p2 = { (float)std::stod(parts[4]), (float)std::stod(parts[5]), (float)std::stod(parts[6]) };
                segs.push_back({ p1, p2 });
                updateBBox(p1); updateBBox(p2);
            } catch (...) {}
        } else if (entityType == 100 && parts.size() >= 8) { // Circular Arc
            try {
                float zt = (float)std::stod(parts[1]);
                float xc = (float)std::stod(parts[2]), yc = (float)std::stod(parts[3]);
                float xs = (float)std::stod(parts[4]), ys = (float)std::stod(parts[5]);
                float xe = (float)std::stod(parts[6]), ye = (float)std::stod(parts[7]);

                float r = std::sqrt((xs - xc)*(xs - xc) + (ys - yc)*(ys - yc));
                float a1 = std::atan2(ys - yc, xs - xc);
                float a2 = std::atan2(ye - yc, xe - xc);
                if (a2 <= a1) a2 += 2.0f * 3.14159265f;

                const int S = 16;
                Vec3 prev = { xs, ys, zt };
                updateBBox(prev);
                for (int i = 1; i <= S; ++i) {
                    float a = a1 + (a2 - a1) * ((float)i / S);
                    Vec3 cur = { xc + r * std::cos(a), yc + r * std::sin(a), zt };
                    segs.push_back({ prev, cur });
                    updateBBox(cur);
                    prev = cur;
                }
            } catch (...) {}
        } else if (entityType == 126 && parts.size() >= 8) { // Rational B-spline Curve
            try {
                int K = std::stoi(parts[1]);
                int M = std::stoi(parts[2]);
                int numKnots = K + M + 2;
                int numWeights = K + 1;
                size_t cpStart = 7 + numKnots + numWeights;
                int numPts = K + 1;

                auto parseVal = [](std::string s) -> double {
                    for (char& c : s) if (c == 'D' || c == 'd') c = 'E';
                    return std::stod(s);
                };

                std::vector<Vec3> cps;
                for (int i = 0; i < numPts; ++i) {
                    size_t idx = cpStart + i * 3;
                    if (idx + 2 < parts.size()) {
                        Vec3 pt = { (float)parseVal(parts[idx]), (float)parseVal(parts[idx + 1]), (float)parseVal(parts[idx + 2]) };
                        cps.push_back(pt);
                        updateBBox(pt);
                    }
                }
                for (size_t i = 0; i + 1 < cps.size(); ++i) {
                    segs.push_back({ cps[i], cps[i + 1] });
                }
            } catch (...) {}
        } else if (entityType == 106 && parts.size() >= 3) { // Copious Data
            try {
                int IP = std::stoi(parts[1]);
                int N = std::stoi(parts[2]);
                auto parseVal = [](std::string s) -> double {
                    for (char& c : s) if (c == 'D' || c == 'd') c = 'E';
                    return std::stod(s);
                };
                int stride = (IP == 1 ? 2 : (IP == 2 ? 3 : 6));
                size_t ptStart = (IP == 1 ? 4 : 3);
                float zT = (IP == 1 && parts.size() >= 4 ? (float)parseVal(parts[3]) : 0.0f);

                std::vector<Vec3> pts;
                for (int i = 0; i < N; ++i) {
                    size_t idx = ptStart + i * stride;
                    if (IP == 1 && idx + 1 < parts.size()) {
                        Vec3 pt = { (float)parseVal(parts[idx]), (float)parseVal(parts[idx + 1]), zT };
                        pts.push_back(pt);
                        updateBBox(pt);
                    } else if (idx + 2 < parts.size()) {
                        Vec3 pt = { (float)parseVal(parts[idx]), (float)parseVal(parts[idx + 1]), (float)parseVal(parts[idx + 2]) };
                        pts.push_back(pt);
                        updateBBox(pt);
                    }
                }
                for (size_t i = 0; i + 1 < pts.size(); ++i) {
                    segs.push_back({ pts[i], pts[i + 1] });
                }
            } catch (...) {}
        }
    }

    if (segs.empty()) return false;
    RibbonizeSegments(segs, tris, minB, maxB);
    return !tris.empty();
}

bool LoadBREP(const std::wstring& wpath, std::vector<Triangle>& tris, Vec3& minB, Vec3& maxB) {
    tris.clear();
    minB = {1e9f, 1e9f, 1e9f};
    maxB = {-1e9f, -1e9f, -1e9f};

    FILE* f = _wfopen(wpath.c_str(), L"r");
    if (!f) return false;

    std::vector<Vec3> verts;
    std::vector<LineSeg> segs;
    char line[256];
    bool inTShapes = false;
    int veCountDown = 0;

    auto updateBBox = [&](const Vec3& pt) {
        minB.x = std::min(minB.x, pt.x); minB.y = std::min(minB.y, pt.y); minB.z = std::min(minB.z, pt.z);
        maxB.x = std::max(maxB.x, pt.x); maxB.y = std::max(maxB.y, pt.y); maxB.z = std::max(maxB.z, pt.z);
    };

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "TShapes", 7) == 0) { inTShapes = true; continue; }
        if (inTShapes) {
            if (strncmp(line, "Ve", 2) == 0) {
                veCountDown = 2; // tolerance, then coords
                continue;
            }
            if (veCountDown > 0) {
                veCountDown--;
                if (veCountDown == 0) {
                    float x, y, z;
                    if (sscanf(line, "%f %f %f", &x, &y, &z) == 3) {
                        Vec3 pt = {x, y, z};
                        verts.push_back(pt);
                        updateBBox(pt);
                    }
                }
            }
        }
    }
    fclose(f);

    if (verts.empty()) return false;

    for (size_t i = 0; i + 1 < verts.size(); ++i) {
        segs.push_back({ verts[i], verts[i+1] });
    }

    if (segs.empty()) return false;
    RibbonizeSegments(segs, tris, minB, maxB);
    return !tris.empty();
}

bool SaveSTL(const std::wstring& wpath, const std::vector<Triangle>& tris) {
    if (tris.empty()) return false;
    FILE* f = _wfopen(wpath.c_str(), L"wb");
    if (!f) return false;
    char header[80] = "ModelPeek Native High-Speed STL Exporter";
    fwrite(header, 1, 80, f);
    uint32_t count = (uint32_t)tris.size();
    fwrite(&count, 4, 1, f);
    for (const auto& t : tris) {
        fwrite(&t.normal, 4, 3, f);
        fwrite(&t.v[0], 4, 3, f);
        fwrite(&t.v[1], 4, 3, f);
        fwrite(&t.v[2], 4, 3, f);
        uint16_t attr = 0;
        fwrite(&attr, 2, 1, f);
    }
    fclose(f);
    return true;
}

// 256x256 Soft Rasterizer for CAD Thumbnails
void RenderThumbnail(const std::vector<Triangle>& tris, const Vec3& minB, const Vec3& maxB, int W, int H, const std::wstring& outBmp) {
    std::vector<uint32_t> pixels(W * H, 0xFF1E222B); // CAD dark slate background
    std::vector<float> zbuffer(W * H, 1e9f);

    Vec3 center = (minB + maxB) * 0.5f;
    Vec3 size = maxB - minB;
    float maxDim = std::max({size.x, size.y, size.z});
    if (maxDim < 1e-3f) maxDim = 1.0f;

    float yaw = 45.0f * 3.14159265f / 180.0f;
    float pitch = 30.0f * 3.14159265f / 180.0f;

    float cosYaw = std::cos(yaw), sinYaw = std::sin(yaw);
    float cosPitch = std::cos(pitch), sinPitch = std::sin(pitch);

    Vec3 light1 = Vec3{0.6f, 0.8f, 0.5f}.normalize();
    Vec3 light2 = Vec3{-0.5f, 0.3f, -0.4f}.normalize();

    auto project = [&](const Vec3& pt, float& sx, float& sy, float& sz) {
        Vec3 p = pt - center;
        float x1 = p.x * cosYaw - p.z * sinYaw;
        float z1 = p.x * sinYaw + p.z * cosYaw;
        float y2 = p.y * cosPitch - z1 * sinPitch;
        float z2 = p.y * sinPitch + z1 * cosPitch;

        float scale = (float)(std::min(W, H)) * 0.42f / (maxDim * 0.5f);
        sx = W * 0.5f + x1 * scale;
        sy = H * 0.5f - y2 * scale;
        sz = z2;
    };

    auto edgeFunction = [](float ax, float ay, float bx, float by, float cx, float cy) {
        return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
    };

    for (const auto& tri : tris) {
        float sx[3], sy[3], sz[3];
        for (int j = 0; j < 3; ++j) {
            project(tri.v[j], sx[j], sy[j], sz[j]);
        }

        Vec3 n = tri.normal;
        float nx1 = n.x * cosYaw - n.z * sinYaw;
        float nz1 = n.x * sinYaw + n.z * cosYaw;
        float ny2 = n.y * cosPitch - nz1 * sinPitch;
        float nz2 = n.y * sinPitch + nz1 * cosPitch;
        Vec3 rotN = Vec3{nx1, ny2, nz2}.normalize();

        float diff1 = std::max(0.0f, rotN.dot(light1));
        float diff2 = std::max(0.0f, rotN.dot(light2)) * 0.4f;
        float ambient = 0.25f;
        float intensity = std::min(1.0f, ambient + diff1 * 0.65f + diff2);

        uint8_t r = (uint8_t)std::clamp((int)(144.0f * intensity + 20.0f), 0, 255);
        uint8_t g = (uint8_t)std::clamp((int)(164.0f * intensity + 25.0f), 0, 255);
        uint8_t b = (uint8_t)std::clamp((int)(174.0f * intensity + 30.0f), 0, 255);
        uint32_t color = 0xFF000000 | (r << 16) | (g << 8) | b;

        int minX = std::max(0, (int)std::floor(std::min({sx[0], sx[1], sx[2]})));
        int maxX = std::min(W - 1, (int)std::ceil(std::max({sx[0], sx[1], sx[2]})));
        int minY = std::max(0, (int)std::floor(std::min({sy[0], sy[1], sy[2]})));
        int maxY = std::min(H - 1, (int)std::ceil(std::max({sy[0], sy[1], sy[2]})));

        float area = edgeFunction(sx[0], sy[0], sx[1], sy[1], sx[2], sy[2]);
        if (std::abs(area) < 1e-4f) continue;
        float invArea = 1.0f / area;

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                float w0 = edgeFunction(sx[1], sy[1], sx[2], sy[2], px, py) * invArea;
                float w1 = edgeFunction(sx[2], sy[2], sx[0], sy[0], px, py) * invArea;
                float w2 = edgeFunction(sx[0], sy[0], sx[1], sy[1], px, py) * invArea;

                if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                    float z = w0 * sz[0] + w1 * sz[1] + w2 * sz[2];
                    int idx = y * W + x;
                    if (z < zbuffer[idx]) {
                        zbuffer[idx] = z;
                        pixels[idx] = color;
                    }
                }
            }
        }
    }

    BITMAPFILEHEADER bfh = {0};
    BITMAPINFOHEADER bih = {0};
    bfh.bfType = 0x4D42;
    bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + W * H * 4;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = W;
    bih.biHeight = -H;
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;

    FILE* out = _wfopen(outBmp.c_str(), L"wb");
    if (out) {
        fwrite(&bfh, sizeof(bfh), 1, out);
        fwrite(&bih, sizeof(bih), 1, out);
        fwrite(pixels.data(), 4, pixels.size(), out);
        fclose(out);
    }
}

std::wstring FindOCCPython() {
    WCHAR selfPath[MAX_PATH];
    GetModuleFileNameW(NULL, selfPath, MAX_PATH);
    PathRemoveFileSpecW(selfPath);

    std::wstring pEmbedded = std::wstring(selfPath) + L"\\python\\python.exe";
    if (PathFileExistsW(pEmbedded.c_str())) return pEmbedded;

    std::wstring pSameDir = std::wstring(selfPath) + L"\\python.exe";
    if (PathFileExistsW(pSameDir.c_str())) return pSameDir;

    std::wstring p1 = std::wstring(selfPath) + L"\\occt\\python.exe";
    if (PathFileExistsW(p1.c_str())) return p1;

    std::wstring p2 = L"D:\\SoftWare2\\FreeCAD 1.1\\bin\\python.exe";
    if (PathFileExistsW(p2.c_str())) return p2;

    return L"python.exe";
}

std::wstring GetCadProcessorScript() {
    WCHAR selfPath[MAX_PATH];
    GetModuleFileNameW(NULL, selfPath, MAX_PATH);
    PathRemoveFileSpecW(selfPath);

    std::wstring s1 = std::wstring(selfPath) + L"\\cad_processor.py";
    if (PathFileExistsW(s1.c_str())) return s1;

    std::wstring s2 = std::wstring(selfPath) + L"\\..\\src\\worker\\cad_processor.py";
    return s2;
}

bool RunCadConverter(const std::wstring& inputStep, const std::wstring& outputStl) {
    std::wstring pythonExe = FindOCCPython();
    std::wstring script = GetCadProcessorScript();

    WCHAR fullIn[MAX_PATH] = {0};
    WCHAR fullOut[MAX_PATH] = {0};
    GetFullPathNameW(inputStep.c_str(), MAX_PATH, fullIn, NULL);
    GetFullPathNameW(outputStl.c_str(), MAX_PATH, fullOut, NULL);

    std::wstringstream cmd;
    cmd << L"\"" << pythonExe << L"\" \"" << script << L"\" convert \"" 
        << fullIn << L"\" \"" << fullOut << L"\"";

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::wstring cmdStr = cmd.str();
    std::vector<WCHAR> cmdLine(cmdStr.begin(), cmdStr.end());
    cmdLine.push_back(L'\0');

    if (!CreateProcessW(NULL, cmdLine.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        std::wcerr << L"CreateProcessW FAILED, err=" << GetLastError() << L" cmd=" << cmdStr << std::endl;
        return false;
    }

    WaitForSingleObject(pi.hProcess, 15000);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0) {
        std::wcerr << L"Python process failed with exitCode=" << exitCode << L" cmd=" << cmdStr << std::endl;
    }

    return PathFileExistsW(fullOut) == TRUE;
}

bool Process3DThumbnail(const std::wstring& input, const std::wstring& output, int size) {
    std::wstring ext;
    size_t dot = input.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        ext = input.substr(dot);
        for (auto& c : ext) c = towlower(c);
    }

    std::vector<Triangle> tris;
    Vec3 minB, maxB;
    bool loaded = false;

    // 1. Fast Native C++ loaders
    if (ext == L".stl") {
        loaded = LoadSTL(input, tris, minB, maxB);
    } else if (ext == L".obj") {
        loaded = LoadOBJ(input, tris, minB, maxB);
    } else if (ext == L".ply") {
        loaded = LoadPLY(input, tris, minB, maxB);
    } else if (ext == L".dxf") {
        loaded = LoadDXF(input, tris, minB, maxB);
    } else if (ext == L".pcd") {
        loaded = LoadPCD(input, tris, minB, maxB);
    } else if (ext == L".gcode") {
        loaded = LoadGCode(input, tris, minB, maxB);
    } else if (ext == L".3ds") {
        loaded = Load3DS(input, tris, minB, maxB);
    } else if (ext == L".step" || ext == L".stp") {
        loaded = LoadSTEP(input, tris, minB, maxB);
    } else if (ext == L".iges" || ext == L".igs") {
        loaded = LoadIGES(input, tris, minB, maxB);
    } else if (ext == L".brep" || ext == L".brp") {
        loaded = LoadBREP(input, tris, minB, maxB);
    }

    // 2. Extended formats conversion fallback (.step, .stp, .iges, .igs, .brep, .brp, .gltf, .glb, .3mf, .dae, .fbx, etc.)
    if (!loaded) {
        std::wstring tempStl = output + L".tmp.stl";
        if (RunCadConverter(input, tempStl) && PathFileExistsW(tempStl.c_str())) {
            loaded = LoadSTL(tempStl, tris, minB, maxB);
            DeleteFileW(tempStl.c_str());
        }
    }

    if (!loaded || tris.empty()) {
        return false;
    }

    RenderThumbnail(tris, minB, maxB, size, size, output);
    return PathFileExistsW(output.c_str()) == TRUE;
}
inline bool Convert3DCadToStl(const std::wstring& input, const std::wstring& output) {
    std::wstring ext;
    size_t dot = input.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        ext = input.substr(dot);
        for (auto& c : ext) c = towlower(c);
    }
    bool converted = false;
    std::vector<Triangle> tris;
    Vec3 minB, maxB;
    if (ext == L".step" || ext == L".stp") {
        if (LoadSTEP(input, tris, minB, maxB) && SaveSTL(output, tris)) converted = true;
    } else if (ext == L".iges" || ext == L".igs") {
        if (LoadIGES(input, tris, minB, maxB) && SaveSTL(output, tris)) converted = true;
    } else if (ext == L".brep" || ext == L".brp") {
        if (LoadBREP(input, tris, minB, maxB) && SaveSTL(output, tris)) converted = true;
    }
    if (!converted) {
        converted = RunCadConverter(input, output);
    }
    return converted && (PathFileExistsW(output.c_str()) == TRUE);
}