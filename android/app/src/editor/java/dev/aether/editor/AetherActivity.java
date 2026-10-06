package dev.aether.editor;

import android.app.NativeActivity;
import android.content.Intent;
import android.os.Bundle;
import android.media.AudioManager;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;

/** Platform lifecycle and text input only; authoring controls belong to the native editor. */
public final class AetherActivity extends NativeActivity {
    // Associate the JNI text bridge with this class loader as well as loading
    // the NativeActivity entry point through manifest metadata.
    static { System.loadLibrary("aether_android"); }
    private EditorTextInput editorTextInput;
    private ModelPicker modelPicker;
    private ExternalLinks externalLinks;
    private volatile int playAudioFocusState; // 0 denied/released, 1 authorized, 2 delayed, -1 lost
    private volatile boolean playAudioDemand;
    private boolean playAudioForeground;
    private AudioManager playAudioManager;
    private AudioFocusRequest playAudioRequest;
    private AudioManager.OnAudioFocusChangeListener playAudioListener;
    private long playAudioGeneration;

    /** Called by the native frame loop. It only schedules platform work on the UI thread. */
    public void setPlayAudioDemand(boolean wanted) {
        if (playAudioDemand == wanted) return;
        playAudioDemand = wanted;
        if (!wanted) playAudioFocusState = 0;
        runOnUiThread(() -> { if (wanted && playAudioDemand && playAudioForeground) acquirePlayAudioFocus(); else releasePlayAudioFocus(); });
    }
    public int getPlayAudioFocusState() { return playAudioFocusState; }
    @SuppressWarnings("deprecation")
    private void acquirePlayAudioFocus() {
        if (playAudioListener != null || !playAudioDemand || !playAudioForeground) return;
        if (playAudioManager == null) playAudioManager = (AudioManager)getSystemService(AUDIO_SERVICE);
        if (playAudioManager == null) { playAudioFocusState = 0; return; }
        final long generation = ++playAudioGeneration;
        playAudioListener = change -> {
            if (generation != playAudioGeneration || !playAudioDemand || !playAudioForeground) return;
            playAudioFocusState = change == AudioManager.AUDIOFOCUS_GAIN ? 1 : -1;
            // Permanent loss is not retried every frame. A new Play or Activity
            // foreground transition is the explicit next request boundary.
        };
        int result;
        try {
            if (Build.VERSION.SDK_INT >= 26) {
                playAudioRequest = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                    .setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
                    .setAcceptsDelayedFocusGain(true).setWillPauseWhenDucked(true)
                    .setOnAudioFocusChangeListener(playAudioListener, new Handler(Looper.getMainLooper())).build();
                result = playAudioManager.requestAudioFocus(playAudioRequest);
            } else result = playAudioManager.requestAudioFocus(playAudioListener, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
            playAudioFocusState = result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED ? 1 :
                result == AudioManager.AUDIOFOCUS_REQUEST_DELAYED ? 2 : 0;
        } catch (RuntimeException failure) { playAudioFocusState = 0; android.util.Log.w("Astra.Audio", "Audio focus denied", failure); }
    }
    @SuppressWarnings("deprecation")
    private void releasePlayAudioFocus() {
        ++playAudioGeneration; playAudioFocusState = 0;
        try {
            if (playAudioManager != null && playAudioListener != null) {
                if (Build.VERSION.SDK_INT >= 26 && playAudioRequest != null) playAudioManager.abandonAudioFocusRequest(playAudioRequest);
                else playAudioManager.abandonAudioFocus(playAudioListener);
            }
        } catch (RuntimeException failure) {
            android.util.Log.w("Astra.Audio", "Audio focus release failed", failure);
        } finally { playAudioListener = null; playAudioRequest = null; }
    }

    @Override protected void onCreate(Bundle state) {
        // NativeActivity starts native code in super.onCreate: establish the dry
        // editor route first, including launches that do not originate in the shell.
        Intent intent = getIntent();
        intent.putExtra("aether.empty_workspace", true);
        intent.putExtra("aether.editor_empty", true);
        intent.putExtra("aether.editor_ui", true);
        intent.putExtra("aether.map_preview", true);
        intent.putExtra("aether.ocean_preview", false);
        intent.putExtra("aether.material_preview", false);
        intent.putExtra("aether.water_fft", false);
        super.onCreate(state);
        // Teclas de volume ajustam a mídia (o fluxo do Play) mesmo sem som tocando;
        // sem isso cada fabricante escolhe o fluxo padrão (toque, em vários aparelhos).
        setVolumeControlStream(AudioManager.STREAM_MUSIC);
    }

    @Override protected void onResume() {
        super.onResume();
        playAudioForeground = true;
        if (playAudioDemand) acquirePlayAudioFocus();
        if (editorTextInput == null) editorTextInput = new EditorTextInput(this);
        editorTextInput.start();
        if (modelPicker == null) modelPicker = new ModelPicker(this);
        modelPicker.start();
        if (externalLinks == null) externalLinks = new ExternalLinks(this);
        externalLinks.start();
    }

    @Override protected void onPause() {
        playAudioForeground = false; releasePlayAudioFocus();
        if (editorTextInput != null) editorTextInput.stop();
        if (modelPicker != null) modelPicker.stop();
        if (externalLinks != null) externalLinks.stop();
        super.onPause();
    }
    @Override protected void onDestroy() {
        playAudioDemand = false; releasePlayAudioFocus(); super.onDestroy();
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        // O seletor de arquivos responde aqui. Quando o resultado não é dele, o
        // NativeActivity continua recebendo o que sempre recebeu.
        if (modelPicker != null && modelPicker.onActivityResult(requestCode, resultCode, data)) return;
        super.onActivityResult(requestCode, resultCode, data);
    }

    @Override protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        // The native session owns the open document for the Activity lifetime.
        if (!java.util.Objects.equals(getIntent().getStringExtra("astra.project_path"),
                                      intent.getStringExtra("astra.project_path"))) {
            android.util.Log.w("Astra.Editor", "Conflicting live project launch rejected");
            android.widget.Toast.makeText(this,
                "Feche o projeto atual antes de abrir outro.",
                android.widget.Toast.LENGTH_LONG).show();
        }
    }
}
