#!/usr/bin/env python3
"""Aponta as entradas do catalogo para a geracao HD v2.

O enum NAO muda: o nome de catalogo continua o mesmo e so a arte por tras dele
troca. E por isso que esta adocao nao toca em uma linha de codigo -- o
identificador do icone e um indice, e o indice vem do nome.

Cada entrada adotada ganha `dark_ui_ready`. As folhas v2 ja foram desenhadas
SOBRE fundo escuro, com brilho externo; passa-las pela inversao de luminancia
que a v1 exige apagaria exatamente o que elas tem de bom.

O que NAO esta aqui esta de fora de proposito. `assets/file` continua na arte
antiga porque a folha v2 nao tem um documento simples -- o mais proximo traz um
sinal de mais, e um arquivo que ja existe com um "+" desenhado nele e pior do
que um icone de outra geracao. `editor/author-orbit` tambem fica: o globo v2 tem
corpo escuro e desaparece dentro da pastilha acesa, que e exatamente onde a
ferramenta ativa precisa ser vista.
"""
from __future__ import annotations

import json
from pathlib import Path

SLICES = "../source/hd-v2/sliced"

ADOPTED = {
    # Barra de autoria e viewport.
    "editor/author-camera": "sheet-1-00",
    "editor/author-object": "sheet-1-02",
    "editor/author-select": "sheet-3-00",
    "editor/author-move": "sheet-3-01",
    "editor/author-rotate": "sheet-3-02",
    "editor/author-scale": "sheet-3-03",
    "editor/author-frame": "sheet-3-11",
    "editor/author-grid": "sheet-3-06",
    "editor/author-zoom": "sheet-4-06",
    "editor/author-eye": "sheet-2-08",
    "editor/author-eye-off": "sheet-2-09",
    "editor/author-folder": "sheet-7-04",
    "editor/author-more": "sheet-4-07",
    "editor/author-play": "sheet-5-11",
    "editor/author-stop": "sheet-5-13",
    "editor/author-undo": "sheet-5-05",
    "editor/author-redo": "sheet-5-06",
    "editor/author-settings": "sheet-4-09",
    "editor/author-sun": "sheet-1-11",
    # Componentes.
    "component/character": "sheet-1-08",
    "component/collider": "sheet-1-03",
    "component/joint": "sheet-1-04",
    "component/look": "sheet-3-10",
    "component/physics": "sheet-2-00",
    "component/add": "sheet-2-03",
    "lighting/sun": "sheet-1-01",
    "scripting/code": "sheet-4-01",
    # Recursos: ainda nao todos em uso, mas o M06.2 vai precisar deles e a arte
    # ja existe. Adotar agora evita uma segunda passada de meia geracao.
    "assets/folder": "sheet-7-04",
    "assets/folder-open": "sheet-4-02",
    "assets/material": "sheet-7-08",
    "assets/texture": "sheet-7-09",
    "assets/video": "sheet-7-10",
    "assets/search": "sheet-4-06",
    "assets/filter": "sheet-6-03",
    "assets/import": "sheet-7-00",
    "assets/save": "sheet-5-03",
    "assets/package": "sheet-2-01",
    "assets/static-mesh": "sheet-7-06",
    "assets/file-mesh": "sheet-7-07",
}


def main() -> None:
    catalogue_path = Path("assets/astra-visual/icons/named/catalog.json")
    catalogue = json.loads(catalogue_path.read_text(encoding="utf-8"))
    sliced = Path("assets/astra-visual/icons/source/hd-v2/sliced")

    missing = [name for name in ADOPTED if name not in catalogue["icons"]]
    if missing:
        raise SystemExit(f"nomes fora do catalogo: {missing}")
    absent = [slice_name for slice_name in ADOPTED.values()
              if not (sliced / f"{slice_name}.png").exists()]
    if absent:
        raise SystemExit(f"fatias ausentes: {absent}")

    for name, slice_name in ADOPTED.items():
        entry = catalogue["icons"][name]
        entry.pop("source", None)
        entry["raster"] = f"{SLICES}/{slice_name}.png"
        entry["dark_ui_ready"] = True
        entry["generation"] = "hd-v2"
    catalogue_path.write_text(json.dumps(catalogue, ensure_ascii=False, indent=2) + "\n",
                              encoding="utf-8")
    print(f"{len(ADOPTED)} icones adotados da geracao hd-v2")


if __name__ == "__main__":
    main()
