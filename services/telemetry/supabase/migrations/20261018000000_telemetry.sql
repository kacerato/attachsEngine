-- GT (DESEMPENHO-GRAFICOS-VIVOS-2026-10-17): telemetria de desempenho dos testadores.
--
-- Contrato de acesso: nenhuma tabela é legível ou gravável por anon/authenticated.
-- A Edge Function `ingest` valida o lote e chama telemetry_ingest/telemetry_forget
-- com a chave de serviço; o proprietário lê pelo painel ou por SQL.
-- Nada pessoal: id de instalação aleatório (apagável), modelo/GPU do aparelho e
-- números de quadro. Sem e-mail, nome, localização, Android ID ou conteúdo de projeto.

create schema if not exists telemetry;
revoke all on schema telemetry from public, anon, authenticated;

create table telemetry.installations (
  id uuid primary key,
  first_seen timestamptz not null default now(),
  last_seen timestamptz not null default now(),
  app_package text not null check (length(app_package) <= 128),
  device jsonb not null default '{}'::jsonb
);

create table telemetry.sessions (
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

create table telemetry.windows (
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
create index windows_received_at on telemetry.windows (received_at);

-- Eventos raros: execução do Astra Bench, falha nativa (assinatura, sem dados do
-- usuário) e "Relatar lentidão" com comentário curto do testador.
create table telemetry.events (
  id bigint generated always as identity primary key,
  session_id uuid not null references telemetry.sessions(id) on delete cascade,
  received_at timestamptz not null default now(),
  kind text not null check (kind in ('bench','crash','slow_report')),
  payload jsonb not null check (pg_column_size(payload) <= 16384)
);

alter table telemetry.installations enable row level security;
alter table telemetry.sessions enable row level security;
alter table telemetry.windows enable row level security;
alter table telemetry.events enable row level security;
-- Intencionalmente sem políticas: RLS ligada e nenhuma política = nenhum acesso
-- para anon/authenticated mesmo que o esquema venha a ser exposto por engano.

-- Agregado para o painel: FPS e tempos por GPU, nível, versão e cena.
create view telemetry.performance_by_gpu with (security_invoker = true) as
select s.gpu_name, coalesce(w.tier, s.tier_resolved) as tier, s.app_version, w.scene, w.mode,
       count(*) as windows,
       count(distinct s.installation_id) as installations,
       percentile_cont(0.5) within group (order by w.fps) as fps_median,
       percentile_cont(0.1) within group (order by w.fps) as fps_p10,
       percentile_cont(0.5) within group (order by w.interval_p95) as interval_p95_median,
       percentile_cont(0.5) within group (order by w.thread_cpu_mean) as cpu_median,
       percentile_cont(0.5) within group (order by w.gpu_mean) as gpu_median,
       max(w.thermal) as thermal_max
from telemetry.windows w join telemetry.sessions s on s.id = w.session_id
group by 1,2,3,4,5;

-- Gravação de um lote já validado pela Edge Function. Revalida limites aqui:
-- o banco não confia no chamador.
create or replace function public.telemetry_ingest(batch jsonb)
returns jsonb
language plpgsql
security definer
set search_path = telemetry, pg_temp
as $$
declare
  installation uuid := (batch->>'installation')::uuid;
  session uuid := (batch->'session'->>'id')::uuid;
  device jsonb := coalesce(batch->'device', '{}'::jsonb);
  window_item jsonb;
  event_item jsonb;
  accepted integer := 0;
  recent integer;
begin
  if batch->>'schema' <> 'astra.telemetry.batch/1' then
    raise exception 'schema não suportado';
  end if;
  if jsonb_array_length(coalesce(batch->'windows','[]'::jsonb)) > 120
     or jsonb_array_length(coalesce(batch->'events','[]'::jsonb)) > 20 then
    raise exception 'lote grande demais';
  end if;

  insert into installations (id, app_package, device)
  values (installation, left(batch->'app'->>'package', 128), device)
  on conflict (id) do update set last_seen = now(), device = excluded.device;

  -- Limite por instalação: no máximo 2.000 janelas por hora (~5 h de jogo contínuo).
  select count(*) into recent from windows w join sessions s on s.id = w.session_id
  where s.installation_id = installation and w.received_at > now() - interval '1 hour';
  if recent > 2000 then
    raise exception 'limite de envio por hora atingido';
  end if;

  insert into sessions (id, installation_id, started_at, app_version, app_code, device_model,
                        gpu_name, tier_recommended, tier_resolved)
  values (session, installation, (batch->'session'->>'started_at')::timestamptz,
          left(batch->'app'->>'version', 64), (batch->'app'->>'code')::integer,
          left(coalesce(device->>'model', ''), 128), left(coalesce(device->'gpu'->>'name', ''), 128),
          nullif(batch->'session'->>'tier_recommended',''), nullif(batch->'session'->>'tier_resolved',''))
  on conflict (id) do nothing;

  for window_item in select * from jsonb_array_elements(coalesce(batch->'windows','[]'::jsonb)) loop
    if window_item->>'schema' <> 'astra.telemetry.window/1' then continue; end if;
    insert into windows (session_id, seq, scene, mode, tier, frames, elapsed_ms, fps,
                         interval_p50, interval_p95, interval_p99, thread_cpu_mean, thread_cpu_p95,
                         gpu_mean, gpu_p95, render_width, render_height, render_scale, thermal, raw)
    values (session, (window_item->>'seq')::integer, left(window_item->>'scene', 96),
            window_item->>'mode', nullif(window_item->>'tier',''),
            (window_item->>'frames')::integer, (window_item->>'elapsed_ms')::real, (window_item->>'fps')::real,
            (window_item->'interval_ms'->>1)::real, (window_item->'interval_ms'->>2)::real,
            (window_item->'interval_ms'->>3)::real,
            (window_item->'thread_cpu_ms'->>0)::real, (window_item->'thread_cpu_ms'->>2)::real,
            (window_item->'gpu_ms'->>0)::real, (window_item->'gpu_ms'->>2)::real,
            (window_item->'render'->>0)::integer, (window_item->'render'->>1)::integer,
            (window_item->'render'->>2)::real, (window_item->>'thermal')::smallint, window_item)
    on conflict (session_id, seq) do nothing;
    accepted := accepted + 1;
  end loop;

  for event_item in select * from jsonb_array_elements(coalesce(batch->'events','[]'::jsonb)) loop
    insert into events (session_id, kind, payload)
    values (session, event_item->>'kind', coalesce(event_item->'payload','{}'::jsonb));
  end loop;

  return jsonb_build_object('accepted', accepted);
end;
$$;

-- "Apagar meus dados": remove a instalação e, em cascata, sessões, janelas e eventos.
create or replace function public.telemetry_forget(installation uuid)
returns jsonb
language sql
security definer
set search_path = telemetry, pg_temp
as $$
  with removed as (delete from installations where id = installation returning id)
  select jsonb_build_object('removed', count(*)) from removed;
$$;

revoke all on function public.telemetry_ingest(jsonb) from public, anon, authenticated;
revoke all on function public.telemetry_forget(uuid) from public, anon, authenticated;
grant execute on function public.telemetry_ingest(jsonb) to service_role;
grant execute on function public.telemetry_forget(uuid) to service_role;

-- Retenção: janelas e eventos por 180 dias; o agregado é recalculado das linhas vivas.
create extension if not exists pg_cron;
select cron.schedule('telemetry-retention', '17 3 * * *', $$
  delete from telemetry.windows where received_at < now() - interval '180 days';
  delete from telemetry.events where received_at < now() - interval '180 days';
  delete from telemetry.sessions s where s.received_at < now() - interval '180 days'
    and not exists (select 1 from telemetry.windows w where w.session_id = s.id);
$$);
