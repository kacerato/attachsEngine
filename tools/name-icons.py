#!/usr/bin/env python3
"""Materializa a árvore de ícones nomeados a partir de icons/naming.json.

O fatiamento produz rótulos posicionais (A01, B14). Eles são certos para
conferir a folha e errados para o código: `icon_03_07` não diz nada em um
`if` seis meses depois, e renomear um ícone quebraria todo uso. Esta etapa
separa as duas coisas — a posição continua sendo a verdade da folha, e o nome
semântico é o que a interface referencia.

Rótulos marcados como `duplicate` não entram na árvore. Eles são o MESMO desenho
que outro já nomeado, repetido em outra folha; publicá-los com um sufixo
inventado daria dois nomes para uma coisa só e a próxima pessoa escolheria o
errado. Eles continuam em icons/sliced, acessíveis pelo rótulo.

Uso:
    python tools/name-icons.py
"""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path

EXPORT_SIZES = (96, 64, 48, 32, 24)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path("assets/astra-visual/icons")
    parser.add_argument("--naming", type=Path, default=root / "naming.json")
    parser.add_argument("--sliced", type=Path, default=root / "sliced")
    parser.add_argument("--output", type=Path, default=root / "named")
    arguments = parser.parse_args()

    naming = json.loads(arguments.naming.read_text(encoding="utf-8"))
    mapping = naming["icons"]

    if arguments.output.exists():
        shutil.rmtree(arguments.output)

    catalogue: dict[str, dict] = {}
    duplicates: dict[str, str] = {}
    missing: list[str] = []
    collisions: list[str] = []

    for label, target in mapping.items():
        master = arguments.sliced / f"{label}.png"
        if not master.exists():
            missing.append(label)
            continue
        if isinstance(target, dict):
            duplicates[label] = target["duplicate"]
            continue
        if target in catalogue:
            # Dois rótulos disputando o mesmo nome é erro de mapeamento, não algo
            # a resolver na hora: o segundo sobrescreveria o primeiro em silêncio.
            collisions.append(f"{target} ({catalogue[target]['label']} e {label})")
            continue

        category, _, name = target.partition("/")
        directory = arguments.output / category
        directory.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(master, directory / f"{name}.png")
        for size in EXPORT_SIZES:
            source = arguments.sliced / f"{label}-{size}.png"
            if source.exists():
                shutil.copyfile(source, directory / f"{name}-{size}.png")
        catalogue[target] = {"label": label, "category": category, "name": name}

    report = {
        "icons": catalogue,
        "duplicates": duplicates,
        "unmapped": sorted(
            path.stem
            for path in arguments.sliced.glob("[A-Z][0-9][0-9].png")
            if path.stem not in mapping
        ),
    }
    (arguments.output / "catalog.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False, sort_keys=True), encoding="utf-8"
    )

    print(f"nomeados: {len(catalogue)}")
    print(f"duplicados (fora da arvore): {len(duplicates)}")
    if report["unmapped"]:
        print(f"sem nome: {', '.join(report['unmapped'])}")
    if missing:
        print(f"AVISO rotulos sem fatia: {', '.join(missing)}")
    if collisions:
        print(f"ERRO nomes repetidos: {'; '.join(collisions)}")
        raise SystemExit(1)
    for category in sorted({entry["category"] for entry in catalogue.values()}):
        count = sum(1 for entry in catalogue.values() if entry["category"] == category)
        print(f"  {category}/: {count}")


if __name__ == "__main__":
    main()
