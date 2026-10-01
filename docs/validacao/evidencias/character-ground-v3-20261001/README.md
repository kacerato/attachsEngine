# Character3 — chão e gravidade

`character_ground`4/4: degrau real Jolt; descida de borda real Jolt; criação/Inspector/IME/Undo/Redo/arquivo; migração/arquivo/gravidade ao vivo/preservação de velocidade e gizmo. Degrau30cm: desligado X1.187, ligado X5.946, olho máximo1.701m. Snap desligado mantém olho1.648m sobre a borda; ligado produz apoio no piso inferior, olho1.400m. São poses de cápsula simuladas no host, não vídeo de aparelho.

Regressões: CharacterMotor2/2, comandos de script/30–60Hz1/1, numeric tween4/4, conexão física3D4/4 e2D3/3. Schema13/13 após regeneração da fachada; atlas5/5. C#54/54 com quatorze fixtures, incluindo CharacterGroundProbe que usa StepHeight/FloorSnapLength/Gravity gerados.

`host-ground.png`, `host-gravity.png` e `host-landscape.png`: UI executável rasterizada. Mostram a categoria Chão e a gravidade em Locomoção. Cápsula e segmentos de medida vêm do ComponentVisualProvider real; a ferramenta usa câmera enquadrada no personagem para inspecionar os limites. Não constituem debug de contato nem prova do renderer Vulkan Android.

Projeto independente CharacterGround-20261001: piso/degrau com Body+Collider reais; duas cápsulas com subida0/0.4m e scripts editáveis. TimeProbe anterior está presente e devolve o relógio a1 após seu próprio aceite; a comparação usa tempo simulado, com2s de assentamento antes dos3s de movimento. Cada personagem deve registrar CHARACTER GROUND PASS: blocked/climbed. Exportador salva e reabre a cena antes de publicar o projeto.

`manifest.json` registra a instalação, hashes e igualdade byte a byte de SDK/atlas empacotados, fixture enviada e estado do keyguard. `deviceCharacterGroundPass=false` distingue a instalação do aceite físico. Não foi simulada entrada para ultrapassar a tela de bloqueio.

Contagem34 schemas/33 fachadas/57 receitas/230 ícones; três novos campos no personagem existente. ABI30/cena16/prefab3/Character3. Não encerra P07 ou o plano completo. [Contrato e referências](../../../planos/CHARACTER-GROUND-2026-10-01.md).
