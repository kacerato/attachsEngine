"""Comparação de imagem do A/B temporal (tools/android-temporal-ab.ps1).

Para cada caso válido, compara a captura com a do TAA nativo na escala 1.0 da
MESMA rodada: PSNR e erro absoluto médio (0..255) num recorte central que tira
barras de UI. A referência não é "verdade" — é o caminho nativo sem ampliação —,
então o número mede o quanto cada reconstrução se afasta dele, não a qualidade
absoluta. O resultado volta para o summary.json, em `image`.
"""
import json
import math
import sys

from PIL import Image, ImageChops, ImageStat


def crop(image):
    width, height = image.size
    # Tira 12% de cima (barra do editor/HUD) e 6% das bordas.
    return image.crop((int(width * .06), int(height * .12), int(width * .94), int(height * .94))).convert('RGB')


def compare(reference_path, capture_path):
    reference = crop(Image.open(reference_path))
    capture = crop(Image.open(capture_path))
    if capture.size != reference.size:
        capture = capture.resize(reference.size, Image.BILINEAR)
    difference = ImageChops.difference(reference, capture)
    stat = ImageStat.Stat(difference)
    mean_absolute = sum(stat.mean) / 3.0
    mse = sum(value * value for value in stat.rms) / 3.0
    psnr = float('inf') if mse == 0 else 10.0 * math.log10(255.0 * 255.0 / mse)
    return {'reference': reference_path, 'meanAbsoluteError': round(mean_absolute, 4),
            'psnr': None if math.isinf(psnr) else round(psnr, 3)}


def main(path):
    with open(path, encoding='utf-8-sig') as file:
        summary = json.load(file)
    references = {}
    for case in summary['cases']:
        if case['mode'] == 'taa' and abs(case['scale'] - 1.0) < 1e-6 and case['valid']:
            references[case['round']] = case['capture']
    for case in summary['cases']:
        reference = references.get(case['round'])
        if not reference or not case['valid']:
            case['image'] = None
            continue
        case['image'] = compare(reference, case['capture'])
    with open(path, 'w', encoding='utf-8') as file:
        json.dump(summary, file, ensure_ascii=False, indent=2)
    for case in summary['cases']:
        image = case.get('image')
        print(f"{case['name']}: " + (('idêntica à referência' if image['psnr'] is None else f"PSNR {image['psnr']} dB, erro médio {image['meanAbsoluteError']}")
                                         if image else 'sem comparação'))
    return 0


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('uso: temporal_ab_images.py <summary.json>', file=sys.stderr)
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
