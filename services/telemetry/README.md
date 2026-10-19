# Telemetria de desempenho (GT)

Plano: `docs/planos/DESEMPENHO-GRAFICOS-VIVOS-2026-10-17.md`, bloco GT. Estado em
18/10/2026: **recepção gravando em produção; o app ainda não envia.** O Supabase bateu o
limite de 2 projetos gratuitos; a recepção foi para uma Neon Function ao lado do banco,
com `DATABASE_URL` injetada pelo Neon. O site só repassa. A pasta `supabase/` fica como
referência do esquema original e não é publicada.

## Peças

| Peça | Onde | Estado |
|---|---|---|
| Janela agregada no app (JSON `astra.telemetry.window/1`) | `native/profiler/telemetry_window.{h,cpp}` | implementada, `aether_telemetry_tests` 3/3 |
| Recepção, validação, esquema, apagar instalação, retenção | `neon/index.mjs` → Neon Function `ingest` | publicada; 12/12 local (PGlite); ponta a ponta em produção |
| Endereço estável usado pelo app | AstraDocs `api/telemetry/ingest.js` (repasse) | `https://astraengine.com.br/api/telemetry/ingest/` |
| Banco | Neon `astra-telemetry` (`dry-bar-61879098`), branch `main`, banco `telemetry`, us-east-1 | esquema v1 criado pela função |
| Retenção de 180 dias | gatilho `retencao-180-dias`, 04:17 UTC, `POST /retention` | ativo |
| Consentimento, fila local, envio, "Apagar meus dados", envelope do aparelho | app Android (Java) | pendente |
| Ligação do nativo ao laço de quadros e ao JNI | `platform/android/android_main.cpp` | pendente |
| Remoção no jogo exportado (módulo e permissão de internet) | exportador | pendente |

A URL usa barra final: o site tem `trailingSlash` e responde 308 a `/api/telemetry/ingest`.
Função direta: `https://br-mute-dawn-b70p1deg-ingest.compute.c-13.us-east-1.aws.neon.tech/`.

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

- O app só conhece a URL do site; não tem chave nem credencial de banco.
- A função usa o `DATABASE_URL` que o Neon injeta no runtime e consulta pelo endpoint SQL
  por HTTP (sem dependências). Nenhuma tabela é exposta fora da função.
- Limite: 120 janelas e 20 eventos por lote, 256 KB por requisição, 2.000 janelas por
  instalação por hora. Lote inválido é recusado inteiro; janela repetida (sessão, seq) é ignorada.

## Publicar uma nova versão da função

1. `esbuild services/telemetry/neon/index.mjs --format=esm --minify --platform=node --target=node24 --outfile=<tmp>/index.mjs`
2. Zipar `index.mjs` na raiz do arquivo e publicar no slug `ingest` do branch `br-mute-dawn-b70p1deg`
   (plugin do Neon `deploy_function`, ou `neon functions deploy ingest --src services/telemetry/neon/index.mjs`).
3. Conferir com um POST `{"schema":"x"}` (espera 400) e um lote de teste seguido de `astra.telemetry.delete/1`.
