package dev.aether.editor;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.widget.Toast;

import java.nio.charset.StandardCharsets;

/**
 * Abre no navegador os links que o editor nativo pede (a página de referência
 * de um componente, pelo menu do Inspector). O nativo enfileira; aqui só se
 * consulta, confere o esquema e entrega ao sistema. Só https: o editor nunca
 * pede outro esquema, e aceitar um `intent:` ou `file:` vindo de dado autoral
 * abriria outra aplicação sem o usuário saber.
 */
public final class ExternalLinks {
    private static native byte[] poll();

    private final Activity activity;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private boolean running;

    public ExternalLinks(Activity activity) { this.activity = activity; }

    private final Runnable tick = new Runnable() {
        @Override public void run() {
            if (!running) return;
            byte[] link = poll();
            if (link != null) open(new String(link, StandardCharsets.UTF_8));
            handler.postDelayed(this, 150);
        }
    };

    private void open(String link) {
        if (!link.startsWith("https://")) return;
        try {
            activity.startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(link)));
        } catch (ActivityNotFoundException error) {
            Toast.makeText(activity, "Nenhum navegador disponível para abrir a referência.", Toast.LENGTH_LONG).show();
        }
    }

    public void start() { if (!running) { running = true; handler.post(tick); } }
    public void stop() { running = false; handler.removeCallbacks(tick); }
}
