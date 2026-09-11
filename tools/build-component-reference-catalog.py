"""Generate reference documentation, never execute/validate Astra binaries.

Atualizado em 10/09/2026: os caminhos de física e scripts passaram para
`native/runtime/`, e a antiga frase "sem build/execução" foi substituída pelo
que vale hoje — compilado e coberto por testes de host, SEM validação no
aparelho. Cobertura de host não é aprovação de família no Android.

Inputs are primary-source snapshots plus the individually curated TSV. This
does not add types to the engine registry or establish runtime support.
"""
import argparse
import collections
import json
from pathlib import Path
import re
from urllib.parse import quote
import zipfile

ROOT = Path(__file__).resolve().parents[1]
UNITY_REV = "a2a4a31aee6dfb63c2ef36eea79d817a6e31349b"
ITSMAGIC_REV = "d977bf8765c72567e221acb845934d30b070ac43"
STAGES = {
    "A": ("Fundamentos e código", "M2 / S0–S5", "Identidade, arquivo v10, registro, buffers, compilador e lifecycle; não depende dos módulos gráficos adicionais."),
    "B": ("Composição visual e recursos", "M2–M5", "A; importação com IDs de assets, renderer de cena e material compartilhado/override."),
    "C": ("Física universal 3D", "M6 / S5", "A+B; shapes, ownership, mundo único, comandos e eventos físicos."),
    "D": ("Animação, áudio e comportamentos", "M7 / S5–S6", "A+B+C; relógios, binding de propriedades e ciclo de vida de recursos."),
    "E": ("Ambiente, efeitos e streaming", "M5 / M8", "B; rendergraph de cena, alvos, histórico, HDR e orçamento de GPU."),
    "F": ("2D", "Expansão após fundamentos", "A+B; renderer e backend físico 2D próprios, assets de sprites/tiles."),
    "G": ("Interface de jogo", "M7–M9 / S6", "A+B+D; árvore de UI, layout, input/foco e recursos tipográficos."),
    "H": ("Terreno, caminhos e navegação", "M8–M9", "B+C+D; jobs, assets derivados, edição por regiões e invalidação."),
    "J": ("Módulos especializados e distribuição", "M9–M11 / S7–S8", "A–H conforme o módulo; backend comprovado, ciclo Android, empacotamento e licença."),
    "R": ("Semântica de referência ainda insuficiente", "Investigação explícita", "Consultar fonte/demonstração primária para resolver o que a página não especifica; não cadastrar um placeholder."),
}
BASES = {"Component", "Behaviour", "MonoBehaviour", "Renderer", "Collider", "Collider2D", "Joint", "Joint2D", "Effector2D", "GridLayout", "Light2DBase", "AnchoredJoint2D"}
PARTIAL = {
    "Transform3D": ("native/editor/editor_document.h", "TRS e hierarquia existentes; ainda não componente independente."),
    "ComponentRegistry e ScriptBehavior": ("native/scene/components.h;native/scene/script_behavior.h;managed/Astra.Scripting", "Novo código de instâncias, schema, compilação e Play; sem compilação nem execução autorizadas neste bloco."),
    "PhysicsBody3D": ("native/scene/physics_body.h;native/runtime/scene_physics.cpp", "Corpo v3 com composição, sensor, massa, velocidades, damping, gravidade, repouso, comandos e eventos C#; código compilado e coberto por testes de host; sem validação no aparelho. Filtros e mutação geral em Play pendentes."),
    "Collider3D Caixa": ("native/scene/collider.h;native/runtime/scene_physics.cpp", "Caixa repetível com centro/rotação locais, owner explícito neste objeto ou ancestral, compound Jolt e sugestão por vértices; compilado e coberto por testes de host; sem validação no aparelho."),
    "Collider3D Esfera": ("native/scene/collider.h;native/runtime/scene_physics.cpp", "Esfera repetível, pose local e owner explícito em compound Jolt; sugestão geométrica. Sem build/execução; escala global uniforme exigida."),
    "Collider3D Cápsula": ("native/scene/collider.h;native/runtime/scene_physics.cpp", "Cápsula Y rotacionável/repetível com pose local e owner explícito, ajuste geométrico e compound; compilado e coberto por testes de host; sem validação no aparelho. Escala global uniforme exigida."),
    "Collider3D e ShapeAsset": ("native/scene/collider.h", "Caixa/esfera/cápsula repetíveis, pose local, owner explícito, composição e sensor no corpo. Sem build/execução. ShapeAsset, malha, convexos e filtros por instância pendentes."),
    "Joint3D Ponto": ("native/scene/joint.h;native/runtime/scene_physics.cpp", "Junta repetível por ponto, dois corpos por referência, âncoras locais, undo/remapeamento e consumidor Jolt; compilado e coberto por testes de host; sem validação no aparelho. Ruptura e edição em Play pendentes."),
    "Joint3D Dobradiça": ("native/scene/joint.h;native/runtime/scene_physics.cpp", "Dobradiça repetível, eixos/âncoras, limites em graus e motores ligados ao Jolt; compilado e coberto por testes de host; sem validação no aparelho. Ruptura, frames completos e atualização de motor em Play pendentes."),
    "CharacterMotor3D": ("native/editor/editor_character.h;native/runtime/scene_physics.cpp", "Cápsula/movimento no mundo compartilhado; input e API de personagem ainda parciais."),
    "Camera": ("native/scene/camera.h;native/editor/editor_scene_camera.h;native/platform/android/instanced_renderer.cpp", "Componente anexável, prioridade, FOV e planos ligados ao consumidor e aos fontes GLSL; compilado e coberto por testes de host; sem validação no aparelho. Ortográfica, roll, múltiplas saídas e render targets pendentes."),
    "MeshRenderer": ("native/scene/mesh_renderer.h;native/editor/editor_map_scene.cpp;native/editor/editor_screen.cpp", "Componente anexável, escolha de geometria do pacote, material por instância e migração v10 escritos; compilado e coberto por testes de host; sem validação no aparelho. GUIDs, submeshes, materiais compartilhados e miniaturas renderizadas pendentes."),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--snapshots", type=Path, default=ROOT / "build/component-reference-research")
    args = parser.parse_args()
    reference = args.snapshots
    def read(name):
        return json.loads((reference / name).read_text(encoding="utf-8"))
    profiles = {}
    for line in (ROOT / "docs/componentes/perfis.tsv").read_text(encoding="utf-8").splitlines()[1:]:
        if not line.strip():
            continue
        keys, purpose, use, astra, needs, stage, work = line.split("|")
        for key in keys.split(","):
            profiles[key] = dict(function=purpose, useCase=use, astraTarget=astra, dependencies=needs, stage=stage, implementation=work)
    toc = {x["name"]: x for x in read("unity-runtime-classes-toc.json")}
    entries, excluded = [], []
    for value in read("unity-components-source.json"):
        if value["name"] not in toc:
            excluded.append(value)
            continue
        item = dict(engine="Unity", version="6000.0", revision=UNITY_REV, name=value["name"],
                    category=toc[value["name"]]["category"], base=value["base"], abstract=value["abstract"],
                    kind="base" if value["name"] in BASES or value["abstract"] else "component-or-engine-helper",
                    sources=["https://docs.unity3d.com/6000.0/Documentation/ScriptReference/" + toc[value["name"]]["link"] + ".html",
                             "https://github.com/Unity-Technologies/UnityCsReference/blob/" + UNITY_REV + "/" + value["source"]])
        item.update(profiles.get("U:" + value["name"], {}))
        entries.append(item)
    magic = collections.defaultdict(list)
    for item in read("itsmagic-components-source.json"):
        magic[item["name"]].append(item)
    with zipfile.ZipFile(reference / "itsmagic.zip") as archive:
        paths = archive.namelist()
        for name, aliases in sorted(magic.items()):
            canonical = next((a for a in aliases if "/Components/" in a["source"]), aliases[0])
            source = canonical["source"]
            text = archive.read(next(p for p in paths if p.endswith("/" + source))).decode("utf-8")
            item = dict(engine="ItsMagic", version="Documentação 2.0", revision=ITSMAGIC_REV, name=name,
                        category=canonical["category"], kind="component", package="JAVARuntime",
                        sources=["https://itsmagic.com.br/documentation/" + quote(source.removesuffix(".mdx")) + "/",
                                 "https://github.com/ITsMagic-Software/Documentation/blob/" + ITSMAGIC_REV + "/" + quote(source)],
                        aliases=sorted({a["source"] for a in aliases}),
                        signatures=sorted(set(re.findall(r'signature:\s*"([^"]+)"', text))),
                        attributes=[dict(name=n, type=t) for n, t in re.findall(r'name:\s*"([^"]+)"\s*,\s*type:\s*"([^"]+)"', text)],
                        referenceLimit="Descrições da API são frequentemente automáticas; assinatura/atributo não comprova algoritmo interno ou resultado em dispositivo.")
            item.update(profiles.get("I:" + name, {}))
            entries.append(item)
    missing = [x["engine"] + ":" + x["name"] for x in entries if "function" not in x]
    if missing:
        raise SystemExit("Perfis de referência ainda necessários: " + ", ".join(missing))
    for item in entries:
        partial = PARTIAL.get(item["astraTarget"])
        item["astraStatus"] = "parcial" if partial else "sem implementação universal vinculada"
        item["astraEvidence"] = partial[0].split(";") if partial else []
        item["remaining"] = partial[1] if partial else item["implementation"]
        item["validation"] = "Este catálogo não autoriza nem executa testes; alterações novas não compiladas/não executadas."
        if item["stage"] == "R":
            item["astraStatus"] = "pendente de investigação e implementação"
        if item["engine"] == "ItsMagic":
            item["functionEvidence"] = "Interpretação explícita da API pública; limitações indicadas na função e em referenceLimit."
        else:
            item["functionEvidence"] = "API pública e classe da versão fixada; correspondência Astra é proposta de arquitetura."
    result = dict(schemaVersion=1, date="2026-09-10", coverage={
        "Unity": "117 tipos no cruzamento entre descendentes públicos de Component no CsReference e índice da API 6000.0; inclui bases/auxiliares rotulados, não 117 botões Add.",
        "ItsMagic": "153 nomes únicos marcados Component: yes; 203 registros antes de remover duplicações de categoria.",
        "pendingUnityPackages": ["uGUI/TextMeshPro", "Cinemachine", "AI Navigation", "Input System", "URP", "HDRP", "Animation Rigging", "Splines", "XR", "Netcode"],
        "boundary": "Pacotes/versões adicionais, Asset Store e scripts de usuário ampliam o catálogo; não há alegação de inventário fechado de todo o ecossistema Unity."},
        stages={k: dict(name=v[0], plan=v[1], prerequisites=v[2]) for k, v in STAGES.items()},
        excludedUnitySourceCandidates=excluded, entries=entries)
    output = ROOT / "docs/componentes"
    (output / "catalogo.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for engine in ("Unity", "ItsMagic"):
        rows = [x for x in entries if x["engine"] == engine]
        lines = [f"# {engine} → Astra: quadro por tipo", "", f"{len(rows)} entradas. Data: 10/09/2026. [Critérios, etapas e limites](README.md).", "",
                 "Função e caso descrevem a referência. Alvo e trabalho descrevem a direção da Astra; existência nesta lista não cadastra nem implementa um componente.", ""]
        for stage, (title, plan, prerequisites) in STAGES.items():
            selected = sorted((x for x in rows if x["stage"] == stage), key=lambda x: x["name"])
            if not selected:
                continue
            lines += [f"## {stage} — {title}", "", f"Plano: {plan}. Dependências comuns: {prerequisites}", "",
                      "| Tipo / fonte | Função e quando usar | Alvo Astra | Necessita | Trabalho específico / situação |",
                      "|---|---|---|---|---|"]
            for item in selected:
                name = f"[{item['name']}]({item['sources'][0]})"
                if item["kind"] == "base": name += " (base)"
                details = item["implementation"] + ". **" + item["astraStatus"] + "**."
                if item["astraEvidence"]: details += " " + item["remaining"]
                lines.append("| " + " | ".join([name, item["function"] + ". Uso: " + item["useCase"], item["astraTarget"], item["dependencies"], details]) + " |")
            lines.append("")
        (output / (engine.lower() + ".md")).write_text("\n".join(lines), encoding="utf-8")
    print(f"Documentação gerada: {len(entries)} entradas, nenhum componente cadastrado ou teste executado.")


if __name__ == "__main__":
    main()
