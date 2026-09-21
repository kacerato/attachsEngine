package dev.aether.editor.shell;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

/** Resource source selection, independent of the display name of a project. */
public final class ProjectSceneSource {
    // Keep aligned with editor_archive.cpp; the native reader owns payload migration.
    // Esquecer de subir isto junto com a versão nativa faz o shell RECUSAR abrir
    // qualquer projeto salvo pela build nova, com "Formato de cena não
    // reconhecido" -- foi o que aconteceu ao subir para v12. O teste gerenciado
    // `ArquivoDeCenaTests` compara esta constante com a versão que o escritor
    // nativo emite, para que a divergência falhe no host e não no aparelho.
    private static final int LAST_SUPPORTED_ARCHIVE_VERSION = 13;
    private ProjectSceneSource() {}

    public static boolean isIndependent(Project project) throws IOException {
        if (!SceneTemplate.EMPTY.equals(project.templateId)) return false;
        File archive = new File(project.path, "scenes/editor.aescene");
        if (!archive.exists()) {
            File descriptor = new File(project.path, "project.json");
            if (!descriptor.exists()) return false;
            if (descriptor.length() > 1024 * 1024) throw new IOException("Descritor excede o limite");
            try {
                org.json.JSONObject data = new org.json.JSONObject(new String(
                        Files.readAllBytes(descriptor.toPath()), StandardCharsets.UTF_8));
                return "independent".equals(data.optString("resourceSource", "legacy"));
            } catch (org.json.JSONException error) {
                throw new IOException("Descritor inválido", error);
            }
        }
        // Read only the format header. Full validation remains owned by the
        // native archive reader. Never fall back to a demo on corrupt input.
        try (BufferedReader reader = Files.newBufferedReader(archive.toPath(), StandardCharsets.UTF_8)) {
            StringBuilder header = new StringBuilder();
            for (int c; (c = reader.read()) != -1 && c != '\n'; ) {
                if (header.length() >= 128) throw new IOException("Cabeçalho de cena excede o limite");
                header.append((char)c);
            }
            String[] fields = header.toString().trim().split("\\s+");
            if (fields.length != 4 || !fields[0].equals("AETHER_EDITOR")
                    || !fields[1].matches("[1-9][0-9]?")
                    || Integer.parseInt(fields[1]) > LAST_SUPPORTED_ARCHIVE_VERSION
                    || !fields[2].matches("[0-9]{1,20}")
                    || !fields[3].matches("[0-9]{1,5}")) {
                throw new IOException("Formato de cena não reconhecido");
            }
            return fields[2].equals("0");
        }
    }
}
