package dev.aether.editor.shell;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.List;

/**
 * Fila em disco da telemetria de desempenho (plano GT).
 *
 * O renderer acrescenta uma janela por linha em {@code <sessão>.jsonl} e deixa a
 * GPU e o nível em {@code <sessão>.env.json}; o shell escreve {@code <sessão>.session}
 * ao abrir o editor. Esta classe só junta esses arquivos em lotes
 * "astra.telemetry.batch/1" e decide o que apagar depois de cada resposta. Não toca
 * rede nem APIs Android, para ser testada na JVM.
 */
final class TelemetryQueue {
    static final int WINDOWS_PER_BATCH = 120;
    /** Sessão sem dados novos há este tempo já terminou: pode sair do disco. */
    static final long SESSION_RETENTION_MILLIS = 12L * 60L * 60L * 1000L;

    interface Sender {
        /** Código HTTP da resposta; negativo quando não houve resposta. */
        int post(String batchJson);
    }

    static final class Result {
        int sent, dropped;
        boolean interrupted;
    }

    private final File directory;

    TelemetryQueue(File directory) { this.directory = directory; }

    File directory() { return directory; }

    void beginSession(String sessionId, String startedAtIso) throws IOException {
        Files.createDirectories(directory.toPath());
        Files.write(new File(directory, sessionId + ".session").toPath(),
                startedAtIso.getBytes(StandardCharsets.UTF_8));
    }

    int pendingWindows() {
        int count = 0;
        File[] files = directory.listFiles();
        if (files == null) return 0;
        for (File file : files) {
            String name = file.getName();
            if (name.endsWith(".jsonl") || name.endsWith(".sending")) count += readLines(file).size();
        }
        return count;
    }

    Result flush(String installation, JSONObject app, JSONObject device, Sender sender, long nowMillis) {
        Result result = new Result();
        File[] sessions = directory.listFiles((dir, name) -> name.endsWith(".session"));
        if (sessions == null) return result;
        for (File session : sessions) {
            String id = session.getName().substring(0, session.getName().length() - ".session".length());
            if (!flushSession(id, session, installation, app, device, sender, result)) {
                result.interrupted = true;
                return result;
            }
            File live = new File(directory, id + ".jsonl");
            if (!live.exists() && nowMillis - session.lastModified() > SESSION_RETENTION_MILLIS) {
                delete(session);
                delete(new File(directory, id + ".env.json"));
            }
        }
        return result;
    }

    /** Apaga tudo o que ainda não foi enviado (usado por "Apagar meus dados"). */
    void clear() {
        File[] files = directory.listFiles();
        if (files != null) for (File file : files) delete(file);
    }

    private boolean flushSession(String id, File session, String installation, JSONObject app, JSONObject device,
                                 Sender sender, Result result) {
        File live = new File(directory, id + ".jsonl"), sending = new File(directory, id + ".sending");
        claim(live, sending);
        List<String> lines = readLines(sending);
        if (lines.isEmpty()) { delete(sending); return true; }
        JSONObject environment = readJson(new File(directory, id + ".env.json"));
        String startedAt = readText(session).trim();
        int offset = 0;
        while (offset < lines.size()) {
            int end = Math.min(lines.size(), offset + WINDOWS_PER_BATCH);
            JSONArray windows = new JSONArray();
            for (String line : lines.subList(offset, end)) {
                try { windows.put(new JSONObject(line)); } catch (JSONException ignored) { ++result.dropped; }
            }
            int status = windows.length() == 0 ? 200
                    : sender.post(batch(installation, app, device, environment, id, startedAt, windows));
            if (status == 200) result.sent += windows.length();
            else if (status == 400 || status == 413) result.dropped += windows.length();
            else {
                // Limite, servidor fora ou sem rede: guarda o resto para a próxima vez.
                write(sending, lines.subList(offset, lines.size()));
                return false;
            }
            offset = end;
        }
        delete(sending);
        return true;
    }

    static String batch(String installation, JSONObject app, JSONObject device, JSONObject environment,
                        String sessionId, String startedAt, JSONArray windows) {
        try {
            JSONObject deviceWithGpu = new JSONObject(device.toString());
            JSONObject gpu = environment != null ? environment.optJSONObject("gpu") : null;
            deviceWithGpu.put("gpu", gpu != null ? gpu : new JSONObject().put("name", ""));
            JSONObject session = new JSONObject().put("id", sessionId).put("started_at", startedAt);
            if (environment != null) {
                for (String key : new String[] {"tier_recommended", "tier_resolved"}) {
                    String value = environment.optString(key, "");
                    if (!value.isEmpty()) session.put(key, value);
                }
            }
            return new JSONObject()
                    .put("schema", "astra.telemetry.batch/1")
                    .put("installation", installation)
                    .put("app", app)
                    .put("device", deviceWithGpu)
                    .put("session", session)
                    .put("windows", windows)
                    .put("events", new JSONArray())
                    .toString();
        } catch (JSONException error) {
            throw new IllegalStateException(error);
        }
    }

    /** O renderer pode estar acrescentando: renomear separa o que vai ser enviado. */
    private static void claim(File live, File sending) {
        if (!live.exists()) return;
        if (!sending.exists()) {
            if (live.renameTo(sending)) return;
        }
        List<String> lines = readLines(live);
        try {
            Files.write(sending.toPath(), lines, StandardCharsets.UTF_8,
                    StandardOpenOption.CREATE, StandardOpenOption.APPEND);
            delete(live);
        } catch (IOException ignored) { }
    }

    private static List<String> readLines(File file) {
        List<String> lines = new ArrayList<>();
        if (!file.isFile()) return lines;
        try {
            for (String line : Files.readAllLines(file.toPath(), StandardCharsets.UTF_8))
                if (!line.trim().isEmpty()) lines.add(line);
        } catch (IOException ignored) { }
        return lines;
    }

    private static String readText(File file) {
        try { return new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8); }
        catch (IOException error) { return ""; }
    }

    private static JSONObject readJson(File file) {
        if (!file.isFile()) return null;
        try { return new JSONObject(readText(file)); } catch (JSONException error) { return null; }
    }

    private static void write(File file, List<String> lines) {
        try { Files.write(file.toPath(), lines, StandardCharsets.UTF_8); } catch (IOException ignored) { }
    }

    private static void delete(File file) {
        if (file.exists() && !file.delete()) file.deleteOnExit();
    }
}
