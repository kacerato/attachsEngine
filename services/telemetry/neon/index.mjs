// GT: recepção da telemetria de desempenho como Neon Function no projeto
// astra-telemetry. O Neon injeta DATABASE_URL no runtime; nenhuma credencial
// passa pelo app, pelo site ou pelo repositório. Sem dependências: as consultas
// usam o endpoint SQL por HTTP do Neon (o mesmo do @neondatabase/serverless).
//   POST /           lote astra.telemetry.batch/1 ou astra.telemetry.delete/1
//   POST /retention  apaga janelas/eventos com mais de 180 dias (gatilho diário)
// O site astraengine.com.br/api/telemetry/ingest/ repassa para cá.

export const SchemaVersion = 1;

const Schema = `
create schema if not exists telemetry;
create table if not exists telemetry.meta (key text primary key, value text not null);
create table if not exists telemetry.installations (
  id uuid primary key,
  first_seen timestamptz not null default now(),
  last_seen timestamptz not null default now(),
  app_package text not null check (length(app_package) <= 128),
  device jsonb not null default '{}'::jsonb
);
create table if not exists telemetry.sessions (
  id uuid primary key,
  installation_id uuid not null references telemetry.installations(id) on delete cascade,
  started_at timestamptz not null,
  received_at timestamptz not null default now(),
  app_version text not null check (length(app_version) <= 64),
  app_code integer not null,
  device_model text not null check (length(device_model) <= 128),
  gpu_name text not null check (length(gpu_name) <= 128),
  tier_recommended text check (tier_recommended in ('c','b','a','s')),
  tier_resolved text check (tier_resolved in ('c','b','a','s'))
);
create table if not exists telemetry.windows (
  id bigint generated always as identity primary key,
  session_id uuid not null references telemetry.sessions(id) on delete cascade,
  received_at timestamptz not null default now(),
  seq integer not null,
  scene text not null check (length(scene) <= 96),
  mode text not null check (mode in ('play','edit')),
  tier text check (tier in ('c','b','a','s')),
  frames integer not null check (frames > 0),
  elapsed_ms real not null,
  fps real not null,
  interval_p50 real, interval_p95 real, interval_p99 real,
  thread_cpu_mean real, thread_cpu_p95 real,
  gpu_mean real, gpu_p95 real,
  render_width integer, render_height integer, render_scale real,
  thermal smallint,
  raw jsonb not null,
  unique (session_id, seq)
);
create index if not exists windows_received_at on telemetry.windows (received_at);
create table if not exists telemetry.events (
  id bigint generated always as identity primary key,
  session_id uuid not null references telemetry.sessions(id) on delete cascade,
  received_at timestamptz not null default now(),
  kind text not null check (kind in ('bench','crash','slow_report')),
  payload jsonb not null check (pg_column_size(payload) <= 16384)
);
create or replace view telemetry.performance_by_gpu as
select s.gpu_name, coalesce(w.tier, s.tier_resolved) as tier, s.app_version, w.scene, w.mode,
       count(*) as windows, count(distinct s.installation_id) as installations,
       percentile_cont(0.5) within group (order by w.fps) as fps_median,
       percentile_cont(0.1) within group (order by w.fps) as fps_p10,
       percentile_cont(0.5) within group (order by w.interval_p95) as interval_p95_median,
       percentile_cont(0.5) within group (order by w.thread_cpu_mean) as cpu_median,
       percentile_cont(0.5) within group (order by w.gpu_mean) as gpu_median,
       max(w.thermal) as thermal_max
from telemetry.windows w join telemetry.sessions s on s.id = w.session_id
group by 1,2,3,4,5;
insert into telemetry.meta (key, value) values ('schema_version', '${SchemaVersion}')
on conflict (key) do update set value = excluded.value;
`;

const Limits = { bodyBytes: 256 * 1024, windowsPerHour: 2000 };

// Executor injetável para teste; em produção fala com o endpoint SQL do Neon.
let execute = null;
export function setExecutor(executor) { execute = executor; schemaReady = false; }

function httpExecutor(connection) {
  const host = new URL(connection).hostname;
  const endpoint = `https://${host.replace(/^[^.]+\./, "api.")}/sql`;
  return async (query, params = []) => {
    const response = await fetch(endpoint, {
      method: "POST",
      headers: { "Neon-Connection-String": connection, "content-type": "application/json" },
      body: JSON.stringify({ query, params: params.map((value) => (value === null || value === undefined ? null : typeof value === "object" ? JSON.stringify(value) : String(value))) }),
    });
    const body = await response.json();
    if (!response.ok) throw new Error(body?.message ?? `sql ${response.status}`);
    return body.rows ?? [];
  };
}

function executor() {
  if (execute) return execute;
  const connection = process.env.DATABASE_URL;
  if (!connection) return null;
  execute = httpExecutor(connection);
  return execute;
}

let schemaReady = false;
async function ensureSchema(sql) {
  if (schemaReady) return;
  for (const statement of Schema.split(/;\s*\n/).map((s) => s.trim()).filter(Boolean)) await sql(statement);
  schemaReady = true;
}

const uuid = /^[0-9a-f]{8}-[0-9a-f]{4}-[1-8][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
const text = (value, maximum) => typeof value === "string" && value.length > 0 && value.length <= maximum;
const finite = (value) => typeof value === "number" && Number.isFinite(value);
const tiers = new Set(["c", "b", "a", "s"]);
const isId = (value) => text(value, 36) && uuid.test(value);

// Recusa o lote inteiro em vez de gravar parte dele: um app com bug não deve
// contaminar o agregado com janelas meio preenchidas. Devolve o campo inválido.
export function validateBatch(batch) {
  if (!batch || batch.schema !== "astra.telemetry.batch/1") return "schema";
  if (!isId(batch.installation)) return "installation";
  const app = batch.app;
  if (!app || !text(app.package, 128) || !text(app.version, 64) || !Number.isInteger(app.code)) return "app";
  const device = batch.device;
  if (!device || !text(device.model, 128)) return "device";
  const session = batch.session;
  if (!session || !isId(session.id)) return "session";
  if (!text(session.started_at, 40) || Number.isNaN(Date.parse(session.started_at))) return "session.started_at";
  for (const key of ["tier_recommended", "tier_resolved"])
    if (session[key] !== undefined && session[key] !== null && !tiers.has(session[key])) return `session.${key}`;
  const windows = batch.windows;
  if (!Array.isArray(windows) || windows.length > 120) return "windows";
  for (const item of windows) {
    if (!item || item.schema !== "astra.telemetry.window/1") return "window.schema";
    if (!Number.isInteger(item.seq) || !Number.isInteger(item.frames) || item.frames <= 0) return "window.counts";
    if (!finite(item.fps) || !finite(item.elapsed_ms)) return "window.fps";
    if (item.mode !== "play" && item.mode !== "edit") return "window.mode";
    if (!text(item.scene, 96)) return "window.scene";
    if (item.tier !== undefined && item.tier !== null && item.tier !== "" && !tiers.has(item.tier)) return "window.tier";
    for (const key of ["interval_ms", "thread_cpu_ms"]) {
      const distribution = item[key];
      if (!Array.isArray(distribution) || distribution.length !== 5 || !distribution.every(finite)) return `window.${key}`;
    }
    if (item.gpu_ms !== null && item.gpu_ms !== undefined &&
        (!Array.isArray(item.gpu_ms) || item.gpu_ms.length !== 5 || !item.gpu_ms.every(finite))) return "window.gpu_ms";
  }
  const events = batch.events ?? [];
  if (!Array.isArray(events) || events.length > 20) return "events";
  for (const event of events) {
    if (!event || !["bench", "crash", "slow_report"].includes(event.kind)) return "event.kind";
    if (JSON.stringify(event.payload ?? {}).length > 16384) return "event.payload";
  }
  return null;
}

const json = (status, body) => new Response(JSON.stringify(body), {
  status, headers: { "content-type": "application/json; charset=utf-8", "cache-control": "no-store" } });
const at = (distribution, index) => (Array.isArray(distribution) ? distribution[index] : null);

async function ingest(sql, body) {
  if (body?.schema === "astra.telemetry.delete/1") {
    if (!isId(body.installation)) return json(400, { error: "installation" });
    const removed = await sql("delete from telemetry.installations where id = $1 returning id", [body.installation]);
    return json(200, { removed: removed.length });
  }
  const invalid = validateBatch(body);
  if (invalid) return json(400, { error: `inválido: ${invalid}` });

  const { device, session } = body;
  await sql(`insert into telemetry.installations (id, app_package, device) values ($1, $2, $3::jsonb)
             on conflict (id) do update set last_seen = now(), device = excluded.device`,
            [body.installation, body.app.package, device]);
  const [{ recent }] = await sql(`select count(*)::int as recent from telemetry.windows w
             join telemetry.sessions s on s.id = w.session_id
             where s.installation_id = $1 and w.received_at > now() - interval '1 hour'`, [body.installation]);
  if (Number(recent) + body.windows.length > Limits.windowsPerHour) return json(429, { error: "limite" });

  await sql(`insert into telemetry.sessions (id, installation_id, started_at, app_version, app_code, device_model,
                                             gpu_name, tier_recommended, tier_resolved)
             values ($1, $2, $3, $4, $5, $6, $7, $8, $9) on conflict (id) do nothing`,
            [session.id, body.installation, session.started_at, body.app.version, body.app.code,
             String(device.model).slice(0, 128), String(device.gpu?.name ?? "").slice(0, 128),
             session.tier_recommended || null, session.tier_resolved || null]);
  let accepted = 0;
  for (const item of body.windows) {
    const inserted = await sql(`insert into telemetry.windows (session_id, seq, scene, mode, tier, frames, elapsed_ms, fps,
                                   interval_p50, interval_p95, interval_p99, thread_cpu_mean, thread_cpu_p95,
                                   gpu_mean, gpu_p95, render_width, render_height, render_scale, thermal, raw)
             values ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14,$15,$16,$17,$18,$19,$20::jsonb)
             on conflict (session_id, seq) do nothing returning id`,
      [session.id, item.seq, item.scene, item.mode, item.tier || null, item.frames, item.elapsed_ms, item.fps,
       at(item.interval_ms, 1), at(item.interval_ms, 2), at(item.interval_ms, 3), at(item.thread_cpu_ms, 0),
       at(item.thread_cpu_ms, 2), at(item.gpu_ms, 0), at(item.gpu_ms, 2), at(item.render, 0), at(item.render, 1),
       at(item.render, 2), Number.isInteger(item.thermal) ? item.thermal : null, item]);
    accepted += inserted.length;
  }
  for (const event of body.events ?? [])
    await sql("insert into telemetry.events (session_id, kind, payload) values ($1, $2, $3::jsonb)",
              [session.id, event.kind, event.payload ?? {}]);
  return json(200, { accepted });
}

async function retention(sql) {
  const windows = await sql("delete from telemetry.windows where received_at < now() - interval '180 days' returning id");
  const events = await sql("delete from telemetry.events where received_at < now() - interval '180 days' returning id");
  const sessions = await sql(`delete from telemetry.sessions s where s.received_at < now() - interval '180 days'
                              and not exists (select 1 from telemetry.windows w where w.session_id = s.id) returning id`);
  return json(200, { windows: windows.length, events: events.length, sessions: sessions.length });
}

export default {
  async fetch(request) {
    const path = new URL(request.url).pathname.replace(/\/+$/, "");
    if (request.method !== "POST") return json(405, { error: "método" });
    const sql = executor();
    if (!sql) return json(503, { error: "banco não configurado" });
    try {
      await ensureSchema(sql);
      if (path === "/retention") return await retention(sql);
      if (path !== "") return json(404, { error: "rota" });
      if (Number(request.headers.get("content-length") ?? 0) > Limits.bodyBytes) return json(413, { error: "tamanho" });
      const raw = await request.text();
      if (raw.length > Limits.bodyBytes) return json(413, { error: "tamanho" });
      let body;
      try { body = JSON.parse(raw); } catch { return json(400, { error: "json" }); }
      return await ingest(sql, body);
    } catch (error) {
      console.error("[telemetry] gravação falhou", error?.message);
      return json(500, { error: "gravação" });
    }
  },
};
