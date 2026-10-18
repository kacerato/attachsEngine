# Telemetria de desempenho (GT)

Plano: `docs/planos/DESEMPENHO-GRAFICOS-VIVOS-2026-10-17.md`, bloco GT. Estado em
18/10/2026: **recepção publicada no site, banco ainda não ligado.** O Supabase bateu o
limite de 2 projetos gratuitos; o proprietário escolheu a Vercel. O código de servidor
vive no AstraDocs (`api/telemetry/`, commit `fa31066`), não aqui. A pasta `supabase/`
fica como referência do esquema original e não é publicada.

## Peças

| Peça | Onde | Estado |
|---|---|---|
| Janela agregada no app (JSON `astra.telemetry.window/1`) | `native/profiler/telemetry_window.{h,cpp}` | implementada, `aether_telemetry_tests` 3/3 |
| Recepção, validação, apagar instalação | AstraDocs `api/telemetry/ingest.js` + `_store.js` | publicada em `https://astraengine.com.br/api/telemetry/ingest/`; responde 503 sem banco |
| Esquema Postgres (criado na primeira chamada), agregado `performance_by_gpu` | AstraDocs `api/telemetry/_store.js` | testado em Postgres em memória (PGlite) 10/10 |
| Retenção de 180 dias | AstraDocs `api/telemetry/retention.js`, cron diário em `vercel.json` | publicada |
| Banco Neon ligado ao projeto `astra-docs` | painel da Vercel › Storage | **pendente (proprietário)** |
| Consentimento, fila local, envio, "Apagar meus dados", envelope do aparelho | app Android (Java) | pendente |
| Ligação do nativo ao laço de quadros e ao JNI | `platform/android/android_main.cpp` | pendente |
| Remoção no jogo exportado (módulo e permissão de internet) | exportador | pendente |

A URL usa barra final: o site tem `trailingSlash` e responde 308 a `/api/telemetry/ingest`.

## Lote enviado pelo app (`astra.telemetry.batch/1`)

```json
{
  "schema": "astra.telemetry.batch/1",
  "installation": "<uuid aleatório, apagável>",
  "app": {"package": "dev.aether.editor", "version": "0.3.3-preview", "code": 31},
  "device": {"model": "25053PC47G", "manufacturer": "Xiaomi", "sdk": 35,
             "gpu": {"name": "Adreno (TM) 825", "vendor": 20803, "device": 0, "driver": "..."},
             "ram_mb": 12000, "refresh_hz": 120, "screen": [2772, 1280]},
  "session": {"id": "<uuid>", "started_at": "2026-10-18T20:00:00Z",
              "tier_recommended": "s", "tier_resolved": "s"},
  "windows": [ { "schema": "astra.telemetry.window/1", "seq": 0, "...": "..." } ],
  "events": [ {"kind": "bench", "payload": {}} ]
}
```

Apagar: `{"schema": "astra.telemetry.delete/1", "installation": "<uuid>"}`.

## Acesso

- O app só conhece a URL; não tem chave nem credencial de banco.
- A função lê `DATABASE_URL` (ou `POSTGRES_URL`), injetada pela integração Neon da Vercel.
  Nenhuma tabela é exposta fora da função.
- Limite: 120 janelas e 20 eventos por lote, 256 KB por requisição, 2.000 janelas por
  instalação por hora. Lote inválido é recusado inteiro; janela repetida (sessão, seq) é ignorada.
- `CRON_SECRET` opcional: com ele definido, só a Vercel chama a retenção.

## Ligar o banco (proprietário)

1. Vercel › projeto `astra-docs` › Storage › Create Database › Neon (plano gratuito) ›
   conectar aos ambientes Production e Preview.
2. Redeploy do último deploy de produção, para a função receber a variável.
3. Conferir: um POST com `{"schema":"x"}` passa de 503 para 400.
