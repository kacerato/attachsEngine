# Física 2D e áudio — continuação de 2026-09-30

Pacote de aceite editável preparado com EditorSession, componentes tipados, histórico, serializeEditorDocument e importação WAV transacional. Nenhum tipo de cena novo: os componentes existentes continuam com seus consumidores reais. AudioVoice acrescenta observação do backend pela ABI23; pedido de reprodução, voz observada, saída autorizada e audibilidade são evidências distintas.

O projeto gerado está em `tests/fixtures/runtime-families/project/Families-ADB-20260930/`. Física: corpo dinâmico, parede estática, consultas com identidade de colisor, velocidade, pose e contato. Áudio: WAV autoral de seis segundos, fonte e ouvinte, comandos Play/Pause/Resume/Stop e cursor nativo. Os dois Behaviors não criam dependências, não inventam recursos e emitem WAIT/FAIL em vez de PASS quando o cenário não satisfaz os critérios.

## Evidência e limites

Resultados e hashes no [manifesto](manifest.json). Build host `aether_tests`/`aether_ui_preview` passou. SDK e os dois probes C# compilaram com zero avisos e erros. Filtros nativos: audio_ **5/5** (inclui regressão de gizmos), physics2d **8/8**, path_script_abi **1/1**, play_scene_abi_v20 **1/1** e play_active_self **1/1**. O caso AudioVoice usa PCM e consumidor SceneAudio reais, com captura da ABI nativa; não executa o CLR do jogo.

O primeiro caso de remoção falhou porque a fixture ignorava a fila estrutural. [Resultado inicial](audio-before-safe-point.txt) preservado. O teste passou após verificar componente válido enquanto pendente, chamar flush no ponto seguro e exigir ComponentMissing após a remoção, mesmo com diagnóstico retido. Não foi enfraquecida a validação do runtime.

APK final: **264336992 bytes**, SHA-256 **066FAD8868A544E8E8D68C66FD57E9720EB542FBEFF8CECE10A53D9881E4E657**. `:app:assembleDebug` passou em **2m23s**, 40 tarefas, oito executadas. A DLL Astra.Scripting dentro do ZIP coincide byte a byte com o SDK Release compilado, SHA-256 **83494EFA2395D283D9CACC1AA1BC96918ABB9513BCA1CA22FEE2B2A748FD80C3**.

O exportador final passou leitura/deserialize/serialize exato da cena que publicou em uma importação independente. Como cada importação gera seu GUID, a comparação com o pacote entregue normalizou apenas GUID e caminho físico do descritor; todos os demais bytes de cena/registro/scripts/WAV coincidem. Recusa de saída não vazia passou, conservando os hashes dos seis arquivos. Nenhuma reexecução sobrescreveu o projeto entregue.

Capturas `audio-picker-search-*.png` usam a UI nativa executável com rasterização de host e um clipe realmente importado. Elas verificam layout e o estado de busca vazia; o fundo do viewport não é um frame Vulkan de jogo. **853×394:** zero instâncias descartadas, glifos ausentes ou recortes. **1200×700:** zero descartadas/glifos ausentes e três recortes; captura mostra marcador do gizmo na borda do viewport, sem campo ou ação do Inspector cortados. Não se atribui cada primitiva pelo PNG. Busca, voltar, +WAV, limpar clipe e paginação estão íntegros e legíveis em ambas.

ADB estava sem transportes. O anúncio mDNS `192.168.30.69:39229` recusou conexão (10061); foi solicitado o endereço atual da depuração sem fio. Nenhuma instalação, Play C# no aparelho, importação SAF, captura Android, audibilidade ou qualificação de foco/thermal foi obtida nesta continuação. A autorização ADB permanece; falta conexão física, não aprovação adicional.

A qualificação Android anterior de Path permanece em `../p15a-android-20260930/`; não é evidência para estes cenários. A suíte integral não é necessária para o recorte e não foi executada. O conjunto completo do atlas, lightmap/GI/probes e as demais famílias do roadmap continuam pendentes.

## Referências e workflow

[Contrato AudioVoice/ABI23](../../../planos/AUDIO-API-ABI-2026-09-30.md), [áudio e recursos](../../../planos/AUDIO-2026-09-30.md), [foco/SAF Android](../../../planos/ANDROID-AUDIO-FOCUS-WAV-2026-09-30.md), [física 2D](../../../planos/PHYSICS2D-2026-09-30.md) e [design das famílias](../../../planos/DESIGN-AUDIO-PHYSICS2D-TWEEN-2026-09-30.md) registram contratos, versões, limites e referências oficiais Unity 6000.0/Godot 4.5. O workflow comunitário de CollisionShape2D foi conferido pela transcrição de Godot Tutorials, sem observação de frames nesta rodada.

NÃO IREI SER SIMPLISTA NO DESIGN.

A mudança de UI preserva a superfície contextual existente, viewport dominante e identidade Astra. Não cria novo painel nem controles sem efeito. O estado vazio agora diferencia ausência de WAV de filtro sem resultados. Nenhum ícone ou conceito novo foi necessário para esse ajuste textual.
