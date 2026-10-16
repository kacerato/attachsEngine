# Bloco 1 — execução e aceite

Escopo autorizado: D01–D09/D12, P01–P08/P10–P11 e M01–M08, com todas as propriedades da auditoria. Os 71 itens permanecem no escopo total. Não usar este registro como declaração de conclusão.

Branch: `codex/terrain-voxel-generator`. ADB suspenso por solicitação do proprietário. A validação atual é local; toque/GPU Android continuam pendentes.

## Em implementação

- Receita AETERRAIN5: propriedades completas dos pincéis e recursos de caminho persistidos, mantendo leitores AETERRAIN1/2/3/4.
- Pincéis: dimensões XYZ, orientação, cubo arredondado, inversão, opacidade acumulativa/alvo, profundidade, trava de altura, filtro e restauração de geometria/materiais separados.
- Campo de autoria separado da triangulação; reutilização de chunks sem mudanças e halo para dependências de normais. Atualizações incrementais de amostras quando preservam a semântica do histórico.
- Caminhos: pontos e segmentos, pista/acostamento, perfis separados, elevação/rampa, materiais e cruzamentos. Interface com Traçado, Perfil e Materiais.
- Prévia de configurações antes de aplicar, além da prévia durante o gesto.

## Ainda necessários para fechar o bloco

- Completar publicação localizada no renderer, histórico/checkpoints/GC, interface altura↔voxel e LOD com bordas contínuas/colisão.
- Completar contratos de vizinhança e resoluções independentes dos mapas. Pontos no viewport e nós individuais das curvas já têm implementação e validação local; falta a conferência no aparelho.
- Biblioteca de materiais e assets originais com perfis opcionais; consumidores PBR, height blend, projeções, noises, distance resampling e clusters M01–M08.
- Conferência de todos os controles e suas propriedades; regressões, medições, capturas executáveis, APK, publicação da branch e nota pública sanitizada.

## Evidências

- Base anterior: `build/terrain-evidence/block1-baseline-tests.log`, 34/34 cenários locais.
- Hipótese visual (não implementação): `build/terrain-evidence/block1-path-concept.png`, gerada a partir da captura antiga do editor.
- Biblioteca preparada: 82 entradas de material/configuração, 146 texturas únicas, originais preservados. A comparação de dimensões, hashes e pixels foi registrada em `build/terrain-evidence/block1-assets-validation.json`. O catálogo ainda não está conectado ao material de runtime; isso não fecha M01–M08.
- Oito referências de mask map do MapMagic não foram encontradas nos cinco pacotes. Elas estão listadas no catálogo como dependências ausentes; não foram substituídas por mapas inventados.
- A divergência dos filtros na borda finita foi corrigida e a rodada completa posterior passou em 43/43 cenários (`block1-host-tests10.log`). A revisão seguinte passou em 47/47 cenários (`block1-host-tests14.log`), incluindo reabertura e arraste dos pontos na superfície aplicada.
- Exportação GLB passou a compartilhar somente vértices idênticos em todos os atributos. Não há redução de triângulos no nível original nem redução de texturas. O teste compara posições de todos os índices e verifica LOD com bordas presas.
- Perfis de caminho agora têm edição dos nós individuais, incluindo inserção, remoção e coordenadas, sem substituir a curva toda. A prévia usa a mesma curva da geração e mostra pista, acostamento e caps. Capturas posteriores: `ui-1280-4-s1.png` e `ui-655-4-s1.png`; a revisão compacta passou no teste de acesso aos dois extremos do gráfico sem rolar.
- Revisão funcional `ea85ce0` publicada na branch. Checagem de compilação de seis unidades Android ARM64 passou em 6/6 (`block1-android-source-check.log`). Isso não equivale a execução no aparelho. O APK seguinte está em compilação; não há confirmação de entrega completa.

## Disco e eficiência da execução

- A medição local encontrou Git/LFS entre os maiores consumidores de I/O: os intermediários Android estavam versionados e eram relidos no diff do editor.
- Commit `2fca165`, publicado na mesma branch: 1.932 arquivos gerados retirados do índice; `android/app/build/` e `android/app/.cxx/` adicionados ao ignore. Os arquivos locais foram preservados para aproveitar a compilação.
- Compilação limitada a um processo. Não executar simultaneamente outra compilação completa ou inventários amplos de diretórios.
- A revisão automática bloqueou a exclusão física tanto em lote quanto individual da pasta temporária; não houve limpeza física nessas tentativas. Os caches identificados estão em `build/terrain-evidence/block1-storage-inventory.json`.
- A remoção anterior de duplicatas de extração verificadas por hash preservou arquivos extraídos e arquivos compactados originais. Não confundir essa economia anterior com a tentativa bloqueada atual.

Referências oficiais e fontes/versionamento dos plugins: [auditoria](auditoria.md). Registro completo de propriedades: [matriz de 71 itens](plano-implementacao.csv).

## Revisão de caminhos e publicação por faixas

- Modos de altura explícitos: seguir superfície ou alturas XYZ dos pontos. Rampa uniforme permanece uma mistura independente, e altura definida substitui os dois modos. O Y intermediário tem efeito no modo por pontos.
- Curva geométrica aceita -1 a 2 para vala/crista; opacidades de textura e vegetação continuam 0 a 1. As curvas são independentes.
- Handles no viewport acompanham a superfície da prévia. Arrastar usa um plano estável durante o gesto para não realimentar a reconstrução.
- A publicação Vulkan compara regiões alinhadas, prepara vértices e índices alterados em uma única submissão e preserva alocações quando os tamanhos não mudam. Crescimento/redução dos buffers ainda usa substituição integral. Sincronização global e reconstrução de filas/culling continuam presentes; não anunciar D07 totalmente fechado nem desempenho no aparelho sem medi-lo.
- Referência de capacidade: [Unity Splines 2.8.4 — Knots](https://docs.unity3d.com/Packages/com.unity.splines@2.8/manual/knots.html), [Unity 6000.0 — Mesh.SetVertexBufferData](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Mesh.SetVertexBufferData.html). A implementação Astra mantém seus próprios dados e fluxo de toque.

- Transferência Vulkan real no Windows: `block1-vulkan-buffer-tests.log`, 4/4 cenários, NVIDIA GeForce RTX 3050 6GB Laptop GPU. O cenário de três edições de palavras enviou 12 bytes de 1.052.672; leitura de volta conferiu todos os bytes, preservação das regiões intactas, recusa atômica de lote inválido e reutilização do fence. É teste de buffer, não benchmark de fluidez da engine no Android.
- Planner de atualização: `block1-buffer-tests.log`, 3/3; alterações contíguas/distantes/dispersas, limites de bloco e mudança de tamanho.
- Contratos de material: `block1-material-contracts.log`, 6/6. Isso valida interpretação dos canais e preservação dos dados do catálogo; ainda não valida um consumidor PBR de terreno.
- Cruzamentos Smart escolhem a cobertura mais forte antes de compor, com desempate por ID estável. O modo de sobrepintura por ordem continua disponível. Seguir terreno passou a interpolar a altura, evitando amostragem em degraus.
- O campo aplicado é mantido para alinhar os handles. Fechar libera esse cache; reabrir reconstrói apenas o campo em worker, sem republicar uma malha duplicada. O teste de toque roteado move o ponto no viewport e cancela restaurando XYZ sem criar histórico confirmado.

## Prévia indexada sem perda

- A prévia durante o gesto agora compartilha o mesmo indexador sem perda do GLB. Somente vértices idênticos em todos os 14 atributos são compartilhados, preservando costuras, normais, UVs e pesos distintos.
- `block1-host-tests15.log`: 48/48 cenários. A reconstrução indexada é comparada por bytes; o cenário de arraste confere cada posição, normal empacotada, UV e peso publicado, mantém os triângulos e exige redução de vértices na prévia.
- Essa otimização não reduz resolução de assets nem decima geometria. Não representa medição de FPS Android.
