# Telemetria de desempenho (GT)

Plano: `docs/planos/DESEMPENHO-GRAFICOS-VIVOS-2026-10-17.md`, bloco GT. Estado em
18/10/2026: **código no repositório, nada publicado.** Criar o projeto
`astra-telemetry` no Supabase e publicar a função exigem autorização do proprietário.

## Peças

| Peça | Arquivo | Estado |
|---|---|---|
| Janela agregada no app (JSON `astra.telemetry.window/1`) | `native/profiler/telemetry_window.{h,cpp}` | implementada, `aether_telemetry_tests` 3/3 |
| Esquema, RLS sem acesso público, agregado, retenção 180 dias | `supabase/migrations/20261018000000_telemetry.sql` | escrito, não aplicado |
| Recepção e validação do lote | `supabase/functions/ingest/index.ts` | escrito, não publicado |
| Consentimento, fila local, envio, "Apagar meus dados", envelope do aparelho | app Android (Java) | pendente |
| Ligação do nativo ao laço de quadros e ao JNI | `platform/android/android_main.cpp` | pendente |
| Remoção no jogo exportado (módulo e permissão de internet) | exportador | pendente |

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

- O app só conhece a URL da função e a chave pública; não lê nem grava tabela nenhuma.
- A função usa a chave de serviço do próprio ambiente da Edge Function e chama
  `public.telemetry_ingest(jsonb)` / `public.telemetry_forget(uuid)`, `SECURITY DEFINER`,
  executáveis só por `service_role`.
- Todas as tabelas do esquema `telemetry` têm RLS ligada e nenhuma política.
- Limite: 120 janelas e 20 eventos por lote, 256 KB por requisição, 2.000 janelas por
  instalação por hora.

## Publicação (quando autorizada)

1. Criar o projeto `astra-telemetry` na organização do proprietário (custo consultado
   em 17/10/2026: US$ 0/mês).
2. Aplicar a migração e publicar a função `ingest` (CLI do Supabase ou conector).
3. Conferir os avisos de segurança (advisors) e testar que a chave pública não lê nem
   grava nada diretamente.
