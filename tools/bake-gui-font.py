"""Build the Dear ImGui font from the same licensed source as the native editor.

Dependencies: fonttools[woff] (fonttools and brotli). Run from the repository root.
The generated static instance retains the original font names and OFL license.
"""
from pathlib import Path
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

root = Path(__file__).resolve().parent.parent
font = TTFont(root / "assets/astra-visual/fonts/Inter-Variable.woff2")
instantiateVariableFont(font, {"wght": 400, "opsz": 14}, inplace=True)
font.flavor = None
output = root / "assets/astra-visual/ui/gui-inter.ttf"
font.save(output)
print(f"{output}: {output.stat().st_size} bytes")
