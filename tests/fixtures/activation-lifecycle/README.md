# Aceite de ativação no aparelho

Em um projeto vazio separado, copie `ActivationProbe.cs` para a raiz e os dois arquivos de `scenes/` para a pasta de cenas. O manifesto do projeto deve apontar `mainScene` para `scenes/main.ascene` e `editorScene` para `scenes/editor.aescene`. Não substitua a cena de um projeto de trabalho.

Abra o projeto, aguarde a publicação do código e entre em Play. A cena contém Driver ativo, Parent inativo e Child localmente ativo dentro de Parent. O controlador verifica o estado pelos métodos reais da API e conta os callbacks do filho.

Após sete segundos de simulação, o HUD/Console deve mostrar:

```text
O1A PASS: Awake=1 Start=1 Enable=3 Disable=2; mesma instancia
```

Uma invariável violada gera `O1A FAIL` com o motivo no Console. Pare o Play, salve e reabra: Parent deve continuar desligado e Child ligado localmente. Repita o Play; o resultado deve ser o mesmo. A cena não contém geometria: ela valida estado, identidade, persistência e callbacks, sem alegar uma comparação de imagem ou colisão.
