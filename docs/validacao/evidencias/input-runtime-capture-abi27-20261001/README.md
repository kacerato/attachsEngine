# Captura de gameplay — ABI27, 01/10/2026

[Contrato e referência Unity 1.11.2](../../../planos/INPUT-RUNTIME-CAPTURE-2026-10-01.md). [Manifest](manifest.json) registra hashes, contagens e limites.

Host final: captura4/4 e entrada48/48. O teste pela ABI inclui pausa real do aplicativo e cancelamento antes de interromper os quadros; outros cenários exercitam pulso Android de teclado/gamepad entre quadros, release gate, pressão inicial mantida, perfil salvo/importado, mouse, eixo neutro/inversão, tecla negativa, Escape e perda de foco. Áudio5/5; caminhos1/1, física2D pela ABI1/1, grupos pela ABI1/1. C#54/54; sete fixtures compiladas juntas.

Fixture exportada pelo editor e reaberta pelo serializer. O novo fluxo é API de gameplay, sem UI nova declarada; imagens anteriores de autoria não são usadas como evidência de menu de rebind do jogador.

Build Android passou, SDK/atlas comparados byte a byte; instalação e envio da fixture estão no manifest. Aparelho continuou bloqueado, com showing=true/mIsShowing=true: não foram observados logs RUNTIME CAPTURE COMMITTED/PASS, capture física ou arquivo salvo por essa fixture no aparelho. Não se considera essa etapa fisicamente qualificada.
