# N07 — TIFF e FBX deformável: próximo pacote vertical

Estado vigente em 10/10/2026: build 7612/41541 e material/biblioteca/texturas
**20/20 cenários aprovados**, incluindo FBX deformável com cold Play, TIFF e
Unity pela Biblioteca. TIFF 4/4 originais passou. A
[captura TIFF pronta em 655×300](../../validacao/n01-n07-2026-10-10/host/capturas/tiff-focal-655.ready.png)
foi inspecionada: preview e escolhas de alpha/precisão permanecem visíveis,
Importar é ação dominante e a paginação preserva os alvos de toque. Ela usa
fixture própria e atlas CPU, não comprova imagem Vulkan das fontes privadas.
Dispositivo/Vulkan físico continuam pendentes. As seções abaixo conservam a
sequência de arquitetura; resultados anteriores foram sucedidos pelo
[saldo vigente](N07-saldo-real.md).

## TIFF: dependências e decisões concretas

As quatro fontes do inventário FinalIK são ClassicTIFF RGB intercalado, uma faixa,
sem compressão e com ExtraSamples=0. Smoke/Smoke2 usam endian LE, 256², 8 bits;
MechPatrolbot usa BE, 512², 8 bits; Minebot usa BE, 512², 16 bits. As três primeiras
carregam ICC sRGB IEC61966-2.1 (3144 bytes); Minebot não declara ICC. Inspeção por
Pillow e leitura dos campos TIFF concordaram. Nenhuma imagem privada é publicada.

O quarto canal indefinido exige uma escolha persistida: recusar, ignorar, ou usar
como alfa. Precisão 16→8 exige autorização explícita e perda registrada. Um ICC
arbitrário não será tratado como sRGB pelo nome. O candidato reconhece apenas ICCv2
RGB/XYZ matricial com intenção relativa, primárias sRGB em PCS D50, branco D50/D65, preto zero e TRCs
tabuladas conformes à função sRGB; rejeita LUTs. As três fontes de 8 bits atendem
esse contrato, conforme inspeção independente. Conversão colorimétrica completa
continua aberta. Outros ICC exigem recusa ou aproximação explícita que usa valores
sem transformação de cor. Fonte e metadados originais permanecem preservados.

`resources/tiff_decode.{h,cpp}` está integrado ao CMake, ImageContainer, preparo
de texturas e dispatcher da biblioteca (.tif/.tiff). TextureProfile5 preserva
leitores1–4 e inclui as três decisões na chave do cache. O aceite do pacote
integrado passou em host; sintaxe estrita dos arquivos novos passou. O parser
valida diretório limitado, tipo/count/offset, dimensões antes de alocar, faixas,
sobreposição, endianness e oito orientações. Recusa compressão, tiles, múltiplas
páginas, alfa associado, modos não RGB e amostras não inteiras. Isso é um contrato
inicial, não suporte universal de TIFF nem de documentos Photoshop.

Cadeia implementada e validada em host: TextureProfile schema5 com leitor1–4 → preparo de imagem
com políticas → preview e perdas → fonte imutável/registro/índice → publicação
GeometryPublisher → materiais efetivos → Undo/Redo → cold reopen. A família
reutiliza o caminho transacional e a publicação real já aceitos para TGA.

NÃO IREI SER SIMPLISTA NO DESIGN.

O importador focal mantém a imagem e duas propriedades de48px. Há oito páginas
para TIFF (cinco para as outras texturas): Fonte/canais e Fonte/cor acrescentam
somente escolhas com consumidores; Perdas expande o relatório antes da publicação.
A recusa abre Fonte/canais e permite configurar
e preparar novamente sem fechar; a página permanece durante a preparação.
O glyph textures/source-policy foi integrado ao atlas335 pelo gerador real.
Não é necessário editar JSON nem converter fora da engine para esse contrato.
Qualquer mudança de política que altere texels exige novo preparo; Importar fica
indisponível enquanto a receita exibida divergir da preparação efetiva.

Aceite coeso: importar uma TIFF pelo pacote → escolher canal/precisão/cor pela UI
→ preparar → aplicar em material → observar texels no publisher → reimportar →
Undo/Redo → falha de publicação sem estado parcial → salvar/reabrir/Play. As quatro
fontes originais devem passar sob receita explícita, com perdas persistidas.

`test_tiff_decode.cpp` protege parser/corpus4, migração e ICC matemático; um segundo
cenário no TU `test_editor_library_texture.cpp` percorre o fluxo completo por
toque655×300, material vivo, receita, Undo/Redo, fonte reimportada, rollback gráfico,
archive/cold Play e conflito externo. O alvo combinado passa de16 para18 cenários
preparados; a rodada executada aprovou16/18, com ambos TIFF aprovados. Capturas são produzidas com
ASTRA_TIFF_CAPTURE (.refused/.ready/.losses) e julgadas antes do aceite. Os pixels
do atlas de preview têm ownership durante as capturas sucessivas. Nenhum ADB executado.

## FBX skin/takes: aproveitar a cadeia que já existe

O consumidor existente já possui SkinDefinition (até256 juntas), influência de
quatro juntas u16+pesos unorm16, SkinnedMesh, bounds deformáveis, AnimationClip,
Animation e renderer de deformação. A expansão do decoder deve produzir esses
dados, em vez de criar skeleton/animator paralelo.

`fbx_deformation.{h,cpp}`, `fbx_morph.{h,cpp}` e `fbx_scene_import.{h,cpp}` agora
integram CMake e dispatcher; aceite da expansão permanece pendente. Extrai binds e atributos por vértice de controle,
valida linear/rigid/limites/juntas/pesos, exige autorização para top4, e prepara
takes TRS pelo bake nativo ufbx. Recusa tempos float colapsados, propriedade
animada sem consumidor e steps sem autorização de aproximação. Não é suporte
aceito da família. Receita12, cache, snapshots reais64MiB, publicação transacional,
reconciliação de juntas e rota Rig/Takes foram conectados para a próxima rodada.

O IR intermediário precisa receber JOINTS_0/WEIGHTS_0 antes do importGlb: normais,
costuras e LOD podem duplicar vértices, logo anexar influências depois da preparação
quebraria a correspondência. As influências vêm do vértice de controle ufbx;
geometry_to_bone fornece a matriz de bind compatível com a geometria local.
Aceitar inicialmente linear/rigid; dual quaternion e misturas exigem consumidor
real ou recusa explícita. Mais de quatro pesos exige perda declarada e normalização.

Takes são recursos nomeados com identidade estável por fonte/nome/ordinal. A curva
FBX pode combinar layers, Euler e tangentes; ufbx_bake_anim oferece avaliação e
resampling nativos. O contrato de bake precisa fixar taxa, orçamento de chaves,
intervalo e tratamento de steps. Cast double→float não pode colapsar tempos nem
apagar descontinuidade silenciosamente. Propriedades animadas sem equivalente
recebem diagnóstico, e não canais decorativos.

Antes de publicar: ampliar a contabilização de snapshots para skins/joints/binds,
influências, canais e chaves; preservar o teto real64MiB do histórico de recursos.
Reimportação precisa reconciliar SkinnedMesh.bones, clips/Animation e caches de
deformação junto a mapa/receita/índice. Fonte animada sem geometria requer um caminho
de sub-recurso/compatibilidade de rig; o perfil estático não deve fingir aceitá-la.

Aceite coeso: personagem original com skin e take → preparar/selecionar pela engine
→ instanciar → Animation altera ossos e vértices consumidos → editar clipe nativo
→ reimportar preservando overrides → Undo/Redo → cold Play. Rig incompatível e
publicação recusada devem preservar fontes, referências, cursor e geometria anterior.

## Origem das decisões

[Unity6000.0 Texture Import](https://docs.unity3d.com/6000.0/Documentation/Manual/ImportingTextures.html)
separa fonte de derivado otimizado; TIFF não implica que seu conteúdo inteiro vira
runtime. Na Astra, as perdas e escolhas precisam estar na receita autoral.
[LibTIFF4.7.2 TIFFReadRGBAImage](https://libtiff.gitlab.io/libtiff/functions/TIFFReadRGBAImage.html)
documenta origem, quantização e ausência de correção colorimétrica no raster RGBA;
um decoder RGBA por si só não resolve ICC ou precisão.
[ICC: interpretação sRGB](https://www.color.org/chardata/rgb/sRGB.pdf) fornece a
função de transferência e matriz PCS. [Registro ICC sRGB](https://registry.color.org/rgb-registry/srgbprofiles)
distingue versões/perfis e intents; um nome de perfil não equivale a uma conversão.

[Unity6000.0 Rig](https://docs.unity3d.com/6000.0/Documentation/Manual/FBXImporter-Rig.html)
expõe root/rig e política de pesos, distinguindo importação de retargeting. O próximo
pacote usa o rig genérico existente, sem declarar avatar humanoide por nomenclatura.
[Unity6000.0 Euler resampling](https://docs.unity3d.com/6000.0/Documentation/Manual/AnimationEulerCurveImport.html)
explicita bake quaternion por frame e diferenças de fidelidade; a escolha de
amostragem e a aproximação de steps precisam estar na receita, não escondidas.
[Godot4.5 Advanced Import](https://docs.godotengine.org/en/4.5/tutorials/assets_pipeline/importing_3d_scenes/advanced_import_settings.html)
fundamenta edição de sub-recursos no contexto do arquivo. Código oficial fixado:
`ufbx v0.20.0`, estruturas skin/bake em `native/third_party/ufbx/ufbx.h`; o código
define pesos ordenados, bind geometry_to_bone e o risco de steps colapsarem em float.
