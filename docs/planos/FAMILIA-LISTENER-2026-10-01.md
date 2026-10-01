# F060 — listener de jogo

O listener existente possui consumidor real em `SceneAudio`, propriedades tipadas, criação pelo catálogo/receitas, persistência e fachada C#. Esta revisão fecha o aceite direcionado; não amplia o mixer ou o formato de clip por associação.

`AudioListener` tem enabled, volume [0,1] e prioridade inteira [0,255]. A seleção percorre objetos ativos do mundo e escolhe a maior prioridade; empate conserva a primeira ordem de travessia. Transform mundial define posição, frente +Z e up +Y normalizados. Movimento atualiza velocidade; trocar o escolhido reinicia o histórico de Doppler. Remover/desativar o escolhido seleciona o próximo elegível. Sem listener, uma voz espacial informa MissingListener, sem inventar a câmera como substituto. Voz plana não exige câmera/listener.

O consumidor usa miniaudio 0.11.23: listener e fonte alimentam o DSP real. A saída offline nos cenários é explicitamente selecionada para medir PCM; ela não é evidência de som ouvido no aparelho. Estado autoral fica no componente, caches e vozes pertencem a SceneAudio, que libera vozes/dispositivo no reset/lifecycle. Inspector e SDK escrevem o modelo validado; scene archive salva/reabre os valores.

NÃO IREI SER SIMPLISTA NO DESIGN.

A captura executável 853×394 mostra a superfície contextual existente, com ativo, volume e prioridade legíveis. O objeto de escuta tem ícone próprio já integrado ao atlas. Nenhum novo conceito visual foi criado neste aceite. A captura é do host; não é frame Vulkan Android.

Referências: [Unity 6000.0 AudioListener](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AudioListener.html) separa escuta espacial da fonte; [Godot 4.5 AudioListener3D](https://docs.godotengine.org/en/4.5/classes/class_audiolistener3d.html) permite escutar fora da câmera. A Astra extrai essa independência e define seleção por prioridade própria; não promete a política de listener único da Unity ou make_current da Godot.

O alvo `aether_audio_listener_family_tests` passou quatro cenários. Um novo cenário integrado mede áudio audível/silêncio/atenuação ao alterar prioridade, enabled, posição e destruir listeners; os anteriores cobrem decode real, lifecycle, import/binding, histórico, save/reopen e Play. Logs, captura e conferência do APK ficam em `docs/validacao/evidencias/families-audio-listener-20261001/`. Não houve instalação ou execução física desta revisão. F058, F059 e F061 conservam suas próprias lacunas de streaming, prioridade de fontes, blend, sends, effects e snapshots.
