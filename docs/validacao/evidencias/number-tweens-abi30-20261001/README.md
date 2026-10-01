# Numeric property tweens — ABI30

Host: `number_tween` 4/4 — ABI/Start/controles/layouts/teardown; Light/Camera reais e autoria; RGB/easing/ortográfica/inatividade/habilitação/elegibilidade; concorrência/remoção/sessão/capacidade/liberação e hot path. O teste de hot path percorre 1000 frames com 256 estados retidos e um ativo; tempo está no log e não caracteriza desempenho Android.

Regressões: conexão de tween3/3, controles de tween2/2, controles de timer4/4, tempo5/5, schema13/13 e atlas5/5. Managed54/54, com treze fixtures compiladas e layouts24/32 bytes. O teste ABI usa FakeRuntime para dirigir os callbacks nativos; a fixture C# compilada não prova execução CLR no aparelho.

Capturas reais do executável de UI host: `host-light.png` e `host-camera.png` mostram os componentes após avance nativo0.25s — intensidade2 e lente70°, respectivamente; `host-landscape.png` verifica espaço/legibilidade. Os valores vêm do scheduler, não são texto de diagnóstico inventado. O preview rasteriza a UI; não é captura do renderer Vulkan ou prova de alteração visual da iluminação na GPU.

`manifest.json`, quando produzido após build/install, registra APK, SDK/atlas conferidos byte a byte, fixture enviada e estado do keyguard. `deviceNumberTweenPass=false` conserva o limite de evidência: instalação e envio não são aceite físico. Nenhum evento de desbloqueio foi simulado. Projeto NumberTweens-20261001 contém script editável e cena salva/reaberta pelo exportador real.

Não foi criada UI autoral nova neste pacote. O Inspector existente reflete valores do componente em Play; as capturas não qualificam toda a UX mobile do editor. [Contrato, referência e limites](../../../planos/NUMBER-TWEENS-2026-10-01.md).
