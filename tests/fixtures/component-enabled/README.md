# Aceite da habilitação de componentes

Em um projeto isolado sem recursos importados, copie `EnabledProbe.cs` para a raiz
e `scenes/` para a pasta de cenas. Compile/aplique o código e entre em Play.
A cena contém Driver, Worker, um corpo dinâmico com colisor e Camera com Olhar,
Animação e LOD inicialmente desligados.

Em seis Updates, a fixture confere propriedades geradas pela ABI, sincronização
Behavior/Component, suspensão e retomada de callbacks, atividade do objeto,
raycast com o último colisor desligado/religado e autodesativação. O resultado
esperado é `ENABLED PASS`; falhas lançam `ENABLED FAIL` e são isoladas pelo host.
Stop deve recuperar os valores autorados.

Para autoria, selecione Camera, ligue Olhar/Animação/LOD pelos cabeçalhos, salve,
desfaça/refaça LOD e confira o estado. Desligue os três, salve, encerre/reabra
o projeto e repita Play/Stop. A fixture exige os três inicialmente desligados.

`scenes/editor.aescene` é reproduzível com:

```
build/editor-host/aether_tests.exe --write-enabled-fixture tests/fixtures/component-enabled/scenes/editor.aescene
```

Validada em 28/09/2026 no dispositivo 25053PC47G, projeto O1Enabled0928, com dois
resultados PASS, incluindo após reabertura. A animação real e a seleção/fade de
LOD são protegidos pelos testes nativos; esta fixture não contém malha animada.
Detalhes e limites: [relatório](../../../docs/planos/O1A-HABILITACAO-2026-09-28.md).
