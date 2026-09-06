package dev.aether.editor.shell;

import android.content.Context;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;

/**
 * Índice de projetos em disco.
 *
 * A gravação é atômica (arquivo temporário, fsync, rename) porque o editor roda
 * no aparelho e pode ser morto pelo sistema a qualquer momento: um índice
 * truncado apagaria a lista de projetos do usuário.
 */
public final class ProjectStore {
    private static final String TAG = "AstraShell";
    private static final String INDEX = "projects.json";

    private final Context context;
    private final List<Project> projects = new ArrayList<>();

    public ProjectStore(Context context) {
        this.context = context.getApplicationContext();
        load();
    }

    public List<Project> projects() { return projects; }

    /** Raiz dos projetos do usuário, visível pelo gerenciador de arquivos. */
    public File root() {
        File external = context.getExternalFilesDir(null);
        File root = new File(external != null ? external : context.getFilesDir(), "Projects");
        if (!root.isDirectory() && !root.mkdirs()) Log.w(TAG, "sem raiz de projetos: " + root);
        return root;
    }

    private File indexFile() { return new File(context.getFilesDir(), INDEX); }

    private void load() {
        projects.clear();
        File file = indexFile();
        if (!file.isFile()) { seed(); return; }
        try {
            String raw = new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
            JSONArray array = new JSONArray(raw);
            for (int i = 0; i < array.length(); ++i) projects.add(Project.fromJson(array.getJSONObject(i)));
        } catch (Exception error) {
            Log.w(TAG, "índice ilegível, recomeçando", error);
            seed();
        }
    }

    /**
     * Primeira execução: a prateleira já vem com os quatro projetos de exemplo.
     *
     * Dois deles apontam para cenas que a engine ainda não monta. Eles ficam
     * assim mesmo, com selo EM BREVE no card, porque a alternativa — sumir com
     * eles — esconderia o que falta construir e faria a tela mentir sobre o
     * estado da engine.
     */
    private void seed() {
        projects.add(newProject("Forest Test", SceneTemplate.byId("forest"), 1, 214));
        projects.add(newProject("Water Lab", SceneTemplate.byId("ocean"), 2, 96));
        projects.add(newProject("Backroom Demo", SceneTemplate.byId("backroom"), 1, 357));
        projects.add(newProject("Vehicle Sandbox", SceneTemplate.byId("vehicle"), 1, 128));
        save();
    }

    private Project newProject(String name, SceneTemplate template, int scenes, int assets) {
        File directory = new File(root(), name);
        return new Project(name, directory.getAbsolutePath(), template.id, template.thumbnail, scenes, assets);
    }

    public boolean exists(String name) {
        for (Project project : projects) if (project.name.equalsIgnoreCase(name)) return true;
        return false;
    }

    /** Cria a pasta e o descritor do projeto; devolve null se o disco recusar. */
    public Project create(String name, SceneTemplate template) {
        File directory = new File(root(), name);
        File scenes = new File(directory, "scenes");
        if (!scenes.isDirectory() && !scenes.mkdirs()) {
            Log.w(TAG, "não foi possível criar " + scenes);
            return null;
        }
        Project project = new Project(name, directory.getAbsolutePath(), template.id,
                template.thumbnail, 1, template.id.equals(SceneTemplate.EMPTY) ? 0 : 24);
        try {
            JSONObject descriptor = new JSONObject();
            descriptor.put("format", "ASTRA-PROJECT-1");
            descriptor.put("project", project.toJson());
            descriptor.put("mainScene", "scenes/main.ascene");
            writeAtomic(new File(directory, "project.json"), descriptor.toString(2));
            writeAtomic(new File(scenes, "main.ascene"), sceneStub(template));
        } catch (Exception error) {
            Log.w(TAG, "projeto criado sem descritor", error);
        }
        projects.add(0, project);
        save();
        return project;
    }

    private String sceneStub(SceneTemplate template) {
        return "{\n  \"format\": \"ASTRA-SCENE-1\",\n  \"template\": \"" + template.id + "\",\n"
                + "  \"nodes\": []\n}\n";
    }

    public void save() {
        try {
            JSONArray array = new JSONArray();
            for (Project project : projects) array.put(project.toJson());
            writeAtomic(indexFile(), array.toString(2));
        } catch (Exception error) {
            Log.w(TAG, "índice não gravado", error);
        }
    }

    private static void writeAtomic(File target, String content) throws IOException {
        File temporary = new File(target.getParentFile(), target.getName() + ".tmp");
        try (FileOutputStream stream = new FileOutputStream(temporary)) {
            stream.write(content.getBytes(StandardCharsets.UTF_8));
            stream.getFD().sync();
        }
        if (!temporary.renameTo(target)) {
            throw new IOException("rename falhou: " + temporary + " -> " + target);
        }
        // Um rename só é durável depois que o diretório também é sincronizado.
        try (RandomAccessFile directory = new RandomAccessFile(target.getParentFile(), "r")) {
            directory.getFD().sync();
        } catch (IOException ignored) {
            // Alguns sistemas de arquivos do Android recusam abrir diretórios;
            // o rename já é atômico, então isso só custa durabilidade extra.
        }
    }
}
