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

    @Override public void onSplashFinished() {
        shell.showProjects();
    }

    @Override public void onProjectPicked(Project project) {
        beginOpening(project);
    }

    /**
     * R1: a tela de carregamento cobre a troca de Activity pelo tempo que ela
     * realmente leva. Antes havia 1,8 s de espera fixa com barra inventada antes
     * de sequer pedir o editor. Se a abertura é recusada aqui, volta aos projetos.
     */
    private void beginOpening(Project project) {
        shell.showLoading(project);
        shell.post(() -> { if (!openInEditor(project)) shell.showProjects(); });
    }

    @Override protected void onStop() {
        super.onStop();
        // O editor já cobre o shell; ao voltar, a lista de projetos é o ponto de partida.
        if (shell.screen() == ShellView.Screen.LOADING) shell.showProjects();
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
                beginOpening(created);
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

    /** Abre o editor nativo Aether mantendo o projeto escolhido no shell. */
    private boolean openInEditor(Project project) {
        SceneTemplate template = SceneTemplate.byId(project.templateId);
        if (template.previewExtra == null && !SceneTemplate.EMPTY.equals(template.id)) {
            toast(project.name + ": cena indisponível.");
            return false;
        }
        java.io.File scenes = new java.io.File(project.path, "scenes");
        if (!scenes.isDirectory() && !scenes.mkdirs()) {
            toast("Não foi possível abrir a pasta de cenas do projeto.");
            return false;
        }
        Intent intent = new Intent(this, AetherActivity.class);
        final boolean independent;
        try {
            independent = ProjectSceneSource.isIndependent(project);
        } catch (java.io.IOException error) {
            android.util.Log.e("AstraProjects", "Falha ao ler a origem da cena", error);
            toast("Não foi possível abrir a cena. O arquivo foi preservado.");
            return false;
        }
        if (!independent && !dev.aether.editor.BuildConfig.INCLUDE_LEGACY_DEMOS) {
            toast("Este projeto depende de um pacote de demonstração legado. Arquivos preservados.");
            android.util.Log.w("AstraProjects", "Projeto legado requer build de migração com pacotes: " + project.path);
            return false;
        }
        intent.putExtra("aether.empty_workspace", independent);
        intent.putExtra(template.previewExtra != null ? template.previewExtra : "aether.map_preview", true);
        intent.putExtra("aether.editor_ui", !"aether.material_preview".equals(template.previewExtra));
        intent.putExtra("aether.editor_empty", SceneTemplate.EMPTY.equals(template.id));
        intent.putExtra("aether.free_camera", "aether.ocean_preview".equals(template.previewExtra));
        intent.putExtra("aether.water_fft", template.spectralWater);
        intent.putExtra("aether.target_fps", 120.0f);
        // Profiling follows the actual project-opening route; the renderer
        // Activity remains private to the application.
        intent.putExtra("aether.profile_frames",
                getIntent().getBooleanExtra("aether.profile_frames", false));
        intent.putExtra("astra.project_path", project.path);
        intent.putExtra("astra.project_name", project.name);
        startActivity(intent);
        return true;
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
