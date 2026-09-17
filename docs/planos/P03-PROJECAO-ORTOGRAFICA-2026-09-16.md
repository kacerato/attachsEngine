# P03 — contrato de projeção ortográfica

Continuação iniciada em 16/09/2026, atualizada em 17/09, do plano de universalidade e do registro P01–P04 de 15/09. **Autoria ortográfica, vista, zoom, persistência e Play implementados, com percurso visual ADB conferido.** Isso não encerra todo P03. Os registros inferiores descrevem os estágios anteriores e suas limitações naquele momento.

## Integração de autoria e execução — 17/09

- `astra.camera` versão 2 acrescenta `orthographic_half_height` e `projection`. A leitura da versão 1 mantém os quatro números antigos, a identidade da instância e perspectiva; versão 2 salva os novos campos depois desses números. Valores desconhecidos de modalidade são rejeitados.
- O grupo Lente oferece Perspectiva/Ortográfica. FOV e meia altura aparecem conforme a modalidade; seus valores permanecem guardados ao alternar. Near/far continuam universais.
- `SceneCameraPose`, Ver, Pilotar, Play e ponte Android transportam a mesma modalidade/extensão. O renderer reinicia a modalidade ao sair da câmera para impedir vazamento para a órbita editorial.
- Pilotar: zoom/pinça alteram exponencialmente a extensão ortográfica; pan usa unidades por pixel da extensão. Pose e lente são publicadas juntas em um comando fundível por gesto, com undo/redo/cancelamento. Na perspectiva o zoom continua sendo deslocamento axial.
- Representação da câmera usa faces paralelas no volume ortográfico, mantendo orientação e planos; escala do objeto não altera a lente.
- Cascatas de sombra passam a cobrir as faces paralelas próximas/distantes. A mesma política de cortes, crossfade, margem e cache permanece. A meia altura já invalida o cache de projeção.
- LOD passa a medir `erro * alturaEmPixels / (2 * meiaAltura)` na ortográfica. Distância/FOV não reduzem detalhe; zoom muda o orçamento projetado, mantendo histerese e dither.
- Materiais opacos e água usam direção de observação paralela. Impostores orientam suas faces pela direção óptica. Água converte depth linear para distância axial e não aplica o fator angular de uma câmera perspectiva à espessura.
- SPIR-V regenerado para malhas e duas famílias de água. Não existe promessa de paridade de importação, pós-processamento ou recursos de câmera da Unity.

### Limites explícitos

TAA temporal permanece em resolve espacial. HZB/culling GPU recebeu o contrato ortográfico descrito abaixo; movimento invalida seu histórico, e roll/subviewport mantêm as restrições conservadoras existentes. A ausência dessas otimizações não pode retirar objetos. Preview independente/PiP, múltiplos targets, câmera física e máscaras de saída continuam fora deste bloco. Água, impostores e sombras ainda precisam de cenas dedicadas no dispositivo; compilação não equivale a validação visual desses materiais.

### Evidência de host

A conferência no aparelho encontrou uma colisão anterior: `ConsoleRowBase` e `ComponentEnumBase` usavam `0x6f000000`. Com um console associado ao projeto e mensagens presentes, o console consumia seletores mesmo fechado. Linhas do console foram movidas para a faixa livre `0x51000000`; IDs de componentes permanecem iguais. O caso de regressão liga um console populado antes de alternar a modalidade. A instrumentação temporária de diagnóstico foi removida.

Build concluído. Filtros `orthographic_` 7/7, `p03_` 2/2, `Lod_` 11/11 e `cascade_` 14/14 passaram; há sobreposição e os filtros também incluem casos anteriores. Novo teste do inspetor alterna modalidade por toque e confirma volta, arquivo v2, resolução da câmera, zoom sem dolly e histórico. APK compilado e instalado via ADB; evidência visual final será registrada abaixo.

## Histórico das etapas anteriores

### Evidência ADB — 17/09/2026

Dispositivo 25053PC47G, projeto `P01Camera0915i`, APK Debug instalado com sucesso (SHA256 `02150350ee48d452e0df565c10f8c2da98fbacd83ee1c627ca3dcf1ac9c14227`). Câmera existente carregada da versão 1. Procedimento pela UI: abrir componente → Lente → alternar Projeção → Ver → Pilotar → Zoom → Salvar → Desfazer → Salvar → Refazer → Salvar → encerrar/reabrir aplicativo → abrir projeto → Ver → Play/Stop. No APK final também foi conferido toque no cubo pela imagem ortográfica, com seleção correta na hierarquia/inspetor.

| Evidência em `docs/validacao/evidencias/p01-p04-20260915/` | Resultado observado |
|---|---|
| `ortho-live.png` | Ortográfica ativa, meia altura 5, esfera/cubo e grade no mesmo enquadramento |
| `ortho-zoom.png` | Meia altura 3,50228 e aumento visual dos dois objetos |
| `ortho-saved.aescene`, `ortho-undo.aescene`, `ortho-redo.aescene` | Versão 2, mesma instância 1, modalidade 1; extensão 3,50228024 → 5 → 3,50228024; posição da câmera preservada |
| `ortho-reopened.png` | Modalidade e extensão conservadas após reiniciar o aplicativo e reabrir o projeto |
| `ortho-play.png` | Play renderiza os objetos com projeção paralela; grade editorial omitida pelo comportamento existente de Play |
| `ortho-pick-final.png` | Toque sobre o cubo seleciona Cubo, mantendo a vista ortográfica |
| `ortho-device.log` | Captura do processo durante reabertura/Play: sem `Validation Error`, `FATAL EXCEPTION` ou `Fatal signal`; existem avisos `Shader-OutputNotConsumed`, portanto não é um log sem avisos |

O percurso revelou pequeno arredondamento de Euler quando o zoom reconstruía uma pose idêntica. O APK final preserva a transformação original em zoom exclusivamente de lente; teste confirma pitch/roll exatos. Os filtros P03 passaram novamente (2/2). O teste antigo de console foi atualizado para respeitar as abas atuais Problemas/Logs e conferir as regiões de toque de ambas; `console_` passou 2/2. Não foi executada a suíte inteira.

**Ainda não conferidos no aparelho:** água/absorção em cena dedicada, impostores/LOD com níveis reais, todas as cascatas de sombra, retrato e pinça física de dois dedos, clipping extremo e soak. Esses itens permanecem no roteiro, assim como preview independente e as otimizações temporais/GPU mencionadas acima. A seleção e os contratos de projeção têm evidência de host adicional; isso não substitui esses percursos de dispositivo.

## Implementado

O contrato de frustum recebe uma modalidade explícita `CameraProjection` e meias extensões ortográficas em unidades de cena. Os nomes históricos `PerspectiveVisibilitySettings` e `PerspectiveFrustum` permanecem para compatibilidade de código. Tangentes de perspectiva não são reaproveitadas para armazenar extensões.

| Consumidor real | Comportamento implementado |
|---|---|
| Construção e validação | Extensão vertical positiva, proporção, planos e modalidade válidos; FOV não interfere na modalidade ortográfica |
| Projeção editorial | Coordenadas X/Y independentes de Z; orientação mundial e pré-rotação do display preservadas |
| Seleção | Raios paralelos com origem deslocada pelo pixel; `screenPointToRay` e `pickNearest` usam a origem correta |
| Interpolação de raios | Direção e deslocamento da origem afins em NDC; compatível com a reconstrução futura por triângulo de tela inteira |
| Segmentos editoriais | Recorte lateral com largura constante e planos próximo/distante; divisão homogênea unitária |
| Culling CPU | Seis planos de volume retangular, raio expandido e comportamento conservador para dados inválidos |
| HZB CPU | Retângulo de limites sem redução pela distância; profundidade linear normalizada para comparação com um futuro attachment ortográfico |
| Gizmos | Comprimento mundial derivado da extensão e do tamanho do viewport, sem crescer com a distância |
| Política da grade | Espaçamento e mistura de escalas derivados da extensão, sem mudança indevida ao elevar a câmera |
| Guarda GPU | Contrato de 128 bytes, extensão ortográfica explícita e profundidade linear; histórico utilizável apenas com pose estacionária, respeitando elegibilidade do renderer |

A seleção ortográfica usa a origem no plano da câmera. A continuação de 17/09 acrescenta o intervalo de seleção descrito abaixo, sem deslocar a origem matemática usada pelos gizmos.

## Continuação de 17/09 — seleção limitada pelos planos da câmera

`EditorRay` agora transporta distâncias mínima/máxima de seleção. Em perspectiva elas incluem o comprimento do raio cru, para representar planos de profundidade e não uma esfera em torno da câmera. Na ortográfica a direção é unitária e paralela, portanto o intervalo coincide com near/far.

`pickNearest` descarta candidatos inteiramente fora do intervalo. Para malhas, inicia a consulta de triângulos no plano próximo e converte a distância de volta para a origem original; isso preserva uma segunda superfície visível quando a primeira superfície da mesma malha ficou cortada. O resultado também é limitado pelo plano distante. As interseções matemáticas dos gizmos continuam usando a origem original, sem truncar um arraste na borda do volume.

Essa correção é consumida também pelo editor em perspectiva atual. Não altera formato de cena, IDs ou parâmetros salvos. Raios construídos manualmente mantêm por padrão o intervalo positivo amplo anterior.

Testes adicionados: objetos antes/depois dos planos não interceptam o toque; duas superfícies da mesma malha com a primeira cortada; acerto de triângulo limitado pelo far; raio oblíquo limitado por profundidade axial. Integração gráfica da ortográfica permanece pendente.

Build de host concluído em 17/09. Execução: `camera_clip_` 3/3 (dois novos e um existente), `session_` 51/51 e `orthographic_` 4/4. Sem novo APK ou validação ADB desta alteração.

## Integração pendente antes de expor no inspetor

### Avanço de 17/09 — ponte interna e projeção GPU

- `InstancedRenderer::setSceneOrthographicHalfHeight` recebe a meia altura; zero conserva perspectiva. Entrada não finita ou não positiva volta ao modo perspectiva. Nenhum controle do editor chama esse método ainda.
- O mesmo estado alimenta o frustum CPU e o vértice gráfico. A meia altura ocupa `worldToViewRow0.w`, antes reservado, mantendo o tamanho/layout do UBO. XYZ continuam sendo a linha da base óptica.
- `dirt_road_vertex.glsl`, compartilhado pelos vértices de malha e água, usa extensão em unidades de cena, clip.w=1 e profundidade linear. O deslocamento temporal utiliza o mesmo clip.w. Isso cobre projeção da geometria, não certifica refração ou sombreamento da água.
- Grade: varying adicional leva o deslocamento da origem, enquanto os raios ficam paralelos. O fragmento intersecta o plano usando essa origem, calcula profundidade a partir da posição original da câmera e grava profundidade linear. Perspectiva conserva origem única e profundidade anterior.
- Céu: a modalidade ortográfica usa uma direção paralela comum, sem simular abertura perspectiva. No fundo sólido editorial o passe de céu continua omitido pelo caminho existente.
- Nos passes exclusivos de céu/grade, `textureIndices.w` transporta os bits da meia altura. Nos passes de material o mesmo campo continua sendo índice da textura emissiva: seus push constants não foram modificados para transportar projeção.
- A meia altura entra na chave de invalidação de HZB, histórico temporal e cache de sombras. TAA usa resolve espacial sem jitter quando a modalidade ortográfica estiver ativa. A guarda existente do kernel de culling GPU recusa a modalidade até sua ABI ser ampliada.
- Regeneradas e validadas as famílias SPIR-V `dirt_road`, `water_surface`, `water_spectral`, `editor_grid` e `dirt_road_sky`. Os avisos existentes de precisão padrão nos fragmentos de água não impediram a compilação.

Não há alegação de imagem ortográfica validada no aparelho: falta autoria/ponte Android e os consumidores abaixo. A presença da função no renderer não libera uma opção incompleta ao usuário.

1. Versionar `scene::Camera`, preservando a leitura da versão 1, IDs e a ordem dos campos antigos; adicionar modalidade e extensão apenas junto aos consumidores gráficos.
2. Levar modalidade e extensão por `SceneCameraPose`, sessão, Play e ponte Android. Separar zoom de extensão e dolly posicional no modo ortográfico.
3. Validar visualmente os vértices de malha e água já atualizados: clip.w=1 e profundidade linear; conferir os contratos de material em todos os caminhos de desenho.
4. Validar grade e céu já atualizados: origem variável, direção paralela, interseção e `gl_FragDepth` linear; conferir oclusão, pré-rotação e modalidades de fundo.
5. Corrigir consumidores de profundidade: reconstrução de posição, refração/efeitos de tela, sombras/cascatas, LOD projetado, HZB GPU e reprojeção temporal. Qualquer fallback precisa ser explícito e documentado por consumidor.
6. Atualizar a representação da câmera para caixa com faces paralelas. Não desenhar um frustum de perspectiva para uma câmera ortográfica.
7. Expor o campo no grupo Lente e trocar FOV por extensão conforme modalidade; só então gerar APK e conferir salvamento, reabertura, seleção, giro do aparelho, geometria/grade e Play por ADB.

## Validação

Casos de host adicionados em `test_editor_view.cpp`: tamanho constante em diferentes distâncias e seleção do objeto mais próximo; ida/volta de projeção sob yaw/pitch/roll e pré-rotação; clipping/culling e profundidade linear do HZB; escala de gizmos e grade independente da distância. **4/4 passaram**, após compilação de `aether_tests` em `build/editor-host`. Os filtros de regressão `view_`, `Frustum_`, `Hzb_`, `camera_ray_` e `the_display_pre_rotation_` também passaram (16/16, 3/3, 15/15, 1/1 e 1/1; filtros por substring têm sobreposição, portanto estes números não são uma contagem única da suíte).

Após a integração GPU básica, `:app:assembleDebug` concluiu com sucesso em 17/09 (28 s na execução registrada). O novo APK foi compilado, mas não instalado nesta continuação. A modalidade ortográfica permanece sem caminho de autoria/ativação na interface e sem validação visual. Não considerar este contrato como conclusão de P03, preview independente ou paridade com Unity/Godot.

## Continuação — HZB ortográfico, 17/09

- Push constants de `GpuCullParameters` ampliadas de 112 para 128 bytes, preservando offsets anteriores. Flag explícita e duas meias extensões transportadas ao kernel `draw_cull.comp`; SPIR-V regenerado e validado pelo gerador.
- Referência CPU e shader usam footprint paralelo e depth linear `(z-near)/(far-near)`. Perspectiva mantém sua equação. Histérese, slots ausentes, orçamento de texels e política de retorno visível permanecem.
- Histórico ortográfico só é utilizável com posição/yaw/pitch exatamente estacionários. Qualquer movimento torna os candidatos visíveis e zera a sequência de oclusão. Mudanças de lente/viewport continuam invalidando a pirâmide no renderer. Roll e subviewport conservam as restrições anteriores.
- Host: `GpuCull_` passou 9/9. Corpus ortográfico cobre yaw, pitch, pré-rotação de superfície, candidatos próximos/distantes, parede parcial, casos visíveis/ocluídos e descarte do histórico por translação/rotação. É comparação da referência do kernel com HZB CPU, não execução do compute em GPU.
- Android Debug compilado e instalado. SHA256 `d7bd2f3587f971ea554d1e844314bf7813d9a73be7f00c0035b42678f3142024`. Projeto `P01Camera0915i` reaberto; editor e Play exibem esfera/cubo, e Stop devolve o controle editorial. Capturas `ortho-hzb-integration.png` e `ortho-hzb-play.png` na pasta de evidências P01–P04. Log `ortho-hzb-device.log` sem os padrões Validation Error/FATAL EXCEPTION/Fatal signal consultados.
- Limite de evidência: este projeto não comprova dispatch/ganho de oclusão GPU ortográfica. Objetos autorados/subviewport têm exclusões existentes, e o log não confirmou criação do consumidor compute nesse percurso. Falta uma cena elegível dedicada com telemetria de dispatch, objetos atrás de oclusores e movimento. Não declarar essa otimização validada no aparelho antes disso.
- Próximas entregas de P03 continuam sendo manipulação visual de lente/clip e preview independente com target real; TAA temporal ortográfico permanece pendente.

## Continuação — controles de lente na vista, 17/09

A vista da câmera agora apresenta uma faixa compacta com FOV (perspectiva) ou meia altura (ortográfica), plano Próximo e Distante. Valores e unidades acompanham o componente real. A faixa respeita a largura do viewport, a posição dos controles compactos e só aparece onde há espaço útil.

Cada toque abre o editor numérico existente, com identidade de entidade/instância/propriedade da câmera visualizada. Selecionar outro objeto não redireciona a edição. O mesmo contrato de validação impede far menor ou igual a near, e as alterações entram no histórico normal. Não foram acrescentados campos duplicados ao arquivo de cena.

Validação: host compilado; `p03_` 3/3, incluindo câmera visualizada diferente da seleção, alteração/undo de FOV, rejeição de clip inválido e extensão ortográfica. APK Debug compilado e instalado via ADB. No projeto P01Camera0915i foram conferidos a faixa ortográfica e o toque abrindo a edição com o valor correto: `lens-controls.png` e `lens-controls-edit.png` na pasta de evidências P01–P04. A aplicação do valor e o undo foram conferidos no host; não repetir essa evidência como edição concluída no aparelho.

Escopo restante: esta faixa é edição numérica contextual, não são alças 3D arrastáveis nos planos do frustum. Alças, preview independente com render target próprio, retrato físico e validação GPU dedicada continuam pendentes. P03 permanece aberto.

## Continuação — alças mundiais de lente e recorte, 17/09

Implementadas três alças no volume da câmera selecionada quando seu componente está aberto e as representações estão ligadas. Lente ajusta FOV em perspectiva ou meia altura em ortográfica; Próximo/Distante ajustam a profundidade dos respectivos planos. A lente usa uma seção do volume a 5 unidades, limitada pelo intervalo near/far; os planos usam suas distâncias reais, sem deslocar artificialmente o plano distante para caber na tela.

Desenho e interação compartilham `editor_camera_handles.h`: transformação óptica mundial sem escala, projeção e parâmetro de menor distância entre raio e eixo. A vista fica congelada durante o gesto; deslocamento é calculado desde o início, sem acumular erro por frame. Ângulos quase axiais recusam a alça para evitar amplificação de ruído. Alças fora do viewport não criam regiões invisíveis; os campos numéricos continuam disponíveis.

O gesto mantém a transformação da entidade, agrupa o arraste em uma operação, bloqueia interferência de outro dedo e restaura documento/histórico ao cancelar. Near/far permanecem ordenados e limitados pelo contrato do componente. Nenhum campo novo é salvo no arquivo de cena; a geometria é derivada.

Evidência: host compilado e `p03_` 4/4. Novo caso percorre as três alças pela UI, confirma undo/cancel e preservação do redo; cobre limites e ambas as modalidades. APK compilado e instalado, SHA256 `51775448ca3c9f0e7d5abf79facfe3254298b95ce35551a0246d3b0dc9780535`.

ADB no projeto P01Camera0915i: arraste de Lente mudou meia altura 3,50228 → 4,64337; undo restaurou 3,50228. Próximo mudou 0,1 → 0,795711. Undo e Salvar restauraram os valores iniciais. Evidências `camera-handles-before.png`, `camera-handles-drag.png`, `camera-handles-near.png`, `camera-handles-restored.png` em `docs/validacao/evidencias/p01-p04-20260915/`.

Limitações de validação: a alça Distante estava fora do enquadramento com far=2000, portanto seu gesto foi conferido no host, não no dispositivo. Perspectiva, cancelamento físico, hierarquias com escala/shear e retrato ainda precisam de percurso específico no aparelho. Preview independente com target próprio, TAA ortográfico e comprovação de dispatch HZB seguem pendentes. Este bloco não encerra P03.

## Continuação — preparação de vista independente

A sessão passou a possuir `EditorCameraPreview`, com câmera fixada, orçamento de extensão/pixels/frequência, snapshot de frustum completo e token de trabalho. Resultado de revisão antiga não pode ser publicado; mudança de cena fecha a prévia, e recuperação de alvo invalida a imagem anterior. O estado não altera seleção, órbita editorial ou relógio de simulação.

Host compilado e `p03_` 5/5. **Não há ainda renderização offscreen, imagem PiP ou validação ADB deste contrato.** O agendador é preparação para o backend, não conclusão do preview. Estado global e roteiro de integração: [Progresso de 17/09](PROGRESSO-ASTRA-2026-09-17.md). Estimativa de P03 continua em aproximadamente 70%, sem elevar o índice por uma API ainda sem imagem.

## Continuação — preview Vulkan independente, 17/09

A etapa anterior foi integrada ao renderer. `instanced_camera_preview.inl` cria alvo de cor amostrável, profundidade, framebuffer, buffer de constantes e conjunto de descritores exclusivos da vista. O passe usa a mesma topologia compatível de subpasses/pipelines, reutiliza malhas, instâncias e materiais, mas calcula seu próprio frustum e lista de desenhos. Transparências são ordenadas para a câmera da prévia. A imagem chega à UI por binding próprio, sem passagem pelo atlas de ícones e sem duplicar a simulação.

Recursos são atualizados/liberados após a espera do fence do renderer; o resultado só é anunciado depois da conclusão do frame e validado pelo token/revisão do agendador. Fechar a janela solicita liberação no renderer; troca de cena fecha o modelo, republicação de geometria e recuperação do renderer invalidam a imagem. Esses caminhos existem no código; não confundir isso com certificação de todos os percursos de lifecycle no aparelho.

**Uso:** selecionar uma câmera, abrir seu componente e tocar **Prévia**. A janela mostra o nome da câmera e pode ser fechada pelo ×. Selecionar outro objeto mantém a câmera fixada. Em cena estática, uma revisão nova solicita atualização; a frequência máxima usa relógio monotônico, pois o relógio de simulação não avança no modo Edit. Esse erro foi detectado e corrigido durante a conferência ADB.

### Evidência desta entrega

- Host compilado e filtro `p03_` **6/6**, incluindo pin/close pela UI, troca de seleção, publicação e preservação da órbita.
- Android `:app:assembleDebug` concluído, APK instalado. SHA-256: `0D3C26CC082D65B759918F6C48C5D049C47A9D0987ADEDB3945A87F60A39DC38`.
- No aparelho 25053PC47G, projeto P01Camera0915i: prévia e viewport com enquadramentos distintos; ocultar Cubo remove a geometria das duas vistas; desfazer restaura; orbitar o editor preserva o enquadramento fixado. Cena salva após desfazer a alteração de visibilidade; propriedades da câmera não foram modificadas neste percurso.
- [Cubo oculto nas duas vistas](../validacao/evidencias/p01-p04-20260915/camera-preview-hidden-cube.png) e [restauração com órbita independente](../validacao/evidencias/p01-p04-20260915/camera-preview-independent.png) pertencem ao APK final. `camera-preview-first.png` é registro intermediário anterior à correção do relógio.
- [Log do processo](../validacao/evidencias/p01-p04-20260915/camera-preview-device.log): nenhuma correspondência para `VUID-`, `Validation Error`, `Fatal signal` ou `FATAL EXCEPTION` na janela capturada. Não é soak nem certificação geral dos shaders.

### Limites e pendências

Uma única prévia no workspace Cena, em edição; padrão 640×360 e até 15 Hz. Ainda sem arraste/redimensionamento do painel, ajuste de orçamento pela UI ou mensagem específica para falha do backend. **Água omitida; sombras e pós-processamento desativados**, com indicação no rodapé. Sem céu/grade editorial e sem histórico temporal, HZB ou seleção de LOD independentes. Não declarar fidelidade completa de Play nem paridade Unity.

Faltam percursos físicos de fechar/reabrir, trocar cena, recriar surface e retrato; corpus de materiais texturizados/alpha/complexos, custo GPU/memória e comportamento prolongado. A construção de constantes de material ainda espelha o caminho principal e deve ser centralizada para evitar divergência futura. P03 continua aberto; os pacotes P01/P02/P04 permanecem na sequência do plano.


## Continuação — configuração e falhas da prévia, 17/09

Implementados dois controles compactos no painel: resolução alterna 320×180, 640×360 e 960×540; frequência máxima alterna 5, 15 e 30 Hz. São limites reais do alvo/agendador, não somente rótulos. Alterar configuração invalida a imagem e o trabalho pendente; conclusão da configuração anterior não pode publicar. Preferências são da sessão, ainda sem persistência entre reinicializações. A proporção permanece 16:9 e o painel não ganha redimensionamento livre nesta etapa.

Falha do backend agora interrompe tentativas automáticas e apresenta uma ação por toque para tentar novamente. Recuperação do alvo ou mudança de configuração também limpam o erro. Isso evita realocar recursos repetidamente quando o backend não consegue renderizar. Revisões antigas são rejeitadas sem registrar um falso erro para a revisão atual.

Android Debug compilado e instalado por ADB. Os controles novos ainda não receberam conferência visual física nesta rodada; as imagens anteriores comprovam a prévia básica, não estes controles. Água, sombras, pós-processamento, lifecycle físico completo e custo medido continuam pendentes.

Percentual atualizado: **plano ampliado aproximadamente 30% (faixa 25–35%); P03 aproximadamente 70%**. Estimativas mantidas: configuração e erro melhoram uma entrega parcial, mas não encerram as lacunas de aceitação. Cada continuação deve informar novamente o percentual e a base da alteração ou manutenção; não somar pontos automaticamente por quantidade de commits.

Validação complementar: alvo host `aether_tests` compilado, filtro `p03_` **6/6**. Caso do agendador ampliado para falha, bloqueio de repetição automática, nova tentativa e rejeição de conclusão após mudança de resolução. A compilação host geral falhou em `tests/native/ui_preview_main.cpp`, alvo auxiliar `aether_ui_preview`; não declarar o build host inteiro aprovado. Android e alvo de testes passaram separadamente.


## Continuação — suspensão, recuperação e conferência física, 17/09

A prévia agora libera seu alvo ao sair do workspace Cena ou entrar em Play, mantendo a câmera fixada para solicitar uma imagem nova ao retornar. A validação da entidade/epoch acontece antes do bloqueio por falha: câmera removida ou cena substituída não deixa um painel preso em erro. Uma solicitação sem conclusão por mais de cinco segundos, observada pelo agendador enquanto a aplicação avança frames, vira falha com nova tentativa explícita; conclusões atrasadas não publicam. Isso não detecta um bloqueio completo da thread/GPU.

Android Debug compilado e instalado. No projeto P01Camera0915i foram conferidos os controles passando de 640×360/15 Hz para 960×540/30 Hz e o percurso fechar/reabrir com a imagem e configuração preservadas na sessão. Evidências: `camera-preview-settings.png` e `camera-preview-reopened.png` em `docs/validacao/evidencias/p01-p04-20260915/`. Não foram alteradas propriedades da cena. Troca de projeto, recuperação de surface e suspensão por Play ainda precisam de percurso físico específico; não estão comprovadas pelas capturas.

Host: corrigido namespace ausente nas chamadas `editWaterRoute` da ferramenta `ui_preview_main.cpp`; build geral agora concluído. Filtro P03: **6/6**, com cobertura adicional de timeout, conclusão atrasada e suspensão/retorno do modelo. Nenhum benchmark de custo gráfico foi executado.

**Percentual atualizado: plano ampliado ≈30% (25–35%); P03 ≈70%.** Houve avanço de robustez e evidência da prévia, mas a estimativa arredondada permanece: faltam fidelidade gráfica, lifecycle físico completo e cobertura de câmeras/hierarquias. Próximo bloco de implementação: tipos ricos P01 (cor/HDR, vetores e referências com consumidores reais), seguido do painel de impacto P02; fidelidade avançada da câmera permanece explicitamente aberta.
