# I4 — ambientes em conjunto e navegação UV · 28/09/2026

Implementação: `9644b320`. Continuação de `bd67ebce` e `7b85bfc1`. Fecha o recorte pendente do item 87: mapas HDRI e perfis de ambiente em conjunto. A edição conjunta de texturas já veio em `df0c1538`; a de materiais, em `bd67ebce`. Não adiciona tipos de componente nem declara concluídos os demais blocos do inventário.

## Capacidade implementada

**Perfis de ambiente:** selecionar vários arquivos abre o Inspector do perfil ativo com quantidade e valores mistos. Editar ou copiar um campo altera somente essa propriedade em todos os selecionados, inclusive quando o valor escolhido já pertence ao ativo. As condições de visibilidade/editabilidade dos descritores continuam valendo para todos os alvos. Os ambientes vinculados são sincronizados; peso, forma e prioridade da instância não viram dados do perfil. A janela focada conserva seu próprio alcance.

**HDRI:** a receita permanece em rascunho por mapa. O gesto num campo altera só esse campo nos selecionados; toque longo no valor misto oferece os valores individuais. Reverter restaura cada receita salva. Aplicar prepara os derivados reais no worker Android, preserva o cache anterior, verifica fontes/receitas novamente e publica o registro apenas quando todo o lote está pronto. Cancelamento, troca de projeto e conflito não publicam parte do lote. A publicação sinaliza a atualização da biblioteca consumida pelo renderer. Prévia, exposição de prévia e contagem de usos continuam relativas ao ativo.

**Histórico e disco:** uma operação de recurso por gesto/lote; Desfazer/Refazer verifica os alvos antes de publicar. Materiais e perfis compartilham `EditorImportTransaction::publishTextBatch`, usando o journal e seus arquivos companheiros, sem um segundo mecanismo de transação. O histórico HDRI guarda receitas e hashes, sem acumular cópias dos panoramas; restaura os derivados pelo cache.

**UV:** o seletor mantém o título fixo e rola o corpo, incluindo a lista de texturas. Recorta desenho e regiões de toque. A lateral com menos de 320 unidades de largura usa um eixo por linha; a larga mantém duas colunas. Deslocamento, escala, rotação e Zerar deixam de desaparecer quando falta altura. Principal e janela focada guardam suas rolagens separadamente.

## Referências concretas

- [Godot 4.5 — Import process, Reimporting multiple assets](https://docs.godotengine.org/en/4.5/tutorials/assets_pipeline/import_process.html#reimporting-multiple-assets): reimportar vários recursos preservando parâmetros não escolhidos. Adaptação: rascunho por GUID, gesto por campo e publicação conjunta.
- [Godot 4.5 — código de ImportDock](https://github.com/godotengine/godot/blob/4.5/editor/docks/import_dock.cpp): parâmetros editados são separados dos caminhos e controlam quais opções se aplicam ao conjunto. A Astra usa suas receitas tipadas e seu journal existente.
- [Unity 6.0 — Inspector options](https://docs.unity3d.com/6000.0/Documentation/Manual/InspectorOptions.html): propriedades comuns e valores mistos no Inspector.
- [Unity 6.0 — Volumes in URP](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/Volumes.html): componente de volume referencia o perfil que guarda aparência. Na Astra, perfil compartilhado e propriedades espaciais da instância continuam separados.

Nenhuma imagem conceitual foi usada como evidência. As capturas são da UI executável; a composição existente foi mantida e os controles estreitos foram refinados após inspeção. Foram reutilizados os ícones existentes de material, HDRI e perfil, sem conceitos novos exigindo outro ícone.

## Validação

Build host e APK Debug Android passaram. Suíte nativa: **1.239/1.239**. Após os refinamentos finais do estado de preparo, apresentação numérica e conflito de rascunho, os seis testes dos filtros `project_hdri_opens`, `multiple_` e `environment_batch` passaram novamente.

Testes de comportamento:

- `multiple_material_assets_edit_one_property_atomically_and_keep_focused_scope`: agora alcança Zerar por arraste numa superfície **853 × 394**, sem aumentar a janela para contornar o defeito. Continua cobrindo campo isolado, recurso vivo, persistência, conflito, histórico e janela focada.
- `multiple_environment_profiles_preserve_other_fields_and_synchronize_their_users`: editar/copiar valor igual ao ativo, preservar exposição individual e peso de volume, sincronizar consumidores, arquivos reabertos, recusa de conflito e histórico conjunto.
- `project_hdri_opens_in_properties_with_preview_uses_and_recipe`: inclui rascunho conjunto aberto enquanto outra reimportação altera a receita; Aplicar recusa o conflito, e Reverter adota os dados realmente salvos.
- `environment_batch_prepares_real_hdris_and_publishes_or_replays_all_recipes_atomically`: fontes Radiance realmente importadas, cancelamento, conflito na segunda fonte, ausência de publicação parcial, invalidação do renderer, cache anterior, recusa íntegra se cache faltar, Desfazer/Refazer e registro reaberto do disco.

No aparelho **25053PC47G**, APK instalado, projeto **I4Materiais0928** (cópia isolada; original preservado):

1. Dois perfis reais, com cores de neblina diferentes: seleção mostrou `—`; copiar a cor do ativo aplicou a diferença ao outro; Desfazer restaurou a divergência e Refazer reaplicou. Conferência da interface e arquivos.
2. Dois HDRIs registrados com panoramas 256 e 512: seleção mostrou `—`; copiar 512 e elevar amostras GGX de 32 para 64 publicou os dois pelo worker. Um Desfazer restaurou 256/512 e 32 amostras; Refazer voltou a 512/512 e 64. Conferência do registro e Inspector.
3. Dois materiais: deslocamento U para 0,05, rolagem real da lista, Zerar e Desfazer recuperando 0,05; Refazer voltou a zero. Cabeçalho continuou acessível.

Após reinstalar o APK atualizado e reabrir o aplicativo, o Inspector confirmou as receitas 512/512 e 64 amostras. O estado de preparo de outro lote foi capturado com apenas Cancelar, sem controles herdados da importação individual.

Capturas: [perfis mistos](../capturas/i4-ambientes/perfis-mistos.png), [perfis aplicados](../capturas/i4-ambientes/perfis-aplicados.png), [perfis desfeitos](../capturas/i4-ambientes/perfis-desfazer.png), [HDRIs mistos](../capturas/i4-ambientes/hdri-mistos.png), [copiar receita](../capturas/i4-ambientes/hdri-copiar.png), [HDRIs aplicados](../capturas/i4-ambientes/hdri-aplicados.png), [HDRIs desfeitos](../capturas/i4-ambientes/hdri-desfazer.png), [reabertura](../capturas/i4-ambientes/hdri-reabertos.png), [preparo do lote](../capturas/i4-ambientes/hdri-preparo.png), [rolagem UV no aparelho](../capturas/i4-ambientes/uv-rolagem.png) e [UV em janela curta no host](../capturas/i4-ambientes/uv-host-janela-curta.png).

A sincronização de perfis com componentes e a invalidação da biblioteca de mapas têm evidência host. Os recursos de ambiente da fixture Android não foram vinculados à cena: as capturas do viewport não são prova de comparação visual da iluminação renderizada. Cancelamento/conflitos foram validados no host, não provocados durante escrita no aparelho.

## Limites operacionais explícitos

- Recursos do projeto são editados em modo de edição; o espelho de Play continua separado.
- O lote HDRI aceita até **128 MiB de derivados acumulados**, respeitando os limites do importador por fonte. Excesso é recusado antes da publicação; lotes grandes devem ser divididos.
- O histórico HDRI depende dos caches validados. Se forem removidos externamente durante a sessão, Desfazer recusa o lote inteiro com diagnóstico; não executa uma importação pesada na thread de UI nem restaura parcialmente. O histórico não é persistente entre sessões.
- Tipos sem editor conjunto próprio continuam na visão de tipos/contagem; não se inventa um Inspector de propriedades para recursos sem esse consumidor.

## Próximos passos por dependência

O plano principal continua sendo [Expansão de objetos, componentes e API](EXPANSAO-OBJETOS-COMPONENTES-API-2026-09-26.md), com execução detalhada pelo [inventário de Inspector e objetos](INVENTARIO-UNITY-INSPECTOR-OBJETOS-2026-09-26.md). A próxima frente é **O1**; não foi iniciada nesta entrega.

| Pacote seguinte | Implementação vertical e aceite |
|---|---|
| **O1a — estado e identificação** | Expor `ActiveSelf` separado de `ActiveInHierarchy`; tags com catálogo persistente, Inspector, `CompareTag` e buscas; habilitação uniforme apenas para componentes com lifecycle real. Aceite: pai inativo/filho ativo, salvar/reabrir e scripts consultando os dois estados corretamente. |
| **O1b — acesso e lifecycle** | `GetComponent` de comportamento pelo tipo, busca global, remoção da própria instância e destruição com atraso no ponto seguro. Preservar geração de handles e callbacks; testar objeto destruído, script removido e troca de cena. Mensagens 132–134 precisam de contrato explícito de destino, assinatura e falha antes da API. |
| **O1c + P mínimo — instanciação** | `Instantiate` exige identidade e clonagem de hierarquia/componentes/referências; instanciação de prefab exige recurso e overrides funcionais. Implementar a fatia necessária de P junto, antes de prometer Instantiate(prefab). Aceite: duas instâncias independentes, referências internas remapeadas, edição/salvamento e destruição sem corromper o recurso. |
| **O2 — primitivas** | Esfera, cilindro, cápsula, quad e plano com geometria, normais, UV, bounds e colisores corretos; criação por editor e API usando o mesmo caminho. Aceite: criar, editar, salvar/reabrir e entrar em Play com colisão real. |
| **O4 — ícones de objeto** | Metadado persistente de ícone/rótulo, seletor e consumidor no viewport; filtros de Gizmos. Capturar no aparelho e validar seleção, zoom e sobreposição antes de fechar. |
| **P completo; S → M; T; C** | Expandir prefabs/overrides; sprites antes de tilemap; terreno com autoria e runtime; Canvas apoiado em texto/input reais. Continuar por famílias do atlas, sem aumentar contagem por nomes vazios. |

`Static` (115) não deve voltar como flag decorativa: cada opção só entra com seu consumidor de iluminação, oclusão, batching ou navegação. A ordem do inventário é referência; dependências reais, como P para Instantiate(prefab), precisam ser antecipadas de forma explícita.

Continuação em 28/09: [O1a — ativação e lifecycle](O1A-ATIVACAO-2026-09-28.md) entrega o item 113 e corrige scripts sob hierarquia inativa. Tags e habilitação uniforme continuam pendentes.
