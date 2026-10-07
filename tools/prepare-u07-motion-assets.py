"""Derive the eight usable locomotion clips from the official Godot TPS rig.

Retains the authored skeleton/meshes; removes the dedicated root-motion track
for force-driven in-place playback and compacts unused buffer references.
No archive or engine resource is hand-written. Import the resulting standard GLB.
"""
import argparse, copy, hashlib, json, struct
from pathlib import Path

CLIPS = {
    'Idle-cycle': 'Idle', 'walking_nogun-cycle': 'Walk',
    'running_nogun-cycle': 'Run', 'jump_1_up': 'Jump',
    'jump_2_upwards-cycle': 'Rise', 'jump_3_midair-cycle': 'Apex',
    'jump_4_falling-cycle': 'Fall', 'jump_5_hardlanding': 'Land',
}

def derive(source, output):
    raw = source.read_bytes()
    assert struct.unpack_from('<4sII', raw) == (b'glTF', 2, len(raw))
    size, kind = struct.unpack_from('<II', raw, 12)
    assert kind == 0x4e4f534a
    model = json.loads(raw[20:20+size])
    binary_offset = 20+size
    binary_size, binary_kind = struct.unpack_from('<II', raw, binary_offset)
    assert binary_kind == 0x004e4942
    binary = raw[binary_offset+8:binary_offset+8+binary_size]
    selected = {a['name']: a for a in model['animations'] if a['name'] in CLIPS}
    assert selected.keys() == CLIPS.keys(), 'Missing authored clips'
    model['animations'] = [copy.deepcopy(selected[name]) for name in CLIPS]
    for animation, name in zip(model['animations'], CLIPS.values()):
        animation['name'] = name
        # Godot's AnimationTree extracts the dedicated root-motion track.
        # This sample's rigid body is driven by forces instead. Removing that
        # track keeps the authored bind root in place and prevents a second
        # displacement/rotation authority inside the visual rig.
        roots = {i for i, node in enumerate(model['nodes']) if node.get('name') == 'root'}
        assert len(roots) == 1
        animation['channels'] = [c for c in animation['channels'] if c['target']['node'] not in roots]
        sampler_ids = sorted({c['sampler'] for c in animation['channels']})
        sampler_remap = {old: new for new, old in enumerate(sampler_ids)}
        animation['samplers'] = [animation['samplers'][i] for i in sampler_ids]
        for channel in animation['channels']: channel['sampler'] = sampler_remap[channel['sampler']]
    used = set()
    def access(owner, key):
        if key in owner: used.add(owner[key])
    for mesh in model['meshes']:
        for primitive in mesh['primitives']:
            used.update(primitive['attributes'].values()); access(primitive, 'indices')
            for target in primitive.get('targets', []): used.update(target.values())
    for skin in model.get('skins', []): access(skin, 'inverseBindMatrices')
    for animation in model['animations']:
        for sampler in animation['samplers']: access(sampler, 'input'); access(sampler, 'output')
    remap = {old: new for new, old in enumerate(sorted(used))}
    def rewrite(owner, key):
        if key in owner: owner[key] = remap[owner[key]]
    for mesh in model['meshes']:
        for primitive in mesh['primitives']:
            primitive['attributes'] = {k: remap[v] for k, v in primitive['attributes'].items()}
            rewrite(primitive, 'indices')
            for target in primitive.get('targets', []):
                for k, v in target.items(): target[k] = remap[v]
    for skin in model.get('skins', []): rewrite(skin, 'inverseBindMatrices')
    for animation in model['animations']:
        for sampler in animation['samplers']: rewrite(sampler, 'input'); rewrite(sampler, 'output')
    model['accessors'] = [model['accessors'][old] for old in sorted(used)]
    view_used = set()
    for accessor in model['accessors']:
        assert 'sparse' not in accessor
        if 'bufferView' in accessor: view_used.add(accessor['bufferView'])
    for image in model.get('images', []):
        if 'bufferView' in image: view_used.add(image['bufferView'])
    view_remap = {old: new for new, old in enumerate(sorted(view_used))}
    compact, views = bytearray(), []
    for old in sorted(view_used):
        view = copy.deepcopy(model['bufferViews'][old]); assert view['buffer'] == 0
        start, length = view.get('byteOffset', 0), view['byteLength']
        compact.extend(b'\0' * (-len(compact) % 4))
        view['byteOffset'] = len(compact)
        compact.extend(binary[start:start+length]); views.append(view)
    for accessor in model['accessors']:
        if 'bufferView' in accessor: accessor['bufferView'] = view_remap[accessor['bufferView']]
    for image in model.get('images', []):
        if 'bufferView' in image: image['bufferView'] = view_remap[image['bufferView']]
    model['bufferViews'] = views
    # The original uses external Godot materials. Keep their explicit color
    # materials instead of inventing unavailable texture bindings.
    model['asset']['extras'] = {'source': 'https://github.com/godotengine/tps-demo',
                               'original_sha256': hashlib.sha256(raw).hexdigest(),
                               'changes': 'Eight named clips, dedicated root-motion track removed for force-driven in-place motion, unused buffers compacted.'}
    model['buffers'] = [{'byteLength': len(compact)}]
    encoded = json.dumps(model, separators=(',', ':')).encode()
    encoded += b' ' * (-len(encoded) % 4)
    compact.extend(b'\0' * (-len(compact) % 4))
    result = struct.pack('<4sII', b'glTF', 2, 28+len(encoded)+len(compact))
    result += struct.pack('<II', len(encoded), 0x4e4f534a)+encoded
    result += struct.pack('<II', len(compact), 0x004e4942)+compact
    output.parent.mkdir(parents=True, exist_ok=True); output.write_bytes(result)
    print(json.dumps({'path': str(output), 'bytes': len(result), 'clips': list(CLIPS.values()),
                      'joints': len(model['skins'][0]['joints']), 'sha256': hashlib.sha256(result).hexdigest()}))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(); parser.add_argument('source', type=Path); parser.add_argument('output', type=Path)
    args = parser.parse_args(); derive(args.source, args.output)
