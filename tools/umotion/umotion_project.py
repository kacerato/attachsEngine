"""Leitor do projeto UMotion Pro (.asset YAML): objetos animados, pose de
referencia e chaves por canal (lista binaria: int32 n + n x 30 bytes)."""
import re, struct, sys

KEY = struct.Struct('<BiBfffffBBBB')  # 30 bytes

def keys(hexs):
    b = bytes.fromhex(hexs)
    if len(b) < 4: return []
    n = struct.unpack_from('<i', b, 0)[0]
    if 4 + n * 30 != len(b): raise ValueError('key list size %d for %d keys' % (len(b), n))
    out = []
    for k in range(n):
        _, frame, _, value, tin, tout, win, wout, mode, a, c, d = KEY.unpack_from(b, 4 + k * 30)
        out.append(dict(frame=frame, value=value, tin=tin, tout=tout, win=win, wout=wout, mode=mode, flags=(a, c, d)))
    return out

def parse(path):
    lines = open(path, encoding='utf-8').read().split('\n')
    objects = []; i = 0
    def value(line): return line.split('valueInternal:', 1)[1].strip()
    while i < len(lines):
        l = lines[i]
        if l.startswith('  - TransformPath:'):
            obj = dict(path=value(lines[i + 1]), channels={}); objects.append(obj); i += 2; continue
        m = re.match(r'^    (LocalPosition|LocalRotation|LocalScale)ChannelList:', l)
        if m and objects:
            kind = m.group(1); chans = []; i += 1
            while i < len(lines) and lines[i].startswith('    - CurrentValue:'):
                ref = float(value(lines[i + 3])); clips = []
                j = i + 4
                while j < len(lines) and not lines[j].startswith('    - CurrentValue:') and not re.match(r'^    [A-Z]', lines[j]) and not lines[j].startswith('  - ') and not lines[j].startswith('  ' + 'P') and not lines[j].startswith('  C'):
                    if 'newKeyListSerialized:' in lines[j]:
                        clips.append(keys(lines[j].split(':', 1)[1].strip()))
                    j += 1
                chans.append(dict(ref=ref, layers=clips)); i = j
            obj['channels'][kind] = chans; continue
        if l.startswith('    RotationModePerClip:'):
            objects[-1]['rotationMode'] = int(value(lines[i + 1]).split()[0]) if 'valueInternal' in lines[i + 1] else 0
        i += 1
    return objects

if __name__ == '__main__':
    objs = parse(sys.argv[1])
    for o in objs:
        c = o['channels']
        pos = [x['ref'] for x in c.get('LocalPosition', [])]; rot = [x['ref'] for x in c.get('LocalRotation', [])]
        nk = sum(len(l) for ch in c.values() for x in ch for l in x['layers'])
        print(o['path'].split('/')[-1] or '<raiz>', 'rm', o.get('rotationMode'), 'pos', ['%.4f' % v for v in pos], 'rot', ['%.4f' % v for v in rot], 'keys', nk)

def refkeys(hexs):
    """Chaves de referencia (Child-Of): byte, int32 quadro, byte, int32 n, n x int32 endereco."""
    b = bytes.fromhex(hexs); n = struct.unpack_from('<i', b, 0)[0]; o = 4; out = []
    for _ in range(n):
        frame = struct.unpack_from('<i', b, o + 1)[0]; cnt = struct.unpack_from('<i', b, o + 6)[0]
        addr = list(struct.unpack_from('<%di' % cnt, b, o + 10)) if cnt else []
        out.append((frame, addr)); o += 10 + 4 * cnt
    if o != len(b): raise ValueError('ref key list')
    return out

def settings(path):
    """Ajustes por objeto que nao sao canais: tipo, IK e Child-Of."""
    lines = open(path, encoding='utf-8').read().split('\n'); out = []; cur = None
    for i, l in enumerate(lines):
        if l.startswith('  - TransformPath:'):
            cur = dict(path=lines[i + 1].split('valueInternal:')[1].strip()); out.append(cur)
        elif cur is not None and re.match(r'^    (Type|IkEnable|IkChainLength|IkReferenceMode|IkTargetRotationMode|IkPlaneAngleOffset|IkLegacyMode|ChildOfEnable|ChildOfIKPinning):', l):
            cur[l.strip()[:-1]] = lines[i + 1].split('valueInternal:')[1].strip()
        elif cur is not None and re.match(r'^    (IkTarget|IkPoleTarget|SiblingOnOtherSide):', l):
            cur[l.strip()[:-1]] = lines[i + 2].split('Address:')[1].strip()[:24] + ' len ' + lines[i + 3].split('Length:')[1].strip()
        elif cur is not None and re.match(r'^    (IkFkBlendChannel|IkPoleRotationChannel|ChildOfParentChannel):', l):
            j = i + 1
            while 'newKeyListSerialized' not in lines[j]: j += 1
            hx = lines[j].split(':', 1)[1].strip()
            if 'ChildOfParent' in l: cur[l.strip()[:-1]] = refkeys(hx)
            else: cur[l.strip()[:-1]] = [(k['frame'], round(k['value'], 3)) for k in keys(hx)]
    return out
