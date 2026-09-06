package dev.aether.editor.shell;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.view.WindowInsets;
import android.widget.FrameLayout;
import android.widget.Toast;

import dev.aether.editor.AetherActivity;

/**
 * Porta de entrada do ASTRA: logo, projetos, criação e carregamento.
 *
 * O shell fica fora do NativeActivity de propósito. Ele precisa existir antes
 * do Vulkan subir — é ele que decide qual projeto o editor vai abrir — e o
 * único caminho até a cena passa por aqui.
 */
public final class AstraShellActivity extends Activity implements ShellView.Listener {
    private static final long SPLASH_MILLIS = 2200L;
    private static final long LOAD_MILLIS = 1800L;

    private FrameLayout root;
    private ShellView shell;
    private ProjectStore store;
    private NewProjectSheet sheet;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().setStatusBarColor(Color.BLACK);
        getWindow().setNavigationBarColor(Color.BLACK);

        store = new ProjectStore(this);
        shell = new ShellView(this, store);
        shell.setListener(this);

        root = new FrameLayout(this);
        root.setBackgroundColor(Design.VOID);
        root.addView(shell, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        setContentView(root);

        shell.showSplash(SPLASH_MILLIS);
    }

    @Override public void onLoadFinished(Project project) {
        if (project == null) { shell.showProjects(); return; }
        openInEditor(project);
        shell.showProjects();
    }

    @Override public void onProjectPicked(Project project) {
        shell.showLoading(project, LOAD_MILLIS);
    }

    @Override public void onNewProjectRequested() {
        if (sheet != null && sheet.getParent() != null) return;
        sheet = new NewProjectSheet(this, store, new NewProjectSheet.Listener() {
            @Override public void onCreate(String name, SceneTemplate template) {
                Project created = store.create(name, template);
                sheet.dismiss();
                if (created == null) {
                    toast("Não foi possível criar a pasta do projeto.");
                    return;
                }
                shell.showLoading(created, LOAD_MILLIS);
            }

            @Override public void onUnavailable(SceneTemplate template) {
                toast(template.name + " ainda não existe na engine.");
            }
        });
        root.addView(sheet, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
    }

    @Override public void onNavUnavailable(String label) {
        toast(label + ": em breve.");
    }

    /**
     * O editor ainda é o preview nativo por cena. Enquanto ele não abre projetos
     * de verdade, o shell traduz o template para o preview equivalente e avisa
     * quando não existe nenhum.
     */
    private void openInEditor(Project project) {
        SceneTemplate template = SceneTemplate.byId(project.templateId);
        if (template.previewExtra == null) {
            toast(project.name + ": cena vazia — editor em breve.");
            return;
        }
        Intent intent = new Intent(this, AetherActivity.class);
        intent.putExtra(template.previewExtra, true);
        intent.putExtra("aether.free_camera", "aether.ocean_preview".equals(template.previewExtra));
        intent.putExtra("aether.target_fps", 120.0f);
        intent.putExtra("astra.project_path", project.path);
        intent.putExtra("astra.project_name", project.name);
        startActivity(intent);
    }

    private void toast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
    }

    @Override public void onBackPressed() {
        if (sheet != null && sheet.getParent() != null) { sheet.dismiss(); return; }
        super.onBackPressed();
    }

    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if (focused && android.os.Build.VERSION.SDK_INT >= 30) {
            android.view.WindowInsetsController controller =
                    getWindow().getDecorView().getWindowInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
            }
        }
    }
}
