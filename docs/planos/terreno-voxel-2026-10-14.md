# Terreno e voxel — execução em três blocos

Branch: `codex/terrain-voxel-generator`. Base: `7260c58`. Pedido de 14/10/2026.

1. **Pacotes e contrato.** Extrair Digger 6.0, MicroSplat 3.9.41 e Path Painter II 2.1.10; catalogar código, texturas, dependências e licença. Definir o recorte executável e ligar ao editor nativo Astra.
2. **Autoria completa do recorte.** Gerador determinístico com seed, terreno por altura, voxel suave e blocos; chunks de 16 células; elevar, baixar, nivelar, suavizar, escavar, adicionar e pintar; caminhos por amostras de pincel; quatro materiais iniciais com texturas fornecidas. Receita persistida no GLB, colisores triangulares, revisão e aplicação atômica, Undo/Redo e reabertura. UI de ferramentas e propriedades com viewport disponível, inspirada na organização mobile da ITsMagic.
3. **Aceite integrado.** Testar geometria, limites, cancelamento, persistência, importação, publicação, edição, Undo/Redo e colisão. Compilar e capturar a UI executável, testar Android se houver dispositivo disponível; publicar documentação de desenvolvimento no AstraDocs ao concluir. Declarar separadamente o que passou no host e no aparelho.

## Limites explícitos do recorte

Mundo finito autorável, 8–128 células por lado e 8–64 em altura, sujeito ao orçamento de 600 mil amostras e 200 mil triângulos. Não promete mundo infinito, streaming de regiões, toda a coleção de shaders MicroSplat, vegetação procedural, erosão hidráulica nem paridade integral com os plugins. Esses itens não são necessários para criar, editar, salvar e jogar no terreno deste recorte. O orçamento é validação real, não um botão decorativo.

## Referências de capacidade

- Unity 6 (6000.7): Terrain Layers — https://docs.unity.com/en-us/engine/6000.7/manual/creating-environments/script-terrain/terrain-textures
- Godot 4.6: SurfaceTool e geração de geometria com atributos — https://docs.godotengine.org/en/4.6/classes/class_surfacetool.html
- ITsMagic: Terrain — https://itsmagic.com.br/documentation/pt/docs/api-reference/Components/Terrain/ — organização de ferramenta e propriedades; não declaração de portabilidade do runtime.

## Reaproveitamento e evidência

Os arquivos extraídos ficam em `build/terrain-packages`, com `manifest.json` por pacote (GUID, caminho, tamanho e SHA-256). Originais preservados em Downloads/plugins. A licença fornecida autoriza uso/adaptação; texto preservado junto das texturas incorporadas.

- Digger: `BasicOperation.cs`, `Voxel.cs`, `MarchingCubesJob.cs`: área de influência, densidade, chunks e operações. A Astra usa implementação C++ própria; não depende de Burst/Unity.Jobs. Poligonização inicial por tetraedros, não o job Unity.
- MicroSplat: `TextureArrayConfig.cs` e `MicroSplatTriplanar.cs`: organização de camadas, escala e projeção. O material inicial usa atribuição por inclinação/altura e projeção pelo eixo dominante; não equivale a blending triplanar do shader original.
- Path Painter: lógica principal distribuída em DLLs Unity; `ImpMgr.cs` é o importador. Reaproveitar texturas e fluxo de pincel/caminho, sem afirmar portar o algoritmo das DLLs.

## Estado

Bloco 1 concluído: Digger 151, MicroSplat 517, Path Painter 85 arquivos de conteúdo. Bloco 2 implementado no recorte descrito. Bloco 3: build nativo e 9/9 cenários passaram, incluindo sessão nova, colisão Jolt e toque na UI; capturas nativas em dois tamanhos. Sem aparelho ADB, aceite Android permanece pendente. Docs compiladas e 339.399 links internos verificados sem erros; revisão desktop/mobile. Publicadas no commit AstraDocs `4b20eec21290a6bd52247abce7ae1430ec8e4734`, deploy `dpl_ACvqLKijfb4Eozuqr7c4cziVKmih` READY; domínio respondeu HTTP 200 com o guia e aviso de desenvolvimento. Não há nova distribuição APK. Evidências e limites em `docs/terrain-voxel-validation-2026-10-14.md`.
