"""Avaliacao de referencia do projeto UMotion no espaco da Unity (sem a Astra):
FK (quaternion progressivo, Hermite por quadro), IK de plano rotativo como o
solver do UMotion e Child-Of. Compara a caixa com a curva exportada pelo UMotion.
Uso: python umotion_reference_eval.py <projeto.asset> <exportado.anim> [delta_plano]"""
import sys, math, re
import numpy as np
sys.path.insert(0, __file__.rsplit('\\', 1)[0].rsplit('/', 1)[0])
import umotion_project as u

def qnorm(q): q = np.array(q, float); return q / np.linalg.norm(q)
def qmat(q):
    x, y, z, w = q
    return np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)], [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)], [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])
def slerp(a, b, t):
    a = qnorm(a); b = qnorm(b); d = np.dot(a, b)
    if d < 0: b = -b; d = -d
    if d > 0.9995: return qnorm(a + (b - a) * t)
    th = math.acos(min(1, d)); s = math.sin(th)
    return (math.sin((1 - t) * th) * a + math.sin(t * th) * b) / s
def hermite(keys, f, fallback):
    if not keys: return fallback
    if f <= keys[0]['frame']: return keys[0]['value']
    if f >= keys[-1]['frame']: return keys[-1]['value']
    for i in range(len(keys) - 1):
        a, b = keys[i], keys[i + 1]
        if a['frame'] <= f <= b['frame']: break
    ma = a['tout']; mb = b['tin']
    if (a['mode'] >> 4) & 15 == 3 or (b['mode'] & 15) == 3: return a['value']
    dt = b['frame'] - a['frame']
    if (a['mode'] >> 4) & 15 == 2: ma = (b['value'] - a['value']) / dt
    if (b['mode'] & 15) == 2: mb = (b['value'] - a['value']) / dt
    t = (f - a['frame']) / dt; t2 = t * t; t3 = t2 * t
    return (2*t3-3*t2+1)*a['value'] + (t3-2*t2+t)*dt*ma + (-2*t3+3*t2)*b['value'] + (t3-t2)*dt*mb
def keys_of(ch): return ch['layers'][0] if ch['layers'] else []

def main():
    asset, anim = sys.argv[1], sys.argv[2]
    delta = float(sys.argv[3]) if len(sys.argv) > 3 else 0.0
    variant = sys.argv[4] if len(sys.argv) > 4 else ''
    objs = u.parse(asset); sett = u.settings(asset)
    path_index = {o['path']: i for i, o in enumerate(objs)}
    parent = [path_index.get(o['path'].rsplit('/', 1)[0], -1) if '/' in o['path'] else (0 if o['path'] else -1) for o in objs]
    def ref_local(i):
        c = objs[i]['channels']
        return (np.array([x['ref'] for x in c['LocalPosition']]), qnorm([x['ref'] for x in c['LocalRotation']][:4]))
    def fk_local(i, f):
        c = objs[i]['channels']
        p = np.array([hermite(keys_of(x), f, x['ref']) for x in c['LocalPosition']])
        rot = c['LocalRotation']; ks = [keys_of(x) for x in rot]
        if not ks[0]: return p, qnorm([x['ref'] for x in rot][:4])
        frames = [k['frame'] for k in ks[0]]
        if f <= frames[0]: return p, qnorm([k[0]['value'] for k in ks[:4]])
        if f >= frames[-1]: return p, qnorm([k[-1]['value'] for k in ks[:4]])
        j = max(n for n in range(len(frames)) if frames[n] <= f)
        qa = qnorm([ks[c][j]['value'] for c in range(4)]); qb = qnorm([ks[c][j + 1]['value'] for c in range(4)])
        pa = ks[4][j]['value']; pb = ks[4][j + 1]['value']
        if pb - pa <= 0: return p, qa
        alpha = (hermite(ks[4], f, 0) - pa) / (pb - pa)
        return p, slerp(qa, qb, alpha)
    def M(p, q): m = np.eye(4); m[:3, :3] = qmat(q); m[:3, 3] = p; return m
    def worlds(locals_):
        W = [None] * len(objs)
        def w(i):
            if W[i] is not None: return W[i]
            if not objs[i]['path']: W[i] = np.eye(4); return W[i]
            W[i] = w(parent[i]) @ M(*locals_[i]); return W[i]
        for i in range(len(objs)): w(i)
        return W
    import os
    if os.environ.get('UMOTION_DUMP'):
        # Posicoes de mundo so do FK num quadro (referencia para comparar rigs).
        f = float(os.environ['UMOTION_DUMP'])
        W = worlds([fk_local(i, f) for i in range(len(objs))])
        for i, o in enumerate(objs):
            if o['path']: print('UDUMP %s %.5f %.5f %.5f' % (o['path'].rsplit('/', 1)[-1], *W[i][:3, 3]))
        return
    def perp(v):
        a = np.abs(v)
        return np.array([-v[2], 0, v[0]]) if (a[2] > a[0] and a[2] > a[1]) else np.array([-v[1], v[0], 0])
    def look(f, up):
        z = f / np.linalg.norm(f); x = np.cross(up, z); x /= np.linalg.norm(x); y = np.cross(z, x)
        return np.stack([x, y, z], 1)
    def fromto(a, b):
        a = a / np.linalg.norm(a); b = b / np.linalg.norm(b); v = np.cross(a, b); s = np.linalg.norm(v); c = np.dot(a, b)
        if s < 1e-9: return np.eye(3)
        k = v / s; K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]]); th = math.atan2(s, c)
        return np.eye(3) + math.sin(th) * K + (1 - math.cos(th)) * K @ K
    def axisangle(axis, deg):
        k = axis / np.linalg.norm(axis); K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]]); th = math.radians(deg)
        return np.eye(3) + math.sin(th) * K + (1 - math.cos(th)) * K @ K
    # Curva exportada da caixa.
    text = open(anim, encoding='utf-8').read()
    sec = text[text.index('  m_PositionCurves:'):text.index('  m_ScaleCurves:')]
    keysx = re.findall(r'time: ([-\d.e]+)\n\s+value: \{x: ([-\d.e]+), y: ([-\d.e]+), z: ([-\d.e]+)\}', sec)
    ref_curve = {round(float(t) * 60): np.array([float(x), float(y), float(z)]) for t, x, y, z in keysx}
    iks = [i for i, s in enumerate(sett) if s.get('IkEnable') == '1']
    cube = path_index['Root/Cube']; root = path_index['Root']
    worst = 0
    for f in sorted(ref_curve):
        loc = [fk_local(i, f) if objs[i]['path'] else (np.zeros(3), np.array([0, 0, 0, 1.0])) for i in range(len(objs))]
        W = worlds(loc)
        for h in iks:
            s = sett[h]; tip = int(s['IkTarget'].split()[0][8:16][:2], 16) if False else None
            tip = [i for i in range(len(objs)) if objs[i]['path'].endswith('Wrist_Joint_01') and (('Left' in objs[i]['path']) == ('Left' in objs[h]['path']))][0]
            mid = parent[tip]; top = parent[mid]
            # Zera a cadeia na referencia (pos e rot).
            fkW = worlds(loc)
            if 'noreset' not in variant:
                for b in (top, mid, tip): loc[b] = ref_local(b) if b != tip else (ref_local(b)[0], loc[b][1])
                if 'rotonly' in variant:
                    for b in (top, mid): loc[b] = (fk_local(b, f)[0], ref_local(b)[1])
            W = worlds(loc)
            zeroW = fkW if 'fkzero' in variant else W
            start = W[top][:3, 3]; axis = W[tip][:3, 3] - start
            v5 = zeroW[top][:3, :3] @ perp(ref_local(mid)[0])
            an = axis / np.linalg.norm(axis); plane = v5 - an * np.dot(v5, an)
            plane = axisangle(axis, float(s['IkPlaneAngleOffset']) + delta) @ plane
            plane = plane - an * np.dot(plane, an); plane /= np.linalg.norm(plane); bin_ = np.cross(an, plane)
            goal = W[h][:3, 3]; dist = np.linalg.norm(goal - start)
            upper = W[mid][:3, 3] - start; lower = W[tip][:3, 3] - W[mid][:3, 3]
            along = (upper @ upper + dist * dist - lower @ lower) / (2 * dist); side = math.sqrt(max(0, upper @ upper - along * along))
            v7 = plane * side + an * along
            R = look(v7, bin_) @ look(upper, bin_).T
            # aplica rotacao de mundo ao osso de cima: R_local' = parent^-1 R W
            def rotate(b, Rw):
                Wp = W[parent[b]][:3, :3]; Wb = W[b][:3, :3]
                newlocal = Wp.T @ Rw @ Wb
                from scipy.spatial.transform import Rotation as Rot
                q = Rot.from_matrix(newlocal).as_quat(); loc[b] = (loc[b][0], q)
            rotate(top, R); W = worlds(loc)
            v6 = W[tip][:3, 3] - W[mid][:3, 3]; v9 = start + an * dist - W[mid][:3, 3]
            rotate(mid, look(v9, bin_) @ look(v6, bin_).T); W = worlds(loc)
            rotate(top, fromto(an, goal - start)); W = worlds(loc)
            if 'wristworld' in variant:
                Wp = W[mid][:3, :3]; from scipy.spatial.transform import Rotation as Rot
                loc[tip] = (loc[tip][0], Rot.from_matrix(Wp.T @ fkW[tip][:3, :3]).as_quat()); W = worlds(loc)
        # Child-Of da caixa.
        po = None
        for frame, addr in sett[cube]['ChildOfParentChannel']:
            if frame <= f: po = addr[-1] if addr else None
        if po is not None:
            Wp = worlds([fk_local(i, f) if objs[i]['path'] else (np.zeros(3), np.array([0, 0, 0, 1.0])) for i in range(len(objs))])[po] if 'fkparent' in variant else W[po]
            Wc = Wp @ M(*loc[cube])
        else: Wc = W[cube]
        rel = np.linalg.inv(W[root]) @ Wc
        err = np.linalg.norm(rel[:3, 3] - ref_curve[f]); worst = max(worst, err)
        if 'quiet' not in variant and (f % 10 == 0 or err > 0.004): print('f%03d err %.4f' % (f, err))
    print('WORST %.4f' % worst)

main()
