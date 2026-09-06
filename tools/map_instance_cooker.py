"""Append a cooked static asset as one movable, level-zero render instance.

Offline only. Bakes source node transforms, remaps resources, preserves packed
UVs/tangents and emits disjoint index ranges required by the spatial cooker.
"""
import pathlib
import shutil
import struct
import numpy as np

HEADER = '<10I5Q6f7f3I'
DRAW = '<4I16f4fIfI'


def append_instance(target, source, position=(0, 1, 1), pivot=(0, .6, 0), scale=1.0):
    if not np.isfinite(scale) or scale <= 0:
        raise ValueError('instance scale must be finite and positive')
    target, source = pathlib.Path(target), pathlib.Path(source)
    a, b = target.read_bytes(), source.read_bytes()
    ha, hb = list(struct.unpack_from(HEADER, a)), struct.unpack_from(HEADER, b)
    if tuple(ha[:4]) != tuple(hb[:4]) or tuple(ha[1:4]) != (3, 144, 48):
        raise ValueError('incompatible cooked mesh layout')
    textures = bytearray(a[ha[10]:ha[10]+ha[4]*16])
    materials = bytearray(a[ha[11]:ha[11]+ha[5]*80])
    draws = bytearray(a[ha[12]:ha[12]+ha[6]*108])
    vertices = bytearray(a[ha[13]:ha[13]+ha[7]*48])
    indices = list(struct.unpack_from(f'<{ha[8]}I', a, ha[14]))
    for i in range(hb[4]):
        textures += b[hb[10]+i*16:hb[10]+(i+1)*16]
        matches = list(source.parent.glob(f'texture_{i:03d}*.aetex'))
        if not matches:
            raise ValueError(f'missing texture {i}')
        for path in matches:
            shutil.copyfile(path, target.parent / path.name.replace(f'texture_{i:03d}', f'texture_{ha[4]+i:03d}'))
    for i in range(hb[5]):
        material = bytearray(b[hb[11]+i*80:hb[11]+(i+1)*80])
        for slot in range(4):
            texture = struct.unpack_from('<I', material, slot*4)[0]
            if texture != 0xffffffff:
                struct.pack_into('<I', material, slot*4, texture+ha[4])
        flags = struct.unpack_from('<I', material, 68)[0]
        struct.pack_into('<I', material, 68, flags | (1 << 6))
        materials += material
    draw_count = ha[6]
    bounds_low = np.array(ha[15:18], dtype=float)
    bounds_high = np.array(ha[18:21], dtype=float)
    for i in range(hb[6]):
        record = list(struct.unpack_from(DRAW, b, hb[12]+i*108))
        if record[24] != 0:
            continue  # dynamic meshes cannot select a static LOD independently
        first, count, base, material = record[:4]
        source_indices = struct.unpack_from(f'<{count}I', b, hb[14]+first*4)
        matrix = np.array(record[4:20]).reshape((4,4), order='F')
        normal_matrix = np.linalg.inv(matrix[:3,:3]).T
        remap, points = {}, []
        first_index = len(indices)
        for index in source_indices:
            index += base
            if index not in remap:
                packed = bytearray(b[hb[13]+index*48:hb[13]+(index+1)*48])
                point = ((matrix @ np.array((*struct.unpack_from('<3f', packed),1)))[:3]-pivot)*scale
                struct.pack_into('<3f', packed, 0, *point)
                for offset, transform in ((12,normal_matrix),(20,matrix[:3,:3])):
                    direction = transform @ (np.array(struct.unpack_from('<3h',packed,offset))/32767)
                    direction /= max(np.linalg.norm(direction),1e-12)
                    struct.pack_into('<3h',packed,offset,*np.clip(np.rint(direction*32767),-32767,32767).astype(int))
                remap[index] = len(vertices)//48
                vertices += packed
                points.append(point)
            indices.append(remap[index])
        low, high = np.min(points,axis=0), np.max(points,axis=0)
        centre = (low+high)*.5+position
        radius = float(np.linalg.norm((high-low)*.5))
        model = np.eye(4)
        model[:3,3] = position
        draws += struct.pack(DRAW,first_index,count,0,material+ha[5],
                             *model.flatten(order='F'),*centre,radius,0,0,draw_count)
        bounds_low = np.minimum(bounds_low,low+position)
        bounds_high = np.maximum(bounds_high,high+position)
        draw_count += 1
    ha[4:9] = [ha[4]+hb[4],ha[5]+hb[5],draw_count,len(vertices)//48,len(indices)]
    sections = (textures,materials,draws,vertices,struct.pack(f'<{len(indices)}I',*indices))
    output = bytearray(144)
    for i, section in enumerate(sections):
        output += bytes((-len(output)) % 16)
        ha[10+i] = len(output)
        output += section
    ha[15:21] = [*bounds_low,*bounds_high]
    ha[28] = len(indices)//3
    struct.pack_into(HEADER,output,0,*ha)
    target.write_bytes(output)
    return {'source': str(source), 'drawCount': draw_count-struct.unpack_from('<I',a,24)[0],
            'position': list(position), 'pivot': list(pivot), 'scale': scale, 'collision': 'box-proxy'}
