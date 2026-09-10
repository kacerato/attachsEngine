package dev.aether.editor.shell;

import android.content.Context;

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
import java.util.Collections;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.nio.file.StandardCopyOption;

/**
 * Índice de projetos em disco.
 *
 * A gravação é atômica (arquivo temporário, fsync, rename) porque o editor roda
 * no aparelho e pode ser morto pelo sistema a qualquer momento: um índice
 * truncado apagaria a lista de projetos do usuário.
 */
public final class ProjectStore {
    private static final Logger LOG = Logger.getLogger("AstraShell");
    private static final String INDEX = "projects.json";

    private final File privateRoot;
    private final File projectRoot;
    private boolean writable = true;
    private final List<Project> projects = new ArrayList<>();

    public ProjectStore(Context context) {
        this(context.getFilesDir(), new File(context.getExternalFilesDir(null) != null
                ? context.getExternalFilesDir(null) : context.getFilesDir(), "Projetos"));
    }

    ProjectStore(File privateRoot, File projectRoot) {
        this.privateRoot = privateRoot;
        this.projectRoot = projectRoot;
        load();
    }

    public List<Project> projects() { return Collections.unmodifiableList(projects); }

    /** Raiz dos projetos do usuário, visível pelo gerenciador de arquivos. */
    public File root() {
        if (!projectRoot.isDirectory() && !projectRoot.mkdirs()) LOG.warning("sem raiz de projetos: " + projectRoot);
        return projectRoot;
    }

    private File indexFile() { return new File(privateRoot, INDEX); }

    private void load() {
        projects.clear();
        File file = indexFile();
        if (file.isFile()) try {
            String raw = new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
            JSONArray array = new JSONArray(raw);
            for (int i = 0; i < array.length(); ++i) {
                Project project = Project.fromJson(array.getJSONObject(i));
                // Old seeded cards never had project files. Keep real user data,
                // including legacy folders, and omit only entries with no folder.
                if (!project.path.isEmpty() && new File(project.path).isDirectory()) projects.add(project);
            }
        } catch (Exception error) {
            LOG.log(Level.WARNING, "índice ilegível; recuperando descritores", error);
            projects.clear();
            try {
                File backup = File.createTempFile("projects-corrupt-", ".json", privateRoot);
                Files.copy(file.toPath(), backup.toPath(), StandardCopyOption.REPLACE_EXISTING);
            } catch (IOException backupError) {
                writable = false;
                LOG.log(Level.WARNING, "backup falhou; índice protegido contra sobrescrita", backupError);
            }
        }
        File[] directories = projectRoot.listFiles(File::isDirectory);
        if (directories == null) return;
        for (File directory : directories) {
            File descriptor = new File(directory, "project.json");
            if (!descriptor.isFile()) continue;
            try {
                JSONObject json = new JSONObject(new String(Files.readAllBytes(descriptor.toPath()), StandardCharsets.UTF_8));
                if (!"ASTRA-PROJECT-1".equals(json.optString("format"))) continue;
                Project stored = Project.fromJson(json.getJSONObject("project"));
                boolean known = false;
                for (Project project : projects) if (new File(project.path).getCanonicalFile().equals(directory.getCanonicalFile())) known = true;
                if (!known) projects.add(new Project(stored.name, directory.getAbsolutePath(), stored.templateId,
                        stored.thumbnail, stored.scenes, stored.assets));
            } catch (Exception error) { LOG.log(Level.WARNING, "descritor ilegível: " + descriptor, error); }
        }
    }

    public boolean exists(String name) {
        for (Project project : projects) if (project.name.equalsIgnoreCase(name)) return true;
        return false;
    }

    /** Cria a pasta e o descritor do projeto; devolve null se o disco recusar. */
    public Project create(String name, SceneTemplate template) {
        if (!writable || name == null || template == null || !template.ready
                || !SceneTemplate.EMPTY.equals(template.id)) return null;
        name = name.trim();
        if (name.isEmpty() || name.equals(".") || name.equals("..") || name.matches(".*[\\\\/:\\p{Cntrl}].*") || exists(name)) return null;
        File directory = new File(root(), name);
        if (directory.exists()) return null;
        File scenes = new File(directory, "scenes");
        if (!scenes.isDirectory() && !scenes.mkdirs()) {
            LOG.warning("não foi possível criar " + scenes);
            return null;
        }
        Project project = new Project(name, directory.getAbsolutePath(), template.id,
                template.thumbnail, 1, 0);
        try {
            JSONObject descriptor = new JSONObject();
            descriptor.put("format", "ASTRA-PROJECT-1");
            descriptor.put("resourceSource", "independent");
            descriptor.put("project", project.toJson());
            descriptor.put("mainScene", "scenes/main.ascene");
            descriptor.put("editorScene", "scenes/editor.aescene");
            writeAtomic(new File(scenes, "main.ascene"), sceneStub(template));
            writeAtomic(new File(directory, "project.json"), descriptor.toString(2));
        } catch (Exception error) {
            LOG.log(Level.WARNING, "falha ao criar projeto", error);
            return null;
        }
        projects.add(0, project);
        if (!save()) { projects.remove(project); return null; }
        return project;
    }

    private String sceneStub(SceneTemplate template) {
        return "{\n  \"format\": \"ASTRA-SCENE-1\",\n  \"template\": \"" + template.id + "\",\n"
                + "  \"nodes\": []\n}\n";
    }

    public boolean save() {
        if (!writable) return false;
        try {
            JSONArray array = new JSONArray();
            for (Project project : projects) array.put(project.toJson());
            writeAtomic(indexFile(), array.toString(2));
            return true;
        } catch (Exception error) {
            LOG.log(Level.WARNING, "índice não gravado", error);
            return false;
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
