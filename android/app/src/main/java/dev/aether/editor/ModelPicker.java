package dev.aether.editor;

import android.app.Activity;
import android.content.Intent;
import android.content.ContentResolver;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.RejectedExecutionException;
import android.database.Cursor;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.provider.OpenableColumns;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

/**
 * O seletor de arquivos do sistema, para importar um modelo.
 *
 * <p>O URI do seletor é {@code content://}, não um caminho de arquivo, e não é
 * convertido em um: quem lê os bytes é o {@link android.content.ContentResolver},
 * que é o único que tem a permissão concedida junto com o URI. Em Android
 * moderno o caminho POSIX correspondente muitas vezes não existe e, quando
 * existe, não é legível pelo aplicativo.
 *
 * <p>O conteúdo atravessa para o nativo de uma vez, com o nome que o provedor
 * declara. Copiar para dentro do projeto é decisão do editor, não daqui.
 */
public final class ModelPicker {
    private static native long poll();
    private static native void submit(long token, byte[] bytes, byte[] name, byte[] diagnostic);

    /** Teto de leitura. Acima disto a importação é recusada com motivo, não travada. */
    private static final int MAXIMUM_BYTES = 128 * 1024 * 1024;
    private static final int REQUEST = 0x6D_6F_64; // "mod"

    // One provider read at a time, no unbounded queue and no Activity retained
    // by the worker. JNI tokens reject completion after cancel/project closure.
    private static final ThreadPoolExecutor READER = new ThreadPoolExecutor(1, 1, 0L,
            TimeUnit.MILLISECONDS, new SynchronousQueue<>(), runnable -> {
                Thread thread = new Thread(runnable, "Astra-AssetReader");
                thread.setDaemon(true); return thread;
            });
    private final Activity activity;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private boolean running;
    private long openToken;

    public ModelPicker(Activity activity) { this.activity = activity; }

    private final Runnable tick = new Runnable() {
        @Override public void run() {
            if (!running) return;
            long token = poll();
            // Um pedido com token diferente do aberto significa que o nativo
            // desistiu do anterior — por exemplo porque o usuário pediu de novo
            // depois de o seletor ter sido morto junto com o processo dele.
            if (token != 0 && token != openToken) open(token);
            handler.postDelayed(this, 120);
        }
    };

    public void start() { if (!running) { running = true; handler.post(tick); } }

    public void stop() {
        // Só o laço de consulta para. O pedido aberto NÃO é cancelado aqui: abrir
        // o seletor pausa esta Activity, e cancelar em `onPause` cancelaria toda
        // importação no exato momento em que o usuário começa a escolher o
        // arquivo. O resultado chega em `onActivityResult`, já de volta.
        running = false;
        handler.removeCallbacks(tick);
    }

    private static byte[] encode(String value) {
        return value == null ? null : value.getBytes(StandardCharsets.UTF_8);
    }

    private void open(long token) {
        openToken = token;
        try {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            // `*/*` de propósito: provedores declaram GLB como octet-stream, como
            // model/gltf-binary ou como nada. Filtrar por tipo esconderia o
            // arquivo que o usuário quer; a extensão é conferida depois, e o
            // conteúdo é validado pelo leitor nativo de qualquer forma.
            intent.setType("*/*");
            activity.startActivityForResult(intent, REQUEST);
        } catch (RuntimeException error) {
            submit(token, null, null, encode("Nenhum aplicativo de arquivos respondeu ao pedido."));
            openToken = 0;
        }
    }

    /** Chamado pela Activity. Devolve verdadeiro quando o resultado era deste seletor. */
    public boolean onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != REQUEST || openToken == 0) return false;
        long token = openToken;
        // Keep this token while the asynchronous reader runs. The polling tick
        // must not reopen the picker for the same still-pending native request.
        Uri uri = data == null ? null : data.getData();
        if (resultCode != Activity.RESULT_OK || uri == null) {
            // Cancelar não é erro: o nativo distingue as duas coisas.
            submit(token, null, null, null);
            return true;
        }
        ContentResolver resolver = activity.getApplicationContext().getContentResolver();
        try {
            READER.execute(() -> readSource(token, uri, resolver));
        } catch (RejectedExecutionException busy) {
            submit(token, null, null, encode("A leitura anterior ainda está terminando; tente novamente."));
        }
        return true;
    }

    private static void readSource(long token, Uri uri, ContentResolver resolver) {
        try (InputStream stream = resolver.openInputStream(uri)) {
            if (stream == null) {
                submit(token, null, null, encode("O arquivo escolhido não pôde ser aberto."));
                return;
            }
            ByteArrayOutputStream buffer = new ByteArrayOutputStream();
            byte[] chunk = new byte[64 * 1024];
            int read;
            while ((read = stream.read(chunk)) > 0) {
                if (poll() != token) return;
                if (buffer.size() + read > MAXIMUM_BYTES) {
                    submit(token, null, null, encode("O arquivo é grande demais para esta importação."));
                    return;
                }
                buffer.write(chunk, 0, read);
            }
            submit(token, buffer.toByteArray(), encode(displayName(uri, resolver)), null);
        } catch (Exception error) {
            submit(token, null, null, encode("Não foi possível ler o arquivo escolhido."));
        }
        return;
    }

    private static String displayName(Uri uri, ContentResolver resolver) {
        try (Cursor cursor = resolver.query(uri, null, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int column = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (column >= 0) {
                    String value = cursor.getString(column);
                    if (value != null && !value.isEmpty()) return value;
                }
            }
        } catch (Exception ignored) {
            // Sem nome utilizável: o editor usa um padrão. Falhar a importação
            // por causa do nome seria trocar um problema pequeno por um grande.
        }
        String path = uri.getLastPathSegment();
        if (path == null || path.isEmpty()) return "modelo.glb";
        int slash = path.lastIndexOf('/');
        return slash >= 0 && slash + 1 < path.length() ? path.substring(slash + 1) : path;
    }
}
