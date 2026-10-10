"""Rebuild the traceable private-source inventory; never copy package payloads."""
import argparse
import csv
import hashlib
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def classify(path):
    ext = path.suffix.lower()
    name = path.as_posix().lower()
    if ext == '.cs':
        if 'baker' in name: return 'N06/N08', 'code-reference'
        if any(s in name for s in ('ik', 'ground', 'interaction', 'rotationlimit', 'poser')):
            return 'N03/N04/N05/N08', 'code-reference'
        if 'bozo' in name: return 'N05/N07/N08/N10/N11', 'code-reference'
        return 'N08/N16', 'code-reference'
    if ext in ('.fbx', '.obj'): return 'N03/N05/N06/N07/N11', 'conversion-required'
    if ext in ('.mat', '.shader', '.shadergraph'): return 'N02/N07/N14', 'mapping-required'
    if ext in ('.png', '.jpg', '.jpeg', '.tga', '.tif', '.psd', '.ttf'): return 'N07/N10/N14/N15', 'asset-source'
    if ext in ('.anim', '.controller', '.mask'): return 'N01/N02/N03/N05/N06/N07/N09', 'conversion-required'
    if ext in ('.prefab', '.unity', '.asset', '.json'): return 'N05/N07/N08/N10/N12/N13', 'mapping-required'
    if ext in ('.html', '.pdf', '.txt', '.rtf', '.css'): return 'N01-N16', 'documentation-reference'
    if ext in ('.dll', '.mdb', '.bundle', '.asmdef'): return 'N08/N16', 'unity-runtime-unavailable'
    if ext == '.unitypackage': return 'N07/N16', 'container-extracted-separately'
    return 'N07/N16', 'unclassified-review-required'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, default=Path('C:/Users/donod/Downloads/analise-plugins/analise-plugins'))
    parser.add_argument('--polygon-package', type=Path, default=Path('C:/Users/donod/Downloads/POLYGON Icons Pack - Low Poly 3D Art by Synty v1.01/poly.unitypackage'))
    parser.add_argument('--polygon-library', type=Path, default=ROOT/'local-assets/synty-polygon-icons-v1.01')
    args = parser.parse_args()
    packages = ['UMotionPro-1.29p04', 'FinalIK-2.2', 'BoZo-2.2', 'FinalIK-integracoes', 'BoZo-integracoes']
    rows = []
    for package in packages:
        for path in sorted((args.source / package).rglob('*')):
            if not path.is_file() or path.suffix == '.meta': continue
            blocks, status = classify(path)
            with path.open('rb') as source:
                digest = hashlib.file_digest(source, 'sha256').hexdigest()
            rows.append(dict(package=package, path=path.relative_to(args.source).as_posix(),
                             bytes=path.stat().st_size, sha256=digest, blocks=blocks, source_status=status,
                             astra_status='not-verified-by-this-inventory'))
    polygon_catalog = args.polygon_library/'catalog.json'
    polygon_present = args.polygon_package.is_file() and polygon_catalog.is_file()
    if polygon_present:
        packages.append('POLYGONIcons-v1.01')
        polygon = json.loads(polygon_catalog.read_text(encoding='utf-8'))
        for entry in polygon['assets']:
            path = args.polygon_library/entry['output']
            with path.open('rb') as source:
                digest = hashlib.file_digest(source, 'sha256').hexdigest()
            if digest != entry['sha256']: raise ValueError('POLYGON source changed: '+entry['path'])
            blocks, status = classify(path)
            rows.append(dict(package='POLYGONIcons-v1.01', path=entry['path'], bytes=path.stat().st_size,
                             sha256=digest, blocks=blocks, source_status=status,
                             astra_status='source-preserved-see-local-baked-catalog'))
        for path in [args.polygon_package,args.polygon_package.parent/'Readme.txt']:
            if not path.is_file(): continue
            with path.open('rb') as source:
                digest = hashlib.file_digest(source, 'sha256').hexdigest()
            if path == args.polygon_package and digest != polygon['packageSha256']:
                raise ValueError('POLYGON archive differs from extracted catalog')
            rows.append(dict(package='POLYGONIcons-v1.01',path=path.as_posix(),bytes=path.stat().st_size,
                             sha256=digest,blocks='N07/N10/N11/N14/N16',source_status='source-container-or-license',
                             astra_status='not-a-native-resource'))
    out = ROOT / 'docs/planos/execucao-2026-10-10'
    out.mkdir(parents=True, exist_ok=True)
    with (out/'plugins-itens.csv').open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
        writer.writeheader(); writer.writerows(rows)
    declarations = []
    for catalog in ['codigo-catalogo.json', 'BoZo-codigo-catalogo.json']:
        for entry in json.loads((args.source/catalog).read_text(encoding='utf-8-sig')):
            path = Path(entry['path'].replace('\\', '/'))
            blocks, _ = classify(path)
            for declaration in entry.get('declarations', []):
                declarations.append(dict(path=path.as_posix(), line=declaration['line'],
                                         declaration=declaration['declaration'], blocks=blocks,
                                         astra_status='reference-not-an-astra-api'))
    with (out/'plugins-declaracoes.csv').open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=['path', 'line', 'declaration', 'blocks', 'astra_status'])
        writer.writeheader(); writer.writerows(declarations)
    counts = Counter(row['package'] for row in rows)
    text = '''# Cobertura dos plugins fornecidos — N01–N16

Inventário de origem em 10/10/2026. Presença de arquivo ou referência não declara
capacidade implementada. `plugins-itens.csv` cobre cada arquivo dos quatro pacotes
e das integrações extraídas, com caminho, SHA-256, tamanho, blocos responsáveis e
estado de origem. Os `.meta` permanecem junto às fontes e são lidos pelos
conversores de GUID; não são contados novamente como capacidades. Miniaturas
originais não demonstram preview/render Astra.

`plugins-declaracoes.csv` relaciona cada declaração encontrada nos catálogos
de código fornecidos a caminho/linha/blocos. Declarações incluem campos privados,
tipos, membros herdados e exemplos: não são contagem de APIs utilizáveis Astra.
Não inclui bytecode das DLLs nem inventa corpo de método onde há apenas metadados.

Fonte local: `C:/Users/donod/Downloads/analise-plugins/analise-plugins`.
Licença fornecida: `../licenca_de_uso_de_materiais.txt`, autoriza uso e adaptação
dentro dos direitos do emissor, pede comunicação prévia para comercialização e
preserva direitos de terceiros. Este registro não publica o payload privado.

| Pacote extraído | Arquivos inventariados | Cobertura |
|---|---:|---|
'''
    for package in packages:
        text += f'| {package} | {counts[package]} | Todos os arquivos presentes, sem `.meta` |\n'
    text += f'''
POLYGON Icons v1.01: {'localizado e inventariado' if polygon_present else 'não localizado'}.
Origem adicional: `{args.polygon_package.as_posix()}`; licença/aviso `Readme.txt`
ao lado do pacote. A extração local está em `{args.polygon_library.as_posix()}`.
O catálogo original mantém GUID/meta e SHA-256; 520 prefabs e 520 FBX são fontes,
não tipos de componente. `baked-catalog.json` local identifica sprites/GLB derivados
por Blender e suas limitações; não comprova conversão do shader/lighting Unity.
As saídas privadas permanecem fora da documentação pública e do atlas da engine.

'''
    text += '''

## Responsabilidade por capacidade

| Origem concreta | Capacidades e destino | Estado Astra nesta auditoria |
|---|---|---|
| UMotion `Assets/UMotionEditor` e 75 HTML em `Manual` | Pose, seleção, curves, layers, intervals, cues N01; property animation N02; rig/mirror N03; IK/Child-Of/pinning N04; root motion N05; bake/FBX N06; import N07; API N08; Timeline N09 | Manual/DLL são referência; não executados no Android nem tratados como implementação Astra |
| UMotion `Assets/UMotionExamples` | Kyle, quatro `.anim`, quatro controllers, projetos, materiais/texturas, cena N01/N03/N05/N07/N16 | Receitas Kyle existentes devem ser reconciliadas; inventário não certifica importação de todos itens |
| FinalIK `Assets/Plugins/RootMotion/FinalIK` | Aim/LookAt/CCD/FABRIK/FABRIKRoot/trig/limb/arm/leg/biped/fullbody/finger/VR, limites, posers N03/N04 | Código Unity exige backend nativo e editor; VRIK exige tracking externo |
| FinalIK Grounder/Interaction/Offset/HitReaction/Recoil/Utilities | Contatos, interação, twist, inércia, reação, ragdoll, locomotion N04/N05/N12/N15 | Não basta anexar nomes de componentes |
| FinalIK Baker e Editor | Bake/export N06; inspeção/gizmos/SDK N08; recursos de demos N07/N16 | Conversão independente de Unity necessária |
| `FinalIK-integracoes` (11 pacotes) | VR SDK, Photon/PUN, Animation Rigging e outras pontes N04/N05/N08/N13/N14 | Dependências externas preservadas; não equivalem a rede/XR Astra |
| BoZo `Assets/BoZo_StylizedModularCharacters` | Body/outfit/attachments, shapes, proporções, LOD/merge, cores/materiais, save/icon capture, creator UI N05/N07/N10/N11/N13/N15 | Mapear modelo/runtime nativos; bugs de origem descritos em ANALISE-BOZO não devem ser transplantados |
| BoZo materiais/shadergraphs e integrações Built-In/URP | Materiais, texturas, bake N07/N14; editor/extensions N08 | Shaders customizados precisam conversão específica e diagnóstico |
| POLYGON Icons v1.01 `Assets/PolygonIcons` | 520 sprites/modelos, 17 materiais e 16 paletas N07/N10/N11/N14 | Fonte/GUID preservados; sprites/GLB offline existentes não equivalem a shader ou cena Unity nativos |
| Todos os FBX, imagens, Unity YAML/prefabs/controllers | Biblioteca e formatos N07; cenas exemplos desmontáveis N16 | CSV separa origem de suporte; arquivo convertido só é aceito após consumer/editor/persistência |

## Ligações com todo o plano

N01–N06 usam a autoria/rig/movimento/exportação pesquisadas; N07 possui inventário
e conversores; N08 dá comandos tipados aos mesmos backends; N09 compõe recursos;
N10 implementa UI de jogo e editor; N11 geometria/UV/material; N12 física e
regeneração; N13 save/replay/rede; N14 render/atmosfera/2D e opcionais; N15
orçamentos mensurados; N16 docs, distribuição e aceite. Esses vínculos são
responsabilidade de trabalho, não promoção automática de todo arquivo a feature.

## Conversão de materiais

O bloco N07 adiciona leitura tipada do YAML Material da Unity e conversão do
workflow Standard metálico compatível. `.mat` original e `.meta` são preservados.
Shaders customizados, specular workflow, metallic/smoothness packed maps e
propriedades sem consumidor equivalente são recusados com diagnóstico. Texturas
suportadas mantêm referências por GUID e interpretação de cor/dado. O relatório
da conversão não promete equivalência entre BRDFs nem bake do shader Unity.
O estado de compilação, host, pacote e Android deve ser registrado separadamente
na entrega N07; ADB permanece bloqueado pelo proprietário até o fim do plano.

Referências: Unity 6 (6000.0), [Standard Shader parameters](https://docs.unity3d.com/6000.0/Documentation/Manual/StandardShaderMaterialParameters.html),
[URP Lit](https://docs.unity.com/en-us/engine/6000.0/manual/materials-and-shaders/built-in/shaders-in-universalrp/reference/lit-shader),
[Unity text serialization](https://docs.unity3d.com/6000.0/Documentation/Manual/FormatDescription.html).

Regeneração: `python tools/validation/inventory_plugin_sources.py --source <pasta-extraida>`.
'''
    (out/'plugins-cobertura.md').write_text(text, encoding='utf-8')
    print(json.dumps(dict(files=len(rows), packages=dict(counts)), ensure_ascii=False))


if __name__ == '__main__': main()
