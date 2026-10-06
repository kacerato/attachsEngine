# U11 — fechamento integral

Este bloco trata o U11 inteiro do roadmap de objetos/colisão/controle. Não altera o universo do roadmap original nem declara U01–U14 ou a engine completos. Estado final: **U11 concluído e aceito no host e no Android em 2026-10-06**. Nenhum requisito de U11 permanece pendente. Os limites de geometria, desenho e conversão abaixo fazem parte do contrato exposto, não de um fallback silencioso.

## Cadeia existente preservada

Inspector, identificação objeto + UID de Colisor, Body dono, picking geométrico, queries Jolt/ABI44/C#, oclusão e alças de dimensão/centro/rotação já têm entregas próprias. Devem continuar funcionando e entrar na regressão deste bloco.

## Cadeia nova necessária

1. **Faces e vértices:** geometria física real → topologia soldada → seleção no viewport → edição de coordenadas/deslocamento → prévia isolada → validação do solver → recurso imutável próprio → Colisor com a mesma identidade → histórico → salvar/reabrir. Compartilhar uma malha não autoriza modificar outros usuários. Primitivas exigem conversão explícita para representação em malha; superfícies curvas serão aproximadas e esse fato será mostrado. Prévia inválida não pode ser publicada. Cancelamento, troca de projeto/cena/seleção, Play e perda de foco não podem deixar um gesto pendente.
2. **Diagnóstico físico:** visual, geometria de colisão, centro local, centro de massa, apoio e autoridade são conceitos diferentes. COM vem do Jolt; apoio vem de consultas reais ou do estado do motor; autoridade vem de GameWorld. Ausência/erro deve ser visível. Prévia autoral não será apresentada como contato de uma simulação em execução. Sem executar scripts para inspecionar a cena autoral.
3. **Escala:** medir extração/picking/desenho/publicação e diagnóstico com geometria/partes em escala. Orçamento da visualização deve limitar custo de desenho sem substituir a geometria usada pelo picking/solver. A ferramenta fechada não deve cozinhar malhas nem montar mundos físicos por quadro. Reportar contagem real e redução da visualização quando houver.
4. **Workflow mobile:** entrada contextual no Colisor/Body; viewport permanece utilizável; modos faces/vértices, seleção e prévia possuem feedback distinto. Coordenadas com teclado numérico e gestos com captura exclusiva. Ferramentas temporárias saem ao retornar. Ícones entram no atlas executado.

## Referências e decisões

- Unity ProBuilder **6.0.9**, [contextos e modos](https://docs.unity3d.com/Packages/com.unity.probuilder@6.0/manual/modes.html): seleção de objeto e de elementos usam contextos distintos. Adaptação: rota de colisão temporária, sem transformar a edição física em alteração da malha visual.
- Godot **4.5**, [ConvexPolygonShape3D](https://docs.godotengine.org/en/4.5/classes/class_convexpolygonshape3d.html) e [fonte oficial](https://github.com/godotengine/godot/blob/4.5/scene/resources/3d/convex_polygon_shape_3d.cpp): conjunto de pontos alimenta a forma convexa; contorno visual é derivado. Adaptação: cozinhar/validar no backend realmente utilizado, Jolt **5.6.0**, antes de publicar.
- Unity **6000.0**, [Physics Debugger](https://docs.unity3d.com/6000.0/Documentation/Manual/PhysicsDebugVisualization.html): filtros, geometria física e COM distintos. Adaptação: diagnóstico contextual do alvo, com origem autoral/runtime explícita, sem ocupar o viewport permanentemente.
- [UUM-83050](https://issuetracker.unity3d.com/issues/the-physics-debugger-displays-incorrect-collision-volume-when-compared-to-its-collider-bounds): divergência entre volume mostrado e colisão é uma regressão real. Não usar bounds visuais como COM/apoio/forma física.

Documentação e fonte foram consultadas. Vídeos encontrados ainda não foram analisados; não constituem evidência desta entrega.

## Aceite obrigatório, sem exceções

- Selecionar e editar face e vértice, com geometria ocluída e seleção por identidade; aplicar muda a colisão real e preserva visual, irmãos, Body e UID. Malha compartilhada permanece intacta.
- Cancelar preserva cena/recursos; Undo/Redo e arquivo reaberto preservam o recurso correto. Erros geométricos, preview desatualizada e falta de projeto/publicador recusam aplicação com motivo.
- Mostrar COM real de um composto assimétrico, apoio real e ausência de apoio, autoridade física/personagem/livre; estados prévia e Play distintos.
- Captura exclusiva e cancelamento de gestos; navegação simultânea não altera coordenadas nem outro alvo; foco/Play/troca de alvo fecham estado transitório de maneira definida.
- Registrar medidas com escala e visualização limitada, incluindo ferramenta fechada; verificar telas reais no Android e refinar problemas observados.
- Build, testes focados de invariantes/integração e cenário físico no aparelho; vincular evidências a fontes/APK. Somente depois atualizar U11 para concluído.

## Implementação entregue

Não foram adicionados componentes de catálogo nem propriedades sem consumidor. Foram ampliados os sistemas de Colisor, seleção, recursos, histórico e inspeção existentes.

`editor_collider_topology.h` e `editor_collider_authoring.cpp` extraem a geometria física, soldam vértices, identificam faces conectadas coplanares e mantêm um rascunho separado da cena. O BVH preserva o ordinal original do triângulo; o picking real de faces usa esse ordinal para resolver a face. Não usa a amostra desenhada para selecionar. Edição XYZ e alças locais têm captura exclusiva, histórico do rascunho, cancelamento e validação Jolt antes da publicação.

Aplicar escreve um GLB imutável com hash, importa no registro real, valida a configuração física completa e troca somente a fonte do Colisor objeto + UID, em uma transação de histórico. Outro objeto que use a mesma malha mantém sua fonte. Visual, Body dono e demais componentes permanecem intactos. Undo conserva o recurso necessário a Redo. Fontes ausentes, dados inválidos, orçamento excedido e cena/fonte/projeto desatualizados recusam a publicação com diagnóstico; Colisor desativado ou objeto inativo não contornam a validação de malha côncava em Body dinâmico.

`scene_physics_diagnostic.cpp` lê autoridade de GameWorld e COM/probes do Jolt. Apoio de motor é o hit aceito pelo motor; uma consulta sob a geometria não é apresentada como contato de uma simulação. Character usa posição, normal, Body de apoio e a cápsula **atualmente ativa** no solver, incluindo troca de forma para agachar. Não recebe um COM de Body fictício. A prévia autoral não executa scripts nem simula; é reutilizada até a cena mudar e liberada ao fechar. Em Play a leitura é limitada a 10 Hz e os overlays usam a câmera, projeção e roll reais do jogo, sem alças de autoria.

`editor_collider_authoring_ui.inl` integra duas rotas temporárias ao Inspector existente. Faces/vértices, seleção aditiva, elementos ocultos, coordenadas com teclado numérico, Undo/Redo da prévia e conversão explícita ficam nessa rota. A tela compacta prioriza seleção/coordenadas/aplicação; detalhes técnicos ficam em Mais. Diagnóstico separa visual azul, colisão branca, COM amarelo e apoio verde com filtros independentes. Mais alterna informações essenciais e medidas avançadas. Voltar restaura o Inspector e remove os overlays próprios da ferramenta. Três ícones novos foram integrados ao catálogo/atlas realmente executado; não são imagens conceituais.

## Aceite de todos os requisitos

| Requisito | Evidência final | Resultado |
|---|---|---|
| Inspector, Body/filhos, UID e queries nativas/ABI44/C# anteriores | Aceites anteriores vinculados no roadmap; regressão final de 100 cenários inclui seleção, queries e histórico | Preservado |
| Faces/vértices com coordenadas, alças, seleção/oclusão e geometria real | Cinco cenários U11, captura `41-final-face-selection.png`; `33`–`35` para vértice editado no aparelho | Aceito |
| Publicação, recurso compartilhado, visual/UID/owner, persistência, Undo/Redo | Dois projetos realmente salvos no Android; reabertura nativa, raycast Jolt, comparação exata dos arquivos de Undo/Redo | Aceito |
| Cancelamento, segundo dedo, foco, Play, troca de alvo/projeto e erros | Teste de lifecycle/erro/oclusão; 530/530 frames da revisão final inspecionados; arquivo final igual ao salvo antes dos gestos descartados | Aceito |
| Visual, colisão, COM, apoio e autoridade distintos | Capturas `23`, `27`–`30`; Character real com chão objeto 9, COM do composto assimétrico, autoridade Livre na Camera | Aceito |
| Escala, budgets, cache e ferramenta fechada | 100.000 triângulos e 1/16/64/256 partes em Jolt no host; `37`–`39` com 256 Colisores, medidas e saída da ferramenta no Android | Aceito |
| UI executável mobile, estados e ícones | Capturas reais inspecionadas; rota compacta e gestos testados; atlas atualizado instalado | Aceito |
| Builds e regressão dos consumidores | Host e Release Android compilados; 5/5 cenários focados e 100/100 da regressão GUI/runtime | Aceito |

O [aceite estruturado](../../validacao/ui-universal-2026-10-06/u11-integral/acceptance.json) vincula fontes, hashes, arquivos e capturas. As capturas de splash, lançamento incorreto e revisões rejeitadas permanecem identificadas; não foram usadas como aceite da interface.

## Evidência física e análise de frames

POCO F7 / Android 16, editor em paisagem 2772 × 1280. APK Release instalado e extraído do aparelho: SHA-256 **74cc3895afa18fc7f0bb31ebb65e5a91616e812baf3a0f26711cafa0bf5b7011**, idêntico ao build local.

A face da caixa foi publicada com X=1,25 preservando o visual; Undo voltou exatamente ao arquivo original e Redo ao publicado. A esfera recebeu um vértice Y=1,1 por teclado numérico, foi explicitamente convertida para malha e salva. O arquivo extraído foi reaberto pelo importador/registro nativo: o raycast real encontrou essa ponta em Y=1,0999, objeto 6, Colisor UID 3, Body 2. Restaurar somente esse Colisor ao estado anterior reproduziu exatamente o restante da cena. Undo/Redo físicos também reproduziram os arquivos anteriores/publicados byte a byte.

`gestures-final.mp4`: todos os **295 frames** foram extraídos e vistos em dez pranchas indexadas, sem subamostragem. O frame 140 restaura X=1,25 no CANCEL. A gravação termina durante o segundo arraste; seu release não é presumido. `gestures-release-final.mp4`: todos os **235 frames** foram extraídos e vistos em oito pranchas; o frame 103 mostra o casco Jolt validado após UP, X=3,192, Y=0, Z=-0,5, mantido até o frame 234. O dedo simultâneo sobre Sphere não troca LeftBox/UID1. CSV conserva PTS e hash de cada frame, fase observada e revisão individual; vídeo e pranchas permitem reproduzir a leitura. Diferenças de pixels de ROI sob compressão H.264 são métricas auxiliares, não prova isolada de seleção.

A gravação intermediária de 478 frames também foi vista integralmente, mas é separada das 530 imagens da revisão final. Nenhuma dessas capturas representa benchmark GPU/thermal.

## Medidas e limites explícitos

- 100.000 triângulos, 50.451 vértices, uma face coplanar: construir topologia 465,904 ms, validar 516,898 ms, picking BVH 0,006 ms no host de desenvolvimento. O teste verifica o ordinal/face original, além do hit.
- 256 partes no host: montar solver 2,249 ms, diagnóstico 1,491 ms e gerar contornos 1,766 ms. Desenho limitado a 9.600 segmentos; 256 probes reais mantidos. 1/16/64 partes estão no mesmo log.
- Android, 256 Colisores: captura registra última consulta 5,37 ms, UI + diagnóstico 8,54 ms e **uma** montagem autoral. São amostras desse projeto/aparelho, não percentis ou garantia de FPS. Idle reutiliza a prévia; fechar impede novas montagens/consultas, verificado no host.
- Fonte até 100.000 triângulos; limites maiores recusados explicitamente. Desenho da ferramenta até 800 triângulos/512 pontos e contornos gerais até 9.600 segmentos. Picking, validação e publicação preservam a geometria completa dentro do contrato. Isto não substitui o orçamento amplo de U14.
- Caixa editável por conversão; esfera/cápsula/cilindro mostram aviso de aproximação poligonal e conversão para Mesh. Colisor côncavo estático mantém cavidades; publicar côncavo dinâmico exige a cadeia de decomposição de U04, não um hull silencioso. U11 não se anuncia como editor completo de malha visual/ProBuilder.
- Comparação com o snapshot do bloco anterior: 266/268 fontes protegidas permanecem idênticas, incluindo **210 fontes de renderer/shaders**. As duas alterações são funções de leitura na ponte Jolt. Não houve alteração dos trabalhos de desempenho do usuário nesse grupo.

## Como usar e reproduzir

Selecione um objeto → expanda o **Colisor 3D** → **Geometria** → Faces ou Vértices → toque no elemento → edite XYZ ou arraste a alça → **Aplicar** / **Converter**. **Descartar** fecha sem modificar a cena. No Colisor, Body ou Character, **Diagnóstico** abre a inspeção física. Em Play, **Inspecionar** mantém esse diagnóstico como leitura do solver na câmera do jogo.

Reprodução host: `aether_gui_tests.exe u11_`; regressão: `aether_gui_tests.exe`; projeto físico retido: `aether_gui_preview.exe verify-u11-project <device-face-project> <device-undo.aescene>` e `aether_gui_preview.exe verify-u11-vertex <device-vertex-project> <device-face-project/scenes/editor.aescene>`. Build nativo e Android estão nos logs do aceite. `tools/validation/U11Touch.java` injeta MotionEvents pelo serviço Android; não contorna o roteador da engine. `review-u11-frames.py` extrai cada frame e verifica contagem contra ffprobe.

Entrega no checkout **kacerato/attachsEngine**, branch `codex/gameplay-runtime`; não publicada no GitHub neste bloco. Roadmap original preservado: SHA-256 `61727152c2e8485b5c9f8b59252074d4fda5856d18a5971b865069d8317d6b29`. U11 encerrado; U01–U10 e U12–U14 mantêm seus próprios estados, e `engine_complete` permanece falso.
