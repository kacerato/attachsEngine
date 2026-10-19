package dev.aether.editor.shell;

import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.WindowManager;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Telemetria de desempenho opcional do editor (plano GT).
 *
 * Nada é coletado sem o consentimento dado em Configurações. Com ele, cada
 * abertura do editor ganha uma sessão: o renderer grava janelas agregadas de
 * quadros na fila em disco e o shell as envia quando volta à lista de projetos.
 * Vai só o que a folha de Configurações descreve: modelo do aparelho, GPU,
 * versão do app e números de quadro. Nunca nomes, arquivos ou conteúdo de projeto.
 *
 * O jogo exportado não passa por aqui: ele não tem shell, não recebe as opções
 * de telemetria e é publicado sem a permissão de internet.
 */
final class Telemetry {
    static final String ENDPOINT = "https://astraengine.com.br/api/telemetry/ingest/";

    enum Consent { UNKNOWN, GRANTED, DECLINED }

    interface Callback { void done(String message); }

    private static final String PREFERENCES = "astra.telemetry";
    private static final ExecutorService WORKER = Executors.newSingleThreadExecutor();

    private final Context context;
    private final SharedPreferences preferences;
    private final TelemetryQueue queue;
    private final Handler main = new Handler(Looper.getMainLooper());

    Telemetry(Context context) {
        this.context = context.getApplicationContext();
        this.preferences = this.context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE);
        this.queue = new TelemetryQueue(new File(this.context.getFilesDir(), "telemetry"));
    }

    Consent consent() {
        String value = preferences.getString("consent", Consent.UNKNOWN.name());
        try { return Consent.valueOf(value); } catch (IllegalArgumentException error) { return Consent.UNKNOWN; }
    }

    void setConsent(boolean granted) {
        preferences.edit().putString("consent", (granted ? Consent.GRANTED : Consent.DECLINED).name()).apply();
        // Recusar depois de aceitar descarta o que ainda não saiu do aparelho.
        if (!granted) WORKER.execute(queue::clear);
    }

    int pendingWindows() { return queue.pendingWindows(); }

    long lastUploadMillis() { return preferences.getLong("last_upload", 0L); }

    /** Liga a coleta na abertura do editor. Medições de bancada ficam de fora. */
    void addLaunchExtras(Intent intent, boolean benchmarkRun) {
        if (consent() != Consent.GRANTED || benchmarkRun) return;
        String session = UUID.randomUUID().toString();
        try {
            queue.beginSession(session, isoNow());
        } catch (IOException error) {
            return;
        }
        intent.putExtra("aether.telemetry_dir", queue.directory().getAbsolutePath());
        intent.putExtra("aether.telemetry_session", session);
    }

    /** Envia a fila em segundo plano; o callback roda na thread principal. */
    void upload(Callback callback) {
        if (consent() != Consent.GRANTED) return;
        WORKER.execute(() -> {
            TelemetryQueue.Result result;
            try {
                result = queue.flush(installation(), app(), device(), Telemetry::post, System.currentTimeMillis());
            } catch (JSONException error) {
                return;
            }
            if (result.sent > 0) preferences.edit().putLong("last_upload", System.currentTimeMillis()).apply();
            String message = result.interrupted ? "Sem conexão com o servidor; o envio fica para depois."
                    : result.sent > 0 ? result.sent + (result.sent == 1 ? " janela enviada." : " janelas enviadas.")
                    : "Nada pendente.";
            if (callback != null) main.post(() -> callback.done(message));
        });
    }

    /** Apaga no servidor tudo o que esta instalação enviou e recomeça com outra identidade. */
    void forget(Callback callback) {
        String installation = preferences.getString("installation", null);
        WORKER.execute(() -> {
            queue.clear();
            int status = 200;
            if (installation != null) {
                try {
                    status = post(new JSONObject().put("schema", "astra.telemetry.delete/1")
                            .put("installation", installation).toString());
                } catch (JSONException error) {
                    status = -1;
                }
            }
            boolean removed = status == 200;
            if (removed) preferences.edit().remove("installation").remove("last_upload").apply();
            String message = removed ? "Dados apagados do servidor e deste aparelho."
                    : "Não foi possível falar com o servidor. Os dados locais foram apagados; tente de novo.";
            if (callback != null) main.post(() -> callback.done(message));
        });
    }

    private String installation() {
        String value = preferences.getString("installation", null);
        if (value == null) {
            value = UUID.randomUUID().toString();
            preferences.edit().putString("installation", value).apply();
        }
        return value;
    }

    private JSONObject app() throws JSONException {
        return new JSONObject()
                .put("package", context.getPackageName())
                .put("version", dev.aether.editor.BuildConfig.VERSION_NAME)
                .put("code", dev.aether.editor.BuildConfig.VERSION_CODE);
    }

    private JSONObject device() throws JSONException {
        JSONObject device = new JSONObject()
                .put("model", Build.MODEL)
                .put("manufacturer", Build.MANUFACTURER)
                .put("sdk", Build.VERSION.SDK_INT);
        ActivityManager activities = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        if (activities != null) {
            ActivityManager.MemoryInfo memory = new ActivityManager.MemoryInfo();
            activities.getMemoryInfo(memory);
            device.put("ram_mb", memory.totalMem / (1024L * 1024L));
        }
        WindowManager windows = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        if (windows != null) {
            Display display = windows.getDefaultDisplay();
            DisplayMetrics metrics = new DisplayMetrics();
            display.getRealMetrics(metrics);
            device.put("refresh_hz", Math.round(display.getRefreshRate()));
            device.put("screen", new JSONArray().put(Math.max(metrics.widthPixels, metrics.heightPixels))
                    .put(Math.min(metrics.widthPixels, metrics.heightPixels)));
        }
        return device;
    }

    private static int post(String body) {
        HttpURLConnection connection = null;
        try {
            connection = (HttpURLConnection) new URL(ENDPOINT).openConnection();
            connection.setRequestMethod("POST");
            connection.setConnectTimeout(10000);
            connection.setReadTimeout(20000);
            connection.setDoOutput(true);
            connection.setRequestProperty("content-type", "application/json; charset=utf-8");
            byte[] bytes = body.getBytes(StandardCharsets.UTF_8);
            connection.setFixedLengthStreamingMode(bytes.length);
            try (OutputStream output = connection.getOutputStream()) { output.write(bytes); }
            int status = connection.getResponseCode();
            InputStream stream = status >= 400 ? connection.getErrorStream() : connection.getInputStream();
            if (stream != null) stream.close();
            return status;
        } catch (IOException error) {
            return -1;
        } finally {
            if (connection != null) connection.disconnect();
        }
    }

    private static String isoNow() {
        SimpleDateFormat format = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'", Locale.US);
        format.setTimeZone(TimeZone.getTimeZone("UTC"));
        return format.format(new Date());
    }
}
