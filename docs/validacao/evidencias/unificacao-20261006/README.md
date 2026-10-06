# Unificação em main — validação no aparelho (06/10/2026)

APK debug do merge `dcf5c0f5` (main), 239085867 bytes, SHA-256
`a6237c53703fc6113fe5d0fce6099a655ef2626c9a84ba4289d523cb02c27aee`, instalado com
`install -r` (projetos preservados) no Xiaomi 25053PC47G por ADB sem fio.

| Captura | O que mostra |
|---|---|
| 01-shell | Shell abre com a lista de projetos |
| 02-editor | Projeto ADBGameplayValidation aberto pela intent `astra.open_project` |
| 03/04 | Criar › Gameplay com as receitas novas e ícones próprios |
| 05 | Gatilho sonoro: Corpo físico, Colisor 3D, Audio Source, Conexão de evento |
| 06 | Criado e salvo; Inspeção abre na Conexão de evento, aba Quando primeiro |
| 07 | Aba Então inteira na tela: Chamar método, Este objeto, Áudio: tocar |
| 08 | Play em execução (ABI v45), mesmo processo, sem erro do runtime C# |
| 09 | Stop devolve a cena autoral intacta |

Achados:
- Descrição da receita Conexão de evento cortava no painel; encurtada no commit seguinte.
- A camada de validação Vulkan registrava `VUID-vkCmdDraw-renderPass-02684` no
  `InstancedRenderer`: os pipelines de UI são criados para o render pass da UI e
  gravados também dentro do render pass de pós-processo do editor, e as
  dependências de subpass entram na compatibilidade. Corrigido: os dois passes
  usam a mesma dependência externa (`swapchainColorDependency`). Depois da
  correção, 0 linhas `E Aether.Vulkan` na sessão de aceite abaixo.

## Pendências resolvidas (06/10/2026, segunda rodada)

Projeto `Servicos-20261006`, gerado por
`aether_ui_preview write-services-project` (mesmo importador de WAV e mesmo
formato de cena do produto): câmera autorada, `Alvo` com corpo estático e caixa,
`Gatilho sonoro` (sensor estático + AudioSource com clipe WAV importado +
Conexão de evento `Ao entrar no gatilho → Chamar método play`), `Corpo que cai`
dinâmico e a sonda `Scripts/ServicesProbe.cs`; segunda cena `scenes/Fase2`.
Play por toque no aparelho; registro em `pendencias-logcat.txt`.

| Item | Resultado observado |
|---|---|
| Corpo dinâmico entra no sensor | `sensor-enter other=5` |
| Som do Gatilho sonoro | `state=Playing output=True`, cursor 0,00 → 0,22 s; `AUDIO PASS` exige saída ativa e cursor avançando. Reprodução vista pelo AudioFlinger; audibilidade não medida por microfone |
| `ScreenPointToRay` + `Physics.RayCast` | Centro da vista 2772×1178,6 px (dpi 520): `RAY PASS hit=Alvo distance=5.25` |
| Vibração física | Antes: `ignored_for_settings`, uso TOUCH. Com `AudioAttributes USAGE_GAME`: `finished`, 336 ms, `usage: MEDIA audioUsage=USAGE_GAME` no `dumpsys vibrator_manager` |
| Troca de cena por script | `Scenes.Load("Fase2")` → `SCENE PASS active=Fase2` (captura 13) |
| Validação Vulkan | 0 erros |

Ainda não verificado: raio a partir de um toque real na tela (a sonda usa o
centro da vista) e área segura (`safeReported=False`: depende de `WindowInsets`
no shell Java).
