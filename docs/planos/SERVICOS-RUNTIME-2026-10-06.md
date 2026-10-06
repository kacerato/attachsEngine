# Bloco C — serviços de runtime para scripts

Branch `claude/api-componentes`. Terceiro bloco do [roadmap de API e componentes](ROADMAP-API-COMPONENTES-2026-10-06.md). Todas as funções novas entram como **famílias** da ABI42 ([bloco A](API-METODOS-EVENTOS-ABI42-2026-10-06.md)); o núcleo `ScriptSceneAccess` não mudou de versão.

## O que passou a ser possível

| API C# (em `Behavior`) | Família | Referência Unity 6000.0 |
|---|---|---|
| `View.Width/Height/Dpi`, `View.TryGetSafeArea`, `View.State` | `astra.view` | [Screen](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Screen.html) |
| `View.ScreenPointToRay`, `ViewportPointToRay`, `WorldToScreenPoint`, `WorldToViewportPoint` | `astra.view` | [Camera.ScreenPointToRay](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Camera.ScreenPointToRay.html) |
| `Debug.DrawLine`, `Debug.DrawRay` (cor e duração) | `astra.debug` | [Debug.DrawLine](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Debug.DrawLine.html) |
| `Haptics.Vibrate(ms, amplitude)`, `Haptics.Available` | `astra.haptics` | [Handheld.Vibrate](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Handheld.Vibrate.html) |
| Callbacks `ParentChanged()` e `ChildrenChanged()` | `astra.hierarchy` | [OnTransformParentChanged](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/MonoBehaviour.OnTransformParentChanged.html), [OnTransformChildrenChanged](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/MonoBehaviour.OnTransformChildrenChanged.html) |

Exemplo: tocar na tela e acertar um objeto 3D.

```csharp
var ray = View.ScreenPointToRay(pixel);
// Na Astra o comprimento da direção é o alcance da consulta.
if (Physics.RayCast(ray.Origin, ray.Direction * 50) is { } hit)
    Debug.DrawLine(ray.Origin, hit.Point, Color.White, 0.5f);
```

## Cadeia

```text
EditorSession (cada quadro de Play)
  → gameView(): retângulo da vista × pixels por unidade, DPI do sistema,
    câmera autorada de maior prioridade (a mesma do renderer) ou a do editor
  → EditorPlayScene.setGameView → ScriptBridge → astra.view
  → gameViewScreenRay / gameViewWorldToScreen sobre renderer/camera_ray.h

Debug.DrawLine → astra.debug → runtime::DebugLines (4096, envelhece no relógio do mundo)
  → EditorScreen (ramo de Play) projeta com a vista de jogo e desenha

GameWorld (create/destroy/reparent/instantiate aplicados) → HierarchyChange
  → astra.hierarchy → BehaviorWorld (início de FixedUpdate/Update/LateUpdate)
  → ParentChanged no objeto e subárvore; ChildrenChanged no pai cuja lista mudou

android_main: AConfiguration_getDensity + editorScale → setDisplayMetrics
              VibrationEffect.createOneShot + AudioAttributes(USAGE_GAME) via JNI → setHaptics → astra.haptics
```

## Diferenças em relação à referência

| Aspecto | Astra | Classificação |
|---|---|---|
| Coordenadas de tela | Pixels da vista de jogo, origem inferior esquerda | Equivalente |
| Câmera das conversões | A que produziu o quadro; sem câmera autorada, a do editor no Play | Adaptação explícita |
| Área segura | `TryGetSafeArea` devolve falso: nenhuma plataforma informa recortes ainda | Pendente: exige `WindowInsets`/`DisplayCutout` no shell Java |
| DPI | Bucket de densidade do Android (`AConfiguration_getDensity`); zero no host | Equivalente ao `Screen.dpi` que devolve zero quando desconhecido |
| `Application.targetFrameRate` | Não duplicado: já existe como `maximumRenderHz` em `Graphics` | Não aplicável com motivo |
| Debug.DrawLine | Desenhado no Play do editor; jogo exportado não desenha | Adaptação explícita (na Unity, só com Gizmos na Game view) |
| `OnTransformParentChanged` | Entregue no despacho seguinte ao ponto seguro, não dentro do `SetParent` | Adaptação explícita: estrutura muda só no ponto seguro |
| `Handheld.Vibrate` | Duração e amplitude; família ausente sem vibrador; declarada como uso de jogo (sem atributos o Android a trata como toque e a ignora quando o retorno tátil está desligado) | Adaptação explícita |
| `OnBecameVisible/Invisible`, `OnJointBreak`, `OnControllerColliderHit` | Não implementados | Pendente: dependem de retorno de visibilidade do renderer, limite de quebra nas juntas e contatos do personagem |
| Carregar cena (`SceneManager`) | Implementado no bloco C2 (`astra.scenes`), ver `CENAS-EM-PLAY-2026-10-06.md` | Adaptação explícita, descrita lá |

## Validação executada (06/10/2026)

- Host C++: `game_view_*` 1/1 (raio e projeção inversos em perspectiva e ortográfica, origem inferior esquerda), `game_services_*` 2/2 (famílias pelo Play real: estado da vista antes e depois de publicada, recusa de layout, linhas com duração zero e temporizada, reparent pelo ponto seguro gerando ParentChanged/ChildrenChanged uma única vez, recusa após Stop; família de vibração só com vibrador registrado e domínio checado antes da plataforma).
- C#: `GameServicesTests` 2/2 (layouts das tabelas, conversão de cor linear para sRGB, Behavior real usando View/Debug/Haptics e recebendo ParentChanged/ChildrenChanged, geração vencida ignorada). Suíte 517/518; a falha restante é anterior à branch.
- UI executável: `docs/validacao/evidencias/game-services-20261006/debug-lines-*.png` em 853×394 e 1200×700, Play com eixos, raio e anel desenhados pela mesma rotina do editor; zero glifos ausentes e zero recortes.
- Android: `libaether_android.so` arm64 compilado com o JNI do vibrador e as métricas de tela. A permissão `VIBRATE` (normal, sem diálogo) foi declarada no manifesto. No aparelho (06/10, `docs/validacao/evidencias/unificacao-20261006/`): vibração `finished` 336 ms com uso de jogo, DPI 520, `ScreenPointToRay` do centro da vista acertando o alvo. Raio a partir de toque real ainda não exercitado.
