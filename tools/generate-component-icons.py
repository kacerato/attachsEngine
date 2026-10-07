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
    "path/curve": f'''<path d="M18 94C36 10 78 118 110 30" {stroke(6, ACCENT)}/><rect x="10" y="86" width="16" height="16" {stroke(4)}/><rect x="102" y="22" width="16" height="16" {stroke(4)}/><path d="M18 94L31 49M110 30L98 72" {stroke(3)}/>''',
    "component/path-follow": f'''<path d="M13 92C40 9 73 120 114 32" {stroke(5)}/><path d="M53 49L78 61L56 80Z" {stroke(5, ACCENT)}/><circle cx="13" cy="92" r="7" {stroke(4)}/><circle cx="114" cy="32" r="7" {stroke(4)}/>''',
    "path/point": f'''<path d="M11 104C37 16 82 114 117 20" {stroke(4)}/><rect x="49" y="49" width="30" height="30" {stroke(5, ACCENT)}/><path d="M64 32V12M64 116V96M32 64H12M116 64H96" {stroke(4)}/>''',
    "path/tangent": f'''<path d="M12 107C26 31 82 90 115 20" {stroke(4)}/><path d="M21 84L108 37" {stroke(4, ACCENT)}/><circle cx="21" cy="84" r="7" {stroke(4)}/><circle cx="108" cy="37" r="7" {stroke(4)}/><rect x="56" y="54" width="16" height="16" {stroke(4, ACCENT)}/>''',
    "physics/joint-2d": f'''<path d="M17 38h32v32H17Z M79 58h32v32H79Z" {stroke(5)}/>
<path d="M49 54h15v20h15" {stroke(5, ACCENT)}/>
<circle cx="64" cy="64" r="12" {stroke(5, ACCENT)}/>
<path d="M31 88v22h65M86 100l10 10-10 10" {stroke(4)}/>''',
    "physics/constant-force-2d": f'''<rect x="18" y="60" width="43" height="43" {stroke(5)}/>
<path d="M61 80h53M100 66l14 14-14 14M39 60V15M26 29l13-14 13 14" {stroke(6, ACCENT)}/>
<path d="M77 23a25 25 0 0 1 25 25M91 39l11 9 9-11" {stroke(5, ACCENT)}/>''',
    "component/parent-constraint": f'''<rect x="15" y="19" width="33" height="33" {stroke(5)}/>
<rect x="80" y="77" width="33" height="33" {stroke(5)}/>
<path d="M32 56v38h42M61 83l13 11-13 11M59 22a30 30 0 0 1 41 36M88 49l12 9 6-13" {stroke(5, ACCENT)}/>''',
    "component/look-at-constraint": f'''<path d="M13 64c25-35 56-35 81 0-25 35-56 35-81 0Z" {stroke(5)}/>
<circle cx="53" cy="64" r="12" {stroke(5, ACCENT)}/>
<path d="M71 64h37M100 55l9 9-9 9M111 30v13M111 85v13" {stroke(5, ACCENT)}/>''',
    "component/tween-transform": f'''<rect x="14" y="83" width="20" height="20" {stroke(5)}/>
<rect x="94" y="23" width="20" height="20" {stroke(5, ACCENT)}/>
<path d="M35 83c15-58 38 14 62-40M85 43h12V31" {stroke(5, ACCENT)}/>
<path d="M15 114h99M39 108v12M64 108v12M89 108v12" {stroke(4)}/>''',
    "physics/body-2d": f'''<rect x="24" y="22" width="74" height="74" {stroke(5)}/>
<path d="M60 94v23M49 107l11 10 11-10M98 58h20M109 48l9 10-9 10" {stroke(5, ACCENT)}/>
<path d="M38 38h13M38 38v13M73 80h12M85 68v12" {stroke(4)}/>''',
    "physics/collider-2d": f'''<path d="M19 85V28h57M45 107h64V49" {stroke(5, ACCENT)}/>
<rect x="34" y="42" width="60" height="50" rx="10" {stroke(5)}/>
<path d="m76 17 12 12-12 12M8 74l12 12 12-12" {stroke(4, ACCENT)}/>''',
    "audio/source": f'''<path d="M15 51h20l28-22v70L35 77H15Z" {stroke(5)}/>
<path d="M80 46a25 25 0 0 1 0 36M94 31a45 45 0 0 1 0 66" {stroke(5, ACCENT)}/>''',
    "audio/listener": f'''<path d="M35 38c0-23 48-28 48 3 0 15-15 19-18 35-3 14-7 26-20 26-9 0-14-6-14-14" {stroke(5)}/>
<path d="M49 47c0-15 21-15 21-2 0 13-16 11-16 28" {stroke(5, ACCENT)}/>
<path d="M100 35a34 34 0 0 1 0 53M15 40a32 32 0 0 0 0 42" {stroke(4, ACCENT)}/>''',
    "audio/clip": f'''<path d="M24 14h54l26 26v74H24Z M78 14v26h26" {stroke(5)}/>
<path d="M36 76h8l5-18 10 37 10-43 9 30 5-6h9" {stroke(4, ACCENT)}/>''',
    "audio/bus": f'''<path d="M17 28h36M17 64h36M17 100h36M53 28v72M53 64h58" {stroke(5)}/>
<path d="M94 51l14 13-14 13" {stroke(5, ACCENT)}/>
<circle cx="31" cy="28" r="6" fill="{ACCENT}"/>
<circle cx="40" cy="64" r="6" fill="{ACCENT}"/>
<circle cx="25" cy="100" r="6" fill="{ACCENT}"/>''',
    # O mesmo vínculo fonte/objeto distingue os quatro canais de restrição.
    "component/position-constraint": f'''<rect x="18" y="58" width="30" height="30" {stroke(5)}/>
<circle cx="94" cy="34" r="12" {stroke(5, ACCENT)}/>
<path d="M48 73h46V47M78 73l-9-9m9 9-9 9M94 61l-9-9m9 9 9-9" {stroke(5, ACCENT)}/>
<path d="M18 106h32M34 98v16" {stroke(4)}/>''',
    "component/rotation-constraint": f'''<rect x="18" y="62" width="30" height="30" {stroke(5)}/>
<circle cx="94" cy="30" r="12" {stroke(5, ACCENT)}/>
<path d="M52 38a37 37 0 1 1 6 67M52 38l-1 17 16-2M83 44l-7 10" {stroke(5, ACCENT)}/>''',
    "component/scale-constraint": f'''<rect x="18" y="78" width="24" height="24" {stroke(5)}/>
<path d="M24 58V24h78v78H68M44 74l44-44M70 30h18v18" {stroke(5, ACCENT)}/>
<path d="M17 111h26" {stroke(4)}/>''',
    "component/aim-constraint": f'''<path d="m16 106 22-38 16 22-38 16Z" {stroke(5)}/>
<circle cx="93" cy="34" r="20" {stroke(5, ACCENT)}/>
<path d="M93 8v52M67 34h52M47 81l30-29" {stroke(4, ACCENT)}/>
<circle cx="93" cy="34" r="5" fill="{ACCENT}"/>''',
    "component/constant-force": f'''<path d="m18 56 25-14 25 14v29L43 100 18 85Z M18 56l25 14 25-14M43 70v30" {stroke(5)}/>
<path d="M58 42h50M94 28l14 14-14 14M48 25h34M33 16h32" {stroke(6, ACCENT)}/>
<path d="M83 99a25 25 0 0 0 22-27M94 77l11-5 6 11" {stroke(5, ACCENT)}/>''',
    # Irradiância recebida sobre ilhas UV: recurso diferente de uma luz emissora.
    "lighting/lightmap": f'''<rect x="16" y="44" width="82" height="66" rx="5" {stroke(5)}/>
<path d="m28 98 26-42 6 42ZM69 58l17 35-17 5Z" {stroke(4, ACCENT)}/>
<circle cx="96" cy="22" r="11" {stroke(5)}/>
<path d="M96 4v4M114 22h5M78 22h-5M96 38v4M109 9l4-4M82 9l-4-4M105 43l8 13M112 37l10 7" {stroke(4, ACCENT)}/>''',
    "editor/prefab-receive": f'''<path d="m16 36 25-14 25 14v29L41 80 16 65Z M16 36l25 14 25-14M41 50v30" {stroke(5)}/>
<path d="M78 24h30v67H70M82 80 69 91l13 11" {stroke(6, ACCENT)}/>''',
    # A lista mostra a composição; a lupa sobre propriedades abre a inspeção.
    "editor/components": f'''<path d="m16 28 19-11 19 11v22L35 61 16 50Z M16 28l19 11 19-11M35 39v22" {stroke(5)}/>
<path d="M69 28h42M69 44h32M69 76h42M69 92h32" {stroke(6)}/>
<path d="M18 77h31v31H18zM26 92h15M34 84v16" {stroke(5, ACCENT)}/>''',
    "editor/inspection": f'''<path d="M19 25h58M19 49h44M19 73h32M34 17v16M51 41v16M30 65v16" {stroke(6)}/>
<circle cx="80" cy="76" r="24" {stroke(7, ACCENT)}/>
<path d="m98 94 18 18M70 76h20M80 66v20" {stroke(6, ACCENT)}/>''',
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
    "physics/character-platform-carry": f'''<rect x="29" y="14" width="28" height="55" rx="14" {stroke(6)}/><path d="M13 102h69M43 83c15-27 34-42 65-43m-12-9 12 9-12 9" {stroke(6, ACCENT)}/>''',
    "physics/character-ground": f'''<rect x="29" y="15" width="30" height="57" rx="15" {stroke(6)}/><path d="M12 103h39V84h31V65h32M74 32v34M65 41l9-9 9 9M93 81v24M84 96l9 9 9-9" {stroke(6, ACCENT)}/>''',
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
    "event/tween-completion": f'''<path d="M12 91C22 91 21 36 36 36S45 91 58 91V32" {stroke(5)}/><path d="M48 34l9 10 16-24M65 68h22M79 60l9 8-9 8" {stroke(6, ACCENT)}/><rect x="95" y="48" width="22" height="40" rx="3" {stroke(5)}/><path d="M106 56v14" {stroke(5, ACCENT)}/>''',
    "event/physics-connection-2d": f'''<rect x="13" y="29" width="44" height="64" {stroke(5)}/><path d="M24 46h22M35 35v47M60 61h26M79 53l9 8-9 8" {stroke(6, ACCENT)}/><rect x="95" y="40" width="22" height="40" rx="3" {stroke(5)}/><path d="M106 48v14" {stroke(5, ACCENT)}/>''',
    "event/physics-connection": f'''<path d="M14 44l25-15 25 15v31L39 90 14 75V44zM14 44l25 15 25-15M39 59v31" {stroke(5)}/>
<path d="M67 60h22M83 52l9 8-9 8" {stroke(6, ACCENT)}/>
<rect x="95" y="40" width="22" height="40" rx="3" {stroke(5)}/><path d="M106 48v14" {stroke(5, ACCENT)}/>''',
    "event/timeout-connection": f'''<circle cx="35" cy="49" r="25" {stroke(6)}/>
<path d="M35 33v17l12 7M59 64h26M78 56l8 8-8 8" {stroke(6)}/>
<rect x="89" y="46" width="27" height="36" rx="3" {stroke(5)}/>
<circle cx="102" cy="63" r="5" fill="{ACCENT}"/>''',
    "scene/groups": f'''<path d="M37 48h54M37 80h54M37 48v32M91 48v32" {stroke(5)}/>
<rect x="18" y="26" width="34" height="34" rx="5" {stroke(6)}/><rect x="76" y="26" width="34" height="34" rx="5" {stroke(6)}/>
<rect x="18" y="72" width="34" height="34" rx="5" {stroke(6)}/><rect x="76" y="72" width="34" height="34" rx="5" {stroke(6, ACCENT)}/>''',
    "input/mouse": f'''<path d="M32 58v29a32 32 0 0 0 64 0V41a32 32 0 0 0-64 0v17z" {stroke(6)}/>
<path d="M32 58h64M64 9v49" {stroke(5)}/>
<rect x="59" y="26" width="10" height="18" rx="4" fill="{ACCENT}"/>''',
    "input/keyboard": f'''<rect x="12" y="30" width="104" height="68" rx="8" {stroke(6)}/>
<path d="M28 46h6m12 0h6m12 0h6m12 0h6m12 0h4M28 62h6m12 0h6m12 0h6m12 0h6m12 0h4" {stroke(6)}/>
<path d="M30 80h10m14 0h30m14 0h6" {stroke(6, ACCENT)}/>''',
    "input/gamepad": f'''<path d="M38 36h52c15 0 23 19 27 48 2 18-11 24-23 8L80 77H48L34 92C22 108 9 102 11 84c4-29 12-48 27-48Z" {stroke(6)}/>
<path d="M36 48v23M24 60h24" {stroke(6)}/>
<circle cx="91" cy="52" r="5" fill="{ACCENT}"/><circle cx="102" cy="64" r="5" fill="{ACCENT}"/>
<circle cx="55" cy="72" r="7" {stroke(4)}/><circle cx="73" cy="72" r="7" {stroke(4)}/>''',
    "component/lod-group": lod_group(),
    "component/skinned-mesh": skinned_mesh(),
    "component/camera-follow": camera_follow(),
}


# Mechanical joint silhouettes: attachment plates share a visual grammar; the
# lime centre describes the actual degrees of freedom rather than a generic gear.
MECHANISM_MARKS = {
    "fixed": '<path d="M43 48h42v32H43zM52 39v50M76 39v50"',
    "cone": '<path d="m64 37-28 50h56L64 37ZM64 37v40"',
    "swing-twist": '<path d="m64 32-29 51h58L64 32ZM49 91c26 13 40-9 22-19m0 0 13 1m-13-1 3 12"',
    "six-dof": '<path d="M35 64h58M64 35v58M43 85l42-42M35 64l9-7m-9 7 9 7M64 35l-7 9m7-9 7 9"',
    "spring": '<path d="M64 24v12l-16 8 32 12-32 12 32 12-16 10v14"',
}
for name, mark in MECHANISM_MARKS.items():
    ICONS["physics/joint-"+name] = f'<path d="M16 44v40M112 44v40M16 64h15M97 64h15" {stroke(6)}/>' + mark + f' {stroke(5, ACCENT)}/>'
SHAPES["cylinder"] = f'<ellipse cx="64" cy="34" rx="25" ry="10" {stroke(5)}/><path d="M39 34v48c0 14 50 14 50 0V34M39 82c0-14 50-14 50 0" {stroke(5)}/>'
for mode, mark in MODES.items():
    ICONS["physics/"+mode+"-cylinder"] = SHAPES["cylinder"] + mark

# Damped transform family: static structure in INK, spring response in ACCENT.
ICONS["component/spring-position"] = f'<path d="M20 100V26M20 100h80M20 100l30-30" {stroke(5)}/><circle cx="86" cy="38" r="14" {stroke(5)}/><path d="M33 86l9-19 10 10 9-19 10 10 8-19" {stroke(5,ACCENT)}/>'
ICONS["component/spring-rotation"] = f'<path d="M102 68a39 39 0 1 1-14-35M88 19v20h20" {stroke(5)}/><path d="M42 68l10-18 12 28 12-28 10 18" {stroke(5,ACCENT)}/>'
ICONS["component/spring-scale"] = f'<path d="M22 22h84v84H22zM22 74h32v32" {stroke(5)}/><path d="M47 81l7-18 10 10 7-18 10 10 11-29M77 36h15v15" {stroke(5,ACCENT)}/>'

# Physical volume family: boundary in INK, field action in ACCENT.
field_box = f'<path d="M20 34l28-16 60 24v54l-28 16-60-24zM20 34l60 24 28-16M80 58v54" {stroke(4)}/>'
ICONS["physics/field-gravity"] = field_box + f'<path d="M57 40v48M44 74l13 14 13-14" {stroke(6,ACCENT)}/>'
field_2d = f'<path d="M18 20h88v74H18z" {stroke(4)}/><path d="M83 104h11l-11 15h11M101 104h5a7.5 7.5 0 0 1 0 15h-5z" {stroke(3)}/>'
ICONS["physics/field-gravity-2d"] = field_2d + f'<path d="M62 34v45M49 66l13 13 13-13" {stroke(6,ACCENT)}/>'
ICONS["physics/field-wind-2d"] = field_2d + f'<path d="M29 40h40c20 0 20-12 10-12M29 57h66M29 74h40c20 0 20 12 10 12" {stroke(5,ACCENT)}/>'
ICONS["physics/field-drag-2d"] = field_2d + f'<circle cx="42" cy="57" r="13" {stroke(5)}/><path d="M65 47v20M77 42v30M91 36v42" {stroke(5,ACCENT)}/>'
ICONS["physics/field-radial-2d"] = f'<circle cx="62" cy="54" r="41" {stroke(4)}/><path d="M34 26l18 18M42 44h10V34M90 82L72 64M72 76V64h12M86 23a41 41 0 0 1 15 23M93 40l8 6 5-12M83 104h11l-11 15h11M101 104h5a7.5 7.5 0 0 1 0 15h-5z" {stroke(4,ACCENT)}/>'
ICONS["physics/field-wind"] = field_box + f'<path d="M26 49h51c20 0 20-22 5-22M28 64h68M31 79h46c20 0 20 20 5 20" {stroke(5,ACCENT)}/>'
ICONS["physics/field-drag"] = field_box + f'<circle cx="43" cy="64" r="13" {stroke(5)}/><path d="M65 54v20M77 49v30M91 43v42" {stroke(5,ACCENT)}/>'
ICONS["physics/field-radial"] = f'<circle cx="64" cy="64" r="45" {stroke(4)}/><ellipse cx="64" cy="64" rx="45" ry="19" {stroke(3)}/><circle cx="64" cy="64" r="6" fill="{ACCENT}"/><path d="M32 32l20 20M42 52h10V42M96 96L76 76M76 86V76h10M93 28a45 45 0 0 1 15 28M98 48l10 8 5-12" {stroke(5,ACCENT)}/>'

ICONS["path/orientation"] = f'<path d="M18 88c25-31 53-35 92-12" {stroke(6)}/><circle cx="60" cy="66" r="6" fill="{INK}"/><path d="M60 59V20M49 32l11-12 11 12M80 28c15 7 22 21 17 35M87 56l10 7 9-8" {stroke(5,ACCENT)}/>'
ICONS["input/response"] = f'<path d="M36 79V42a8 8 0 0 1 16 0v31l6-9c3-5 11-3 11 3v-6c0-7 12-7 12 0v7c0-7 12-7 12 0v19c0 15-9 23-24 23H56L29 86c-7-9 0-18 7-7Z" {stroke(5)}/><path d="M75 18a27 27 0 1 1 22 41M91 22v17l10 7" {stroke(5,ACCENT)}/>'
# Conexão de evento: emissor com ondas de sinal (INK), raio e seta que levam à
# ação no receptor (ACCENT). Genérico de propósito: não é sensor nem timer.
# Receita "Gatilho sonoro": sensor de caixa tracejado (INK) e ondas de som (ACCENT).
ICONS["event/sound-trigger"] = f'<path d="M18 40h40v48H18z" {stroke(5, extra='stroke-dasharray="8 7"')}/><path d="M66 56h10l14-12v40L76 72H66z" {stroke(5)}/><path d="M98 50a20 20 0 0 1 0 28M106 40a32 32 0 0 1 0 48" {stroke(5,ACCENT)}/><circle cx="38" cy="64" r="6" fill="{ACCENT}"/>'
ICONS["physics/material"] = f'<rect x="12" y="84" width="104" height="26" rx="4" {stroke(5)}/><path d="M24 110l10-26M44 110l10-26M64 110l10-26M84 110l10-26M104 110l8-21" {stroke(4)}/><circle cx="92" cy="40" r="12" {stroke(5,ACCENT)}/><path d="M20 30c10 0 16 50 30 50s20-34 30-40" {stroke(5,ACCENT)}/><path d="M44 80h12" {stroke(5,ACCENT)}/>'
ICONS["component/tween-sequence"] = f'<path d="M12 112h104M36 106v12M64 106v12M92 106v12" {stroke(4)}/><rect x="10" y="22" width="28" height="20" rx="4" {stroke(5)}/><path d="M38 32h12M45 26l6 6-6 6" {stroke(5)}/><rect x="52" y="22" width="28" height="20" rx="4" {stroke(5,ACCENT)}/><rect x="52" y="52" width="28" height="20" rx="4" {stroke(5,ACCENT)}/><path d="M80 32h12M87 26l6 6-6 6" {stroke(5,ACCENT)}/><rect x="94" y="22" width="24" height="20" rx="4" {stroke(5)}/><path d="M106 42v44H24V48M18 54l6-6 6 6" {stroke(4)}/>'
ICONS["component/event-connection"] = f'<circle cx="20" cy="64" r="10" {stroke(6)}/><path d="M36 46a26 26 0 0 1 0 36M45 36a38 38 0 0 1 0 56" {stroke(5)}/><path d="M70 38l-9 26h13l-9 26" {stroke(6,ACCENT)}/><path d="M80 64h10M85 57l7 7-7 7" {stroke(5,ACCENT)}/><rect x="98" y="46" width="20" height="36" rx="4" {stroke(5)}/><circle cx="108" cy="64" r="3.5" fill="{INK}"/>'

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
        if png.exists():
            png.unlink()
        try:
            subprocess.run([executable, "--headless=new", "--disable-gpu", "--hide-scrollbars",
                            "--default-background-color=00000000", "--window-size=512,512",
                            f"--screenshot={png.resolve()}", page.resolve().as_uri()],
                           check=True, capture_output=True, timeout=60)
        except subprocess.TimeoutExpired:
            # Com outra sessão do navegador aberta, o headless às vezes grava a
            # captura e não encerra; o processo é morto pelo timeout. Vale a
            # captura nova (o arquivo antigo foi apagado acima); sem ela, falha.
            if not png.exists():
                raise
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
