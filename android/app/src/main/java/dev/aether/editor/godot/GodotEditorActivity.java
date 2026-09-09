package dev.aether.editor.godot;

import android.widget.Toast;
import org.godotengine.godot.GodotActivity;

/** Hospeda os controles originais SceneTreeDock, CreateDialog e EditorInspector.
 * O processo separado isola o lifecycle Godot do shell e do runtime Aether.
 */
public final class GodotEditorActivity extends GodotActivity {
    @Override
    public int onNewGodotInstanceRequested(String[] arguments) {
        java.util.List<String> options = java.util.Arrays.asList(arguments);
        if (options.contains("--project-manager") || options.contains("-p")) {
            runOnUiThread(() -> startActivity(new android.content.Intent(this,
                dev.aether.editor.shell.AstraShellActivity.class)
                .addFlags(android.content.Intent.FLAG_ACTIVITY_CLEAR_TOP | android.content.Intent.FLAG_ACTIVITY_SINGLE_TOP)));
            return 0;
        }
        if (options.contains("--editor") || options.contains("-e"))
            return super.onNewGodotInstanceRequested(arguments);
        // A UI foi incorporada antes da ponte de execucao, por decisao do projeto.
        // Nunca iniciar um jogo Godot e apresenta-lo como runtime Aether.
        runOnUiThread(() -> Toast.makeText(this,
            "Execução ainda não conectada ao runtime Aether.", Toast.LENGTH_LONG).show());
        return -1;
    }
}
