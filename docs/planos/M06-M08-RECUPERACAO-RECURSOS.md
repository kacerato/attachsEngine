# M06/M08 — recuperação de código e recursos transacionais

13/09/2026 · base `8064704`, branch `codex/gameplay-runtime`. Alterações de trabalho, sem commit/push nesta rodada. Continuação autorizada por “prossiga”; ADB autorizado e aparelho desbloqueado pelo usuário.

Atualização posterior ao relato de GLBs recusados: [compatibilidade com arquivos reais](M08-COMPATIBILIDADE-GLB-REAIS.md). Ela atualiza as restrições de acessores/extensões, o orçamento de seleção e o percurso opcional `Importar na cena` descritos historicamente abaixo.

## Resultado e vínculo com o plano

O [plano mestre](PLANO-MESTRE-ASTRA-EDITOR-RUNTIME-ASSETS.md) foi relido integralmente, incluindo contratos, marcos, fichas e referências. A implementação avança o bloco funcional M06 e a infraestrutura M08.1/M08.3; **não encerra M06, M07 nem M08**. O layout de IDE aceito e os sistemas de viewport/câmera/grade não foram redesenhados.

| Contrato | Implementação desta rodada | Limite |
|---|---|---|
| M06.2 — análise de linguagem | `ProjectLanguage.Analyze` reutiliza compilation e árvores Roslyn; substitui árvores alteradas, remove fontes excluídas e descarta cache de outro projeto | Análise semântica incremental; não é build incremental de assemblies. Ainda lê as fontes para montar o snapshot; cache de um projeto |
| M03.3 / M06.1 — rascunhos | Checkpoint de buffers dirty em `.astra/code-drafts.astra`, escolha Recuperar/Descartar na abertura, restauração de texto e seleção | Janela de até aproximadamente 1 s; composição IME ativa adia checkpoint. Histórico completo, scroll e composição do IME não são serializados |
| TXT07 — conflito externo | Recovery conserva a base original do buffer; salvar compara com a fonte atual e recusa divergência | Resolver conflitos/Salvar como ainda precisa de fluxo dedicado; arquivo removido continua recuperável no buffer, mas não é recriado silenciosamente |
| M08.1 — entrada pelo projeto | Ícone Importar em Arquivos, no workspace de cena e código; entrada retirada do catálogo de objetos | Destino de fontes externas continua `Fontes/`; miniaturas, seleção de destino e inspector de asset permanecem abertos |
| M08.3 — preparação | Leitura do ContentResolver em executor único, sem fila; parsing nativo em worker; um resultado preparado por vez, protegido por projeto/época | Cancelamento cooperativo entre leituras/etapas; chamada de provedor bloqueada não é interrompida à força |
| M08.3 — revisão | Relatório com nós, desenhos/materiais produzidos, omissões do perfil e publicação explícita | Relatório textual funcional; preview 3D, árvore navegável e opções de importação não estão entregues |
| M08.1 — recurso versus instância | `publishModel` registra sem criar objetos; `instantiateModel` cria hierarquia em uma transação Undo; repetir cria IDs independentes com geometria compartilhada | Não é prefab completo nem SceneAsset com overrides persistentes |
| M08.3 — integridade | Backup + journal antes de publicar; fonte e registry por substituição de arquivo; rollback CPU/GPU e recuperação de publicação interrompida | GPU pode recusar a própria restauração: erro é reportado. Não há prova de durabilidade sob queda de energia ou storage cheio |
| OBS01 — importador | Origem Importador no console, severidades, retenção e filtro existentes | AssetId/ImportTransactionId e barramento de todos os produtores continuam pendentes |

[Evidências e resultados](../validacao/2026-09-13-m06-m08-recursos.md).

## Percurso entregue

1. Em **Arquivos**, Importar abre o seletor do Android. O arquivo é lido fora da UI com limite de 128 MiB na ponte.
2. O parser prepara GLB estático em worker, com cancelamento. Não altera documento, registry nem fonte do projeto.
3. O relatório identifica criação/atualização, contagens e recursos ignorados. Cancelar invalida o token do picker e o resultado preparado.
4. **Publicar recurso** compara a fonte no destino com o hash observado na preparação. Se ela mudou, a operação é recusada.
5. O journal é preparado; o registry candidato é validado antes de publicar geometria; só depois são gravadas fonte e registry.
6. **Instanciar** cria nós e renderers na cena. Outra execução cria outra instância. Undo afeta a instanciação, preservando o recurso registrado.
7. **Reimportar** usa a fonte do projeto e o mesmo relatório. Atualiza geometria por identidade existente, preservando os objetos e componentes atuais. A UI informa explicitamente que mudanças na árvore da fonte não são reconciliadas.

## Persistência e recuperação

`AETHER_ASSETS 1` e `AETHER_EDITOR 12` permanecem nos formatos atuais. Não houve conversão de cenas antigas nem inferência de hierarquias perdidas.

O journal `.astra/import-transaction/journal` usa `ASTRA_IMPORT_1`, estado `prepared`/`committed`, destino e flags de existência. `source.backup` e `registry.backup` guardam o par anterior. A abertura do projeto recupera `prepared` antes de carregar recursos; `committed` apenas limpa os resíduos. Falha de recuperação preserva os backups e impede a abertura do editor para evitar salvamento sobre um estado parcialmente recuperado. O detalhe dessa falha de abertura ainda vai para Logcat; falta apresentação correspondente no shell de projetos.

A escrita usa `replaceAssetFile`: arquivo temporário, flush/fsync de arquivo no Android e rename. Não há fsync de diretório nesta infraestrutura; não declarar garantia contra perda de energia. A recuperação foi exercitada com interrupção simulada no host, não com falha física de armazenamento.

O checkpoint `ASTRA_DRAFTS_1` contém os buffers dirty, caminho relativo, base salva, texto editado, seleção e indicação do buffer ativo. Aceita até 16 buffers de 512 KiB, leitura total limitada e caminhos sem fuga/symlink. Conteúdo recuperado não é gravado no arquivo-fonte ao abrir o projeto: exige Recuperar. Após essa ação, volta ao fluxo habitual de build, que só salva se a base ainda corresponder ao disco.

## Correções concretas encontradas

- A fonte era aberta com `wb` antes de o GLB ser aceito. Agora só muda após preparação, revisão e backup.
- O retorno de `AssetRegistry::add/publishImport` era ignorado. Agora o candidato é recusado antes da GPU se o registro falha.
- O registry não era marcado dirty ao importar. Publicação interativa agora grava o registro imediatamente; reidratação sinaliza atualização corretamente.
- Uma segunda fonte com várias primitivas podia indexar nomes usando o deslocamento global em um vetor local. O índice agora é local ao recurso.
- Criação de objetos interrompida podia deixar parte da hierarquia. Instanciação restaura documento e histórico em falha.
- Matriz incompatível com TRS podia ser substituída por identidade. Agora a publicação é recusada com diagnóstico.
- Trocar o registry inteiro durante a reidratação invalidaria a iteração: a carga percorre uma cópia dos registros.
- A primeira integração desenhava o relatório somente no IDE, embora bloqueasse input na cena. ADB detectou o erro; `buildProjectDialogs` passou a ser compartilhado pelos dois workspaces e o fluxo foi repetido.

## Pendências que continuam prioritárias

1. **M08.2: reconciliação estrutural real.** Mapa persistente de nós/subassets, vínculo de instância, base anterior/nova/local, conflitos de renomeação/reparenting, nós adicionados/removidos e preservação de órfãos. As chaves atuais ainda dependem dos nomes do importador; não anunciar identidade universal de nós.
2. **M07/M08: slots e SceneAsset.** Nó com várias primitivas ainda ganha filhos por primitiva. Falta material slot autoral e recurso de cena reutilizável com instâncias aninhadas.
3. **M06/M01/M05:** ComponentInstanceId em logs runtime, produtores render/lifecycle, campos inline do inspetor, resolução de conflitos e validação de recovery/IME no aparelho.
4. **M08: experiência de recursos.** Árvore/preview de importação, relatório navegável em telas pequenas, destinos, miniaturas, progresso por fase e transações de mover/excluir com dependentes.
5. **M09:** texturas/imagens, dependências glTF externas, animação, skins, morphs e cobertura de material. Nenhum desses consumidores foi presumido pronto nesta rodada.
6. **Robustez/performance:** imports grandes, pressão de memória, storage cheio, cancelamento durante provedor bloqueado, perda de processo durante commit no Android e matriz de orientação. GPU e persistência finais continuam síncronas; a biblioteca gráfica ainda é republicada inteira.

## Referências e fronteira de evidência

A [documentação de overrides da Unity](https://docs.unity3d.com/6000.0/Documentation/Manual/PrefabInstanceOverrides.html) orienta a preservação de alterações por instância. Ela não comprova que Astra já possui reconciliador de prefabs. Os contratos do plano mestre e a leitura dos consumidores reais definem o que foi implementado acima.

Rollback de código: reverter os arquivos deste bloco em um commit seletivo futuro, preservando alterações anteriores do usuário. Não apagar projetos, fontes ou backups. A versão anterior não interpreta o novo journal: resolver qualquer transação preparada antes de executar um APK antigo.
