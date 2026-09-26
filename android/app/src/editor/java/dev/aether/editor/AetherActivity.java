package dev.aether.editor;

import android.app.NativeActivity;
import android.content.Intent;
import android.os.Bundle;

/** Platform lifecycle and text input only; authoring controls belong to the native editor. */
public final class AetherActivity extends NativeActivity {
    // Associate the JNI text bridge with this class loader as well as loading
    // the NativeActivity entry point through manifest metadata.
    static { System.loadLibrary("aether_android"); }
    private EditorTextInput editorTextInput;
    private ModelPicker modelPicker;
    private ExternalLinks externalLinks;

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
    }

    @Override protected void onResume() {
        super.onResume();
        if (editorTextInput == null) editorTextInput = new EditorTextInput(this);
        editorTextInput.start();
        if (modelPicker == null) modelPicker = new ModelPicker(this);
        modelPicker.start();
        if (externalLinks == null) externalLinks = new ExternalLinks(this);
        externalLinks.start();
    }

    @Override protected void onPause() {
        if (editorTextInput != null) editorTextInput.stop();
        if (modelPicker != null) modelPicker.stop();
        if (externalLinks != null) externalLinks.stop();
        super.onPause();
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
