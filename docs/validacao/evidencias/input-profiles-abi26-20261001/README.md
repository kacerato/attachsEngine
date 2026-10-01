# Perfis de entrada — ABI26, 01/10/2026

[Contrato e referência Unity 1.11.2](../../../planos/INPUT-PROFILES-2026-10-01.md).

Host: perfis2/2; entrada44/44; layout da ABI1/1; física2D8/8; grupos8/8; caminhos1/1; áudio1/1; API C# gerada1/1. Os arquivos script_paths-tests.txt, script_audio-tests.txt e generated_component-tests.txt são tentativas de filtros que selecionaram zero testes; não contam como validação. Os filtros corrigidos estão nos arquivos path_script_abi-tests.txt, audio_script-tests.txt e generated_csharp-tests.txt.

C#: 53 testes Astra passaram antes da inclusão do cenário de arquivo; seis fixtures compilaram juntas; cenário direcionado de arquivo passou, incluindo layout de InputBindingValue e preservação do arquivo anterior. O duplo deste último testa exclusivamente a fronteira de filesystem; a interpretação do perfil e seu consumidor são exercitados pelos testes nativos reais.

Fixture independente exportada pelo editor e reaberta pelo serializer. Nenhuma UI nova foi declarada: API de jogo e persistência estão separadas da captura autoral e do futuro menu de gameplay.

Build Android passou e APK instalado com Success; SDK/atlas conferidos byte a byte e fixture enviada. [Manifest com hashes e contagens](manifest.json). Rodada C# final: 54/54, incluindo o cenário de arquivo. Android permanece pendente: aparelho com keyguard seguro, sem PROFILE PASS nem PROFILE INPUT observados. Teste host e fixture compilável não substituem prova de execução C# no aparelho.
