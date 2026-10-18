// GT: recebe lotes de telemetria do app Astra e grava pelas funções SQL do banco.
// O app envia só a chave pública (Authorization: Bearer <anon/publishable>); a
// gravação usa a chave de serviço, que só existe aqui no servidor.
import { createClient } from "npm:@supabase/supabase-js@2.45.4";

const MaximumBodyBytes = 256 * 1024;
const uuid = /^[0-9a-f]{8}-[0-9a-f]{4}-[1-8][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;

type Json = Record<string, unknown>;

function reply(status: number, body: Json): Response {
  return new Response(JSON.stringify(body), { status, headers: { "content-type": "application/json" } });
}

function text(value: unknown, maximum: number): value is string {
  return typeof value === "string" && value.length > 0 && value.length <= maximum;
}

function finite(value: unknown): boolean {
  return typeof value === "number" && Number.isFinite(value);
}

// Recusa o lote inteiro em vez de gravar parte dele: um app com bug não deve
// contaminar o agregado com janelas meio preenchidas.
function validateBatch(batch: Json): string | null {
  if (batch.schema !== "astra.telemetry.batch/1") return "schema";
  if (!text(batch.installation, 36) || !uuid.test(batch.installation as string)) return "installation";
  const app = batch.app as Json | undefined;
  if (!app || !text(app.package, 128) || !text(app.version, 64) || !Number.isInteger(app.code)) return "app";
  const device = batch.device as Json | undefined;
  if (!device || !text(device.model, 128)) return "device";
  const session = batch.session as Json | undefined;
  if (!session || !text(session.id, 36) || !uuid.test(session.id as string)) return "session";
  if (!text(session.started_at, 40) || Number.isNaN(Date.parse(session.started_at as string))) return "session.started_at";
  const windows = batch.windows;
  if (!Array.isArray(windows) || windows.length > 120) return "windows";
  for (const item of windows as Json[]) {
    if (item?.schema !== "astra.telemetry.window/1") return "window.schema";
    if (!Number.isInteger(item.seq) || !Number.isInteger(item.frames) || (item.frames as number) <= 0) return "window.counts";
    if (!finite(item.fps) || !finite(item.elapsed_ms)) return "window.fps";
    if (item.mode !== "play" && item.mode !== "edit") return "window.mode";
    if (!text(item.scene, 96)) return "window.scene";
    for (const key of ["interval_ms", "thread_cpu_ms"]) {
      const distribution = item[key];
      if (!Array.isArray(distribution) || distribution.length !== 5 || !distribution.every(finite)) return `window.${key}`;
    }
  }
  const events = batch.events ?? [];
  if (!Array.isArray(events) || events.length > 20) return "events";
  for (const event of events as Json[]) {
    if (!["bench", "crash", "slow_report"].includes(event?.kind as string)) return "event.kind";
  }
  return null;
}

Deno.serve(async (request) => {
  if (request.method !== "POST") return reply(405, { error: "método" });
  const length = Number(request.headers.get("content-length") ?? "0");
  if (length > MaximumBodyBytes) return reply(413, { error: "tamanho" });
  const raw = await request.text();
  if (raw.length > MaximumBodyBytes) return reply(413, { error: "tamanho" });
  let body: Json;
  try { body = JSON.parse(raw); } catch { return reply(400, { error: "json" }); }

  const client = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!,
                              { auth: { persistSession: false } });

  if (body.schema === "astra.telemetry.delete/1") {
    if (!text(body.installation, 36) || !uuid.test(body.installation as string)) return reply(400, { error: "installation" });
    const { data, error } = await client.rpc("telemetry_forget", { installation: body.installation });
    return error ? reply(500, { error: "gravação" }) : reply(200, data as Json);
  }

  const invalid = validateBatch(body);
  if (invalid) return reply(400, { error: `inválido: ${invalid}` });
  const { data, error } = await client.rpc("telemetry_ingest", { batch: body });
  if (error) {
    // O limite por hora vem do banco como exceção; o app deve esperar e tentar depois.
    const limited = error.message?.includes("limite de envio");
    return reply(limited ? 429 : 500, { error: limited ? "limite" : "gravação" });
  }
  return reply(200, data as Json);
});
