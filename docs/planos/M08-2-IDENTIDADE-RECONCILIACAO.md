# M08.2 — identidade de nós e reimportação sem perda

13/09/2026 · branch `codex/gameplay-runtime`, base `8064704` com alterações de trabalho de outro agente preservadas (sem commit). Primeira entrega do [pacote delegado](DELEGACAO-M08-M09-IMPORTACAO-AUTORAL.md). **M08 continua aberto**: esta entrega cobre a Entrega 1; submeshes/MaterialAsset (Entrega 2), texturas/PBR (Entrega 3) e dependências externas/codecs (Entrega 4) não começaram.

## O que foi implementado

| Contrato do plano | Implementação | Onde |
|---|---|---|
| Mapa persistente de nós e subassets, revisão da fonte | `ImportNodeMap` (`ASTRA_NODEMAP 1`): por nó, identidade, pai, nome, id autoral, assinatura geométrica, matriz local, identidades das primitivas e revisão de entrada. Gravado em `.astra/imports/<guid-da-fonte>.nodes` | `native/resources/import_node_map.*` |
| Nome e índice não são a única identidade | Correspondência em camadas: (1) id autoral no `extras` (`astra_id`, `uuid`, `guid`, `id`) único nos dois lados; (2) mesmo pai + mesmo nome, desempate por geometria e pose idênticas; (3) renomeação — mesmo pai, geometria e pose únicas; (4) troca de pai — nome e geometria únicos no arquivo. Mesmo hash de conteúdo prova correspondência posicional | idem |
| Ambiguidade vira conflito | Grupo que sobra com >1 candidato dos dois lados é **relatado** e a publicação é recusada (`ImportAmbiguityPolicy::Refuse`). A revisão lista os grupos e trava *Só recurso*/*Importar na cena* até o usuário escolher **Associar pela ordem** ou **Tratar como novos** | `editor_session.cpp`, `editor_screen.cpp` |
| Vínculo de cada instância | Componente `astra.import.link` v1 em cada objeto instanciado: fonte, nó, instância, primitiva, revisão, raiz, órfão, desvinculado e a **base** (nome, TRS, pai, malha, nº de primitivas) | `native/scene/import_link.h` |
| Base / novo / local | Por campo: local = base → recebe o novo; novo = base → mantém o local; os três diferentes → mantém o local e registra conflito. Campos: nome, posição, rotação, escala, malha, pai | `native/editor/editor_import_reconcile.*` |
| Filhos adicionados/removidos, mudança de pai | Nó que a instância não conhecia (revisão de entrada maior que a da instância) é criado; nó que a instância conhecia e não tem mais foi apagado pelo usuário e **não ressuscita**. Troca de pai aplicada quando o pai local ainda é o da base | idem |
| Órfãos sem exclusão automática | Removido da fonte com script, componente, filhos, referência, flags ou campos alterados fica **órfão** com os dados. Sem nada local, sai da cena. Inspetor oferece *Manter como objeto independente* ou *Apagar objeto* | idem + inspetor |
| Inspetor: alterações locais, reverter, desvincular | Linha de vínculo acima dos componentes (fonte, nó, nº de alterações) e lista com *Reverter nome/posição/rotação/escala/pai/malha à fonte*, *Reverter tudo* e *Desvincular instância*. Tudo pelo histórico | `editor_screen.cpp`, `editor_session.cpp` |
| Duplicação | Duplicar a raiz da instância cria **outra instância** (identidade nova, mesmos nós); duplicar uma peça solta gera objeto independente (`unlinked`) | `editor_history.cpp` |
| Transação sem estado parcial | Journal `ASTRA_IMPORT_2` inclui o mapa com fonte e registro; backup e restauração dos três. `ASTRA_IMPORT_1` continua recuperável. Falha depois da publicação restaura documento, histórico, biblioteca e registro | `editor_import_transaction.h` |
| Formato, migração, legado | Cena `AETHER_EDITOR 12` inalterada: o vínculo é componente versionado (APK antigo o preserva como desconhecido). Registro `AETHER_ASSETS 1` inalterado. Cenas legadas abrem; objetos antigos só ganham vínculo com **prova** (identidade da malha + nomes dos ancestrais), senão ficam como estão | `adoptLegacyImportInstances` |
| Ciclos e cenas reutilizáveis | O vínculo referencia a fonte por `AssetGuid`, o mesmo contrato que um `SceneAsset` usará. Instâncias aninhadas **não** estão prontas: nada consome vínculo dentro de vínculo | — |

### Fluxos ligados

- **Reimportar** (Arquivos → Reimportar ou importar arquivo com o mesmo destino): prévia calcula a correspondência sem publicar e mostra por id autoral, estrutura, renomeados, pai novo, novos, removidos e ambíguos; a publicação reconcilia todas as instâncias em **um passo de desfazer** e informa atualizados/criados/removidos/órfãos/conflitos no console, com uma linha por conflito.
- **Abrir cena**: cena salva antes da última reimportação é reconciliada na abertura, a partir da base guardada em cada vínculo.
- **Reabrir projeto**: o mapa é lido do projeto; mesma fonte gera o mesmo mapa e nada é regravado. Projeto anterior ao mapa ganha o mapa na abertura, determinístico e com as identidades de desenho legadas.

## Decisões e correções durante a entrega

- Objetos legados são provados contra a revisão **anterior** na reimportação. Prová-los contra a nova fazia os nós recém-chegados parecerem apagados pelo usuário. Na abertura de cena, sem revisão anterior, vale a atual e nós ausentes não são recriados (conservador).
- *Desvincular* removia o componente; a reimportação seguinte religava a instância pela adoção de legado. Agora fica a marca `unlinked`.
- A semente da instância copiada usava a revisão do documento, que muda a cada filho aplicado — cada filho virava uma instância. Semente estável na operação.
- A malha trocada pela reconciliação resolve o slot na própria mutação, para Desfazer/Refazer não deixarem o objeto sem malha.

## Limitações declaradas

- Filhos por primitiva continuam existindo (Entrega 2). Estão vinculados com `primitive`, mudanças de contagem de primitivas são reconciliadas, mas nomes das partes não seguem a fonte.
- Ordem entre irmãos não é reconciliada; objetos criados entram ao fim do pai.
- Instância com dois objetos dizendo ser o mesmo nó é **pulada** com aviso, sem mutação. Peça legada duplicada sem marca impede a adoção da instância inteira.
- Objeto de raiz única cuja fonte passa a ter várias raízes recebe as raízes novas como irmãs, no pai da instância.
- Aplicar alteração local de volta ao recurso compartilhado não existe: a fonte GLB nunca é regravada. Escopo compartilhado chega com `MaterialAsset` (Entrega 2).
- Mapa ilegível no disco não bloqueia: as identidades voltam a nascer das chaves da fonte, e isso não é avisado ainda.
- Sem fsync de diretório; a garantia contra queda de energia continua não declarada.

## Evidências

Host (`build/editor-host`, depuração):

- [Testes M08.2](../validacao/evidencias/m082-20260913/host-m082.log): **7/7** — mapa determinístico e ida e volta; id autoral, renomeação e troca de pai em conjunto não veicular; ambiguidade recusada, prévia travada e escolha explícita; duas instâncias com peça deslocada e script, fonte nova com porta e rodas homônimas, salvar/reabrir idêntico e cena antiga reconciliada na abertura; órfão com script e remoção local respeitada, reimportação desfeita em um passo; reverter, duplicar, desvincular e adoção de legado; journal com mapa interrompido e journal v1.
- [Suíte](../validacao/evidencias/m082-20260913/host-suite.log): **872/874**. As duas falhas (`every_console_row_is_reachable_by_touch`, `the_ide_toolbar_is_icons_and_the_rest_lives_in_one_menu`) já falhavam antes desta entrega, na base com as alterações do outro agente (865/867), e não tocam importação.
- [Arquivos reais](../validacao/evidencias/m082-20260913/real-reimport.log), `aether_tests --reimport-glb`: Torre (não veículo), Ford Lotus, CarConcept (Khronos) e Porsche. Em todos: duas instâncias vinculadas (Porsche 392 objetos), peça deslocada aparece como alteração de posição, republicar o mesmo conteúdo não muda nada, correspondência geral forçada casa 100% dos nós sem ambiguidade, reabertura byte a byte idêntica. Tempos são de build de depuração no PC, não medição do aparelho.

Aparelho (APK `537A3788785886B91A031717FEDFF8F9042E9739B59B9F8AF17B4BBAAF41EA4F`, instalado sem apagar dados; projeto de conferência `M08Recursos0913k`; capturas feitas só com o editor em primeiro plano):

1. **Cena legada abre e ganha vínculo comprovado.** Na abertura, os mapas das três fontes anteriores foram gerados e gravados em `.astra/imports/`; o objeto `Cortina:lod4` do Ford, instanciado antes desta entrega, aparece vinculado e "igual à fonte". [Captura](../validacao/evidencias/m082-20260913/legado-ford-vinculado-adb.png).
2. **Fonte sintética** `m082-veiculo.glb` (v1 gerada por `aether_tests --write-m082-fixtures`) registrada pelo painel Arquivos → Reimportar → *Importar na cena*, e instanciada de novo com *Instanciar*: duas instâncias vinculadas; o mapa foi gravado dentro da transação. [Captura](../validacao/evidencias/m082-20260913/duas-instancias-vinculadas-adb.png).
3. **Edição local:** a segunda roda da instância A foi arrastada pelo gizmo; a linha de vínculo passou a "1 alteração local". [Captura](../validacao/evidencias/m082-20260913/roda-alteracao-local-adb.png).
4. **Fonte trocada por v2** (carroceria sobe, porta nova, rodas homônimas paradas) e Reimportar: a prévia mostrou "4 por estrutura · 1 nó novo · 0 removidos". [Captura](../validacao/evidencias/m082-20260913/previa-reimportacao-adb.png).
5. **Só recurso → reconciliação:** [cena salva puxada do aparelho](../validacao/evidencias/m082-20260913/cena-apos-reimportar-adb.log) — roda de A manteve Y = 0,782; as duas carrocerias receberam Y = 0,5; as duas instâncias ganharam `Porta`; cinco objetos por instância, sem duplicação. [Captura](../validacao/evidencias/m082-20260913/reimportado-adb.png).

6. **Reabertura:** APK `FBD95442E60499884F9B6B0F18BB2A966DEFB3FEF8EA0B287498D61FF3B0055D` (com a correção abaixo) instalado por cima, force-stop e projeto reaberto: 281 vínculos antes e depois, cena byte a byte idêntica (SHA-256 `EDD4491D…0FC0`), instâncias e veículos legados de volta ao viewport. [Log](../validacao/evidencias/m082-20260913/reabertura-adb.log), [captura](../validacao/evidencias/m082-20260913/reaberto-apos-reinstalar-adb.png).

Defeito visto no aparelho e corrigido: o cabeçalho do inspetor contava o vínculo como componente ("2 componentes" numa roda com só Malha).

## Pendências desta entrega

- Conferência no aparelho da prévia com ambiguidade e do menu de reverter/desvincular (cobertos no host).
- Aviso de mapa ilegível; resolução de conflito campo a campo na UI (hoje o local vence e o conflito é listado).
- Aceitação IMP03/IMP05/AST01–AST08 completa depende das Entregas 2–4.
