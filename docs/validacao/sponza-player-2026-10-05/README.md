# Sponza: autoria e aceite, 2026-10-05

## Contagem e mudança autoral

Cena original: **3.738.920 triângulos visuais instanciados**. Cena com cápsula visual do player: **3.739.944**. As 219 malhas derivadas de colisão não são instâncias visuais; somar toda a biblioteca de recursos produziria outra contagem. A visibilidade varia com a câmera.

O erro de Play vinha do MeshCollider do grupo `NewSponza_Main_glTF_003`, acompanhado por MeshRenderer sem referência de malha. Os filhos possuem as malhas reais. O patch remove o corpo/colisor/renderizador vazios do grupo e distribui a colisão estática pela arquitetura, sem modificar as malhas visuais. Usa 70 corpos, 227 formas e 393.416 triângulos físicos. Há colisão para o primeiro e segundo pisos; detalhes do terceiro piso, decals e luminárias ficam fora desse recorte.

Player Character, cápsula visual, câmera filha com CameraLook e canvas persistente com joystick/área de olhar/salto usam os consumidores nativos existentes. Iluminação: luz existente convertida em sol, seis luzes pontuais nas luminárias e ajuste do Environment. Configuração gráfica do projeto preservada. Arquivos autorais e instruções de reprodução: `games/sponza-player/`.

## Aceite no host

`author-acceptance.log` registra carregamento das fontes reais, Jolt, SceneGui, caminhada de 5,70001 m, salto de 1,06995 m, alteração de yaw, Stop preservando autoria e round-trip pelo serializer nativo. A ferramenta recusa publicar a saída se esse cenário falhar. Não é uma execução da suíte inteira, nem prova visual de Android.

## Aceite inicial no aparelho

POCO F7/onyx, projeto Sponza existente, APK Release anterior `2EC3BE84C613DB18D5CD28BDF9FCB5F3E19BD441CDA450A5C244D8C9395D3ED3`. Play abriu sem o erro de colisor. As capturas iniciais comprovaram controles e iluminação; a primeira iluminação azul foi refinada após inspeção real.

Antes da correção adicional do renderer: editor estático 117,95–119,94 FPS internos, 600/600 frames reutilizados nas janelas finais. Play: SurfaceFlinger apresentou 48,766 FPS em 27,724 s contínuos, 1.353 frames, mínimo móvel de 47 FPS (`play-surface-summary.json`). **120 FPS no editor não significa 120 FPS em Play.** Play não reutiliza frames estáticos e ainda é limitado pelo trabalho de renderização da Sponza.

O aparelho estava carregando durante essa coleta. Não é comparação de potência ou endurance térmico. O aceite anterior de economia em editor estático permanece um experimento distinto, documentado em `../performance-poco-f7-2026-10-05/`.

## Regressão encontrada durante pressão térmica

`thermal-before.png` e `play-before-fix.log`: ao entrar em pressão severa, a escala máxima caiu de 1 para 0,75. A textura intermediária continuava alocada no tamanho anterior, mas `renderTargetWidth/Height` passaram a reportar o tamanho da política ativa. A reconstrução lia as margens não desenhadas, mostrando faixa azul e geometria repetida.

Correção em `native/platform/android/instanced_renderer.h`: extensão de attachment usa a política da criação dos recursos; extensão desenhada usa o controlador dinâmico, limitada à alocação. O pós-processamento calcula sua razão de amostragem usando essas duas extensões. Não muda os dados autorais, a proteção térmica ou o orçamento dinâmico.

Referência: [Unity 6000.3: Introduction to Dynamic Resolution](https://docs.unity.com/en-us/engine/6000.3/manual/cameras/resolution-scale/dynamic-resolution/introduction), que distingue a alocação persistente da região usada dentro do render target.

## Build e aceite da correção

Build Android `:app:assembleRelease --offline --max-workers=4` aprovado. APK atualizado no aparelho por `adb install -r`, SHA-256 **69074805E5BC0907CA197FDBDA7FEC6D7AF51688CE35D3C07CC07658966F1181**. Assinatura verificada, mesmo certificado de desenvolvimento: `9dd308380ab1606fa4e328a1d62fb9edda5abdf6cf7e5696bc8ccbf7c83bc66e`. O workspace contém outras mudanças anteriores; não se trata de uma revisão Git limpa nem da validação de todos esses outros sistemas.

O teste elevou temporariamente o status informado pelo Android a 3 via `cmd thermalservice override-status 3`; não desligou a proteção térmica. O bloco `finally` executou `reset`, e `IsStatusOverride: false` foi conferido. Esse procedimento testa a transição funcional; não comprova economia térmica. O headroom medido também ultrapassou 1 e a pressão física permaneceu severa depois do reset.

`thermal-after.png`: viewport completo em pressão severa, escala 0,75 e desenho 960×2072 dentro do recurso original 1280×2772. `move.png`, `look.png` e `jump.png`: controles funcionando na mesma condição, sem faixa azul ou repetição. `editor.png`: Stop retornou ao editor, console com Problemas 0. O log mostra 600/600 frames reutilizados após Stop.

Play no APK corrigido: janelas iniciais de 50,86–52,26 FPS internos; janela que inclui movimento/transição de 48,86 e vista seguinte de 55,81 FPS. São vistas e condições diferentes, não um A/B. Nenhum frame estático reutilizado no Play. Após Stop, sob pressão severa, o editor foi limitado a aproximadamente 60 FPS e manteve reutilização 600/600. O resultado de aproximadamente 120 FPS do editor anterior à pressão severa está registrado separadamente; não afirmar 120 FPS sustentados nessa condição aquecida. O próximo pacote do roadmap continua sendo reduzir o custo real da GPU no Play/navegação.

Hashes conferidos no aparelho após Play/Stop: cena `10F98AC40F3F4B5CB02CFD960059498BB57FBF8828F9F3BB8AA719961ED468BC`; configuração gráfica `0A0E76B1456C0D2C36C7B944078073EC93810C05A4EC819A5176AA9588784928`, igual à original. Fontes e texturas preservadas, backup integral anterior fora do versionamento. Evidência de build e hashes em `manifest.json` e `android-build.log`.
