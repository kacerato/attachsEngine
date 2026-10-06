# U11 — edição de dimensões e pose do Collider no viewport

## Contrato deste bloco

Fechar alças de autoria para os tipos de Collider já executáveis: caixa, esfera, cápsula e cilindro editam dimensões; primitivas e malhas com pose local habilitada editam centro e rotação. Malhas não ganham dimensões fictícias nem vértices editáveis por esse caminho. Os dados e limites continuam sendo os mesmos do Inspector, serializer e solver. Transform visual, outras instâncias de Collider e Body proprietário são preservados.

Cadeia: seleção objeto + UID → forma e pose existentes → matriz real da hierarquia → alças desenhadas e roteadas → captura exclusiva → estado inicial congelado → setter tipado → transação de histórico → Inspector/contorno → arquivo nativo → Jolt. Cancelamento de ponteiro ou lifecycle desfaz a transação aberta e preserva o Redo anterior.

## Referências e diferenças deliberadas

- Unity **6000.0**, [Box Collider](https://docs.unity3d.com/6000.0/Documentation/Manual/class-BoxCollider.html) e [Capsule Collider](https://docs.unity3d.com/6000.0/Documentation/Manual/class-CapsuleCollider.html): edição no Scene View complementa campos locais de Center/Size/Radius/Height. A altura autoral desta engine é a meia altura da parte cilíndrica; a alça da cápsula fica no ápice, mas alterar altura não altera raio.
- Godot **4.5-stable**, [CollisionShape3DGizmoPlugin](https://github.com/godotengine/godot/blob/4.5-stable/editor/scene/3d/gizmos/physics/collision_shape_3d_gizmo_plugin.cpp) e [Gizmo3DHelper](https://github.com/godotengine/godot/blob/4.5-stable/editor/scene/3d/gizmos/gizmo_3d_helper.cpp): conversão de raio para o referencial local, handles específicos por forma, estado inicial e commit/cancel com Undo. Implementação própria sobre os consumidores já existentes, sem copiar código da Godot. O resize deste bloco é simétrico em torno do centro, explicitamente indicado na interface; não desloca o centro para manter uma face fixa.
- Godot **4.5**, [EditorNode3DGizmoPlugin](https://docs.godotengine.org/en/4.5/classes/class_editornode3dgizmoplugin.html): desenho, alteração e commit/cancel são partes distintas da autoria. As mesmas propriedades devem mudar por Inspector e alça.
- Uso visual: [análise anterior de Colliders](ANALISE-VIDEO-COLLIDERS-2026-10-05.md) registra 100 quadros consecutivos de um trecho e amostras de contexto de vídeo oficial. É evidência histórica com limites próprios, não prova das novas alças. Nenhum vídeo novo será atribuído a este bloco sem observação quadro a quadro.

Skills instaladas foram conferidas. A skill avoid-ai-design trata de HTML/React, não deste viewport nativo; não foi aplicada como se fosse especializada em gizmos 3D.

## Interação e design

NÃO IREI SER SIMPLISTA NO DESIGN.

O viewport é a superfície de edição. Abrir o Collider e escolher **Escala** apresenta grips de dimensão; **Mover** apresenta eixos para seu centro local; **Girar** apresenta anéis dos canais Euler locais. **Selecionar** permanece seleção de forma. Ao recolher/sair do Collider, ferramentas voltam à transformação do objeto. A instrução contextual identifica Collider/UID e modo; o Inspector continua dando os valores exatos. Overlays OFF retorna ao comportamento de objeto.

Essa troca contextual reutiliza as ferramentas e ícones já executáveis para os mesmos conceitos de posição, rotação e tamanho. Não cria conceito de recurso ou catálogo novo de ícones. Grips têm alvo generoso de toque e desenho pequeno; eixos usam cores funcionais; anéis ocupam apenas o espaço da forma selecionada. Não existe painel permanente adicional. Desenho e roteamento respeitam viewport, câmera, olho, trava e oclusão por geometria real, incluindo a exceção do visual próprio. Eixos/anéis praticamente alinhados ao raio não oferecem alças instáveis.

Estado inicial congelado impede que o arraste persiga a alça reprojetada. Matrizes preservam escala não uniforme e shear dos ancestrais; a matemática do raio também é verificada em frame afim refletido, sem burlar o contrato atual de Transform que rejeita escala autoral negativa. Rotação usa anéis no referencial correspondente à ordem Rz*Ry*Rx; inversa afim resolve o raio e o ângulo cruza ±π sem saltos. Outro dedo não troca ferramenta/foco/câmera durante o gesto. A alteração é reversível em um Undo; Cancel e interrupção de lifecycle restauram a cena.

## Aceite obrigatório

1. Todas as primitivas: raio/extensões/altura reais, limites válidos, pose local e separação da malha. Malha sem pose opt-in não mostra alças ignoradas pelo runtime.
2. Frame afim refletido na matemática do raio, hierarquia autoral com escala não uniforme/shear, perspectiva/ortográfica, rejeição de matriz singular e ângulo quase axial. Isso não habilita escala autoral negativa.
3. UI real: alcançar grip, arrastar em vários frames, manter captura fora do alvo inicial, alterar um Collider e um único Undo, conservar sibling/visual/Body.
4. Centro e rotação locais; cruzamento do ângulo; Cancel/segundo dedo/lifecycle; arquivo e Redo intactos ao cancelar.
5. Arquivo nativo roundtrip e consulta Jolt com dimensão e pose novas, identidade Body/Collider, Undo restaurando resultado anterior.
6. Eye/trava/oclusão, Select e overlays OFF sem alças invisíveis. Captura compacta e captura física com legibilidade e viewport úteis.
7. Android final identificado: dimensões → pose → Save → Undo/Redo → reabertura fria, valores e malha preservados.

## Estado e evidência

**Bloco fechado em 2026-10-06.** [Aceite verificável](../../validacao/ui-universal-2026-10-06/collider-handles/acceptance.json), [registro das 19 capturas físicas](../../validacao/ui-universal-2026-10-06/collider-handles/device-reviewed.json) e [verificador dos registros](../../validacao/ui-universal-2026-10-06/collider-handles/verify_evidence.py).

Host final: build concluído, **3/3 cenários focados e 95/95 cenários nativos de UI/autoria**. Geometria tipada das quatro primitivas, pose local, mesh opt-in, hierarquia com escala não uniforme/shear, frame afim refletido, perspectiva/ortográfica, singularidade, ray quase axial, seam de rotação, captura exclusiva/segundo dedo, Cancel/lifecycle, Undo/Redo, serializer e consultas ao Jolt reais foram aceitos. Não foi executada a suíte inteira da engine. A captura de 800×400 expôs alvos sobrepostos; a revisão final separa as áreas de 32×32 pixels lógicos ao longo do eixo projetado, com linha até o ponto real. O teste roteia e arrasta o grip deslocado, altera somente seu eixo e restaura arquivo em um Undo. Oclusão é avaliada no ponto 3D da forma; a linha e o grip deslocado são indicação autoral em tela.

Android Release gerado/instalado, base.apk confirmado: **872e6a76b2cfb2b32c9a8393d5c31d22faf8891d6aab2c0c26f36d46f9a7b888**. No Xiaomi 25053PC47G, 19 capturas finais foram examinadas individualmente: caixa dimensão → centro → rotação → Save → três Undo, um por gesto → três Redo → abertura fria → Forma/Pose → overlays OFF; cilindro altura/raio, esfera raio e cápsula altura/raio foram arrastados, salvos e desfeitos separadamente. Os arquivos confirmam oito alterações isoladas de propriedade, sem mudar Transform visual, Mesh, sibling, Body ou UID. Caixa/cilindro usam UID1; esfera/cápsula usam seu UID3 real, sem pressupor UID pela ordem visual.

Arquivo original/restaurado: **3af459d0959b2de226bea3f962ad4ab933451e4fa598b134a06e29744b6ad172**. Combinado/Redo/reabertura/estado final: **19c63157d697787cfecfc4dadf56d193503f10cbf5c5f8e550e6e33118e5e2b6**. A caixa conserva meia extensão X 1.29923224, centro X .504778028 e rotação Z 35.5909843 graus; todos os outros campos e entidades permanecem iguais. Cada Undo intermediário é byte-idêntico ao estado anterior correspondente. Efeito de dimensão/centro/rotação no solver foi validado no host; não houve novo Play físico, vídeo ou benchmark nesta revisão.

**268 fontes de desempenho/runtime protegidas, zero alterações** comparadas ao início deste bloco. Esse resultado preserva as otimizações existentes; não mede novos FPS ou thermal. Não foram criados tipos, propriedades de componente, ABI ou serializer novos; as alças usam propriedades já consumidas e persistidas. Zero componentes novos; dimensões de quatro formas existentes e pose das primitivas/malha com opt-in constituem o recorte funcional.

Plano original preservado byte a byte, contagem original 140 planejados/20 parciais/4 completos e `engine_complete=false`; extensão própria em PROGRESSO.json. U11 inteiro e o roadmap universal continuam abertos: faces/vértices, diagnóstico COM/apoio/autoridade e distribuição/orçamento em escala exigem blocos próprios. Alterações locais em `codex/gameplay-runtime`, remote confirmado `https://github.com/kacerato/attachsEngine.git`, sem commit/push nesta continuação. Capturas e logs anteriores à revisão final permanecem históricos e não são atribuídos ao APK aceito.
