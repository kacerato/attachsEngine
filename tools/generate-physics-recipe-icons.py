#!/usr/bin/env python3
"""Generate the six physics recipe glyphs for the named Astra icon atlas.

Requires cairosvg only when regenerating the committed PNG files.
"""

from pathlib import Path

import cairosvg


ROOT = Path("assets/astra-visual/icons/named/physics")
INK = "#e8f0ed"
ACCENT = "#c6ef62"

SHAPES = {
    "sphere": f'''<circle cx="64" cy="59" r="24" stroke="{INK}" stroke-width="6"/>
<path d="M42 49c12 7 32 7 44 0M42 69c12-7 32-7 44 0" stroke="{INK}" stroke-width="3" opacity=".7"/>
<path d="M64 35c-10 12-10 36 0 48M64 35c10 12 10 36 0 48" stroke="{INK}" stroke-width="3" opacity=".7"/>''',
    "capsule": f'''<rect x="46" y="26" width="36" height="66" rx="18" stroke="{INK}" stroke-width="6"/>
<path d="M48 45c9 6 23 6 32 0M48 73c9-6 23-6 32 0" stroke="{INK}" stroke-width="3" opacity=".7"/>''',
    "box": f'''<path d="M40 39h48v48H40z" stroke="{INK}" stroke-width="6" stroke-linejoin="round"/>
<path d="M40 39l12-12h48L88 39M88 39l12-12v48L88 87" stroke="{INK}" stroke-width="4" opacity=".7" stroke-linejoin="round"/>''',
}

MODES = {
    "dynamic": f'''<path d="M17 48h19M12 61h20M20 74h16" stroke="{ACCENT}" stroke-width="5" stroke-linecap="round"/>
<path d="M94 59h18m-8-8 8 8-8 8" stroke="{ACCENT}" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/>''',
    "sensor": f'''<circle cx="64" cy="59" r="40" stroke="{ACCENT}" stroke-width="4" stroke-dasharray="7 7"/>
<path d="M57 109h14M64 102v14" stroke="{ACCENT}" stroke-width="5" stroke-linecap="round"/>''',
    "kinematic": f'''<path d="M22 33V20h13M93 20h13v13M22 85v13h13M93 98h13V85" stroke="{ACCENT}" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M64 99v15m-6-6 6 6 6-6" stroke="{ACCENT}" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>''',
}


def main() -> None:
    ROOT.mkdir(parents=True, exist_ok=True)
    recipes = (
        ("dynamic-sphere", "dynamic", "sphere"),
        ("dynamic-capsule", "dynamic", "capsule"),
        ("sensor-sphere", "sensor", "sphere"),
        ("sensor-capsule", "sensor", "capsule"),
        ("kinematic-box", "kinematic", "box"),
        ("kinematic-sphere", "kinematic", "sphere"),
    )
    for name, mode, shape in recipes:
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" '
               f'viewBox="0 0 128 128" fill="none">\n'
               f'{SHAPES[shape]}\n{MODES[mode]}\n</svg>\n')
        (ROOT / f"{name}.svg").write_text(svg, encoding="utf-8")
        cairosvg.svg2png(bytestring=svg.encode(), write_to=str(ROOT / f"{name}.png"))


if __name__ == "__main__":
    main()
