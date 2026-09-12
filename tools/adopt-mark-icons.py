#!/usr/bin/env python3
"""Aponta o catalogo para os icones no idioma da marca.

O enum NAO muda: o nome de catalogo continua o mesmo e so a arte por tras dele
troca. E por isso que esta adocao nao toca em uma linha de codigo -- o
identificador do icone e um indice, e o indice vem do nome.

Cada entrada ganha `dark_ui_ready`. Os desenhos ja saem em lima sobre
transparente; passa-los pela inversao de luminancia que a geracao anterior exige
os deixaria escuros sobre um painel escuro.

Adotados sao os icones que a interface REALMENTE usa hoje. Os outros 115 nomes
do catalogo continuam na arte anterior: desenhar um por um sem consumidor seria
trabalho sem leitor, e o M06.2 dira quais deles a reconstrucao do IDE precisa.
"""
from __future__ import annotations

import json
from pathlib import Path

SOURCE = "../mark-v1"

ADOPTED = {
    "editor/author-select": "select",
    "editor/author-move": "move",
    "editor/author-rotate": "rotate",
    "editor/author-scale": "scale",
    "editor/author-object": "object",
    "editor/author-camera": "camera",
    "editor/author-frame": "frame",
    "editor/author-grid": "grid",
    "editor/author-orbit": "orbit",
    "editor/author-pan": "pan",
    "editor/author-zoom": "zoom",
    "editor/author-eye": "eye",
    "editor/author-eye-off": "eye-off",
    "editor/author-folder": "folder",
    "editor/author-more": "more",
    "editor/author-play": "play",
    "editor/author-stop": "stop",
    "editor/author-undo": "undo",
    "editor/author-redo": "redo",
    "editor/author-settings": "settings",
    "editor/author-sun": "sun",
    "editor/author-add": "add",
    "editor/author-chevron": "chevron",
    "component/add": "component-add",
    "component/character": "character",
    "component/collider": "collider",
    "component/joint": "joint",
    "component/look": "look",
    "component/physics": "physics",
    "scripting/code": "code",
    "lighting/sun": "light",
    "assets/file": "file",
    "assets/save": "save",
    "assets/search": "search",
    "vfx/particles": "particles",
    "water/author-surface": "water-surface",
    "water/author-physics": "water-physics",
    "water/author-layers": "water-layers",
    "water/author-route": "water-route",
}


def main() -> None:
    catalogue_path = Path("assets/astra-visual/icons/named/catalog.json")
    catalogue = json.loads(catalogue_path.read_text(encoding="utf-8"))
    drawings = Path("assets/astra-visual/icons/mark-v1")

    missing = [name for name in ADOPTED if name not in catalogue["icons"]]
    if missing:
        raise SystemExit(f"nomes fora do catalogo: {missing}")
    absent = [leaf for leaf in ADOPTED.values() if not (drawings / f"{leaf}.png").exists()]
    if absent:
        raise SystemExit(f"desenhos ausentes: {absent}")

    for name, leaf in ADOPTED.items():
        entry = catalogue["icons"][name]
        entry.pop("source", None)
        entry["raster"] = f"{SOURCE}/{leaf}.png"
        entry["dark_ui_ready"] = True
        entry["generation"] = "mark-v1"
    catalogue_path.write_text(json.dumps(catalogue, ensure_ascii=False, indent=2) + "\n",
                              encoding="utf-8")
    print(f"{len(ADOPTED)} icones adotados da geracao mark-v1")


if __name__ == "__main__":
    main()
