"""V-3: variantes da Vila para revalidar "não receber sombra", "não projetar" e "sem sol".

Edita uma CÓPIA de scenes/editor.aescene (nunca o original no aparelho):
  --no-receive NOME   Receive Shadows desligado (flag do objeto; o Mesh Renderer v11 a adota na leitura)
  --no-cast NOME      Cast Shadows desligado (idem)
  --no-sun NOME       objeto na camada 9 e Máscara de culling do sol sem a camada 9

O sol da Vila está gravado como Luz v3, que não tem Máscara de culling; ele é
regravado como v4 com os mesmos valores que `Light::read` deriva da v3 (cor sRGB,
ângulos totais, sombra suave na direcional), e só a máscara muda.
Uso: python tools/v3_shadow_flags_scene_variant.py <entrada> <saída> [opções]
"""
import argparse
import re

ENTITY = re.compile(r'^(\d+) (\d+) (\d+) "((?:[^"\\]|\\.)*)" (.*)$')
SUN_NAME = 'Sol de fim de tarde'
EXCLUDED_LAYER = 9


def linear_to_srgb(value):
    value = max(value, 0.0)
    if value == 1.0:
        return 1.0
    return 12.92 * value if value <= 0.0031308 else 1.055 * value ** (1 / 2.4) - 0.055


def sun_v4(payload, culling_mask):
    v = payload.split()
    kind, enabled, unit, use_ct, shadow_mode, shadow_res = v[0:6]
    linear = [float(x) for x in v[6:9]]
    temperature, intensity, light_range, inner_half, outer_half = v[9:14]
    strength, bias, normal_bias, near = v[14:18]
    if kind == '0':
        shadow_mode = '2'
    color = ['%.9g' % linear_to_srgb(c) for c in linear]
    fields = [kind, enabled, unit, use_ct, shadow_mode, shadow_res, '0', str(culling_mask), '1',
              *color, temperature, intensity, light_range,
              '%.9g' % (float(outer_half) * 2), '%.9g' % (float(inner_half) * 2),
              strength, bias, normal_bias, near]
    return ' '.join(fields)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source')
    parser.add_argument('output')
    parser.add_argument('--no-receive', action='append', default=[])
    parser.add_argument('--no-cast', action='append', default=[])
    parser.add_argument('--no-sun', action='append', default=[])
    args = parser.parse_args()
    text = open(args.source, 'rb').read().decode('utf-8')
    lines = text.split('\n')
    touched = set()
    for index, line in enumerate(lines):
        match = ENTITY.match(line)
        if not match:
            continue
        name = match.group(4)
        head = line[:match.start(5)]
        fields = match.group(5).split(' ')
        # Colisores físicos repetem o nome da casa: só o objeto com Mesh Renderer muda
        # (a camada também decide colisões).
        renders = '"astra.render.mesh"' in line
        if not renders and name != SUN_NAME:
            continue
        # posição, rotação e escala (9), depois active visible cast receive static layer
        if name in args.no_cast:
            fields[11] = '0'
            touched.add(name)
        if name in args.no_receive:
            fields[12] = '0'
            touched.add(name)
        if name in args.no_sun:
            fields[14] = str(EXCLUDED_LAYER)
            touched.add(name)
        rest = ' '.join(fields)
        if name == SUN_NAME and args.no_sun:
            mask = 0xFFFFFFFF & ~(1 << EXCLUDED_LAYER)
            light = re.search(r'"astra\.render\.light" 3 "([^"]*)"', rest)
            if not light:
                raise SystemExit('sol sem Luz v3: formato inesperado')
            rest = rest[:light.start()] + '"astra.render.light" 4 "%s"' % sun_v4(light.group(1), mask) + rest[light.end():]
            touched.add(name)
        lines[index] = head + rest
    wanted = set(args.no_receive + args.no_cast + args.no_sun + ([SUN_NAME] if args.no_sun else []))
    if wanted - touched:
        raise SystemExit('objetos não encontrados: %s' % sorted(wanted - touched))
    open(args.output, 'wb').write('\n'.join(lines).encode('utf-8'))
    print('alterados:', sorted(touched))


if __name__ == '__main__':
    main()
