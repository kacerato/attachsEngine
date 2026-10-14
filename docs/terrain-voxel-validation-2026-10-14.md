# Terreno/voxel — entrega de desenvolvimento

Branch `codex/terrain-voxel-generator`, base `7260c5839dcd6c9a3a89db0b3933cce194ac0788`.

## Executado

- Extração dos três pacotes em `build/terrain-packages`, 753 arquivos de conteúdo, manifests com hashes e originais preservados. Quatro texturas de Path Painter adaptadas para PNG RGB 256×256; licença fornecida e proveniência em `assets/terrain`.
- Implementação C++ própria: altura, voxel suave por tetraedros, blocos, seed, chunks de 16, oito ferramentas, caminhos interpolados, quatro materiais, receitas GLB, geração assíncrona cancelável e publicação transacional de recursos.
- Integração ao menu de criação, painel nativo adaptável, edição numérica e pincel por toque/XYZ; Undo/Redo, raiz/chunks estáveis e preservação de material nos chunks existentes. Colisão Mesh estática no runtime Jolt.

## Verificação

LLVM-MinGW Clang 22.1.8, CMake/Ninja Release, C++20, warnings tratados como erros nos alvos. `cmake --build build/editor-host --target aether_terrain_tests -j 4` concluiu. `build/editor-host/aether_terrain_tests.exe`: **9/9 cenários passaram**.

1. Seed determinística, receita e chunks.
2. Três modos exportados/reimportados pelo parser GLB real; normais unitárias.
3. Escultura, pintura e remoção de densidade alteram a geometria.
4. NaN, entradas inválidas, orçamento e cancelamento preservam saída.
5. Publicação, identidade de raiz/chunks, material personalizado, Undo/Redo, salvar/reabrir e sessão nova carregando registro + fontes do disco. Raycasts Jolt acertam o terreno antes e depois da abertura nova.
6. Falha de publicação mantém a cena anterior.
7. UI nativa em 1280×720 e 655×300, quatro abas, fonte/atlas reais, sem comandos descartados/glyphs ausentes. Evento de toque passa pelo roteamento real e gera o terreno assincronamente.
8. Cancelamento e mudança concorrente de cena descartam o trabalho.
9. Caminho preenche o intervalo entre amostras; escavação com força zero é no-op.

O build precisou de duas correções preexistentes reveladas pelo Clang: inicialização do caractere vazio em ImGui e vida útil da string usada na instância visual do importador UMotion.

## Limites de evidência e do produto

Sem dispositivo em `adb devices`. Não foi produzido/distribuído APK nem comprovado render Vulkan/Android nesta entrega. Capturas são rasterização da UI nativa pelo executável de testes, não screenshots de um telefone. Conceito gerado em `docs/design/terrain-2026-10-14` não é evidência de implementação.

Geração finita completa a cada operação, sem remeshing incremental, LOD/streaming infinito, vegetação ou erosão hidráulica. Pintura atribui camada por região horizontal; projeção pelo eixo dominante não faz blending triplanar. Revisões imutáveis permanecem na biblioteca para Undo e referências de outras cenas, aumentando disco/memória; não há coleta automática nesta revisão. Reutilização dos pacotes é seletiva e documentada, sem portar DLLs Unity.

## Uso

Projeto aberto → criação → **Terreno e voxel** → Gerar e aplicar. Esculpir escolhe ferramenta/raio/força; Pincel no viewport aplica ao toque e XYZ permite editar dentro do volume. Camadas escolhe material. Gerir abre seleção, enquadra, desfaz e controla colisão. Salvar cena é necessário. Para editar um terreno salvo, selecione raiz/chunk e use Gerir → Abrir seleção.

Documentação de desenvolvimento: `https://astraengine.com.br/pt-br/desenvolvimento/2026-10-14/terreno-voxel/`, acompanhada da nota `2026-10-14-terreno-voxel-host`; estado de publicação registrado no plano.
