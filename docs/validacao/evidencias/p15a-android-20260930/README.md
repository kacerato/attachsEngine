# P15a — aceite no Android, 30/09/2026

APK final instalado e executado no Xiaomi `25053PC47G`, Android API 36, landscape 2772×1280. Projeto independente **P15a-ADB-20260930**, criado pela UI do shell, permanece no aparelho. Os projetos anteriores foram preservados. Instalação por `install -r`, sem limpar dados ou logs. As duas conexões wireless identificavam o mesmo serial; somente transport 1 foi usado.

## Produto e correções

`app-debug.apk`: 264336812 bytes, SHA-256 `02A4C8C0F5E0C8491080FBAEA9B5912F68427F12FC6E4306A9E78C4EA8B8FF06`. O `sha256sum` do `base.apk` instalado confirmou o mesmo hash.

- ScenePaths guarda também tentativas de bake inválidas para a mesma geometria/matriz; só tenta novamente quando a entrada muda. Não usa uma curva válida antiga para mascarar a falha.
- Recusa de transformação por descendente controlado pela física agora informa autoridade física.
- Destaque de ponto usa objeto + instância + ID, inclusive com Inspector travado; IDs iguais em caminhos diferentes não destacam vários pontos.
- Desenho e seleção dos marcadores respeitam ocultação/atividade de ancestrais, layers e bloqueios de seleção. O handler revalida um hit antigo.

Referências: [Godot 4.5 PathFollow3D](https://docs.godotengine.org/en/4.5/classes/class_pathfollow3d.html) e [source oficial 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/scene/3d/path_3d.cpp). O princípio de mover filhos por progresso é usado no cenário com cubo; a Astra mantém referência explícita ao Path, distância mundial e autoridade física próprias. Os limites do pacote continuam nos [dados P15a](../../../planos/P15A-CAMINHOS-CURVE3D-2026-09-30.md).

## Cenário executado

Criar projeto vazio → Criar/Gameplay/Caminho Bézier → selecionar Path → Criar/Seguidor de caminho → adicionar PathProbe pelo catálogo, prévia e confirmação → Play. O arquivo C# foi copiado para Scripts e compilado pelo compilador real do Android ao abrir o projeto. O probe não cria objetos nem edita a curva. Consultou a ABI 22 e verificou identidades, comprimento/amostragem mundial, avanço de progresso **e pose**, Stop mantendo ambos e Restart voltando a avançar.

Os logs preservam três PASS: 19:32:28.305, 19:35:03.303 e 19:43:09.789 após reabrir. Também preservam o WAIT real quando a receita Cubo adicionou um corpo físico descendente. Isso bloqueou o seguidor; Collider e Corpo físico foram removidos pelos respectivos menus/confirmadores do editor. Com o cubo apenas visual, o cenário voltou a passar. WAIT não foi convertido em sucesso.

Edição real: ponto #3, X de 3 para −5, comprimento local de 12.455638 para 8.455638; Undo voltou a 3 e Redo a −5. Tangente de saída X mudou de 0 para 1. Save → encerrar/reabrir → Inspector mostrou o mesmo ponto, valor e tangente. A cena salva, reaberta e após sair de Play é byte a byte igual: SHA-256 `DFDAF76A63E0FBE0AC9FC6D3A4FF559FFEE1CEE5A9F63219CF9A70880DB9DE40`. O comprimento mundial consultado depois da edição foi 11.846375819069051, diferente do local pela escala authorada.

Na aba Execução em Play, Restart reduziu o progresso final e voltou a mover o cubo; Stop manteve 0.882187 unidades mundiais. Capturas 45 e 46, separadas por vários segundos, têm bytes/hashes idênticos. O vídeo é uma captura direta do aparelho, sem composição ou simulação.

## Evidências principais

| Arquivo | O que demonstra |
|---|---|
| 05-editor.png, 08-path-created.png | Projeto vazio e criação inicial de Path com três pontos. |
| 16-follower-created.png, 17-follower-inspector.png | Receita, associação ao Path e propriedades reais. |
| 20-script-result.png, 21-script-preview.png | C# compilado no catálogo e confirmação de associação. |
| path-probe.log, path-probe-mesh.log, path-probe-reopened.log | PASS e WAIT, com PID, timestamps e valores reais. |
| 32-points.png, 35-point-value.png, 36-point-undo.png | Identidade do ponto, mudança numérica e Undo. |
| 37-tangents.png, 39-tangent-edited.png | Tangentes antes/depois. |
| 40-wide-path.png, 41-reopened-points.png | Zoom e reabertura da cena editável. |
| 43-runtime-commands.png, 44-ui-restart.png, 45-ui-stop.png, 46-ui-stop-held.png | Comandos e estado do consumidor em Play. |
| path-motion.mp4 | Movimento e parada do cubo filho durante a execução. |
| saved.aescene, reopened.aescene, after-play.aescene, project.json | Dados extraídos do projeto real. |
| manifest.json | Hashes do APK, cenas e artefatos. |

O diário mantém tentativas intermediárias. 02/03 registram a primeira criação cancelada; 09 não abriu tangentes; 11 não abriu um campo numérico; 12 é uma tela de sistema preta, sem prova de crash. Não são evidência de aceite. 33/38 capturam a animação inicial do IME; 34 mostra o teclado estabilizado e 35 o commit. Nenhuma dessas tentativas foi apagada para ocultar o resultado.

## Validação e limites

Build host de aether_tests/aether_ui_preview passou. Filtros focados: paths_ 5/5, path_ 7/7, mesh_collider_visual_uses 1/1; 13 casos distintos nessa rodada, incluindo um caso legado de proteção de caminhos de arquivos. Build Android passou em 1m14s. A varredura retida em error-scan.log não encontrou FATAL/ANR/PATH FAIL/erro CS para o PID final no buffer disponível; não é uma certificação de ausência de erros no aplicativo inteiro.

NÃO IREI SER SIMPLISTA NO DESIGN.

Revisão independente das capturas reais não encontrou bloqueio no Inspector, pontos, tangentes, IME estabilizado ou comandos Play. O viewport continua dominante e o zoom permite reenquadrar a curva. Dívida pequena: a barra numérica diz apenas “Valor”. Um aparelho em paisagem não comprova portrait, frota Android, performance/thermal ou muitas curvas. Gizmos não oferecem drag direto das alças. Não foram qualificados nesta rodada áudio/audibilidade, física 2D, GI/bake/probes nem todos os tipos do atlas; as propriedades restantes de PathFollow continuam com a evidência host previamente registrada.
