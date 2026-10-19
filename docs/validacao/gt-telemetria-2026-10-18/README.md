# GT — telemetria de desempenho no aparelho (18/10/2026)

Aparelho: POCO F7 (25053PC47G, Adreno 825), Astra Dev `dev.aether.editor.u07`, instalado por
`install -r` sobre a 100 (projetos preservados). Versões: 101 (validação de fluxo), 102 e 103
(correção da folha). APK 103 SHA-256 `a74ba75bda63b7fce1bd38deca3409af6667efc52520c35e97ed49a2442fe2ab`.
Fonte: bdd976fd + correção da folha neste commit.

## O que foi validado

| Passo | Evidência | Resultado |
|---|---|---|
| Primeira abertura pede a escolha antes de qualquer coleta | `01-consentimento.png` | folha "Antes de começar", "Agora não" / "Compartilhar" |
| Aceitar volta à lista de projetos | `02-projetos.png` | ok |
| Abrir Sponza liga a coleta | logcat `[Telemetry] coleta ligada (janelas de 600 quadros)` | ok |
| Edição ~60 s, Play ~40 s, voltar à lista envia | `03-editor-sponza.png`, `04-play-sponza.png`; consulta no Neon | 10 janelas gravadas: seq 0–5 `edit`, 6–9 `play` |
| Envelope correto | Neon | `0.3.0-dev.101-gt`, `25053PC47G`, `Adreno (TM) 825`, nível recomendado `a`, resolvido `a` |
| Números | Neon | edição: 60,0–60,1 FPS, p95 18,1–18,7 ms, CPU da thread 2,6–2,9 ms, GPU 10,8–14,4 ms; Play: 58,8–60,1 FPS, p95 19,3–20,4 ms, CPU 3,8–4,5 ms, GPU 13,7–14,1 ms; térmica 0 |
| Configurações mostra o estado | `05-configuracoes.png` | estado cortado pelos botões → corrigido (`08`, `09`) |
| "Apagar dados" em dois toques | `06-confirmar-apagar.png`, `07-apagado.png`, `10-confirmar.png` | servidor ficou com 0 instalações, 0 sessões e 0 janelas |
| Jogo exportado sem internet | export de teste do U07 com o APK 103 como modelo (chave descartável) | só `android.permission.VIBRATE`; launcher `GameBootstrapActivity` |

## Correções feitas a partir das capturas

- O estado ("Último envio…") era cortado pelos dois botões empilhados: botões lado a lado e
  estado com rolagem.
- "Há 0 minuto" → "agora há pouco" no primeiro minuto.
- "Apagar meus dados" quebrava em duas linhas no botão: "Apagar dados", com "Confirmar" no
  segundo toque.
- No pedido de consentimento o interruptor ficava visível e inerte: oculto; a escolha é pelos
  dois botões.

## Observações

- `render` vem como 1280×2772 (dimensões do swapchain antes da rotação), não 2772×1280. Coerente
  entre janelas; ao comparar aparelhos, ler como área.
- O editor está limitado a 60 FPS nesta cena (alvo 120 pedido pelo shell); a telemetria mede o
  que é apresentado.
- A captura do consentimento corrigido não foi refeita: exigiria apagar os dados do app, o que
  apaga os projetos do canal Dev.
- Não testado: envio sem rede e reenvio, limite de 1 MB e retenção de 12 h da sessão no aparelho
  (cobertos só pelo `TelemetryQueueTest`).
