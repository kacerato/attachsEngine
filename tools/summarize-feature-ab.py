"""Tabelas da medição de 22/10/2026: base × G0 e A/B por recurso contra os controles vizinhos.

Uso: python tools/summarize-feature-ab.py <pasta-da-medição> > tabelas.md
Espera <pasta>/<cena>-base/rows.json (S/C/C/S) e <pasta>/<cena>-ab/ab-rows.json
(measure-feature-ab.ps1). A linha de base do G0 vem de docs/validacao/g0-baseline-2026-10-17.
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
G0 = os.path.join(ROOT, 'docs', 'validacao', 'g0-baseline-2026-10-17')
SCENES = {'vila': ('Vila Astra', 'vila'), 'sponza': ('Sponza', 'sponza')}
METRICS = [('gpu_frame_ms', 'GPU quadro'), ('process_cpu_ms', 'CPU processo'), ('gpu_opaque_ms', 'Opaco'),
           ('gpu_post_ms', 'Pós'), ('gpu_shadow_ms', 'Sombra'), ('gpu_sky_ms', 'Céu'),
           ('gpu_auto_exposure_ms', 'Exposição'), ('gpu_probes_ms', 'Sondas'), ('gpu_skinning_ms', 'Skinning'),
           ('gpu_fsr_easu_ms', 'EASU'), ('gpu_fsr_rcas_ms', 'RCAS'), ('gpu_ui_ms', 'UI')]


def load(path):
    if not os.path.exists(path):
        return []
    data = json.load(open(path, encoding='utf-8-sig'))
    return data if isinstance(data, list) else [data]


def mean(values):
    values = [v for v in values if v is not None]
    return sum(values) / len(values) if values else None


def cell(value, digits=2):
    if value is not None and abs(value) < 0.5 * 10 ** -digits:
        value = 0.0
    return '—' if value is None else ('%.*f' % (digits, value)).replace('.', ',')


def base_table(folder, key):
    project, g0_folder = SCENES[key]
    now = load(os.path.join(folder, key + '-base', 'rows.json'))
    old = load(os.path.join(G0, g0_folder, 'rows.json'))
    if key == 'sponza':
        old = [r for r in old if r.get('round') != 1]  # rodada 1 do G0 descartada (primeira abertura)
    print('### %s: base atual × G0 (média das rodadas, ms)\n' % project)
    print('| Métrica | G0 S | Agora S | Δ S | G0 C | Agora C | Δ C |')
    print('|---|---|---|---|---|---|---|')
    rows = [('internalFps', 'FPS interno')] + METRICS
    for metric, label in rows:
        cells = []
        for tier in ('S', 'C'):
            a = mean([r.get(metric) for r in old if r['tier'] == tier])
            b = mean([r.get(metric) for r in now if r['tier'] == tier])
            delta = None if a is None or b is None else b - a
            cells += [cell(a), cell(b), ('+' if delta is not None and delta >= 0.005 else '') + cell(delta)]
        print('| %s | %s |' % (label, ' | '.join(cells)))
    print()


def ab_table(folder, key):
    project = SCENES[key][0]
    rows = sorted(load(os.path.join(folder, key + '-ab', 'ab-rows.json')), key=lambda r: r['step'])
    if not rows:
        return
    controls = [r for r in rows if r['configuration'] == 'base']
    print('### %s: A/B por recurso em S (controle = média dos dois controles vizinhos)\n' % project)
    print('Controles (GPU quadro, ms, na ordem): %s\n' % ', '.join(cell(r.get('gpu_frame_ms')) for r in controls))
    print('| Configuração | GPU quadro | Δ GPU | Opaco | Δ opaco | Pós | Δ pós | Sombra | CPU | Δ CPU |')
    print('|---|---|---|---|---|---|---|---|---|---|')
    for row in rows:
        if row['configuration'] == 'base':
            continue
        before = [c for c in controls if c['step'] < row['step']][-1:]
        after = [c for c in controls if c['step'] > row['step']][:1]
        neighbours = before + after
        out = [row['configuration']]
        for metric in ('gpu_frame_ms', 'gpu_opaque_ms', 'gpu_post_ms'):
            value = row.get(metric)
            reference = mean([c.get(metric) for c in neighbours])
            delta = None if value is None or reference is None else value - reference
            out += [cell(value), ('+' if delta is not None and delta >= 0.005 else '') + cell(delta)]
        out.append(cell(row.get('gpu_shadow_ms')))
        cpu = row.get('process_cpu_ms')
        reference = mean([c.get('process_cpu_ms') for c in neighbours])
        out += [cell(cpu), ('+' if cpu is not None and reference is not None and cpu > reference else '') +
                cell(None if cpu is None or reference is None else cpu - reference)]
        print('| ' + ' | '.join(out) + ' |')
    print()


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    folder = sys.argv[1]
    for key in ('vila', 'sponza'):
        base_table(folder, key)
        ab_table(folder, key)


if __name__ == '__main__':
    main()
