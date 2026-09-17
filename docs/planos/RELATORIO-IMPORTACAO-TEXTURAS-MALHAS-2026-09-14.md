# Astra — importação, texturas, malhas e abertura de projetos

Relatório técnico e roteiro de implementação • 14 de setembro de 2026

**Escopo desta entrega: análise e proposta. Nenhuma mudança na engine, compilação, teste ou operação ADB foi executada nesta rodada.** As capturas e conferências anteriores citadas pertencem ao repositório. O intervalo de aproximadamente 15 segundos de tela preta é o relato do usuário, não uma medição feita nesta análise.

Base final inspecionada: `6c7991e851110fc105291a264071eef37f088ae8`, branch `codex/gameplay-runtime`. O repositório tinha alterações locais de artefatos de build; elas foram preservadas. O commit `6c7991e8` chegou durante a análise e foi incorporado ao diagnóstico. Há uma pequena alteração de texto de prévia nesse commit que, segundo sua própria descrição, não pertence ao APK das capturas.

## 1. Parecer

**A prioridade deve ser tornar os recursos existentes realmente gerenciáveis e eliminar o trabalho pesado entre o fim do loading e o primeiro quadro do editor.** Os commits recentes acrescentaram capacidades importantes. Entretanto, importar um arquivo e desenhar suas texturas ainda não oferece o fluxo de autoria esperado de uma engine.

Os quatro problemas descritos têm correspondência concreta no código:

| Problema | Constatação | Mudança necessária |
|---|---|---|
| Loading termina e surge uma tela preta | A tela inicial usa duração fixa de 1,8 s. Depois, a abertura nativa reimporta fontes sequencialmente antes de terminar a recuperação da cena. | Progresso associado ao trabalho real, primeiro quadro confirmado, preparação assíncrona e reaproveitamento de dados importados. |
| Importação/reimportação vira um popup de texto | O construtor desenha uma janela central, escurece toda a superfície e pagina `importSummary`. | Editor de recurso dentro de Propriedades, com controles, prévia, navegação e relatório de diagnóstico separado. |
| Texturas aparecem, mas não podem ser administradas como recursos | Há decodificação e publicação de texturas. O material autoral persistido guarda parâmetros numéricos, sem um contrato completo de referências de textura editáveis. | Identidade, origem, configurações, vínculos por canal, dependências e substituição de texturas. |
| Malha tem poucas possibilidades de edição | Existem slots e materiais compartilhados. O inspetor continua organizado em poucas opções, e o seletor apresenta índices como “Malha N”. | Inspetor de malha com nomes reais, prévia técnica, superfícies, materiais, geometria, desempenho e acesso aos componentes relacionados. |

O próximo pacote não deve repetir “adicionar Draco”, “adicionar materiais por slot” ou “começar PNG/JPEG”: isso já avançou. Deve completar a ligação **fonte → recurso editável → instância → renderer → reabertura**, incluindo o layout solicitado.

### O que significa alcançar as mesmas funções

É possível usar as engines de referência como critérios verificáveis de capacidade. Não é correto declarar equivalência integral apenas porque existem botões com nomes semelhantes. A própria localização e o comportamento das opções variam entre importador, recurso, componente e pipeline gráfico.

Este relatório registra **112 itens de trabalho**, além do plano de carregamento. É uma matriz delimitada de importação, texturas, malhas e sistemas diretamente relacionados. Não representa o inventário de todos os plugins, todos os métodos de API e todas as versões das engines. A meta de expansão permanece; cada capacidade precisa de referência, contrato, consumidor e demonstração funcional antes de ser marcada como entregue.

## 2. Base documental e estado real da Astra

O plano mestre foi considerado por inteiro, incluindo identidade, recuperação de Activity/surface, entrada inline, console, importação estrutural, transações, aparência, docks, universalidade, desempenho e distribuição. Os marcos particularmente afetados são **M03, M08, M09, M10, M13, M14 e M15**. M06 fornece o padrão visual aprovado; não há motivo para reiniciar o IDE ou refazer o viewport neste pacote.

Documentos locais principais:

- [Plano mestre](C:/Users/donod/Downloads/atchengine/docs/planos/PLANO-MESTRE-ASTRA-EDITOR-RUNTIME-ASSETS.md).
- [Identidade e reconciliação](C:/Users/donod/Downloads/atchengine/docs/planos/M08-2-IDENTIDADE-RECONCILIACAO.md).
- [Submalhas e materiais](C:/Users/donod/Downloads/atchengine/docs/planos/M08-E2-SUBMESHES-MATERIAIS.md).
- [Texturas PBR](C:/Users/donod/Downloads/atchengine/docs/planos/M09-E3-TEXTURAS-PBR.md).
- [Codecs e dependências externas](C:/Users/donod/Downloads/atchengine/docs/planos/M08-M09-E4-DEPENDENCIAS-CODECS.md).
- [Fidelidade do IDE](C:/Users/donod/Downloads/atchengine/docs/design/m06-ide-v2/FIDELIDADE.md).

### Capacidades que devem ser preservadas

| Área | Implementação encontrada | Limite relevante para o próximo pacote |
|---|---|---|
| Identidade importada | Mapa persistente de nós, IDs, revisão, vínculos de origem e reconciliação entre fonte anterior, edição local e nova fonte. | Correspondências ambíguas ainda dependem de escolhas globais pouco visuais; mapa ilegível pode regenerar identidades sem o diagnóstico necessário. |
| Estrutura | Hierarquia e pivôs; várias primitivas de um nó representadas por slots do mesmo objeto. | Migração de cenas antigas conserva casos ambíguos; não há justificativa para fundir peças úteis indiscriminadamente. |
| Materiais | `MeshRenderer` v3, até 256 slots, material de projeto, edição local ou compartilhada e atualização de parâmetros sem reconstruir geometria. | Material autoral é essencialmente numérico; exclusão de material ainda pode disparar reconstrução ampla; histórico compartilhado precisa de contrato próprio. |
| Texturas | PNG/JPEG, KTX2, samplers e caminhos distintos para cor e dados; mapas de cor, normal, metal/rugosidade e emissão. | Gestão visual e referências autorais incompletas; oclusão de material não aplicada; tangentes geradas não equivalem a MikkTSpace. |
| Codecs | Draco e meshopt decodificados; KTX2 pode virar ASTC 4×4 quando o aparelho suporta a amostragem exigida. | Não confundir decoder meshopt com simplificador de malhas. ASTC não significa que PNG/JPEG já tenham encoder ASTC. |
| Transformação UV | `KHR_texture_transform` assado nas UVs quando os usos daquele conjunto concordam. | Transformações diferentes por textura não são atendidas integralmente. Trocar material exige preservar a UV original. |
| Reflexão | Geometria refletida, orientação e tangentes compensadas no caminho de importação. | Isso não transforma automaticamente todo o runtime físico/autoral em um sistema de TRS negativo arbitrário. |
| glTF externo | Seleção múltipla SAF e empacotamento de dependências em GLB, com diagnósticos de dependência. | Há limites de quantidade/tamanho, associação por nome e manifest `.deps` fora do fechamento transacional principal. Fluxo externo não foi confirmado no aparelho pela documentação consultada. |
| Renderização | Infraestrutura de LOD, culling, colisão estática e instâncias existe em áreas da engine. | Não é prova de que todo modelo importado receba configuração autoral, geração e persistência desses sistemas. |

Há documentação que precisa ser reconciliada: comentários de `gltf_import.h` ainda descrevem seleção de um único arquivo e ausência de imagens externas, enquanto a entrada Android ganhou empacotamento de dependências. Outros comentários mantêm a transformação UV como inteiramente inaplicada. Esses comentários não anulam os commits novos, mas podem induzir o próximo agente ao diagnóstico errado.

### Evidências locais de implementação

| Evidência | Ponto de entrada |
|---|---|
| Duração fixa e transição para Activity | [AstraShellActivity.java:22](C:/Users/donod/Downloads/atchengine/android/app/src/main/java/dev/aether/editor/shell/AstraShellActivity.java:22) e [ShellView.java:130](C:/Users/donod/Downloads/atchengine/android/app/src/main/java/dev/aether/editor/shell/ShellView.java:130) |
| Reabertura sequencial de fontes | [android_main.cpp:671](C:/Users/donod/Downloads/atchengine/native/platform/android/android_main.cpp:671) |
| Finalização do renderer seguida da recuperação do projeto | [android_main.cpp:711](C:/Users/donod/Downloads/atchengine/native/platform/android/android_main.cpp:711) |
| Acumulação/publicação de biblioteca | [editor_session.cpp:1715](C:/Users/donod/Downloads/atchengine/native/editor/editor_session.cpp:1715) e [editor_session.cpp:1922](C:/Users/donod/Downloads/atchengine/native/editor/editor_session.cpp:1922) |
| Espera da GPU e reconstrução de recursos | [instanced_renderer.cpp:3904](C:/Users/donod/Downloads/atchengine/native/platform/android/instanced_renderer.cpp:3904) |
| Popup atual | [editor_screen.cpp:1861](C:/Users/donod/Downloads/atchengine/native/editor/editor_screen.cpp:1861) |
| Slots, seletor e inspetor de malha | [editor_screen.cpp:603](C:/Users/donod/Downloads/atchengine/native/editor/editor_screen.cpp:603), [editor_screen.cpp:713](C:/Users/donod/Downloads/atchengine/native/editor/editor_screen.cpp:713), [editor_screen.cpp:815](C:/Users/donod/Downloads/atchengine/native/editor/editor_screen.cpp:815) |
| Contrato persistido de material | [material_asset.h](C:/Users/donod/Downloads/atchengine/native/resources/material_asset.h) e [material_asset.cpp](C:/Users/donod/Downloads/atchengine/native/resources/material_asset.cpp) |

As linhas são pontos de navegação no snapshot analisado; podem mudar com novos commits.

## 3. A tela preta: diagnóstico e correção proposta

### Sequência encontrada

```text
Escolher projeto
  → animação de loading com duração fixa de 1,8 s
  → abrir AetherActivity
  → inicialização assíncrona do renderer básico
  → collectRendererInitialization no loop nativo
  → reimportProjectSources, fonte por fonte
      → ler bytes, interpretar GLB, decodificar geometria/texturas
      → reunir biblioteca acumulada
      → esperar GPU e reconstruir recursos
  → recuperar documento da cena
  → editor finalmente apresenta seu conteúdo
```

**Confirmado por leitura:** a animação inicial não representa a conclusão desse trabalho. A reabertura percorre recursos de malha registrados com fonte, inclusive antes de recuperar o documento final da cena. A publicação passa por `vkDeviceWaitIdle` e reconstrução de buffers, descritores e filas.

**Inferência forte:** esse trabalho síncrono no loop nativo contribui para a lacuna visual. A inicialização básica assíncrona não torna assíncrona toda a abertura do projeto. O desenho do editor espera uma cadeia posterior de trabalho pesado.

**Ainda não determinado:** quanto dos 15 s vem de leitura, codecs, geração de mipmaps, cópias CPU, upload, espera GPU, picking ou criação de pipelines. Não é possível atribuir os 15 s a uma única função sem medir o cenário do aparelho.

Há também amplificação de trabalho: ao importar A, depois B, depois C, a publicação reúne A, depois A+B, depois A+B+C. Com fontes de tamanhos semelhantes, o volume acumulado processado pode crescer como a soma de 1 até N. Isso é uma propriedade do fluxo, não um benchmark desta rodada.

### Contrato de abertura recomendado

1. **Uma sessão de carregamento com ID e etapas reais.** Estados propostos: abrir projeto, recuperar documento, resolver recursos, preparar dados, publicar GPU e pronto. Falha e cancelamento são estados próprios. Uma troca de projeto ou Activity invalida resultados antigos.
2. **Continuidade visual.** Manter uma superfície leve de carregamento durante a transição; retirar sua cobertura somente quando a nova superfície tiver apresentado um quadro válido. “Renderer inicializado” e “quadro apresentado” são eventos diferentes. Se a UI nativa completa ainda depende do atlas, o host pode manter a cobertura leve até esse ponto.
3. **Editor aparece antes de todas as texturas finais.** Após identidade e documento estarem resolvidos, permitir hierarquia e navegação; recursos pendentes usam representação explícita de carregamento, sem modificar o conteúdo salvo. O usuário não deve interpretar objetos ainda não residentes como objetos apagados.
4. **Preparação CPU em tarefas.** I/O, parsing, codecs e preparação de imagens saem do loop de apresentação. Publicação Vulkan respeita a propriedade dos recursos e o orçamento por quadro; mover chamadas Vulkan cegamente para outra thread seria insuficiente.
5. **Publicação inicial em lote.** Para a primeira correção estrutural, reunir as fontes necessárias e publicar uma vez. Depois evoluir para atualização incremental por recurso, com handles estáveis e liberação após o uso GPU terminar. Não remover a espera de segurança antes de implantar o controle de vida útil que a substitui.
6. **Cache derivado em disco.** Reabrir dados preparados, não repetir o importador inteiro para fontes inalteradas. A chave deve incluir conteúdo de todas as dependências, perfil de importação, versões de importador/codecs/schema e formato alvo. A ideia de separar derivados regeneráveis das fontes é bem estabelecida no DDC da Unreal; a Astra pode implementá-la localmente sem adotar um serviço de nuvem.[^19]
7. **Orçamento global.** O limite por arquivo não limita o conjunto do projeto. Planejar memória de imagens decodificadas, mipmaps, staging, buffers de geometria e residência GPU conjuntamente. Deduplicação deve respeitar conteúdo, semântica de cor, formato e perfil.
8. **Recuperação e diagnóstico.** Um cache inválido regenera somente o necessário; fonte ausente gera referência ausente com ação de localizar. Falha de um modelo não deve encerrar a abertura de todo o editor.

A medição futura deve registrar início, primeiro quadro de UI, primeira cena utilizável e recursos completos. Android distingue primeiro desenho e conclusão de exibição; essa distinção é útil, mas os indicadores do sistema para abertura de Activity não substituem métricas próprias de abertura de projeto.[^25]

**Critério funcional proposto:** não existir intervalo preto entre loading e editor; progresso permanecer responsivo; fontes inalteradas não serem novamente decodificadas em uma reabertura com cache válido. O tempo total alvo será definido após a medição inicial, com projeto, aparelho e condições registrados.

## 4. Referências: o que adotar e onde há diferenças

| Referência | Contribuição verificável | Aplicação proposta na Astra |
|---|---|---|
| Unity — importador de modelo | Configuração de geometria e tratamento separado de materiais; configurações persistentes de importação.[^1][^2] | Perfil de importação por recurso; controles organizados, comparação antes de reimportar. |
| Unity — presets | Reutilização de conjuntos de propriedades e padrões de importação.[^7] | Presets do projeto, por pasta e por recurso, com origem do valor visível. |
| Godot — importação 3D | Configuração geral e opções avançadas por nó, malha e material.[^8][^9] | Árvore interna do GLB com configuração por item. O editor avançado da Godot é uma janela; a Astra deve adaptá-lo ao dock pedido pelo usuário. |
| Unreal — Interchange | Pipelines configuráveis, prévia dos recursos resultantes e detecção de conflitos.[^18] | Trabalho de importação estruturado, saídas inspecionáveis e políticas persistentes. |
| Unreal — editores de recursos | Prévia especializada, detalhes de textura, geometria, colisão e dependências.[^16][^17] | Propriedades com prévia útil e ferramentas especializadas, sem transformar o painel em log. |
| PlayCanvas — pipeline | Regras de importação, preservação de remapeamentos e políticas separadas para tipos de recurso.[^20] | Controles de atualização por categoria; aproveitamento do padrão de inspetores por tipo.[^21] |

**Limites da comparação:** a documentação de modelo da Unity utilizada inclui seu importador FBX. Ela serve como referência funcional; não prova que a Unity importe GLB pelo mesmo caminho. A versão/pipeline também altera opções de renderização. Na Unreal, a página atual de Interchange informa que a janela de conflitos já não oferece a antiga escolha de substituir/preservar material: copiar um vídeo antigo como especificação atual produziria erro.[^18]

Na Godot, algumas propriedades dependem do renderer: transições suaves de certas faixas de visibilidade não têm o mesmo suporte nos modos Mobile e Compatibility. Há inclusive uma opção de limite de mipmaps documentada como não implementada no importador de textura. A Astra deve oferecer capacidades efetivas e explicitar disponibilidade, não reproduzir controles inoperantes.[^10][^12]

### Pesquisa visual e vídeos

As páginas oficiais de importadores e editores listadas nas fontes contêm capturas dos respectivos painéis. Elas são referências para hierarquia de informação, prévia e organização. A identidade visual continua sendo a Astra e o IDE aprovado.

| Material | O que observar | Limite de uso |
|---|---|---|
| [Unity: Recorded Video Session — Importing 3D Art](https://learn.unity.com/tutorial/live-session-importing-3d-art-into-unity) | Sequência entre geometria, normais e materiais; organização dos assuntos. | Sessão histórica identificada como Unity 2017.3. Não usar como lista atual de opções. |
| [Unity Asset Manager — Quick start guide](https://learn.unity.com/tutorial/unity-asset-manager-guide?version=6.0) | Demonstração de busca, filtros e gerenciamento de assets, incluindo vídeo na página. | Produto de gerenciamento de assets; inspiração de navegação, não dependência de nuvem para Astra. |
| [Gwizz: Godot 4 Import 3D Models Tutorial](https://www.youtube.com/watch?v=16Uou6LWQ5Y) | Referência audiovisual complementar para o fluxo de entrada de modelos. | Publicado em 16/08/2023. Metadados e descrição localizados; reprodução/transcrição integral indisponível nesta pesquisa. |

As afirmações técnicas do relatório se apoiam nas documentações e no código, não em suposições sobre frames de vídeos que não foram assistidos integralmente. Não foram inventados timestamps.

## 5. Importação e reimportação dentro de Propriedades

### Comparação com a interface registrada

A [captura de importação do CarConcept](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/m09e4-20260914/previa-carconcept-astc-adb.png), incluída nos commits recentes e inspecionada nesta análise, mostra uma janela central grande, duas páginas de informação técnica e a região de Propriedades vazia ao lado. Portanto, o problema não é somente quantidade de texto: o espaço onde a edição deveria acontecer fica ocioso enquanto a janela bloqueia a cena.

| Hoje, na captura | Resultado especificado |
|---|---|
| Avisos de codec, UV e oclusão ocupam o corpo da janela. | Visão geral curta, contadores navegáveis e detalhes técnicos em seção própria. |
| Não há prévia do recurso dentro da janela. | Prévia real no dock, com peça/material selecionável e controle de enquadramento. |
| As opções se resumem principalmente ao destino da importação. | Configurações persistentes e editáveis antes da publicação. |
| Ações largas e explicações repetidas no browser e no popup. | Ação principal contextual, ferramentas compactas e ajuda por foco/toque prolongado. |
| Propriedades não acompanha o arquivo selecionado. | Arquivo selecionado ativa o inspetor do recurso; objeto selecionado ativa seus componentes. |

### Composição proposta

O painel deve reutilizar os recortes, superfícies, bordas, tipografia, estados e ícones raster da linha do IDE. O documento de fidelidade já identifica primitivas suficientes no renderer: não há evidência de que trocar toda a tecnologia de UI seja pré-requisito para esse desenho.

| Região do dock | Conteúdo | Comportamento |
|---|---|---|
| Cabeçalho | Ícone do tipo, nome real, caminho abreviado, voltar, fixar e menu. | Distingue recurso de instância; não usa índice GPU como nome. |
| Navegação | Visão geral e seções disponíveis para aquele recurso. | Poucas abas visíveis; seções adicionais em navegação compacta. |
| Prévia | Modelo, material ou textura real. | Altura ajustável, enquadrar, girar, fundo e modo técnico; câmera independente do viewport da cena. |
| Corpo | Campos inline, listas de recursos, miniaturas e grupos recolhíveis. | Busca por propriedade, favoritos, avançado e multisseleção quando compatível. |
| Estado | Etapa de importação e contadores de avisos/erros. | Um resumo curto; tocar abre os itens relevantes. |
| Rodapé | Uma ação primária contextual e ações secundárias por ícone/menu. | Mantém o significado de importar, atualizar e instanciar separado. |

Em paisagem, ocupa a região de Propriedades e permite redimensionamento. Em retrato ou largura insuficiente, deve existir um modo de foco desse mesmo dock com botão de retorno e estado preservado. Isso não exige impor retrato ao editor 3D inteiro nem interferir na preferência de orientação do IDE.

Não basta mover o retângulo do popup para a direita. É necessário substituir sua fonte de dados: `importSummary` pode continuar sendo exportação textual do diagnóstico, mas o painel precisa receber objetos de propriedade, recursos, avisos e conflitos identificados.

### Três contextos sem confusão

**Arquivo selecionado:** mostra o recurso e seu perfil persistido. Alterações de importação ficam em rascunho até “Atualizar”. Ajustes que não exigem reconstrução, como nome autoral ou organização, usam comandos próprios.

**Nova importação:** o seletor de arquivos do Android entrega a entrada, e o dock mostra o trabalho pendente. “Importar” cria o recurso. Uma opção compacta “Adicionar à cena”, inicialmente explícita, permite completar as duas operações. Fechar o dock não deve perder silenciosamente um trabalho em andamento.

**Objeto selecionado:** mostra componentes e sobrescritas da instância. Um vínculo abre o recurso no mesmo dock. Voltar restaura seleção, rolagem e grupos abertos. O caminho recurso → material → textura também deve ter retorno previsível.

### Personalização que efetivamente agrega valor

- **Visual:** largura e tamanho da prévia, densidade, seções abertas, favoritos, ordenação e posição do console. Salvar preferências por usuário/workspace.
- **Importação:** presets, valores padrão do projeto/pasta, opções por recurso e por nó. Salvar junto ao projeto, com precedência definida.
- **Autoria:** material compartilhado, variante de recurso e alteração local da instância. Mostrar o alcance antes de editar.
- **Extensão:** grupos de propriedades registrados por capacidade, comandos identificados e pós-processamento de importação. O schema precisa definir tipo, unidade, limites, referência permitida, impacto da mudança e disponibilidade.

Proposta de precedência: padrão da engine → padrão do projeto → regra de pasta → preset escolhido → ajuste do recurso → ajuste do subrecurso. Alterações da instância pertencem a outra camada; não devem mudar o perfil de importação acidentalmente. O painel mostra “herdado” ou “alterado” e oferece retorno ao valor herdado.

### Linguagem visual e redução de texto

Usar nomes curtos como **Importar, Atualizar, Origem, Materiais, Texturas, Colisão, Detalhes**. Comandos como localizar, extrair, duplicar, comparar e desvincular podem usar ícone com descrição ao toque prolongado/foco. Ação principal e operações ambíguas continuam nomeadas. Retirar texto excessivo não significa deixar o usuário adivinhar a função de cada ícone.

Informações de codec, bytes, extensões e formatos ficam na seção técnica. Avisos de perda visual permanecem acessíveis e destacados, mas não ocupam a tela inteira. Ícones devem ser PNG/RGBA com alpha verdadeiro e bordas limpas; nenhuma placa preta embutida e nenhum SVG novo. Usar os tokens já aprovados, sem pintar todas as linhas de verde.

### Reimportação como comparação visual

Árvore com nós adicionados, removidos, alterados e ambíguos; comparação fonte anterior/nova/valor local; destaque da peça na prévia. Por conflito, oferecer manter alteração local, aceitar fonte ou associar explicitamente o nó correspondente, quando aplicável.

A escolha “associar pela ordem” deve virar uma decisão informada por prévia e correspondências, não um atalho opaco que pode trocar peças. O commit precisa fechar fonte empacotada, registro, mapa de nós, perfil, dependências e metadados de uma vez. O estado anterior permanece recuperável quando houver falha.

## 6. Contratos necessários para texturas e materiais

### Separar imagem, textura, uso no material e residência

Proposta de modelo autoral — nomes abaixo são contratos propostos, não classes já existentes:

```text
SourceAsset → conteúdo original e dependências
ImportProfile → como produzir recursos derivados
ImageAsset → imagem identificada, origem e conteúdo
TextureAsset → interpretação, mips, formato e variantes de plataforma
MaterialTextureBinding → canal, textura, sampler, UV e transformação
MaterialAsset → fatores e bindings persistidos
MeshAsset → superfícies e atribuições de material
MeshRenderer → referências e alterações da instância
GpuResidency → recursos transitórios do aparelho
```

Uma imagem pode ser usada como cor e como dado. Esses usos não autorizam misturar conversões sRGB. Um sampler diferente não precisa obrigatoriamente duplicar os pixels da imagem, embora gere outro binding. Uma textura embutida no GLB precisa de identidade estável para continuar referenciada após reimportação. Os conceitos de imagem, textura e sampler são separados no glTF.[^23]

O `MaterialAsset` deve evoluir com migração explícita: arquivos antigos continuam com os fatores atuais; novos arquivos armazenam referências por GUID e origem do binding. Extrair um material importado deve preservar aparência e referências, sem exigir que o usuário recrie os quatro mapas manualmente.

### O que cada canal deve permitir

Cor base e emissão usam interpretação de cor; normal e mapas de propriedades usam dados. Os campos oferecem miniatura, localizar, trocar, remover vínculo, canal, conjunto UV, repetição, deslocamento e rotação. Remover um vínculo não apaga a imagem do projeto. Rugosidade e metalicidade devem mostrar de onde vem cada canal da imagem combinada.

Oclusão precisa entrar no modelo material e no shader com intensidade própria. Culling de face, corte alfa e transparência precisam de estados corretos no pipeline, no caminho de sombras e na prévia. A presença do fator no parser não comprova a reprodução visual.

**Transformação UV por binding é uma prioridade.** O bake atual resolveu arquivos compatíveis, mas não representa integralmente transformações diferentes em texturas que compartilham UV. A extensão Khronos define essa transformação no uso da textura.[^24] Preservar UV de origem e aplicar dados de material por binding evita destruir a base necessária para trocar o material depois.

O documento atual registra pressão sobre o bloco de push constants. Portanto, planejar armazenamento material adequado em buffers/descritores e contratos shader, em vez de acrescentar campos sem examinar os limites existentes. A atualização de textura ou material deve invalidar somente os recursos dependentes.

### Gerenciador visual de texturas

Grid/lista com miniaturas, nome, resolução e indicadores de referência; busca por nome/tipo/pasta e filtros de ausentes, não usadas, alteradas, sem alpha e acima do orçamento. Ao selecionar, Propriedades mostra imagem real com zoom, canais RGBA, seleção de mip e fundo quadriculado configurável.

Mostrar separadamente tamanho da fonte, tamanho residente, formato efetivo e estimativa de memória. Um rótulo “ASTC” só aparece quando essa variante é a utilizada. A redução de resolução precisa ser visível, reversível e associada a um perfil; não pode parecer corrupção da importação.

Operações necessárias: extrair embutida, substituir fonte, relocalizar dependência, duplicar variante, edição múltipla de perfil, localizar usuários, reimportar seleção e restaurar origem. Arquivos sem usuários devem poder ser filtrados; exclusão exige tratar referências, inclusive distinguir referências dinâmicas por código que não possam ser resolvidas estaticamente.

## 7. Matriz funcional rastreável

**Estados:** E = caminho existente a preservar; P = parcial ou sem fluxo autoral completo; N = lacuna identificada no caminho inspecionado; A = subsistema relacionado ainda precisa de auditoria específica. “A” não significa ausência na engine. Todas as colunas de entrega descrevem propostas.

**Pacotes:** R1 abertura; R2 cache/residência; R3 dock/perfis; R4 texturas/materiais; R5 malha; R6 reimportação/gestão; R7 geometria e consumidores; R8 animação/deformação; R9 expansão gráfica; R10 fechamento. A numeração é detalhada na seção 8.

### 7.1 Importação — 24 itens

As referências de importação usam seus próprios formatos e schemas. As opções abaixo são adaptadas à Astra; presets e automação têm apoio nas referências específicas.[^1][^2][^7][^8][^9][^18][^20][^26][^27]

| ID | Função / referência | Caso e controle proposto | Astra e dependência | Pacote |
|---|---|---|---|---|
| I01 | Escala de importação — Unity `Scale Factor` | Corrigir tamanho de todo o recurso com unidade e prévia de dimensões. | P: TRS importado; falta perfil autoral versionado. | R3 |
| I02 | Conversão de eixos — Unity `Bake Axis Conversion` | Opção explícita para adaptadores; glTF correto preservado por padrão. | P: convenção atual existe; converter exige tratar poses, normais e winding juntos. | R3/R7 |
| I03 | Preservar hierarquia — Unity `Preserve Hierarchy` | Manter pivôs e nós vazios úteis; mostrar estrutura resultante. | E/P: árvore existe; política por recurso falta. | R3 |
| I04 | Inclusão por nó — Godot importação avançada | Excluir elemento da importação sem apagar a fonte; prévia por seleção. | N: precisa filtro de IR e política para descendentes/referências. | R3/R6 |
| I05 | Tipo e nome da raiz — Godot importador de cena | Definir nome autoral e raiz de instância com múltiplas raízes previsíveis. | P: identidade existe; opção e reconciliação precisam integração. | R3/R6 |
| I06 | Importar câmeras/luzes — Unity/Godot | Habilitar por categoria e mapear parâmetros representáveis. | N no importador GLB atual; depende de adaptadores para componentes reais. | R7 |
| I07 | Normais — Unity `Normals` | Preservar, recalcular ou omitir conforme uso; visualizar resultado. | P: atributo importado; faltam política e ferramentas autorais. | R5/R7 |
| I08 | Tangentes — Unity `Tangents` | Preservar ou gerar com convenção documentada; corrigir normal map. | P: geração atual existe; equivalência Mikk não demonstrada. | R4/R7 |
| I09 | Soldagem — Unity `Weld Vertices` | Unir somente vértices equivalentes, preservando costuras e materiais. | N nesse fluxo; requer operação geométrica com erro/limites. | R7 |
| I10 | Organização de buffers — Unity `Optimize Mesh` | Reduzir custo de índices/vértices sem alterar IDs autorais. | P/A: não deduzir otimizador completo dos codecs vendorizados. | R7 |
| I11 | Precisão de armazenamento — Unity `Mesh Compression` | Escolher compactação derivada e mostrar erro estimado. | N como opção autoral; decoder de origem é outro problema. | R7 |
| I12 | Dados CPU editáveis — Unity `Read/Write` | Manter dados para scripts, colisão ou edição com custo explícito. | P: cópias existem; falta política por consumidor e descarte. | R2/R7 |
| I13 | LOD automático — Godot importação 3D | Gerar versões por erro visual, com prévia e configuração. | N no fluxo GLB; runtime LOD existente precisa ser reutilizado. | R7 |
| I14 | Geometria de sombra — Godot importador de cena | Derivar representação econômica para sombras, sem mudar a malha visual. | A: auditar consumidores e compatibilidade de deformação. | R7/R9 |
| I15 | UV para iluminação — Godot importador de cena | Gerar canal separado com resolução/densidade indicadas. | A: depende de unwrap e consumidor de iluminação baked. | R9 |
| I16 | Colisão durante importação — Godot opções por nó | Nenhuma, primitiva, convexa ou malha, com prévia de custo. | P: física/fit existem; cobertura de todos os slots precisa correção. | R7 |
| I17 | Extração/remapeamento de material — Unity materiais | Manter origem, extrair ou associar material existente. | P: materiais de projeto existem; bindings completos faltam. | R4/R6 |
| I18 | Política por categoria — PlayCanvas | Preservar ou atualizar materiais, texturas e hierarquia separadamente. | P: reconciliação existe; perfis e interface granulares faltam. | R3/R6 |
| I19 | Perfil reutilizável — Unity presets | Salvar, duplicar e aplicar configuração em vários recursos. | N no fluxo auditado; exige schema e precedência. | R3 |
| I20 | Pós-processamento — `AssetPostprocessor` / `EditorScenePostImport` | Regras do projeto para nomes, extração e metadados. | A: integração com IDE/API e execução determinística no estágio de importação. | R6/R9 |
| I21 | Importar clips — Godot importador de cena | Selecionar animações, intervalos e taxa de amostragem. | N no GLB atual; requer AnimationAsset e tracks por ID. | R8 |
| I22 | Importar esqueleto — Godot importador de cena | Preservar ossos, bind poses e vínculos entre peças. | N no GLB atual; requer skinning e identidade de ossos. | R8 |
| I23 | Inspeção de saídas — Unreal Interchange | Ver nós, malhas, materiais e texturas antes de publicar. | P: contagem textual existe; falta árvore de saídas e preview real. | R3 |
| I24 | Prévia de conflitos — Unreal Interchange; reconciliação Astra | Inspecionar peça e propriedade afetadas antes da atualização. | P: conflito calculado; faltam decisões individuais e UI comparativa. | R6 |

### 7.2 Texturas e materiais — 26 itens

A Unity oferece configuração de interpretação e importação; a Godot separa processamento de imagem de usos de material; a Unreal fornece uma prévia técnica de textura mais rica. As propostas de persistência e comandos abaixo são da Astra.[^3][^4][^10][^17][^21][^22]

| ID | Função / referência | Caso e controle proposto | Astra e dependência | Pacote |
|---|---|---|---|---|
| T01 | Identidade de textura — inspetores de assets | Selecionar uma textura como recurso próprio e localizar seus usuários. | N como fluxo autoral completo; registro e bindings estáveis. | R4 |
| T02 | Extração de embutidas — Unity `Extract Textures` | Tornar uma imagem do GLB editável fora dele preservando vínculos. | N no painel; transação e identidade de subasset. | R4/R6 |
| T03 | Cor/dados — Unity `sRGB` | Escolher interpretação com padrão baseado no uso, sem converter duas vezes. | E/P: importação distingue; falta controle persistente. | R4 |
| T04 | Origem do alpha — Unity `Alpha Source` | Usar alpha original, desabilitar ou derivar conforme perfil. | P: alpha importado; transformação autoral e perfil faltam. | R4 |
| T05 | Bordas transparentes — Unity textura padrão | Evitar halos em imagens recortadas por processamento explícito. | N como ferramenta; preservar fonte e gerar variante. | R4 |
| T06 | Normais — Godot processamento de textura | Escolher convenção, inversão Y e intensidade com esfera/plano de prévia. | P: mapa e intensidade existem; falta gestão de convenção. | R4 |
| T07 | Resolução por recurso — Unity/Unreal | Tamanho máximo, proporção e tamanho final antes de confirmar. | P: limite atual automático; falta perfil e decisão visível. | R2/R4 |
| T08 | Formato de plataforma — importadores de textura | Mostrar ASTC/RGBA efetivo e alternativa suportada. | P: ASTC KTX2 existe; negociar outras variantes e persistir escolha. | R2/R4 |
| T09 | Compressão e qualidade — Godot | Selecionar estratégia e comparar custo visual/memória. | P: decodificação existe; encoders e comparador precisam integração. | R4/R9 |
| T10 | Mipmaps — Unity `Generate Mipmap` | Gerar/preservar/desligar quando apropriado; estimar memória. | E/P: caminho existente; expor sem opções fictícias. | R4 |
| T11 | Inspeção de mip — Unreal editor de textura | Ver cada nível, resolução e artefatos. | N no painel; preview seleciona subrecurso real. | R4 |
| T12 | Filtro e repetição — glTF samplers | Seletores compactos por binding, com amostra repetida. | E/P: sampler importado; edição autoral falta. | R4 |
| T13 | Anisotropia — gestão de amostragem | Reduzir borrado oblíquo com limite global e override por recurso. | A: auditar suporte efetivo Vulkan e política existente. | R4/R9 |
| T14 | Canais RGBA — Unreal editor de textura | Inspecionar um canal sem exportar a imagem. | N no painel; shader de prévia. | R4 |
| T15 | Zoom e fundo — Unreal editor de textura | Ajustar imagem à área, 1:1, quadriculado e fundo sólido. | N nesse fluxo; preview com clipping e gestos próprios. | R4 |
| T16 | Memória e resolução efetiva — Unreal | Distinguir fonte, derivado e residência atual. | P: contadores de importação; falta contabilização global consultável. | R2/R4 |
| T17 | Streaming por orçamento — Unreal | Manter detalhe conforme uso e prioridade, com estado visível. | A: não comprovado no fluxo de texturas importadas; scheduler de residência. | R2/R9 |
| T18 | Textura no material — PlayCanvas/Khronos | Campo com miniatura, picker, limpar vínculo e localizar recurso. | N no MaterialAsset persistido; versão e migração. | R4 |
| T19 | Transformação por uso — `KHR_texture_transform` | Escala, deslocamento, rotação e UV independentes para cada mapa. | P: bake limitado; novo binding e shader preservando UV original. | R4 |
| T20 | Empacotamento de canais — materiais glTF | Mostrar e remapear metal/rugosidade/oclusão em canais separados. | P: MR importado; edição e oclusão faltam. | R4 |
| T21 | Oclusão de material — glTF | Mapa e intensidade, isoláveis na prévia. | N: contada como não aplicada. Exige canal/material/shader. | R4 |
| T22 | Corte alfa e transparência — glTF | Modo, limiar e visualização; tratar sombras e ordenação. | P: flags existem; gestão completa e equivalência visual pendem. | R4 |
| T23 | Dupla face — glTF | Controle efetivo de face e normais, sem custo invisível. | P: pipeline atual usa culling desativado; semântica incompleta. | R4 |
| T24 | Verniz e aparência avançada — `KHR_materials_clearcoat` | Camada física de revestimento com textura e rugosidade próprias. | N no perfil atual; precisa de material e iluminação, não apenas slider. | R9 |
| T25 | Cubemap, array e HDR — Unreal editor de textura | Prévia por face/camada, exposição e formato adequados. | N no KTX2 atual; auditar outros loaders antes de expandir. | R9 |
| T26 | Substituição em lote — gestão de assets | Trocar fonte ou perfil em seleção compatível com revisão de impacto. | N como fluxo completo; dependências e transação. | R6 |

### 7.3 Malha e componente de renderização — 32 itens

Na Unity, `MeshFilter`, `MeshRenderer` e `SkinnedMeshRenderer` têm responsabilidades distintas. Na Godot, `MeshInstance3D` referencia a malha e herda controles de `GeometryInstance3D`. A Astra pode manter um componente universal com seções por capacidade, desde que recurso compartilhado e instância permaneçam separados.[^5][^6][^11][^12][^28]

| ID | Função / referência | Caso e controle proposto | Astra e dependência | Pacote |
|---|---|---|---|---|
| M01 | Referência de geometria — MeshFilter | Nome real, miniatura, origem e seleção por recurso. | P: referência existe; substituir apresentação por índices. | R5 |
| M02 | Reutilização de malha — Godot MeshInstance3D | Vários objetos usam a mesma geometria sem cópia implícita. | E/P: instâncias importadas; tornar escopo evidente. | R5 |
| M03 | Variante editável — recurso vs instância | Criar derivado de geometria antes de edição destrutiva. | N nesse fluxo; IDs e relação com fonte. | R5/R7 |
| M04 | Prévia de malha — Unreal | Girar, enquadrar, isolar e escolher fundo/iluminação de inspeção. | N no inspetor atual; preview independente. | R5 |
| M05 | Estatísticas — Unreal | Vértices, índices, triângulos, superfícies e memória do recurso selecionado. | P: contadores de importação; falta consulta por malha. | R5 |
| M06 | Submalhas — Unity/Godot | Lista nomeada com seleção e destaque da superfície. | E/P: slots existem; falta visualização e autoria adequadas. | R5 |
| M07 | Materiais por superfície — MeshRenderer | Arrastar/selecionar material com prévia e escopo. | E/P: associação existe; UI e texturas completas faltam. | R4/R5 |
| M08 | Override de material — Godot | Distinguir material da fonte, compartilhado e local. | E/P: escopos existentes; origem de cada campo deve ficar visível. | R4/R5 |
| M09 | Camada de material — Godot `material_overlay` | Aparência adicional sem substituir a base. | A: exige passe e política de custo, além de uma opção UI. | R9 |
| M10 | Wireframe — Unreal preview | Inspecionar topologia sobre superfície ou isoladamente. | A no recurso; reutilizar desenho editorial quando aplicável. | R5 |
| M11 | Normais/tangentes — Unreal preview | Mostrar direções por seleção e escala de gizmo. | N como ferramenta de recurso; dados e overlay. | R5 |
| M12 | UV por canal — Unreal preview | Ver UV0/UV1, sobreposição e cobertura com textura de referência. | P: UV existe nos dados; falta visualizador. | R5 |
| M13 | Cor de vértice — malha glTF | Inspecionar atributo e influência sobre a aparência. | P: COLOR_0 importado; controle de prévia falta. | R5 |
| M14 | Bounds — Godot `custom_aabb` | Exibir volume, recalcular e permitir override justificado. | P: bounds existem; edição e consequências precisam contrato. | R5/R7 |
| M15 | Margem de culling — Godot `extra_cull_margin` | Evitar corte de objetos deformados, com custo indicado. | A: integrar culling real sem inflar bounds de todos os objetos. | R7/R8 |
| M16 | Modos de sombra — Unity `Cast Shadows` | Desligado, normal, dupla face e somente sombra. | P: configuração de sombra existe; modos completos precisam auditoria. | R7/R9 |
| M17 | Recepção de sombra — Unity `Receive Shadows` | Escolha por capacidade material/pipeline, com resultado real. | A: não presumir a mesma localização entre pipelines. | R9 |
| M18 | Participação em GI — Unity/Godot | Estático/dinâmico/excluído conforme backend disponível. | A: integração de renderer e recurso, não checkbox isolado. | R9 |
| M19 | Lightmap e densidade — Unity/Godot | Canal UV, escala e texels por área com prévia. | A: unwrap, bake, dados por instância e shader. | R9 |
| M20 | Probes de luz/reflexo — Unity | Atribuição automática ou explícita e visualização do alcance. | A: auditar sistemas presentes antes de criar novos. | R9 |
| M21 | Vetores de movimento — renderer Unity | Participação correta em efeitos temporais por geometria/pose. | A: dados anterior/atual e suporte do pipeline. | R8/R9 |
| M22 | Camadas de renderização — renderer Unity | Filtrar participação em luz/câmera conforme capacidade. | A: máscaras e persistência comuns ao runtime. | R7/R9 |
| M23 | Oclusão por instância — Godot | Exceção explícita para casos especiais, com visualização. | P/A: culling existe; auditar controle autoral. | R7 |
| M24 | LOD atribuído — Unity `LODGroup` | Associar malhas, limiares e prévia do nível atual. | P: seleção runtime existe; autoria universal pendente. | R7 |
| M25 | Transição de LOD — Unity/Godot | Limiares, histerese e fade quando suportados. | P/A: auditar ligação ao importado e ao shader atual. | R7 |
| M26 | Faixa de visibilidade — Godot | Início/fim, margens e agrupamento HLOD quando aplicável. | A: preservar bounds e dependências entre objetos. | R7/R9 |
| M27 | Colisão visível — Unreal | Mostrar geometria física e distingui-la da visual. | P: colisores existem; ligar inspeção e todos os slots. | R5/R7 |
| M28 | Pontos de anexação — Unreal Socket Manager | Criar ponto nomeado para acessórios, preservando pivôs. | A: usar entidade/transform/ID universal, sem classe “carro”. | R7 |
| M29 | Esqueleto associado — Godot/Unity | Selecionar raiz, visualizar ossos e relação com a malha. | N no fluxo GLB atual; skin/bind poses. | R8 |
| M30 | Pesos de deformação — Unity SkinnedMeshRenderer | Inspecionar influências por vértice e orçamento. | N no GLB atual; atributos e pipeline de skinning. | R8 |
| M31 | Morph targets — Unity/Godot | Lista de formas e pesos com prévia, persistência e animação. | N no fluxo estático; schema e avaliação por pose. | R8 |
| M32 | Bake de pose — Godot MeshInstance3D | Gerar malha derivada de pose/morph preservando o original. | N nesse fluxo; avaliação e publicação derivada explícita. | R8 |

### 7.4 Sistemas dependentes de malhas — 18 itens

Estas capacidades precisam permanecer no catálogo, mas pertencem a componentes, recursos ou ferramentas próprias. Colocar todas dentro de “Malha” criaria exatamente a poluição que o usuário quer evitar. O inspetor oferece atalhos contextuais para elas.

| ID | Função / referência | Caso e controle proposto | Astra e dependência | Pacote |
|---|---|---|---|---|
| D01 | Ajuste de primitiva física — Unreal colisões | Propor caixa, esfera ou cápsula e permitir corrigir. | P: fit existente usa slot principal em caminhos documentados; unir bounds relevantes. | R7 |
| D02 | Casco convexo — Godot/Unity | Colisor de volume simplificado para objeto dinâmico. | A: conferir backend físico e limites reais. | R7 |
| D03 | Decomposição convexa — Godot/Unreal | Número de cascos, erro e prévia para formas complexas. | A: requer gerador e dados persistidos próprios. | R7 |
| D04 | Colisão triangular — Godot/Unity | Usar malha estática ou derivado físico, conforme backend. | P/A: colisão estática existe; contrato autoral por recurso pendente. | R7 |
| D05 | Preparação de colisão — Unity `Cooking Options` | Limpar degenerados e tratar soldagem com diagnóstico. | A: não transplantar limites PhysX para Astra. | R7 |
| D06 | Navmesh — Godot NavigationMesh | Gerar área navegável por máscara, inclinação e dimensões do agente. | A: auditar runtime; geometria visual é entrada, não sistema completo. | R7/R9 |
| D07 | Occluder derivado — Godot importação avançada | Selecionar geradores de oclusão e visualizar cobertura. | A: integrar às estruturas de visibilidade existentes. | R7/R9 |
| D08 | Instanciamento em massa — Godot MultiMesh | Distribuir geometria com transformações/cores e limites de material. | P/A: renderer instanciado existe; falta demonstrar autoria universal. | R7 |
| D09 | Emissão/representação de partículas — Godot GPUParticles3D | Usar geometria como desenho de partículas e expor seus passes. | A: auditar VFX existente e contrato de malha. | R9 |
| D10 | Geometria por arrays — Godot ArrayMesh | Criar malha por código com atributos e superfícies. | A: API pública autoral e publicação incremental. | R7/R9 |
| D11 | Edição topológica — Godot MeshDataTool | Operar vértices/arestas/faces em recurso derivado. | A: conectividade, histórico e atualização de consumidores. | R7/R9 |
| D12 | Construção por superfície — Godot SurfaceTool | Montar geometria, normais e índices por API consistente. | A: API ligada à mesma validação do importador. | R7/R9 |
| D13 | Geometria imediata — Godot ImmediateMesh | Desenhos dinâmicos simples por código com vida útil definida. | A: reaproveitar infraestrutura editorial sem confundir com asset salvo. | R9 |
| D14 | Operações CSG — Godot | União, diferença e interseção para autoria e bake posterior. | A: novo pipeline geométrico se não houver backend existente. | R9 |
| D15 | Tecido/deformação física — Unity SkinnedMeshRenderer | Malha deformável dirigida por simulação, pesos e restrições. | A: solver e integração são projetos próprios. | R9 |
| D16 | Simplificação — Godot LOD | Gerar geometria reduzida preservando bordas e atributos. | N no importador auditado; decoder meshopt não atende essa função. | R7 |
| D17 | UV derivada — Unreal ferramentas UV | Criar/reorganizar canal em variante editável. | A: algoritmo de unwrap, parametrização e editor. | R9 |
| D18 | Exportação de derivados — plano Astra / ferramentas de recurso | Salvar malha/material autoral sem depender do cache ou GLB original. | A: contrato de distribuição e dependências M15. | R10 |

Referências específicas desta família: colisões Unity[^15], mesh Godot[^11], editor Unreal[^16], navegação[^29], geometria procedural[^14], partículas[^30], CSG[^31], malha deformável[^28] e ferramentas UV[^32].

**Sobre “detectar automaticamente o colisor”:** as referências oferecem geração e ajuste, mas a forma visual não determina sozinha a intenção física. Um carro pode usar poucos volumes, uma carroceria convexa ou colisão detalhada estática. Proposta para Astra: modo **Automático** com regra visível, prévia e opção manual. Ele escolhe uma aproximação conforme geometria e tipo de corpo; não deve alegar reconhecer a semântica do objeto ou impor a malha visual como colisor ideal.

### 7.5 Gerenciamento e experiência — 12 itens

| ID | Função | Entrega proposta | Astra e dependência | Pacote |
|---|---|---|---|---|
| W01 | Importador no dock | Substituir modal pelo contexto de recurso em Propriedades. | N no layout atual; modelo estruturado de trabalho. | R3 |
| W02 | Navegação de subassets | GLB → nó/malha → material → textura com retorno preservado. | P: dados distribuídos; seleção independente e breadcrumbs. | R3/R5 |
| W03 | Layout persistente | Prévia redimensionável, favoritos, densidade e grupos abertos. | P/A: usar padrão IDE e preferências de workspace. | R3 |
| W04 | Histórico de recurso | Desfazer/refazer alterações compartilhadas com alcance visível. | N para edição compartilhada conforme documentação atual. | R4/R6 |
| W05 | Dependências e usuários | Lista navegável de quem usa o asset e por qual propriedade. | P: registro existe; grafo autoral completo falta. | R6 |
| W06 | Relocalizar fonte | Resolver dependência externa ou fonte ausente preservando GUID. | P: diagnóstico existe; ação de reparação precisa fluxo. | R6 |
| W07 | Excluir/substituir | Prévia de impacto e referências preservadas quando não resolvidas. | P: operações existentes; consolidar transação e grafo. | R6 |
| W08 | Biblioteca visual | Miniaturas, filtros, busca, seleção múltipla e presets por pasta. | P: browser existe; expandir experiência com dados de recurso. | R3/R6 |
| W09 | Console contextual | Evento estruturado com recurso, etapa, severidade e ação de abrir item. | P: console existe; trocar relatório linear por eventos navegáveis. | R3/R6 |
| W10 | Cancelamento e retomada | Cancelar trabalho sem corromper recurso publicado; restaurar dock após Activity. | P: cancelamento entre etapas existe; fechar contrato de sessão. | R1/R3 |
| W11 | Customização por capacidade | Mesmos campos e comandos para UI, código e automação compatível. | P/A: integrar registro do plano; não criar caminho paralelo exclusivo de GLB. | R3/R9 |
| W12 | Integridade transacional | Fonte, dependências, perfil, mapa e registro com recuperação conjunta. | P: journal existe; `.deps` e falhas de identidade precisam fechamento. | R6 |

## 8. Roadmap em pacotes completos

### R0 — Congelar a base e contratos de execução

**Objetivo:** impedir retrabalho sobre capacidades recém-entregues. Registrar o SHA inicial, alterações locais, documentação válida e responsáveis pelos arquivos compartilhados. Reconciliar a antiga delegação M08/M09 com E2–E4.

**Saídas:** mapa de estados reais, contrato de recursos persistidos versus transitórios e checklist dos 112 IDs. Registrar separadamente “implementado”, “integrado ao editor”, “validado no host” e “validado no aparelho”. Nenhum desses estados pode ser deduzido automaticamente dos demais.

**Concluído quando:** o próximo agente identifica exatamente quais caminhos estender, sem reiniciar codecs, submalhas ou identidade. Abrange M00/M01 e acompanha todos os pacotes seguintes.

### R1 — Abertura sem lacuna preta

**Prioridade imediata.** Reestruturar `AstraShellActivity`, `ShellView` e o fluxo de abertura em `android_main.cpp` em torno do mesmo trabalho identificado. Implantar cobertura visual até o primeiro quadro, etapas reais e recuperação de sessão.

Separar carregamento do documento, preparação CPU e publicação GPU. Manter a thread que apresenta quadros disponível; permitir cancelar/voltar. Na primeira versão estrutural, publicar o conjunto inicial uma vez. Conservar a sincronização de segurança enquanto a infraestrutura incremental não estiver pronta.

**Depende:** R0. **Critério de saída futura:** continuidade visual, estado coerente após interrupção e ausência de N reconstruções acumuladas durante a abertura inicial. As métricas devem informar a etapa lenta, sem porcentagem inventada. Relacionado a M03/M14.

### R2 — Cache e orçamento global de recursos

Introduzir armazenamento de derivados regeneráveis e índices por chave de conteúdo/perfil. Separar handles autorais de posições de buffers e descritores GPU. Implantar carregamento sob demanda dos recursos necessários à cena e prévias.

Começar com orçamento agregado de geometria, texturas e staging; contar dados compartilhados uma vez e registrar picos de transição. Gerar/selecionar resolução antes de materializar cadeias desnecessárias. Evoluir publicação incremental com fences/épocas e descarte seguro; manter cache CPU/disco independente da perda de surface.

**Depende:** R1 e contrato de perfil de R3, que pode ser definido antes da UI. **Critério:** reabrir recurso inalterado aproveita derivados; mudar apenas textura não reconstruirá todas as malhas; limite agregado tem efeito real. Relacionado a M08/M14/M15.

### R3 — Dock de recursos e perfis de importação

Este é um pacote completo de produto: navegação, controles inline, preview, presets, estado e ações. Substituir o bloco modal de `buildProjectDialogs` e separar os dados do painel da string de resumo.

Criar modelo de trabalho com entradas, saídas, opções, avisos, conflitos, progresso e rascunho. Registrar propriedades usando a arquitetura de capacidades já prevista. Implementar visão geral, estrutura, geometria, materiais e texturas conforme suporte real; animação aparece com estado de disponibilidade, sem ação inoperante disfarçada.

**Depende:** R0; usa ciclo de trabalho de R1. **Critério:** importar e reimportar pelo dock, comparar saídas, ajustar perfil, salvar preset e cancelar sem alteração indevida; layout conserva os padrões do IDE. Relacionado a M08/M10 e W01–W03/W09–W11.

### R4 — Texturas e materiais editáveis de ponta a ponta

Evoluir material persistido e seus consumidores: registro de imagens/texturas, bindings, samplers, canal/UV, transformações, alpha, face e oclusão. Migrar materiais v1; preservar a origem dos importados e os overrides.

Entregar gerenciador, seletores com miniaturas, visualizador de canais/mips e troca de textura/material em recurso e instância. Atualização incremental de materiais precisa continuar funcionando. Corrigir a limitação de UV por material e a diferença entre flag dupla face e culling real.

**Depende:** contratos R2/R3. **Critério:** importar modelo, extrair material, trocar textura de uma instância, editar um material compartilhado, salvar, reabrir e reimportar preservando o alcance de cada alteração. Relacionado a M09/M13.

### R5 — Inspetor de malha com ferramentas de inspeção

Reorganizar o componente em geometria, superfícies/materiais e acesso às capacidades relacionadas. Trocar o seletor por recursos nomeados. Implantar preview independente, isolamento de superfície, wireframe, normais/tangentes, UV, bounds e estatísticas reais.

Não é um modelador completo nesta etapa. O resultado é uma ferramenta para entender e controlar a malha importada; operações de gerar ou alterar geometria usam os contratos de derivação de R7.

**Depende:** R3/R4. **Critério:** escolher uma peça pelo nome, identificar seu material e textura, inspecionar UV e distinguir recurso de instância sem recorrer a índices GPU. Relacionado a M08/M10.

### R6 — Reimportação, dependências e histórico

Transformar conflitos em árvore visual e decisões por item. Corrigir recuperação de mapa de nós ilegível; não regenerar IDs silenciosamente. Fechar a transação incluindo `.deps`, perfil e metadados. Usar o journal existente como base, com recuperação após falha.

Entregar localizar usuários, substituir/relocalizar, renomear/mover sem perder identidade, extração consistente e histórico para edição compartilhada. A resolução de glTF externo precisa preservar caminhos relativos e distinguir nomes duplicados; o empacotamento interno não pode apagar a proveniência necessária para reimportar.

**Depende:** R3/R4, integração R2. **Critério:** duas instâncias com alterações distintas atravessam reimportação; fontes removidas/renomeadas são reparáveis; falha de dependência não publica metade do recurso. Relacionado a M08/M15.

### R7 — Geometria derivada, LOD, colisão e API universal

Auditar primeiro `lod_selection`, colisores, estruturas de picking/culling e módulos existentes. Acrescentar ao importado autoria de LOD, simplificação, ajuste físico, colisão derivada e pontos de anexação. Corrigir consumidores ainda limitados ao slot principal, inclusive fit físico e caminhos relacionados à água.

Implantar operações sobre malha como derivados versionados; não reescrever a fonte. Preservar costuras, atributos e fronteiras de material em simplificação/soldagem. API de código e UI usam o mesmo comando e a mesma validação. Câmeras/luzes da fonte podem ser mapeadas aqui para os componentes existentes.

**Depende:** R2/R5/R6. **Critério:** LOD e colisão mudam no runtime, são inspecionáveis e sobrevivem à reabertura/reimportação; todas as superfícies relevantes participam. Relacionado a M09/M12/M14.

### R8 — Animação, esqueleto e deformação

Acrescentar clips e tracks com alvos por ID, poses de ligação, pesos, skinning, morphs e gerenciamento de bounds. Definir quais transformações pertencem ao documento e quais são amostras temporárias do runtime. Prévia de animação não grava poses na cena por acidente.

A API de instância deve permitir controlar clips e pesos. Reimportação precisa reconhecer alterações de esqueleto e preservar vínculos válidos. Malhas com deformação requerem integração de picking, sombras e atualização de limites; importar os arrays sem esses consumidores não conclui o pacote.

**Depende:** R4/R6/R7. **Critério:** modelo animado funciona no preview e em Play; Stop restaura estado autoral; edição/reimportação não troca alvos. Relacionado a M09/M12.

### R9 — Aparência avançada e consumidores especializados

Subdividir por capacidade verificável: revestimento e outros materiais Khronos, iluminação baked/probes, tipos avançados de textura, ferramentas UV, navegação, VFX, CSG e deformação física. Cada subpacote precisa da auditoria “A” correspondente na matriz antes de decidir reutilização ou novo backend.

Materiais avançados exigem parser, asset, inspetor e modelo de iluminação coerente. Recurso exclusivo de uma engine ou limitado por GPU permanece documentado com dependência e alternativa; não recebe botão “compatível” sem implementação. Nanite, por exemplo, não é uma opção genérica que se obtém acrescentando um campo a uma malha.

**Depende:** R2/R4/R7/R8 conforme recurso. **Critério:** contrato e demonstração específicos por ID; registro honesto de limitações. Relacionado a M12/M13/M14.

### R10 — Fechamento autoral, distribuição e evidências

Assegurar que projeto copiado/reaberto possa reconstruir derivados e resolver referências, que um build distribuído contenha os recursos necessários e que dados transitórios de editor não sejam exigidos no runtime.

Consolidar migrações e diagnósticos; só então executar a rodada de validação autorizada para cada conjunto entregue. A documentação deve trazer arquivos, SHA, capacidades e limitações concretas. Não marcar todo M09 como concluído por um modelo estático com texturas.

**Depende:** pacote funcional que se pretende fechar, não da conclusão de todas as extensões futuras. **Critério:** fluxo salvo, reaberto, reimportado e distribuído com evidência e escopo explícito. Relacionado a M15.

### Ordem prática para a próxima delegação

**Primeiro bloco amplo: R1 + R2 + R3.** Ele ataca a tela preta e entrega o importador no lugar correto, apoiado em perfis e trabalho assíncrono. O contrato de recursos de R4 deve ser definido junto, para evitar construir controles que depois precisem ser descartados.

**Segundo bloco: R4 + R5 + R6.** Fecha a autoria de texturas/materiais, o inspetor de malha e a atualização segura de fontes. O usuário passa a conseguir modificar o que importou, em vez de apenas aceitar um resultado.

**Terceiro bloco: R7 + R8.** Acrescenta geração geométrica, consumidores e animação com base estável. R9 permanece dividido por capacidades, sem transformar uma expansão extensa em uma declaração falsa de “paridade total”.

Para trabalho entre agentes, fixar primeiro schemas e interfaces. Evitar dois agentes alterando ao mesmo tempo os grandes pontos centrais `editor_session.cpp`, `editor_screen.cpp` e `android_main.cpp`; dividir por contratos/módulos e integrar mudanças nesses pontos com responsável definido.

## 9. Cenários de aceitação para uma rodada futura autorizada

Os cenários abaixo especificam resultados de produto. **Não foram executados nesta análise.** Devem ser aplicados aos blocos completos, evitando um ciclo de testes triviais a cada controle.

| Cenário | Resultado esperado | Regressão que deve detectar |
|---|---|---|
| Abrir projeto pesado a frio e depois com cache | Loading contínuo; editor responsivo; reutilização de derivados na segunda abertura. | Tela preta, reimportação redundante e UI travada. |
| Trocar de projeto durante preparação | Resultado antigo não aparece no projeto novo. | Publicação atrasada em sessão errada. |
| Importar e cancelar pelo dock | Prévia e opções funcionam; cancelar não altera documento/registro. | Importação parcial invisível. |
| Textura embutida usada por dois materiais | Extrair preserva aparência; alterar escopo local afeta só a instância escolhida. | Perda de GUID ou troca global acidental. |
| Texturas com UV distintas no mesmo material | Cada binding mantém sua transformação após troca e reimportação. | Limitação do bake atual mascarada. |
| Reimportar modelo com nós renomeados/removidos | Conflitos navegáveis, referência preservada e decisão explícita. | Porta/peça ligada ao nó errado. |
| Material/textura ausente | Campo identifica falta e permite localizar; projeto permanece editável. | Asset silenciosamente substituído ou descartado. |
| Modelo com vários slots, collider e LOD | Todos os consumidores usam o conjunto correto e persistem. | Colisão/bounds usando só slot 0. |
| Retomar após perda de surface | Recursos GPU são recuperados sem perder trabalho autoral ou reler tudo desnecessariamente. | Confusão entre vida útil de GPU e documento. |
| Layout com dock estreito e IME | Campos editam inline; ações acessíveis; rolagem não dispara visibilidade. | Regressão de input e excesso de botões. |

A aprovação visual deve comparar o resultado real ao padrão do IDE: proporções, densidade, bordas, alpha dos ícones, hierarquia e estados. A presença de recursos reais na prévia é necessária; uma imagem decorativa não demonstra gerenciamento funcional.

## 10. Fontes e limites da pesquisa

Consulta: 14/09/2026. Unity foi fixada em documentação **6.0** quando disponível. Godot usa o canal **stable**, que pode mudar. Páginas atuais da Epic foram servidas como documentação **5.8**; essa identificação é da página consultada. Versões efetivas de bibliotecas e implementações devem ser fixadas quando o trabalho começar. PlayCanvas usa documentação corrente.

O relatório usa documentação oficial para capacidade e funcionamento; vídeos são complementos de fluxo e composição. A existência de uma opção em uma referência não foi contabilizada como implementação Astra. Linhas “A” conservam explicitamente a incerteza da auditoria de subsistemas fora do importador/inspetor.

[^1]: Unity 6.0 — [Model tab](https://docs.unity3d.com/6000.0/Documentation/Manual/FBXImporter-Model.html). Geometria e opções do importador de modelo; atenção ao escopo FBX.
[^2]: Unity 6.0 — [Materials tab](https://docs.unity3d.com/6000.0/Documentation/Manual/FBXImporter-Materials.html). Materiais embutidos, extração e regras de associação.
[^3]: Unity 6.0 — [Texture Import Settings](https://docs.unity3d.com/6000.0/Documentation/Manual/class-TextureImporter.html). Organização e tipos de textura.
[^4]: Unity 6.0 — [Default texture settings](https://docs.unity3d.com/6000.0/Documentation/Manual/texture-type-default.html). Interpretação de cor, alpha, dados CPU e mipmaps.
[^5]: Unity 6.0 — [Mesh Renderer](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshRenderer.html). Materiais e controles de renderização, com diferenças por pipeline.
[^6]: Unity 6.0 — [Mesh Filter](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshFilter.html). Referência de geometria separada do renderer.
[^7]: Unity 6.0 — [Presets](https://docs.unity3d.com/6000.0/Documentation/Manual/Presets.html). Reutilização de propriedades e padrões.
[^8]: Godot stable — [Advanced Import Settings](https://docs.godotengine.org/en/stable/tutorials/assets_pipeline/importing_3d_scenes/advanced_import_settings.html). Configurações por item e interface avançada.
[^9]: Godot stable — [ResourceImporterScene](https://docs.godotengine.org/en/stable/classes/class_resourceimporterscene.html). Opções de importação de cena, malhas, materiais e animação.
[^10]: Godot stable — [ResourceImporterTexture](https://docs.godotengine.org/en/stable/classes/class_resourceimportertexture.html). Processamento e compressão; inclui ressalvas de opções sem efeito.
[^11]: Godot stable — [MeshInstance3D](https://docs.godotengine.org/en/stable/classes/class_meshinstance3d.html). Superfícies, materiais, colisão derivada e bake de pose.
[^12]: Godot stable — [GeometryInstance3D](https://docs.godotengine.org/en/stable/classes/class_geometryinstance3d.html). Bounds, sombras, visibilidade e limites por renderer.
[^13]: Godot stable — [Mesh LOD](https://docs.godotengine.org/en/stable/tutorials/3d/mesh_lod.html). Geração automática e uso de níveis geométricos.
[^14]: Godot stable — [Procedural geometry](https://docs.godotengine.org/en/stable/tutorials/3d/procedural_geometry/index.html). ArrayMesh, SurfaceTool, MeshDataTool, ImmediateMesh e relação com instanciamento.
[^15]: Unity 6.0 — [Mesh Collider](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshCollider.html). Formas, preparação e condições físicas; limites não transferíveis automaticamente para outro backend.
[^16]: Epic — [Static Mesh Editor UI](https://dev.epicgames.com/documentation/en-us/unreal-engine/static-mesh-editor-ui-in-unreal-engine). Prévia, colisão, sockets, referências e organização do editor.
[^17]: Epic — [Texture Asset Editor](https://dev.epicgames.com/documentation/en-us/unreal-engine/texture-asset-editor-in-unreal-engine). Canais, mips, formatos e dimensões de residência.
[^18]: Epic — [Importing Assets Using Interchange](https://dev.epicgames.com/documentation/en-us/unreal-engine/importing-assets-using-interchange-in-unreal-engine). Pipeline, prévia e mudança no comportamento dos conflitos.
[^19]: Epic — [Using Derived Data Cache](https://dev.epicgames.com/documentation/unreal-engine/using-derived-data-cache-in-unreal-engine?lang=en-US). Derivados regeneráveis, cache e separação da fonte.
[^20]: PlayCanvas — [Asset Import Pipeline](https://developer.playcanvas.com/user-manual/editor/assets/import-pipeline/). Regras e políticas de atualização por tipo de recurso.
[^21]: PlayCanvas — [Asset Inspectors](https://developer.playcanvas.com/user-manual/editor/assets/inspectors/). Organização da edição por tipo de asset.
[^22]: PlayCanvas — [Texture Inspector](https://developer.playcanvas.com/user-manual/editor/assets/inspectors/texture/). Edição e seleção de texturas.
[^23]: Khronos — [glTF 2.0 Specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html). Contratos de imagem, sampler, material, geometria e cena.
[^24]: Khronos — [KHR_texture_transform](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_transform/README.md). Transformação no uso da textura.
[^25]: Android — [App startup time](https://developer.android.com/topic/performance/vitals/launch-time). Primeiro desenho e exibição completa; adaptar métricas para abertura de projeto.
[^26]: Unity 6.0 — [AssetPostprocessor](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AssetPostprocessor.html). Extensibilidade do processo de importação.
[^27]: Godot stable — [EditorScenePostImport](https://docs.godotengine.org/en/stable/classes/class_editorscenepostimport.html). Pós-processamento de cena importada.
[^28]: Unity 6.0 — [Skinned Mesh Renderer](https://docs.unity3d.com/6000.0/Documentation/Manual/class-SkinnedMeshRenderer.html). Malhas deformáveis, pesos, bounds e dependências.
[^29]: Godot stable — [NavigationMesh](https://docs.godotengine.org/en/stable/classes/class_navigationmesh.html). Geometria e parâmetros de navegação.
[^30]: Godot stable — [GPUParticles3D](https://docs.godotengine.org/en/stable/classes/class_gpuparticles3d.html). Relação entre partículas e malhas de desenho.
[^31]: Godot stable — [Prototyping levels with CSG](https://docs.godotengine.org/en/stable/tutorials/3d/csg_tools.html). Ferramentas de composição geométrica.
[^32]: Epic — [Using UV Channels With Static Meshes](https://dev.epicgames.com/documentation/unreal-engine/using-uv-channels-with-static-meshes-in-unreal-engine). Inspeção e geração de canais UV.
[^33]: Khronos — [KHR_materials_clearcoat](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_clearcoat/README.md). Camada de revestimento e parâmetros próprios.
[^34]: Unity 6.0 — [LOD Group](https://docs.unity3d.com/6000.0/Documentation/Manual/class-LODGroup.html). Autoria e prévia de transições entre níveis de detalhe.

As referências de LOD sustentam I13, M24–M26 e D16.[^13][^34] A referência de revestimento sustenta T24.[^33]
