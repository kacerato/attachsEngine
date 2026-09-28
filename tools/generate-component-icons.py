#!/usr/bin/env python3
"""Fonte única dos ícones de componentes e receitas de criação da Astra.

Cada ícone é um SVG 128×128 desenhado aqui, na mesma linguagem visual:
silhueta do conceito em traço claro (INK) e o elemento que DIFERENCIA a variante
em lima (ACCENT) — o raio da esfera sensora, a seta da cinemática, o ponteiro do
timer. Um tipo novo entra acrescentando uma função de desenho e uma linha em
ICONS; o script grava o SVG, rasteriza o PNG e registra no catalog.json. Depois:

    python tools/generate-component-icons.py
    python tools/pack-icon-atlas.py

A rasterização usa o Chrome ou o Edge em modo headless (fundo transparente),
que já estão instalados nas máquinas de desenvolvimento; não há dependência
Python além da biblioteca padrão.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

NAMED = Path("assets/astra-visual/icons/named")
INK = "#e8f0ed"
ACCENT = "#c6ef62"
GENERATION = "component-v1"


def stroke(width: float, colour: str = INK, extra: str = "") -> str:
    return (f'stroke="{colour}" stroke-width="{width}" stroke-linecap="round" '
            f'stroke-linejoin="round" {extra}').strip()


# --- Física: forma × modo (antes em generate-physics-recipe-icons.py) -------
SHAPES = {
    "sphere": f'''<circle cx="64" cy="59" r="24" {stroke(6)}/>
<path d="M42 49c12 7 32 7 44 0M42 69c12-7 32-7 44 0" {stroke(3, extra='opacity=".7"')}/>
<path d="M64 35c-10 12-10 36 0 48M64 35c10 12 10 36 0 48" {stroke(3, extra='opacity=".7"')}/>''',
    "capsule": f'''<rect x="46" y="26" width="36" height="66" rx="18" {stroke(6)}/>
<path d="M48 45c9 6 23 6 32 0M48 73c9-6 23-6 32 0" {stroke(3, extra='opacity=".7"')}/>''',
    "box": f'''<path d="M40 39h48v48H40z" {stroke(6)}/>
<path d="M40 39l12-12h48L88 39M88 39l12-12v48L88 87" {stroke(4, extra='opacity=".7"')}/>''',
}
MODES = {
    "dynamic": f'''<path d="M17 48h19M12 61h20M20 74h16" {stroke(5, ACCENT)}/>
<path d="M94 59h18m-8-8 8 8-8 8" {stroke(5, ACCENT)}/>''',
    "sensor": f'''<circle cx="64" cy="59" r="40" {stroke(4, ACCENT, 'stroke-dasharray="7 7"')}/>
<path d="M57 109h14M64 102v14" {stroke(5, ACCENT)}/>''',
    "kinematic": f'''<path d="M22 33V20h13M93 20h13v13M22 85v13h13M93 98h13V85" {stroke(5, ACCENT)}/>
<path d="M64 99v15m-6-6 6 6 6-6" {stroke(4, ACCENT)}/>''',
    # Estático: apoiado no chão, que é o que não se move.
    "static": f'''<path d="M22 102h84" {stroke(6, ACCENT)}/>
<path d="M30 114l10-10M50 114l10-10M70 114l10-10M90 114l10-10" {stroke(4, ACCENT)}/>''',
}


# --- Luzes: a forma da emissão distingue a modalidade ------------------------
def light_directional() -> str:
    # Sol com raios paralelos: a direção importa, a posição não.
    return f'''<circle cx="40" cy="40" r="16" {stroke(6)}/>
<path d="M40 12v6M40 62v6M12 40h6M62 40h6M20 20l4 4M56 56l4 4M20 60l4-4M56 24l4-4" {stroke(4)}/>
<path d="M64 70l40 40M80 60l36 36M56 84l32 32" {stroke(6, ACCENT)}/>'''


def light_point() -> str:
    # Lâmpada emitindo em todas as direções a partir de um ponto.
    return f'''<path d="M50 88c0-10-12-18-12-34a26 26 0 0 1 52 0c0 16-12 24-12 34z" {stroke(6)}/>
<path d="M52 100h24M56 112h16" {stroke(6)}/>
<path d="M64 8v10M24 24l7 7M104 24l-7 7M10 54h10M108 54h10" {stroke(5, ACCENT)}/>'''


def light_spot() -> str:
    # Refletor com o cone de luz recortado.
    return f'''<path d="M38 18h52l-8 22H46z" {stroke(6)}/>
<path d="M46 40l-28 74h92L82 40" {stroke(5, ACCENT, 'stroke-dasharray="8 6"')}/>
<path d="M40 114h48" {stroke(6, ACCENT)}/>'''


def physics(mode: str, shape: str) -> str:
    return f"{SHAPES[shape]}\n{MODES[mode]}"


# --- Componentes ------------------------------------------------------------
def timer() -> str:
    # Cronômetro: coroa, caixa e o arco já decorrido com o ponteiro em lima.
    return f'''<path d="M56 18h16M64 18v12M92 34l7-7" {stroke(6)}/>
<circle cx="64" cy="70" r="38" {stroke(6)}/>
<path d="M64 42a28 28 0 0 1 28 28" {stroke(7, ACCENT)}/>
<path d="M64 70V50M64 70l13 9" {stroke(6, ACCENT)}/>
<circle cx="64" cy="70" r="4" fill="{ACCENT}"/>'''


def lod_group() -> str:
    # Mesma forma em três níveis de detalhe; a régua de distância é o critério.
    return f'''<circle cx="30" cy="54" r="20" {stroke(5)}/>
<path d="M12 48c11 5 25 5 36 0M12 61c11-5 25-5 36 0M30 34c-8 10-8 30 0 40M30 34c8 10 8 30 0 40" {stroke(2.5, extra='opacity=".75"')}/>
<path d="M72 38l14 8v16l-14 8-14-8V46z" {stroke(5)}/>
<path d="M58 46l14 8 14-8M72 54v16" {stroke(2.5, extra='opacity=".75"')}/>
<path d="M108 44l11 20h-22z" {stroke(5)}/>
<path d="M12 98h104" {stroke(5, ACCENT)}/>
<path d="M30 90v16M72 90v16M108 90v16" {stroke(5, ACCENT)}/>'''


def skinned_mesh() -> str:
    # A pele (contorno) deformada pela cadeia de ossos em lima.
    return f'''<path d="M22 104c-6-12 0-24 12-30l18-10 12-20c6-10 18-16 30-12 12 4 18 16 14 28-3 9-11 15-20 17l-18 4-12 16c-8 10-24 12-36 7z" {stroke(6)}/>
<path d="M34 94l28-28 30-26" {stroke(6, ACCENT)}/>
<circle cx="34" cy="94" r="6" fill="{ACCENT}"/>
<circle cx="62" cy="66" r="6" fill="{ACCENT}"/>
<circle cx="92" cy="40" r="6" fill="{ACCENT}"/>'''


def camera_follow() -> str:
    # Câmera ligada ao alvo por um cabo tracejado; o alvo em movimento é lima.
    return f'''<rect x="12" y="46" width="44" height="36" rx="7" {stroke(6)}/>
<path d="M56 57l16-9v32l-16-9" {stroke(6)}/>
<circle cx="26" cy="38" r="7" {stroke(5)}/>
<circle cx="42" cy="38" r="7" {stroke(5)}/>
<path d="M78 64h14" {stroke(5, ACCENT, 'stroke-dasharray="4 7"')}/>
<circle cx="104" cy="64" r="12" {stroke(6, ACCENT)}/>
<circle cx="104" cy="64" r="3.5" fill="{ACCENT}"/>
<path d="M92 94h24m-7-7 7 7-7 7" {stroke(5, ACCENT)}/>'''


ICONS: dict[str, str] = {
    "scene/prefab": f'''<path d="m28 34 30-17 30 17v36L58 88 28 70Z M28 34l30 18 30-18M58 52v36" {stroke(5)}/>
<path d="M80 86h20a12 12 0 0 0 0-24H88M80 74H68a12 12 0 0 0 0 24h20" {stroke(7, ACCENT)}/>''',
    "primitive/plane": f'''<path d="M16 75 64 45 112 75 64 105Z" {stroke(5)}/>
<path d="M32 85 80 55M48 95 96 65M32 65 80 95M48 55 96 85" {stroke(2, extra='opacity=".55"')}/>
<path d="M64 73V18m-9 10 9-10 9 10" {stroke(5, ACCENT)}/>''',
    "primitive/quad": f'''<path d="M26 28h76v76H26Z" {stroke(5)}/>
<path d="m26 104 76-76" {stroke(3, extra='opacity=".6"')}/>
<path d="M64 66h49m-9-9 9 9-9 9" {stroke(5, ACCENT)}/>''',
    "scene/tag": f'''<path d="M18 24h43l49 49-37 37-49-49V24" {stroke(7)}/>
<circle cx="43" cy="43" r="7" {stroke(5, ACCENT)}/>
<path d="m65 58 24 24m-33-15 18 18" {stroke(5, ACCENT)}/>''',
    "physics/dynamic-sphere": physics("dynamic", "sphere"),
    "physics/dynamic-capsule": physics("dynamic", "capsule"),
    "physics/sensor-sphere": physics("sensor", "sphere"),
    "physics/sensor-capsule": physics("sensor", "capsule"),
    "physics/kinematic-box": physics("kinematic", "box"),
    "physics/kinematic-sphere": physics("kinematic", "sphere"),
    "physics/dynamic-box": physics("dynamic", "box"),
    "physics/sensor-box": physics("sensor", "box"),
    "physics/static-box": physics("static", "box"),
    "physics/static-sphere": physics("static", "sphere"),
    "physics/static-capsule": physics("static", "capsule"),
    "light/directional": light_directional(),
    "light/point": light_point(),
    "light/spot": light_spot(),
    "component/timer": timer(),
    "component/lod-group": lod_group(),
    "component/skinned-mesh": skinned_mesh(),
    "component/camera-follow": camera_follow(),
}


def browser() -> str:
    candidates = [
        os.environ.get("ASTRA_HEADLESS_BROWSER", ""),
        r"C:\Program Files\Google\Chrome\Application\chrome.exe",
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        shutil.which("chromium") or "", shutil.which("google-chrome") or "",
    ]
    for candidate in candidates:
        if candidate and Path(candidate).exists():
            return candidate
    raise SystemExit("Chrome/Edge não encontrado; defina ASTRA_HEADLESS_BROWSER")


def rasterize(executable: str, svg: Path, png: Path) -> None:
    # Página HTML com o SVG em 512 px: o screenshot do headless tem o tamanho
    # da janela e fundo transparente com a cor de fundo zerada.
    with tempfile.TemporaryDirectory() as folder:
        page = Path(folder) / "icon.html"
        page.write_text(
            "<html><body style='margin:0;background:transparent'>"
            f"<img src='{svg.resolve().as_uri()}' width='512' height='512'></body></html>",
            encoding="utf-8")
        subprocess.run([executable, "--headless=new", "--disable-gpu", "--hide-scrollbars",
                        "--default-background-color=00000000", "--window-size=512,512",
                        f"--screenshot={png.resolve()}", page.resolve().as_uri()],
                       check=True, capture_output=True, timeout=60)
    if not png.exists():
        raise SystemExit(f"rasterização falhou: {svg}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--only", nargs="+", choices=tuple(ICONS))
    requested = parser.parse_args().only
    selected = {name: ICONS[name] for name in requested} if requested else ICONS
    executable = browser()
    catalogue_path = NAMED / "catalog.json"
    catalogue = json.loads(catalogue_path.read_text(encoding="utf-8"))
    for name, body in selected.items():
        category, leaf = name.split("/")
        folder = NAMED / category
        folder.mkdir(parents=True, exist_ok=True)
        svg = folder / f"{leaf}.svg"
        svg.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" '
                       f'viewBox="0 0 128 128" fill="none">\n{body}\n</svg>\n', encoding="utf-8")
        rasterize(executable, svg, folder / f"{leaf}.png")
        entry = catalogue["icons"].get(name, {})
        entry.update({"category": category, "name": leaf, "dark_ui_ready": True})
        entry.setdefault("generation", GENERATION)
        entry.pop("raster", None)
        catalogue["icons"][name] = entry
    catalogue_path.write_text(json.dumps(catalogue, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"{len(selected)} ícones gerados")


if __name__ == "__main__":
    main()
