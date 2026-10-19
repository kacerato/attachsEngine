package dev.aether.editor.shell;

import org.json.JSONObject;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.List;

import static org.junit.Assert.*;

public class TelemetryQueueTest {
    @Rule public TemporaryFolder temporary = new TemporaryFolder();

    private static final String SESSION = "5a1c2b9a-1d2e-4f3a-8b8c-7d6e5f4a3b2c";
    private static final String INSTALLATION = "3f1c2b9a-1d2e-4f3a-9b8c-7d6e5f4a3b2c";

    private TelemetryQueue queue(int windows) throws Exception {
        File directory = temporary.newFolder("telemetry");
        TelemetryQueue queue = new TelemetryQueue(directory);
        queue.beginSession(SESSION, "2026-10-18T20:00:00Z");
        Files.write(new File(directory, SESSION + ".env.json").toPath(),
                "{\"gpu\":{\"name\":\"Adreno (TM) 825\",\"vendor\":20803,\"device\":0},\"tier_recommended\":\"s\",\"tier_resolved\":\"a\"}"
                        .getBytes(StandardCharsets.UTF_8));
        List<String> lines = new ArrayList<>();
        for (int seq = 0; seq < windows; ++seq)
            lines.add("{\"schema\":\"astra.telemetry.window/1\",\"seq\":" + seq + ",\"scene\":\"authored-project\",\"mode\":\"edit\",\"fps\":60}");
        Files.write(new File(directory, SESSION + ".jsonl").toPath(), lines, StandardCharsets.UTF_8);
        return queue;
    }

    private static JSONObject app() throws Exception {
        return new JSONObject().put("package", "dev.aether.editor").put("version", "0.3.3").put("code", 31);
    }

    private static JSONObject device() throws Exception {
        return new JSONObject().put("model", "25053PC47G").put("sdk", 35);
    }

    @Test public void batchesCarryEnvelopeGpuAndTiersInChunksOf120() throws Exception {
        TelemetryQueue queue = queue(130);
        List<JSONObject> posted = new ArrayList<>();
        TelemetryQueue.Result result = queue.flush(INSTALLATION, app(), device(), body -> {
            try { posted.add(new JSONObject(body)); } catch (Exception error) { throw new AssertionError(error); }
            return 200;
        }, System.currentTimeMillis());
        assertEquals(130, result.sent);
        assertFalse(result.interrupted);
        assertEquals(2, posted.size());
        JSONObject first = posted.get(0);
        assertEquals("astra.telemetry.batch/1", first.getString("schema"));
        assertEquals(INSTALLATION, first.getString("installation"));
        assertEquals(120, first.getJSONArray("windows").length());
        assertEquals(10, posted.get(1).getJSONArray("windows").length());
        assertEquals("Adreno (TM) 825", first.getJSONObject("device").getJSONObject("gpu").getString("name"));
        assertEquals("25053PC47G", first.getJSONObject("device").getString("model"));
        assertEquals(SESSION, first.getJSONObject("session").getString("id"));
        assertEquals("s", first.getJSONObject("session").getString("tier_recommended"));
        assertEquals("a", first.getJSONObject("session").getString("tier_resolved"));
        assertEquals(0, queue.pendingWindows());
    }

    @Test public void failedUploadKeepsUnsentWindowsAndMergesNewOnes() throws Exception {
        TelemetryQueue queue = queue(130);
        int[] calls = {0};
        TelemetryQueue.Result offline = queue.flush(INSTALLATION, app(), device(),
                body -> ++calls[0] == 1 ? 200 : -1, System.currentTimeMillis());
        assertTrue(offline.interrupted);
        assertEquals(120, offline.sent);
        assertEquals(10, queue.pendingWindows());
        // O renderer continua escrevendo enquanto o envio falhou.
        Files.write(new File(queue.directory(), SESSION + ".jsonl").toPath(),
                "{\"schema\":\"astra.telemetry.window/1\",\"seq\":130}\n".getBytes(StandardCharsets.UTF_8),
                StandardOpenOption.CREATE, StandardOpenOption.APPEND);
        assertEquals(11, queue.pendingWindows());
        TelemetryQueue.Result online = queue.flush(INSTALLATION, app(), device(), body -> 200, System.currentTimeMillis());
        assertEquals(11, online.sent);
        assertEquals(0, queue.pendingWindows());
    }

    @Test public void rejectedBatchIsDroppedNotRetriedForever() throws Exception {
        TelemetryQueue queue = queue(3);
        TelemetryQueue.Result result = queue.flush(INSTALLATION, app(), device(), body -> 400, System.currentTimeMillis());
        assertEquals(3, result.dropped);
        assertFalse(result.interrupted);
        assertEquals(0, queue.pendingWindows());
    }

    @Test public void finishedSessionFilesLeaveAfterRetentionAndClearRemovesEverything() throws Exception {
        TelemetryQueue queue = queue(1);
        long later = System.currentTimeMillis() + TelemetryQueue.SESSION_RETENTION_MILLIS + 1000L;
        queue.flush(INSTALLATION, app(), device(), body -> 200, later);
        assertFalse(new File(queue.directory(), SESSION + ".session").exists());
        assertFalse(new File(queue.directory(), SESSION + ".env.json").exists());

        TelemetryQueue other = new TelemetryQueue(temporary.newFolder("other"));
        other.beginSession(SESSION, "2026-10-18T20:00:00Z");
        Files.write(new File(other.directory(), SESSION + ".jsonl").toPath(),
                "{\"seq\":0}\n".getBytes(StandardCharsets.UTF_8));
        other.clear();
        assertEquals(0, other.directory().listFiles().length);
    }
}
