#!/usr/bin/env python3
"""Gera native/editor/editor_script_templates.h a partir de assets/script-templates.

A fonte da verdade e o CODIGO C# em assets/script-templates: ele e compilado
pelo mesmo compilador do projeto do usuario no teste
`AstraTemplateTests.ModelosDeComportamentoCompilamComOSchemaEsperado`. O header
gerado existe porque o editor roda no Android, sem a arvore de fontes por perto.

Reexecutar depois de editar qualquer modelo:
    python tools/build-script-templates.py
"""
from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "assets" / "script-templates"
OUTPUT = ROOT / "native" / "editor" / "editor_script_templates.h"
CONTRACTS = "Contratos.cs"


def escape(text: str) -> str:
    """Literal C++ seguro: sem depender de raw string nem de trigrafos."""
    out = []
    for ch in text:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append('\\n"\n    "')
        elif ch == "\r":
            continue
        elif ch == "?":
            out.append("\\?")
        else:
            out.append(ch)
    return "".join(out)


def read_entries() -> list[tuple[str, str, str]]:
    entries = []
    for line in (SOURCE / "templates.txt").read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("|")
        if len(parts) != 3:
            raise SystemExit(f"linha invalida em templates.txt: {line}")
        entries.append((parts[0].strip(), parts[1].strip(), parts[2].strip()))
    return entries


def main() -> int:
    entries = read_entries()
    contracts = (SOURCE / CONTRACTS).read_text(encoding="utf-8")
    blocks = []
    for klass, name, description in entries:
        path = SOURCE / f"{klass}.cs"
        if not path.exists():
            raise SystemExit(f"modelo ausente: {path}")
        text = path.read_text(encoding="utf-8")
        needs = "IInteragivel" in text or "IColetavel" in text
        blocks.append((klass, name, description, needs, text))

    lines = [
        "// GERADO por tools/build-script-templates.py — nao editar a mao.",
        "//",
        "// Os modelos de comportamento que o editor oferece ao criar um script.",
        "// A fonte e assets/script-templates/*.cs, compilada no teste gerenciado",
        "// AstraTemplateTests: um modelo que nao compila nunca chega aqui.",
        "//",
        "// `className` e substituido pelo nome que o usuario digitar, no codigo e",
        "// no ComponentId. `contracts` marca os modelos que precisam do arquivo de",
        "// contratos, criado junto quando o projeto ainda nao o tem.",
        "#pragma once",
        '#include <array>',
        '#include <string_view>',
        "",
        "namespace ae::editor {",
        "struct EditorScriptTemplate {",
        "  std::string_view className;",
        "  std::string_view name;",
        "  std::string_view description;",
        "  bool contracts;",
        "  std::string_view source;",
        "};",
        "",
        "inline constexpr std::string_view kEditorScriptContractsFile = \"Contratos.cs\";",
        "inline constexpr std::string_view kEditorScriptContracts =",
        f'    "{escape(contracts)}";',
        "",
        f"inline constexpr std::array<EditorScriptTemplate,{len(blocks)}> editorScriptTemplates{{{{",
    ]
    for index, (klass, name, description, needs, text) in enumerate(blocks):
        comma = "," if index + 1 < len(blocks) else ""
        lines.append("  {")
        lines.append(f'    "{klass}",')
        lines.append(f'    "{name}",')
        lines.append(f'    "{description}",')
        lines.append(f"    {'true' if needs else 'false'},")
        lines.append(f'    "{escape(text)}"')
        lines.append(f"  }}{comma}")
    lines.append("}};")
    lines.append("")
    lines.append("} // namespace ae::editor")
    lines.append("")
    OUTPUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"{OUTPUT.relative_to(ROOT)}: {len(blocks)} modelos")
    return 0


if __name__ == "__main__":
    sys.exit(main())
