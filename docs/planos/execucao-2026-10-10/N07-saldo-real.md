# N07 — saldo contra o plano, páginas 15–16

Revisão de 10/10/2026, após o aceite do build conjunto 7612/41541. O universo
é o plano `output/pdf/Astra-proximos-blocos-2026-10-10.md`, não apenas as extensões
adicionadas nesta rodada. N07 permanece **parcial**.

## Evidência vigente

O build 7612/41541 terminou com sucesso. A execução
[`host/library-integrated.log`](../../validacao/n01-n07-2026-10-10/host/library-integrated.log)
registrou **20/20 cenários**, incluindo o cold Play deformável corrigido e o
pacote Unity pela Biblioteca. A baseline anterior 60941,
`host/aether_unity_material_tests-typed-baseline.log`, registrou 18/19 cenários.
O único insucesso ocorreu no último trecho do cenário FBX deformável, depois
da reabertura, na expectativa de tempo do Play. O relógio limita avanço a
250 ms por frame; a fixture solicitava 500 ms de uma vez. Foi corrigida para
dois avanços de 250 ms, mantendo a expectativa de paleta de juntas e morphs.
A correção foi executada na rodada integrada e passou.

Na mesma rodada: 520/520 FBX estáticos POLYGON decodificados; corpus FBX privado
151 fontes, 150 aceitas e uma recusada por dual quaternion, com 99 skinned,
75 morph, 72 com takes e 23 sem mesh. Categorias se sobrepõem. Esses números
provam a passagem pelo dispatcher/decoder sob as políticas da fixture; não
demonstram autoria, fidelidade visual ou uso completo de todos os assets.
TIFF 4/4 originais e TGA 7/7 originais passaram com políticas explícitas.

O pacote Unity adicionou o vigésimo cenário aprovado: resolução
GUID/fileID exata, prefab nativo reutilizado, clipe editável, controller vinculado
pelo toque, conflito, Undo/Redo, reabertura e Play. Sintaxe estrita final passou
em 86231. A captura unity-library-655.png foi inspecionada em 655×300: detalhes
de origem e ações contextuais cabem na rota focal. Essa captura CPU de UI não
substitui evidência Vulkan. GUI 30/30 e SDK ABI12 real 1/1 passaram no mesmo gate;
o editor de animação teve 53/55 e correções próprias em andamento na raiz.

## Matriz de fechamento

| Exigência do plano | Estado real | Saldo necessário |
|---|---|---|
| Pacotes, origem, versão, conteúdo, dependências e estados por item | Índice persistido, importação privada, busca/tipos e diagnóstico integrados; cenários de pacote passaram | Resolver o inventário inteiro com disposição verificável por item; filtros específicos de capacidade, rig e formato |
| Preview real de modelo, clipe e material; thumbnails do convertido | Preview de clipe existente; material esfera/caixa/plano integrado ao renderer; importador de textura tem atlas CPU real | Viewer independente de modelo e thumbnails geradas do resultado convertido; comprovar imagem material Vulkan no aparelho |
| Conteúdo reutilizável e exemplos desmontáveis | Instâncias/recursos nativos reutilizáveis; pacote Unity aprovou cenário de dois objetos e cold Play | Projetos de exemplo completos dos pacotes pela Biblioteca; transferência entre repousos distintos depende do N03 |
| FBX eixos/unidades/hierarquia/bind/skin/morph/takes | Decoder nativo, perfil12, publicação, extração de clipes, vínculo compatível e cold Play aceitos; corpus acima executado | Dual quaternion/constraints/NURBS e canais sem consumidor continuam recusados |
| Unity meta/YAML/prefab/material/anim/controller compatíveis | Standard e conversão genérica aceitos, com políticas e referências exatas, History/reabertura/Play | Cenas `.unity`, patches/variantes/prefabs aninhados, SkinnedMeshRenderer de prefab e componentes adicionais; Humanoid/compressão/Euler sem bake/PPtr/eventos e controllers complexos permanecem recusados |
| PSD/TIF/TGA e conversão de cor/alpha/normal/UV | TGA e TIFF clássico delimitados aceitos; perfil de textura e shader Standard possuem políticas explícitas | PSD; demais variantes TIFF/ICC/compressão com consumidor; equivalência visual de shaders não suportados não pode ser inferida |
| Reimportação de clipe/rig/material/prefab/colisão com overrides | Material/textura/FBX têm journal e histórico; Unity aprovou merge de canal e conflitos de recurso inteiro | Merge granular de prefab/colisão/rig e revalidação ligada à futura autoria N03; campos sem equivalência continuam diagnóstico |
| Hash, versão, receita, perdas e reprodução portátil | Índice, metadados tipados, origem preservada e receitas persistidas | Reconciliar todos os 2.816 itens do inventário; snapshots órfãos ainda não têm coleta automática |
| Aceite Android | Evidência host segregada de captura CPU e publisher probe | Build/package e aceitação física no encerramento autorizado; nenhum ADB foi executado |

`plugins-itens.csv` registra origem e rastreabilidade; seu estado
`not-verified-by-inventory` não foi substituído por “suportado” a partir da mera
presença de arquivos. Preservar um payload desconhecido é uma disposição de
origem, não uma conversão utilizável. Referências de implementação e limites
estão em `N07-unity-pacote.md`, `N07-fbx-corpus-deformacao.md` e
`N07-formatos-proximo.md`.

## Próximo pacote dependente: N03

O recurso/avaliador de rig já está no CMake, mas falta o caminho de autoria e
aceite. A próxima integração deve reunir: compensação de placement na borda
SceneGraph, autoria de rig/controle/espelho/pose/retarget em uma superfície focal,
publicação transacional dos recursos, History e API sobre os mesmos comandos.
O cenário deve começar com a hierarquia selecionada na engine, aplicar pose em
outra instância posicionada, transferir entre repousos diferentes, salvar e
reabrir, sem teleportar a raiz nem mudar bind/skin implicitamente. Preparação de
backend ou uma captura conceitual não fecha esse pacote.
