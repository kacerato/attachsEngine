# N07 — biblioteca de pacotes preservados

Estado: modelo, índice, transação, rota nativa e seletor Android integrados no
código. O build 7612/41541 e a rodada integrada aprovaram **20/20 cenários**,
incluindo o cold Play FBX deformável corrigido e o novo fluxo Unity pela
Biblioteca. TGA 7/7 originais, TIFF 4/4, FBX 520/520 estáticos e 150/151
deformadores passaram; dual quaternion é recusa explícita. Captura da rota
Unity em 655×300 inspecionada, sem prova Vulkan/aparelho.
[Saldo completo](N07-saldo-real.md). N07 permanece parcial:
preservar não significa executar.

## Referências e decisão

Unity 6000.0 separa identidade/versão e nome visível no
[manifesto](https://docs.unity3d.com/6000.0/Documentation/Manual/upm-manifestPkg.html).
Godot 4.5 separa descoberta, origem, licença e instalação na
[Asset Library](https://docs.godotengine.org/en/4.5/community/asset_library/using_assetlib.html),
[editor](https://github.com/godotengine/godot/blob/4.5-stable/editor/asset_library/asset_library_editor_plugin.cpp)
e [instalador](https://github.com/godotengine/godot/blob/4.5-stable/editor/asset_library/editor_asset_installer.cpp).
A Astra aplica esses princípios à preservação local e conversão explícita;
não executa plugins Unity/C# apenas por importar suas fontes.

## Fluxo real

NÃO IREI SER SIMPLISTA NO DESIGN.

Menu de destinos → Biblioteca → Importar pasta → seletor Android → cópia com
progresso/cancelamento → catálogo → material Standard → conversão → Inspector
real → viewer independente ou aplicar ao objeto.

Uma rota focal substitui os painéis da cena. Faixa única de 48 px concentra
retorno/importação/busca/filtros; linhas de 56 px exibem nome, contexto e ação
efetiva. Diagnósticos substituem a lista e têm paginação própria. Em landscape
baixo, menu de destinos usa duas colunas quando uma não cabe. Filtros/consulta
permanecem na sessão. Não há thumbnails decorativas ou Instanciar sem conversor.
O glyph assets/source-package entra no pipeline real; a captura executável
[655×300](../../validacao/n01-n07-2026-10-10/host/capturas/library.655x300.png)
foi inspecionada e preserva legibilidade, alvos de toque e navegação focal.

## Dados, origem e publicação

LibraryIndex guarda pacote, versão da preservação, origem, SHA-256, aviso de
licença e itens tipados. Item tem identidade qualificada pelo pacote, caminho,
GUID Unity quando disponível, hash, conversor/version/receita, perdas,
diagnóstico e dependências entre itens. A integração Unity nova resolve
GUID+fileID exatos e tabelas de sub-recursos .meta dentro da mesma revisão de
pacote; não usa aproximação por nome para corrigir dependência ambígua.
Seu cenário integrado passou na rodada vigente.
AssetRegistry continua autoridade dos recursos nativos; Registered prova
registro compatível, não GPU. Perdas vazias não provam fidelidade total.

Worker preserva todos os arquivos, inclusive .meta, código e licenças. SHA-256
por streaming e manifesto ordenado definem revisão; conteúdo diferente cria
outro pacote. Fontes ficam em Biblioteca/<id>/, manifesto package.astrapackage;
índice em .astra/imports/library-index.astra. Payload privado não vai às docs
públicas. Metadados .meta não viram segundo recurso na lista.

Materiais Standard usam o conversor real. GUID de textura é resolvido somente
no pacote daquela revisão; a identidade nativa inclui o pacote. Revisões com
o mesmo GUID Unity não disputam recursos nem leem a outra origem. Receitas de
textura preservam JSON multilinha; demais campos recusam controles/binários.

EditorImportTransaction publica manifesto+companions+registry+índice no mesmo
journal. Falha pré-commit restaura os quatro; edição externa do índice recusa
publicação/Undo/Redo. Converter/reimportar material e desfazer/refazer restaura
material, receita, dependências e índice juntos. Destinos existentes recusam
overwrite. Undo de adicionar pacote retira o catálogo e PRESERVA fontes e
recursos; Redo recoloca. Não é comando de apagar diretórios.

Android possui modo explícito de pacote, sem exigir GLTF principal. ContentResolver
copia, calcula hashes e informa progresso. Preparo verifica hash/tamanho/data;
a adoção recusa alterações. Cancelar libera worker sem publicação parcial. Lease
mantém staging até Java fechar streams; próxima cópia recusa enquanto isso.
Cancelar antes de iniciar encerra imediatamente. Troca de projeto/epoch descarta
o resultado. Abrir Biblioteca fecha leases da prévia anterior.

## Limites e evidência

Pacote até 4096 arquivos, profundidade 8 no seletor, hashing de 64 KiB. Caminhos inválidos,
colisões/GUIDs duplicados, alterações ou excesso recusam sem omissão silenciosa.
Dependências YAML são inspecionadas até 256 KiB; acima disso a limitação vira perda
registrada. ASTRA_LIBRARY_INDEX_1: 16 MiB, 128 pacotes, 65.536 itens, 128 dependências e
64 perdas por item. Candidato inteiro validado antes da troca; cache por posições;
resolução transitiva com IDs únicos, sem recursão. UI recompõe linhas por mudança
de catálogo/filtro/consulta, sem IO por frame.

Sintaxe -Wall/-Wextra/-Wpedantic/-Werror passou nos arquivos novos e integração
de materiais; clang Android -fsyntax-only passou android_main e model_picker.
Historicamente, o alvo material incluiu doze cenários: cinco materiais/viewer anteriores,
dois de índice, três de biblioteca, um decoder TGA e um fluxo vertical de textura.
Os novos cobrem toque/conversão, Undo/Redo, cold reload, fontes intactas, edição
externa, journal interrompido e revisões com mesmo GUID Unity. A rodada build-temporal-tga-resume executou os doze: 12/12 aprovados, incluindo sete fontes TGA originais. A captura real da Biblioteca permanece valida; a captura TGA antiga mostrou viewport vazio por fixture incompleta e nao prova o importador. A rodada build-curves-texture-publication aprovou tambem a correcao focal e os texels de reimportacao ja usada: 12/12, sete fontes originais. Captura tga-focal.655x300.png inspecionada; atlasCPU e publisherprobe sao evidencias host, sem imagemVulkan/aparelho. Build APK deste pacote ainda pendente; sintaxe
Android não equivale a APK, e captura de UI não prova renderização Vulkan.

## Lacunas reais

N07 continua aberto: miniaturas geradas, filtros por capacidade/rig e exemplos
editáveis completos, PSD, cenas Unity, variantes e prefabs skinned, Humanoid,
animação Unity comprimida e controladores fora do contrato declarado. Os
conversores Unity .anim/prefab/controller genéricos foram integrados, com
histórico, reimportação e consumidor real; o cenário 20 pela Biblioteca passou.
FBX skin/morph/takes/meshless possui aceite de corpus, publicação e cold Play.
Constraints, dual quaternion, retargeting e materiais externos FBX permanecem abertos. Formatos
preservados sem conversor recebem diagnóstico, não menus fictícios. Galeria
POLYGON, fontes e derivados existentes permanecem preservados; sem rebake sem
mudança. Aceite GPU, seletor físico e aparelho pendentes. ADB não utilizado.
