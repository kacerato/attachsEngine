# Evidências: transporte de áudio ABI37

Esta entrega usa o backend miniaudio real com saída offline explícita; não há dispositivo de áudio simulado como evidência física.

- Build host: alvo `aether_audio_listener_family_tests`, incluindo `test_runtime_audio.cpp` e `test_script_audio.cpp`.
- Cenário da bridge: Play repetido/EOF, Pause/Resume, Seek em pausa, limites de duração, PCM após comando, fonte desativada, recriação, remoção segura e handle vencido.
- Cenários de família: WAV, listeners, buses, pitch, espaço, lifecycle, importação real, GUID, Undo, salvar/reabrir e limpeza de binding persistido.
- SDK: compilação C# em diretório isolado, roteamento/identidade/erros e layout anexado da ABI; verificações de layout de Input e Fields preservadas.

Resultado final: **6/6 cenários nativos**, **1/1 cenário de transporte gerenciado** e **2/2 verificações adicionais de layout da ABI** passaram. O build final está atualizado; a verificação de contratos gerados também passou. Os logs e `progress.json` registram os resultados. Houve bloqueio inicial de escrita em DLL mapeada, resolvido usando `--artifacts-path`, sem encerrar outro processo.

```powershell
cmake --build build/editor-host --target aether_audio_listener_family_tests --parallel 3
build/editor-host/aether_audio_listener_family_tests.exe
dotnet build tests/Aether.Tests -c Release --artifacts-path build/audio-transport-dotnet
dotnet build/audio-transport-dotnet/bin/Aether.Tests/release/Aether.Tests.dll AudioTransport
dotnet build/audio-transport-dotnet/bin/Aether.Tests/release/Aether.Tests.dll InputInteractions_TypedTransportLayoutRuntimePoliciesAndFailures
dotnet build/audio-transport-dotnet/bin/Aether.Tests/release/Aether.Tests.dll FieldSampleAndAppendedCallbackPreserveAbi
```

[Auditoria, contrato, referências e pendências](../../../planos/AUDIO-TRANSPORTE-ABI37-2026-10-02.md). Build host, SDK e DSP offline não certificam integração CLR/Android, audibilidade, UI física ou performance em dispositivo. Sem commit/push nesta entrega.
