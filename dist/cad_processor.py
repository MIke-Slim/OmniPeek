import sys
import os
import json
import math
import struct
import base64
import zipfile
import re
import xml.etree.ElementTree as ET

# Attempt to load FreeCAD / OpenCASCADE modules
for p in [r"D:\SoftWare2\FreeCAD 1.1\bin", r"D:\SoftWare2\FreeCAD 1.1\lib",
          os.path.join(os.path.dirname(__file__), "..", "occt", "bin"),
          os.path.join(os.path.dirname(__file__), "..", "occt", "lib")]:
    if os.path.exists(p) and p not in sys.path:
        sys.path.insert(0, p)

HAS_FREECAD = False
try:
    import FreeCAD
    import Part
    HAS_FREECAD = True
except ImportError:
    HAS_FREECAD = False

def _normalize(v):
    l = math.sqrt(v[0]**2 + v[1]**2 + v[2]**2)
    if l < 1e-9:
        return (0.0, 0.0, 1.0)
    return (v[0]/l, v[1]/l, v[2]/l)

def _cross(a, b):
    return (
        a[1]*b[2] - a[2]*b[1],
        a[2]*b[0] - a[0]*b[2],
        a[0]*b[1] - a[1]*b[0]
    )

def _sub(a, b):
    return (a[0]-b[0], a[1]-b[1], a[2]-b[2])

def _add(a, b):
    return (a[0]+b[0], a[1]+b[1], a[2]+b[2])

def _mul(a, s):
    return (a[0]*s, a[1]*s, a[2]*s)

def _get_perp_vectors(dir_vec):
    d = _normalize(dir_vec)
    if abs(d[0]) < 0.9 and abs(d[1]) < 0.9:
        ref = (0.0, 0.0, 1.0)
    else:
        ref = (0.0, 1.0, 0.0)
    u1 = _normalize(_cross(d, ref))
    u2 = _cross(d, u1)
    return u1, u2

def calc_normal(p1, p2, p3):
    u = (p2[0]-p1[0], p2[1]-p1[1], p2[2]-p1[2])
    v = (p3[0]-p1[0], p3[1]-p1[1], p3[2]-p1[2])
    nx = u[1]*v[2] - u[2]*v[1]
    ny = u[2]*v[0] - u[0]*v[2]
    nz = u[0]*v[1] - u[1]*v[0]
    l = math.sqrt(nx*nx + ny*ny + nz*nz)
    if l < 1e-6:
        return (0.0, 0.0, 1.0)
    return (nx/l, ny/l, nz/l)

def write_stl(out_path, triangles):
    if not triangles:
        return False
    with open(out_path, 'wb') as f:
        header = b"ModelPeek Multi-Format STL Converter"
        f.write(header.ljust(80, b'\0'))
        f.write(struct.pack('<I', len(triangles)))
        for norm, p1, p2, p3 in triangles:
            f.write(struct.pack('<3f', norm[0], norm[1], norm[2]))
            f.write(struct.pack('<3f', p1[0], p1[1], p1[2]))
            f.write(struct.pack('<3f', p2[0], p2[1], p2[2]))
            f.write(struct.pack('<3f', p3[0], p3[1], p3[2]))
            f.write(struct.pack('<H', 0))
    return os.path.exists(out_path) and os.path.getsize(out_path) > 84

def edges_to_stl(edges, bbox, out_stl_path, sides=6):
    max_dim = max(bbox.XLength, bbox.YLength, bbox.ZLength)
    if max_dim <= 1e-4:
        max_dim = 10.0
    radius = max(max_dim * 0.005, 0.04)

    triangles = []
    for edge in edges:
        if edge.Length <= 1e-6:
            continue
        n_pts = max(8, min(60, int(edge.Length / (max_dim * 0.02 + 0.05))))
        fc_pts = edge.discretize(Number=n_pts)
        pts = [(p.x, p.y, p.z) for p in fc_pts]
        if len(pts) < 2:
            continue

        rings = []
        for i in range(len(pts)):
            p = pts[i]
            if i == 0:
                tangent = _sub(pts[1], pts[0])
            elif i == len(pts) - 1:
                tangent = _sub(pts[-1], pts[-2])
            else:
                tangent = _sub(pts[i+1], pts[i-1])
            u1, u2 = _get_perp_vectors(tangent)
            ring = []
            for s in range(sides):
                angle = 2.0 * math.pi * s / sides
                offset = _add(_mul(u1, radius * math.cos(angle)), _mul(u2, radius * math.sin(angle)))
                ring.append(_add(p, offset))
            rings.append(ring)

        for i in range(len(rings) - 1):
            r1 = rings[i]
            r2 = rings[i+1]
            for s in range(sides):
                next_s = (s + 1) % sides
                p1 = r1[s]
                p2 = r1[next_s]
                p3 = r2[next_s]
                p4 = r2[s]
                norm1 = _normalize(_cross(_sub(p2, p1), _sub(p3, p1)))
                triangles.append((norm1, p1, p2, p3))
                norm2 = _normalize(_cross(_sub(p3, p1), _sub(p4, p1)))
                triangles.append((norm2, p1, p3, p4))

        # End caps
        start_center = pts[0]
        start_norm = _normalize(_sub(pts[0], pts[1]))
        r_start = rings[0]
        for s in range(sides):
            next_s = (s + 1) % sides
            triangles.append((start_norm, start_center, r_start[next_s], r_start[s]))

        end_center = pts[-1]
        end_norm = _normalize(_sub(pts[-1], pts[-2]))
        r_end = rings[-1]
        for s in range(sides):
            next_s = (s + 1) % sides
            triangles.append((end_norm, end_center, r_end[s], r_end[next_s]))

    return write_stl(out_stl_path, triangles)

def convert_cad_fallback_to_stl(cad_path, out_stl_path):
    ext = os.path.splitext(cad_path)[1].lower()
    triangles = []
    segs = []

    if ext in ['.step', '.stp']:
        with open(cad_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        points = {}
        vertex_to_point = {}
        vertices = {}
        edge_curves = {}
        oriented_edges = {}
        edge_loops_map = {}

        for stmt in content.split(';'):
            h = stmt.find('#')
            if h == -1: continue
            eq = stmt.find('=', h)
            if eq == -1: continue
            id_str = stmt[h+1:eq].strip()
            if not id_str.isdigit(): continue
            eid = int(id_str)

            paren = stmt.find('(', eq)
            if paren == -1: continue
            etype = stmt[eq+1:paren].strip()

            if etype == 'CARTESIAN_POINT':
                inner_open = stmt.find('(', paren + 1)
                inner_close = stmt.find(')', inner_open if inner_open != -1 else paren)
                if inner_open != -1 and inner_close != -1:
                    coords = [float(x.strip()) for x in stmt[inner_open+1:inner_close].split(',') if x.strip()]
                    if len(coords) == 3:
                        points[eid] = tuple(coords)
            elif etype == 'VERTEX_POINT':
                p_hash = stmt.find('#', paren)
                if p_hash != -1:
                    digits = re.findall(r'\d+', stmt[p_hash:])
                    if digits: vertex_to_point[eid] = int(digits[0])
            elif etype == 'EDGE_CURVE':
                refs = [int(x) for x in re.findall(r'#(\d+)', stmt[paren:])]
                if len(refs) >= 2: edge_curves[eid] = (refs[0], refs[1])
            elif etype == 'ORIENTED_EDGE':
                refs = [int(x) for x in re.findall(r'#(\d+)', stmt[paren:])]
                if refs: oriented_edges[eid] = (refs[-1], '.T.' in stmt)
            elif etype == 'EDGE_LOOP':
                refs = [int(x) for x in re.findall(r'#(\d+)', stmt[paren:])]
                if refs: edge_loops_map[eid] = refs
            elif etype == 'POLY_LOOP':
                refs = [int(x) for x in re.findall(r'#(\d+)', stmt[paren:])]
                pl_pts = [points[p] for p in refs if p in points]
                if len(pl_pts) >= 3:
                    p0 = pl_pts[0]
                    for i in range(1, len(pl_pts) - 1):
                        p1, p2 = pl_pts[i], pl_pts[i+1]
                        triangles.append((calc_normal(p0, p1, p2), p0, p1, p2))

        for vid, pid in vertex_to_point.items():
            if pid in points: vertices[vid] = points[pid]

        for eid, (v1, v2) in edge_curves.items():
            if v1 in vertices and v2 in vertices:
                segs.append((vertices[v1], vertices[v2]))

        for lid, oe_list in edge_loops_map.items():
            loop_pts = []
            for oe in oe_list:
                if oe in oriented_edges:
                    ec_id, sense = oriented_edges[oe]
                    if ec_id in edge_curves:
                        v1, v2 = edge_curves[ec_id]
                        v = v1 if sense else v2
                        if v in vertices: loop_pts.append(vertices[v])
            if len(loop_pts) >= 3:
                p0 = loop_pts[0]
                for i in range(1, len(loop_pts) - 1):
                    p1, p2 = loop_pts[i], loop_pts[i+1]
                    triangles.append((calc_normal(p0, p1, p2), p0, p1, p2))

    elif ext in ['.iges', '.igs']:
        with open(cad_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
        p_lines = [l[:64] for l in lines if len(l) >= 73 and l[72] == 'P']
        p_text = ''.join(p_lines)
        for stmt in p_text.split(';'):
            parts = [p.strip() for p in stmt.split(',') if p.strip()]
            if not parts: continue
            if parts[0] == '110' and len(parts) >= 7:
                try:
                    p1 = (float(parts[1]), float(parts[2]), float(parts[3]))
                    p2 = (float(parts[4]), float(parts[5]), float(parts[6]))
                    segs.append((p1, p2))
                except: pass
            elif parts[0] == '100' and len(parts) >= 8:
                try:
                    zt = float(parts[1])
                    xc, yc = float(parts[2]), float(parts[3])
                    xs, ys = float(parts[4]), float(parts[5])
                    xe, ye = float(parts[6]), float(parts[7])
                    r = math.sqrt((xs-xc)**2 + (ys-yc)**2)
                    a1 = math.atan2(ys-yc, xs-xc)
                    a2 = math.atan2(ye-yc, xe-xc)
                    if a2 <= a1: a2 += 2.0 * math.pi
                    prev = (xs, ys, zt)
                    for s in range(1, 17):
                        a = a1 + (a2 - a1) * (s / 16.0)
                        cur = (xc + r * math.cos(a), yc + r * math.sin(a), zt)
                        segs.append((prev, cur))
                        prev = cur
                except: pass

    if segs and len(triangles) < 12:
        all_x = [s[0][0] for s in segs] + [s[1][0] for s in segs]
        all_y = [s[0][1] for s in segs] + [s[1][1] for s in segs]
        all_z = [s[0][2] for s in segs] + [s[1][2] for s in segs]
        max_dim = max(max(all_x)-min(all_x), max(all_y)-min(all_y), max(all_z)-min(all_z), 1.0)
        r = max(max_dim * 0.005, 0.05)
        for p1, p2 in segs:
            dx, dy, dz = p2[0]-p1[0], p2[1]-p1[1], p2[2]-p1[2]
            l = math.sqrt(dx*dx + dy*dy + dz*dz)
            if l < 1e-5: continue
            dir_v = (dx/l, dy/l, dz/l)
            ref = (0.0, 0.0, 1.0) if (abs(dir_v[0]) < 0.9 and abs(dir_v[1]) < 0.9) else (0.0, 1.0, 0.0)
            u1x, u1y, u1z = dir_v[1]*ref[2]-dir_v[2]*ref[1], dir_v[2]*ref[0]-dir_v[0]*ref[2], dir_v[0]*ref[1]-dir_v[1]*ref[0]
            l1 = math.sqrt(u1x*u1x + u1y*u1y + u1z*u1z)
            if l1 < 1e-6: continue
            u1 = (u1x/l1 * r, u1y/l1 * r, u1z/l1 * r)
            q1a = (p1[0]-u1[0], p1[1]-u1[1], p1[2]-u1[2])
            q1b = (p1[0]+u1[0], p1[1]+u1[1], p1[2]+u1[2])
            q1c = (p2[0]+u1[0], p2[1]+u1[1], p2[2]+u1[2])
            q1d = (p2[0]-u1[0], p2[1]-u1[1], p2[2]-u1[2])
            n1 = calc_normal(q1a, q1b, q1c)
            triangles.append((n1, q1a, q1b, q1c))
            triangles.append((n1, q1a, q1c, q1d))

    if triangles:
        return write_stl(out_stl_path, triangles)
    return False

def convert_cad_to_stl(cad_path, out_stl_path, deflection=0.1):
    if HAS_FREECAD:
        try:
            shape = Part.Shape()
            shape.read(cad_path)
            if len(shape.Faces) > 0:
                shape.exportStl(out_stl_path)
                if os.path.exists(out_stl_path) and os.path.getsize(out_stl_path) > 0:
                    return True
            if len(shape.Edges) > 0:
                if edges_to_stl(shape.Edges, shape.BoundBox, out_stl_path):
                    return True
        except Exception as e:
            print(f"WARN: FreeCAD conversion failed: {e}", file=sys.stderr)

    return convert_cad_fallback_to_stl(cad_path, out_stl_path)

# 1. 3MF Converter
def convert_3mf_to_stl(in_path, out_stl_path):
    triangles = []
    with zipfile.ZipFile(in_path, 'r') as z:
        for name in z.namelist():
            if name.endswith('.model'):
                root = ET.fromstring(z.read(name))
                verts = []
                for v in root.findall('.//{*}vertex'):
                    verts.append((float(v.attrib['x']), float(v.attrib['y']), float(v.attrib['z'])))
                for t in root.findall('.//{*}triangle'):
                    v1, v2, v3 = int(t.attrib['v1']), int(t.attrib['v2']), int(t.attrib['v3'])
                    if v1 < len(verts) and v2 < len(verts) and v3 < len(verts):
                        p1, p2, p3 = verts[v1], verts[v2], verts[v3]
                        triangles.append((calc_normal(p1, p2, p3), p1, p2, p3))
    return write_stl(out_stl_path, triangles)

# 2. GLTF / GLB Converter
def convert_gltf_glb_to_stl(in_path, out_stl_path):
    triangles = []
    is_glb = in_path.lower().endswith('.glb')
    if is_glb:
        with open(in_path, 'rb') as f:
            f.seek(12)
            jlen, _ = struct.unpack('<II', f.read(8))
            jdata = json.loads(f.read(jlen).decode('utf-8'))
            blen, _ = struct.unpack('<II', f.read(8))
            bdata = f.read(blen)
            get_buf = lambda idx: bdata
    else:
        with open(in_path, 'r', encoding='utf-8') as f:
            jdata = json.load(f)
        base_dir = os.path.dirname(in_path)
        buffers = []
        for b in jdata.get('buffers', []):
            uri = b.get('uri', '')
            if uri.startswith('data:'):
                buffers.append(base64.b64decode(uri.split(',', 1)[1]))
            else:
                bin_path = os.path.join(base_dir, uri)
                if os.path.exists(bin_path):
                    with open(bin_path, 'rb') as bf:
                        buffers.append(bf.read())
                else:
                    buffers.append(b'')
        get_buf = lambda idx: buffers[idx] if idx < len(buffers) else b''

    accessors = jdata.get('accessors', [])
    bufferViews = jdata.get('bufferViews', [])
    for mesh in jdata.get('meshes', []):
        for prim in mesh.get('primitives', []):
            pos_acc_idx = prim.get('attributes', {}).get('POSITION')
            if pos_acc_idx is None:
                continue
            pos_acc = accessors[pos_acc_idx]
            pos_bv = bufferViews[pos_acc['bufferView']]
            pos_buf = get_buf(pos_bv.get('buffer', 0))
            pos_offset = pos_bv.get('byteOffset', 0) + pos_acc.get('byteOffset', 0)
            verts = []
            for i in range(pos_acc['count']):
                verts.append(struct.unpack_from('<3f', pos_buf, pos_offset + i * 12))

            idx_acc_idx = prim.get('indices')
            if idx_acc_idx is not None:
                idx_acc = accessors[idx_acc_idx]
                idx_bv = bufferViews[idx_acc['bufferView']]
                idx_buf = get_buf(idx_bv.get('buffer', 0))
                idx_offset = idx_bv.get('byteOffset', 0) + idx_acc.get('byteOffset', 0)
                ctype = idx_acc['componentType']
                icount = idx_acc['count']
                indices = []
                fmt = '<H' if ctype == 5123 else ('<I' if ctype == 5125 else '<B')
                step = 2 if ctype == 5123 else (4 if ctype == 5125 else 1)
                for i in range(icount):
                    val, = struct.unpack_from(fmt, idx_buf, idx_offset + i * step)
                    indices.append(val)
                for i in range(0, len(indices), 3):
                    p1, p2, p3 = verts[indices[i]], verts[indices[i+1]], verts[indices[i+2]]
                    triangles.append((calc_normal(p1, p2, p3), p1, p2, p3))
            else:
                for i in range(0, len(verts), 3):
                    p1, p2, p3 = verts[i], verts[i+1], verts[i+2]
                    triangles.append((calc_normal(p1, p2, p3), p1, p2, p3))

    return write_stl(out_stl_path, triangles)

# 3. Collada (.dae) Converter
def convert_dae_to_stl(in_path, out_stl_path):
    root = ET.parse(in_path).getroot()
    ns = {'c': 'http://www.collada.org/2005/11/COLLADASchema'}
    triangles = []
    for mesh in root.findall('.//c:mesh', ns):
        positions = {}
        for src in mesh.findall('c:source', ns):
            sid = src.attrib.get('id', '')
            fa = src.find('c:float_array', ns)
            if fa is not None and fa.text:
                fl = [float(x) for x in fa.text.split()]
                positions[sid] = [(fl[i], fl[i+1], fl[i+2]) for i in range(0, len(fl), 3)]
        pos_id = None
        vn = mesh.find('c:vertices', ns)
        if vn is not None:
            for inp in vn.findall('c:input', ns):
                if inp.attrib.get('semantic') == 'POSITION':
                    pos_id = inp.attrib.get('source', '').lstrip('#')
        verts = positions.get(pos_id, [])
        for prim in mesh.findall('c:triangles', ns) + mesh.findall('c:polylist', ns):
            inputs = prim.findall('c:input', ns)
            stride = max([int(inp.attrib.get('offset', 0)) for inp in inputs], default=0) + 1
            pos_offset = 0
            for inp in inputs:
                if inp.attrib.get('semantic') in ('VERTEX', 'POSITION'):
                    pos_offset = int(inp.attrib.get('offset', 0))
            p_elem = prim.find('c:p', ns)
            if p_elem is not None and p_elem.text:
                idx = [int(x) for x in p_elem.text.split()]
                vcount = prim.find('c:vcount', ns)
                if vcount is not None and vcount.text:
                    counts = [int(x) for x in vcount.text.split()]
                    curr = 0
                    for c in counts:
                        poly = [idx[curr + j * stride + pos_offset] for j in range(c)]
                        for j in range(1, c - 1):
                            p1, p2, p3 = verts[poly[0]], verts[poly[j]], verts[poly[j+1]]
                            triangles.append((calc_normal(p1, p2, p3), p1, p2, p3))
                        curr += c * stride
                else:
                    for i in range(0, len(idx), 3 * stride):
                        p1 = verts[idx[i + pos_offset]]
                        p2 = verts[idx[i + stride + pos_offset]]
                        p3 = verts[idx[i + 2 * stride + pos_offset]]
                        triangles.append((calc_normal(p1, p2, p3), p1, p2, p3))
    return write_stl(out_stl_path, triangles)

# 4. 3DS (.3ds) Converter
def convert_3ds_to_stl(in_path, out_stl_path):
    with open(in_path, 'rb') as f:
        data = f.read()
    triangles = []
    def parse_chunks(offset, end):
        cur_verts = []
        while offset < end:
            if offset + 6 > len(data):
                break
            cid, clen = struct.unpack('<HI', data[offset:offset+6])
            if clen < 6:
                break
            c_end = min(offset + clen, end)
            if cid in (0x4D4D, 0x3D3D, 0x4100):
                parse_chunks(offset + 6, c_end)
            elif cid == 0x4000:
                p = offset + 6
                while p < c_end and data[p] != 0:
                    p += 1
                parse_chunks(p + 1, c_end)
            elif cid == 0x4110:
                nv, = struct.unpack('<H', data[offset+6:offset+8])
                cur_verts = [struct.unpack('<3f', data[offset+8+i*12:offset+8+(i+1)*12]) for i in range(nv)]
            elif cid == 0x4120:
                nf, = struct.unpack('<H', data[offset+6:offset+8])
                for i in range(nf):
                    a, b, c, _ = struct.unpack('<4H', data[offset+8+i*8:offset+8+(i+1)*8])
                    if a < len(cur_verts) and b < len(cur_verts) and c < len(cur_verts):
                        p1, p2, p3 = cur_verts[a], cur_verts[b], cur_verts[c]
                        triangles.append((calc_normal(p1, p2, p3), p1, p2, p3))
            offset += clen
    parse_chunks(0, len(data))
    return write_stl(out_stl_path, triangles)

# 5. G-Code (.gcode) Converter
def convert_gcode_to_stl(in_path, out_stl_path):
    segs = []
    cur_x, cur_y, cur_z = 0.0, 0.0, 0.0
    has_pos = False
    with open(in_path, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith(';') or line.startswith('('):
                continue
            line = line.split(';')[0].strip()
            if line.startswith('G0') or line.startswith('G1'):
                parts = line.split()
                nx, ny, nz = cur_x, cur_y, cur_z
                moved = False
                for p in parts[1:]:
                    if p.startswith('X'): nx = float(p[1:]); moved = True
                    elif p.startswith('Y'): ny = float(p[1:]); moved = True
                    elif p.startswith('Z'): nz = float(p[1:]); moved = True
                if moved:
                    if has_pos:
                        dx, dy, dz = nx - cur_x, ny - cur_y, nz - cur_z
                        dist = math.sqrt(dx*dx + dy*dy + dz*dz)
                        if dist > 0.05:
                            segs.append(((cur_x, cur_y, cur_z), (nx, ny, nz)))
                    has_pos = True
                    cur_x, cur_y, cur_z = nx, ny, nz
    if not segs:
        return False

    all_x = [s[0][0] for s in segs] + [s[1][0] for s in segs]
    all_y = [s[0][1] for s in segs] + [s[1][1] for s in segs]
    all_z = [s[0][2] for s in segs] + [s[1][2] for s in segs]
    max_dim = max(max(all_x)-min(all_x), max(all_y)-min(all_y), max(all_z)-min(all_z), 1.0)
    r = max(max_dim * 0.004, 0.08)

    triangles = []
    for p1, p2 in segs:
        dx, dy, dz = p2[0]-p1[0], p2[1]-p1[1], p2[2]-p1[2]
        l = math.sqrt(dx*dx + dy*dy + dz*dz)
        if l < 1e-5: continue
        dir_v = (dx/l, dy/l, dz/l)
        ref = (0.0, 0.0, 1.0) if (abs(dir_v[0]) < 0.9 and abs(dir_v[1]) < 0.9) else (0.0, 1.0, 0.0)
        u1x, u1y, u1z = dir_v[1]*ref[2]-dir_v[2]*ref[1], dir_v[2]*ref[0]-dir_v[0]*ref[2], dir_v[0]*ref[1]-dir_v[1]*ref[0]
        l1 = math.sqrt(u1x*u1x + u1y*u1y + u1z*u1z)
        if l1 < 1e-6: continue
        u1 = (u1x/l1 * r, u1y/l1 * r, u1z/l1 * r)
        u2 = ((dir_v[1]*u1[2]-dir_v[2]*u1[1]), (dir_v[2]*u1[0]-dir_v[0]*u1[2]), (dir_v[0]*u1[1]-dir_v[1]*u1[0]))

        q1a = (p1[0]-u1[0], p1[1]-u1[1], p1[2]-u1[2])
        q1b = (p1[0]+u1[0], p1[1]+u1[1], p1[2]+u1[2])
        q1c = (p2[0]+u1[0], p2[1]+u1[1], p2[2]+u1[2])
        q1d = (p2[0]-u1[0], p2[1]-u1[1], p2[2]-u1[2])
        n1 = calc_normal(q1a, q1b, q1c)
        triangles.append((n1, q1a, q1b, q1c))
        triangles.append((n1, q1a, q1c, q1d))

        q2a = (p1[0]-u2[0], p1[1]-u2[1], p1[2]-u2[2])
        q2b = (p1[0]+u2[0], p1[1]+u2[1], p1[2]+u2[2])
        q2c = (p2[0]+u2[0], p2[1]+u2[1], p2[2]+u2[2])
        q2d = (p2[0]-u2[0], p2[1]-u2[1], p2[2]-u2[2])
        n2 = calc_normal(q2a, q2b, q2c)
        triangles.append((n2, q2a, q2b, q2c))
        triangles.append((n2, q2a, q2c, q2d))

    return write_stl(out_stl_path, triangles)

# 6. FBX (.fbx) Converter
def convert_fbx_to_stl(in_path, out_stl_path):
    with open(in_path, 'r', encoding='utf-8', errors='ignore') as f:
        text = f.read()

    triangles = []
    # 6.1 Check polygonal mesh (Vertices + PolygonVertexIndex)
    verts_m = re.search(r'Vertices:\s*\*(\d+)\s*\{\s*a:\s*([^}]+)\}', text)
    poly_m = re.search(r'PolygonVertexIndex:\s*\*(\d+)\s*\{\s*a:\s*([^}]+)\}', text)
    if verts_m and poly_m:
        c_raw = [float(x) for x in verts_m.group(2).split(',') if x.strip()]
        verts = [(c_raw[i], c_raw[i+1], c_raw[i+2]) for i in range(0, len(c_raw), 3)]
        idx_raw = [int(x) for x in poly_m.group(2).split(',') if x.strip()]
        poly = []
        for raw_i in idx_raw:
            if raw_i < 0:
                poly.append(~raw_i)
                for j in range(1, len(poly) - 1):
                    p1, p2, p3 = verts[poly[0]], verts[poly[j]], verts[poly[j+1]]
                    triangles.append((calc_normal(p1, p2, p3), p1, p2, p3))
                poly = []
            else:
                poly.append(raw_i)

    # 6.2 Check NURBS Curves (Points sections)
    if not triangles:
        points_all = re.findall(r'Points:\s*\*(\d+)\s*\{\s*a:\s*([^}]+)\}', text)
        segs = []
        for count_str, raw in points_all:
            pts_raw = [float(x) for x in raw.replace('\n', '').split(',') if x.strip()]
            pts = []
            stride = 4 if len(pts_raw) % 4 == 0 else 3
            for i in range(0, len(pts_raw), stride):
                w = pts_raw[i+3] if stride == 4 and abs(pts_raw[i+3]) > 1e-4 else 1.0
                pts.append((pts_raw[i]/w, pts_raw[i+1]/w, pts_raw[i+2]/w))
            for i in range(len(pts) - 1):
                segs.append((pts[i], pts[i+1]))

        if segs:
            all_x = [s[0][0] for s in segs] + [s[1][0] for s in segs]
            all_y = [s[0][1] for s in segs] + [s[1][1] for s in segs]
            all_z = [s[0][2] for s in segs] + [s[1][2] for s in segs]
            max_dim = max(max(all_x)-min(all_x), max(all_y)-min(all_y), max(all_z)-min(all_z), 1.0)
            r = max(max_dim * 0.005, 0.05)
            for p1, p2 in segs:
                dx, dy, dz = p2[0]-p1[0], p2[1]-p1[1], p2[2]-p1[2]
                l = math.sqrt(dx*dx + dy*dy + dz*dz)
                if l < 1e-5: continue
                dir_v = (dx/l, dy/l, dz/l)
                ref = (0.0, 0.0, 1.0) if (abs(dir_v[0]) < 0.9 and abs(dir_v[1]) < 0.9) else (0.0, 1.0, 0.0)
                u1x, u1y, u1z = dir_v[1]*ref[2]-dir_v[2]*ref[1], dir_v[2]*ref[0]-dir_v[0]*ref[2], dir_v[0]*ref[1]-dir_v[1]*ref[0]
                l1 = math.sqrt(u1x*u1x + u1y*u1y + u1z*u1z)
                if l1 < 1e-6: continue
                u1 = (u1x/l1 * r, u1y/l1 * r, u1z/l1 * r)
                u2 = ((dir_v[1]*u1[2]-dir_v[2]*u1[1]), (dir_v[2]*u1[0]-dir_v[0]*u1[2]), (dir_v[0]*u1[1]-dir_v[1]*u1[0]))
                q1a = (p1[0]-u1[0], p1[1]-u1[1], p1[2]-u1[2])
                q1b = (p1[0]+u1[0], p1[1]+u1[1], p1[2]+u1[2])
                q1c = (p2[0]+u1[0], p2[1]+u1[1], p2[2]+u1[2])
                q1d = (p2[0]-u1[0], p2[1]-u1[1], p2[2]-u1[2])
                n1 = calc_normal(q1a, q1b, q1c)
                triangles.append((n1, q1a, q1b, q1c))
                triangles.append((n1, q1a, q1c, q1d))

    return write_stl(out_stl_path, triangles)

def convert_any_to_stl(input_file, out_file):
    ext = os.path.splitext(input_file)[1].lower()
    if ext in [".step", ".stp", ".iges", ".igs", ".brep", ".brp"]:
        return convert_cad_to_stl(input_file, out_file)
    elif ext in [".gltf", ".glb"]:
        return convert_gltf_glb_to_stl(input_file, out_file)
    elif ext == ".3mf":
        return convert_3mf_to_stl(input_file, out_file)
    elif ext == ".dae":
        return convert_dae_to_stl(input_file, out_file)
    elif ext == ".3ds":
        return convert_3ds_to_stl(input_file, out_file)
    elif ext == ".gcode":
        return convert_gcode_to_stl(input_file, out_file)
    elif ext == ".fbx":
        return convert_fbx_to_stl(input_file, out_file)
    return False

def get_cad_info(file_path):
    if not HAS_FREECAD:
        return {}
    shape = Part.Shape()
    shape.read(file_path)
    bbox = shape.BoundBox
    return {
        "x": bbox.XLength,
        "y": bbox.YLength,
        "z": bbox.ZLength,
        "volume": shape.Volume,
        "area": shape.Area,
        "faces": len(shape.Faces),
        "edges": len(shape.Edges),
        "vertices": len(shape.Vertexes)
    }

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: cad_processor.py <convert|info|export> <input_file> [output_file]")
        sys.exit(1)

    cmd = sys.argv[1].lower()
    input_file = os.path.abspath(sys.argv[2])

    if cmd == "convert":
        if len(sys.argv) < 4:
            print("Missing output file argument", file=sys.stderr)
            sys.exit(1)
        out_file = os.path.abspath(sys.argv[3])
        if convert_any_to_stl(input_file, out_file):
            print(f"SUCCESS: Converted {input_file} -> {out_file}")
            sys.exit(0)
        else:
            print(f"FAILED: Conversion failed for {input_file}", file=sys.stderr)
            sys.exit(1)

    elif cmd == "export":
        if len(sys.argv) < 4 or not HAS_FREECAD:
            sys.exit(1)
        out_file = os.path.abspath(sys.argv[3])
        shape = Part.Shape()
        shape.read(input_file)
        ext = os.path.splitext(out_file)[1].lower()
        if ext in [".iges", ".igs"]:
            shape.exportIges(out_file)
        elif ext in [".brep", ".brp"]:
            shape.exportBrep(out_file)
        elif ext == ".step" or ext == ".stp":
            shape.exportStep(out_file)
        print(f"SUCCESS: Exported {out_file}")
        sys.exit(0)

    elif cmd == "info":
        info = get_cad_info(input_file)
        print(json.dumps(info))
        sys.exit(0)
