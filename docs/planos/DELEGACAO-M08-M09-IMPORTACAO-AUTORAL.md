# Delegação — recursos editáveis, reimportação e aparência

13/09/2026. Escopo proposto para o próximo agente; não é implementação concluída. Base local consultada: branch `codex/gameplay-runtime`, HEAD `8064704`, com alterações ainda não commitadas. O estado de trabalho contém entregas posteriores ao HEAD: partir apenas desse commit perde correções recentes. Antes de criar checkout separado, obter um snapshot que inclua as alterações pertinentes, preservando o trabalho existente.

## Pedido para o agente

Continue a Astra em blocos completos, começando pelas pendências de M07/M08 e avançando para M09.1/M09.2. O resultado deve permitir importar um modelo, editar partes e materiais, criar instâncias independentes, atualizar a fonte sem perder alterações locais e reabrir o projeto com a mesma aparência. Nenhum comportamento pode depender de nomes de carros, objetos ou projetos.

Leia integralmente o plano mestre e as atualizações abaixo antes de implementar. Confronte os documentos com os consumidores atuais: nomes propostos não comprovam que serviços já existam. Consulte as referências primárias do plano para cada contrato e fixe versões/licenças de dependências adotadas. Reutilize SceneGraph, registry, histórico, renderer Vulkan e infraestrutura de importação existentes.

- [Plano mestre completo](PLANO-MESTRE-ASTRA-EDITOR-RUNTIME-ASSETS.md).
- [Estado mais recente do importador e evidências](M08-COMPATIBILIDADE-GLB-REAIS.md).
- [Recuperação e recursos transacionais](M06-M08-RECUPERACAO-RECURSOS.md).
- [Pacote funcional do IDE](M06-PACOTE-FUNCIONAL.md), interpretado junto das atualizações posteriores.
- Planos V2, atlas e demais documentos referenciados pelo plano mestre, conforme a área alterada.

Preserve o layout aprovado, a identidade Astra e os ícones raster existentes. Alterações de UI deste pacote devem servir à autoria de recursos; não redesenhar novamente o IDE nem retomar a reforma de câmera/grade. Não adicionar controles sem consumidor real.

## O que já funciona e deve ser preservado

- GLB estático com árvore de nós, pais e pivôs; geometria compartilhada; sparse accessors, quantização e conversão de strip/fan no perfil documentado.
- Fluxo Arquivos → Importar → revisão → Só recurso ou Importar na cena; seleção e enquadramento do resultado.
- Grupo para arquivos com múltiplas raízes, movimentação do conjunto e Undo da instanciação.
- Publicação com staging/journal, preservação da fonte, registry e recuperação; reabertura das fontes registradas.
- Correção do GUID de saída e da iteração invalidada ao substituir o registry. A cópia proprietária em `reimportProjectSources` é indispensável.
- Ford e Porsche apareceram no viewport do aparelho. O Porsche foi movido como grupo, salvo e reaberto sem duplicação. Isso comprova esses fluxos, não fidelidade completa de materiais ou suporte a qualquer GLB.

O perfil atual ainda omite texturas e aparência avançada. Draco/meshopt obrigatórios não possuem decoder nesse caminho. O agrupamento não é SceneAsset/prefab, e atualização geométrica não é reconciliação estrutural completa.

## Entrega 1 — identidade e reimportação sem perda (M08.2)

Introduzir mapa persistente de nós e subassets, revisão da fonte e vínculo de cada instância. Nomes e índices de arrays não podem ser a única identidade. Aproveitar identificadores autorais quando presentes; usar o mapa anterior e evidências estruturais quando ausentes. Correspondência ambígua deve gerar conflito, sem associar silenciosamente a peça errada.

Implementar comparação entre base anterior, fonte nova e edição local. Preservar transformações, nomes locais, habilitação, componentes anexados e referências. Reconhecer filhos adicionados/removidos e mudanças de pai. Quando um nó removido possuir script ou alterações locais, oferecer preservação como órfão/desvinculado ou resolução explícita, sem excluir os dados automaticamente.

Persistir o vínculo e os overrides. Expor no inspetor o que foi alterado localmente e comandos reais para reverter uma alteração à fonte ou desvincular uma instância. Aplicar alterações ao recurso compartilhado somente com escopo explícito; a fonte importada continua preservada. Proteger ciclos de dependência e projetar as referências para cenas reutilizáveis, sem declarar instâncias aninhadas prontas antes de ligar seu consumidor.

Integre o reconciliador à transação existente: falha não deixa cena nova com registry antigo ou recurso parcialmente publicado. Mudanças de formato exigem versão, migração e backup. Cenas legadas ambíguas devem continuar abrindo sem reconstrução adivinhada.

**Conclusão observável:** duas instâncias do mesmo assembly, uma com filho deslocado e script anexado. Reimportar uma fonte com novo filho e nomes repetidos preserva o ajuste/script, atualiza o que não foi sobrescrito, não duplica objetos e apresenta ambiguidades. Salvar/reabrir conserva os vínculos.

## Entrega 2 — submeshes e materiais autorais (M07/M08)

Completar a representação de malha com primitivas/submeshes e slots materiais. Um nó com várias primitivas deve continuar sendo um nó autoral, sem criar filhos apenas porque existem vários desenhos. A extração de draws e o picking mantêm vínculo com o mesmo ObjectId e com o slot correspondente.

Criar ou completar recursos de material com identidade, revisão, referências e serialização. O inspetor apresenta os slots com nomes, material atribuído e alcance da edição: recurso compartilhado ou substituição naquela instância. Trocar um material não deve duplicar malhas nem reconstruir toda a geometria como operação habitual.

Planejar migração dos filhos artificiais já gravados. Só consolidar quando houver correspondência comprovada e quando componentes, referências e alterações forem preserváveis; manter o legado com diagnóstico nos casos ambíguos.

**Conclusão observável:** importar nó com três materiais, trocar apenas um slot em uma instância e editar um material compartilhado em outra operação. O alcance é previsível, os irmãos não mudam indevidamente e tudo persiste em Edit/Play e após reabrir.

## Entrega 3 — texturas e PBR completos no perfil (M09.1/M09.2)

Implementar a cadeia bytes da imagem → decode → mipmaps/sampler → recurso GPU → binding material → shader → persistência/retomada. PNG/JPEG embutidos são o primeiro percurso. Verificar o Android mínimo real e o suporte já existente antes de escolher backend. Limitar dimensões, bytes decodificados, mipmaps e memória total antes das alocações; o tamanho comprimido do GLB não basta.

Ligar base color, metallic/roughness, normal, occlusion e emissive, com semântica correta de canais e espaço de cor. Preservar UVs necessários e seleção de coordenadas por textura, tangentes/handedness e geração quando aplicável. Implementar `KHR_texture_transform` por slot, sem aplicar uma transformação global a todas as texturas.

Completar OPAQUE/MASK/BLEND, alpha cutoff e double-sided no consumidor gráfico: filas, profundidade e ordenação precisam acompanhar o material. Não considerar o trabalho pronto ao armazenar propriedades no parser. Explicitar aproximações e limitações de transparência.

Estender a transação a imagens e materiais, com dependências, hashes, cache e recuperação. Recriar recursos de GPU quando necessário sem perder identidades. Mudanças de fatores materiais atualizam os dados pertinentes, sem redecodificar todas as imagens.

**Conclusão observável:** GLB com vários slots e mapas PBR aparece com as texturas corretas, permite editar fatores/referências e mantém aparência após Save, Play/Stop, retorno do segundo plano e reabertura. Comparação visual usa iluminação e câmera controladas; ausência de textura não pode parecer material final correto.

## Entrega 4 — dependências externas e codecs

Depois dos contratos anteriores, ampliar a entrada para `.gltf` + `.bin` + imagens e GLB com referências externas. Implementar resolvedor para fontes autorizadas pelo SAF, staging e manifesto de dependências. Não transformar `content://` em caminho POSIX, abrir rede implicitamente ou buscar arbitrariamente no armazenamento. Copiar dependências necessárias para o projeto; reabertura deve funcionar offline sem depender da permissão temporária original.

Integrar Draco, meshopt e KTX2/BasisU por adaptadores com versão/licença fixadas e orçamentos de expansão. São trilhas distintas: decoder geométrico não resolve textura e aceitar geometria não comprova decode de BasisU. A negociação considera suporte efetivo do aparelho. Atualizar a matriz de extensões usadas/exigidas e só anunciar suporte quando o consumidor estiver conectado.

Também resolver transformações refletidas/escala negativa de ponta a ponta, incluindo normais, orientação dos triângulos, seleção e hierarquia. Shear precisa de representação suportada ou recusa explícita; não aproximar silenciosamente matrizes. Não relaxar globalmente validações de transform sem revisar os consumidores físicos e gráficos.

**Conclusão observável:** fonte com dependências importada pelo seletor continua utilizável depois de remover o acesso à origem. Modelos comprimidos do perfil produzem geometria/imagens equivalentes às versões sem compressão. Recurso não suportado ou excesso de orçamento mantém a versão anterior íntegra e informa causa específica.

## Pontos de entrada no código

- Parser/representação: `native/resources/gltf_import.cpp/.h`.
- Identidade e dependências: `native/resources/asset_registry.cpp/.h`.
- Comandos/publicação/instância: `native/editor/editor_session.cpp/.h` e `editor_import_transaction.h`.
- Extração/picking: `native/editor/editor_map_scene.cpp/.h`, `editor_pick_mesh.h`.
- Dados autorais e formatos: `native/scene`, `native/runtime`, `native/editor/editor_archive.*` e histórico existente.
- UI: `native/editor/editor_screen.cpp/.h`, descritores de propriedades e filesystem.
- Entrada Android: `ModelPicker.java`, `android_model_picker.cpp/.h`.
- Publicação/retomada: `native/platform/android/android_main.cpp`, `dirt_road_resources.*`, `instanced_renderer.*`, RHI e shaders realmente usados no APK.

Esses caminhos são pontos de investigação, não instrução para duplicar sistemas ou modificar todos sem necessidade.

## Execução, evidências e coordenação

Implementar cada bloco atravessando dados, comandos/UI, renderer/runtime e persistência antes de repetir verificações pequenas. O histórico desta tarefa autoriza ADB, mas o aparelho deve ter um único agente operando a interface por vez. Não executar em paralelo com outra tarefa que esteja usando a tela. Não presumir autorização nova para suites extensas apenas por este documento; seguir as instruções atuais do usuário.

Roteiro de aceitação a registrar quando executado: IMP03, IMP05, IMP06, IMP09; AST01–AST08; GFX01/GFX02; LIF01/LIF07. Usar Ford/Porsche, amostras Khronos e pelo menos um assembly que não seja veículo. Arquivos malformados, cancelamento e falha de alocação devem preservar o projeto anterior. Medir importação/memória quando possível, sem transformar números não medidos em resultados.

Atualizar os docs com base, implementação real, migração, limitações, evidências e testes pendentes. Não marcar M08/M09 inteiros concluídos porque uma parte passou. Não incluir GLBs pessoais grandes em commits. Coordenar commits e integração sobre o snapshot correto; não sobrescrever alterações de outro agente.

## Depois deste pacote

M09.3: animação, skins e morphs com edição, reprodução, bounds e autoridade de pose. Depois, cenas reutilizáveis aninhadas e composição M12 sobre os vínculos consolidados. Acompanhar as pendências de M06/M05: campos numéricos inline, resolução de conflitos de arquivo, identificação de componente nos logs e produtores render/lifecycle.

Uma tarefa paralela mais independente pode receber especificamente campos inline e conflitos do IDE. Isso ainda exige coordenação em `editor_session`, `editor_screen` e `android_main`; dois agentes não devem editar esses arquivos simultaneamente sem divisão explícita.
