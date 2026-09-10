package dev.aether.editor.godot;

import android.content.Context;
import android.content.Intent;
import android.util.AtomicFile;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

/** Prepara somente os arquivos da interface; nao converte nem sobrescreve cenas Aether. */
public final class GodotEditorProject {
    private GodotEditorProject() {}

    public static Intent intent(Context context, String path, String name) throws IOException {
        File directory = new File(path).getCanonicalFile();
        if (!directory.isDirectory()) throw new IOException("Pasta do projeto indisponível");
        prepareScale(context);
        writeIfAbsent(new File(directory, "project.godot"),
            "; Interface original Godot. Cenas Aether permanecem em seu formato próprio.\n"
            + "config_version=5\n\n[application]\nconfig/name=\"" + quote(name) + "\"\n"
            + "\n[rendering]\nrenderer/rendering_method=\"gl_compatibility\"\n"
            + "renderer/rendering_method.mobile=\"gl_compatibility\"\n");
        Intent intent = new Intent(context, GodotEditorActivity.class);
        intent.putExtra("command_line_params", new String[]{"--editor", "--path", directory.getPath(),
            "--language", "pt", "--single-window", "--rendering-method", "gl_compatibility"});
        intent.putExtra("astra.project_path", directory.getPath());
        intent.putExtra("astra.project_name", name);
        return intent;
    }

    private static String quote(String text) {
        return text.replace("\\", "\\\\").replace("\"", "\\\"")
            .replace("\r", "\\r").replace("\n", "\\n").replace("\t", "\\t");
    }

    private static void writeIfAbsent(File destination, String text) throws IOException {
        if (destination.isFile()) return;
        write(destination, text);
    }

    private static void prepareScale(Context context) throws IOException {
        android.content.SharedPreferences preferences = context.getSharedPreferences("godot_ui", Context.MODE_PRIVATE);
        File iconTheme = new File(context.getFilesDir(), "astra-editor-icons.tres");
        try (java.io.InputStream input = context.getAssets().open("godot-ui/astra-icons.tres")) {
            java.io.ByteArrayOutputStream bytes = new java.io.ByteArrayOutputStream();
            byte[] buffer = new byte[8192];
            int count;
            while ((count = input.read(buffer)) != -1) bytes.write(buffer, 0, count);
            write(iconTheme, new String(bytes.toByteArray(), StandardCharsets.UTF_8));
        }
        if (preferences.getBoolean("compact_icons_v1", false)) return;
        File settings = new File(context.getFilesDir(), "config/godot/editor_settings-4.7.tres");
        File parent = settings.getParentFile();
        if (!parent.isDirectory() && !parent.mkdirs()) throw new IOException("Não foi possível preparar a escala do editor");
        String text = settings.isFile() ? new String(Files.readAllBytes(settings.toPath()), StandardCharsets.UTF_8)
            : "[gd_resource type=\"EditorSettings\" format=3]\n\n[resource]\n";
        text = setting(text, "interface/editor/appearance/display_scale", "7");
        text = setting(text, "interface/editor/appearance/custom_display_scale", "1.5");
        text = setting(text, "interface/editor/localization/editor_language", "\"pt_BR\"");
        text = setting(text, "interface/theme/custom_theme", "\"" + quote(iconTheme.getPath()) + "\"");
        write(settings, text);
        if (!preferences.edit().putBoolean("compact_icons_v1", true).commit())
            throw new IOException("Não foi possível registrar a configuração do editor");
    }

    private static String setting(String text, String name, String value) {
        String expression = "(?m)^" + java.util.regex.Pattern.quote(name) + "\\s*=.*$";
        String assignment = name + " = " + value;
        return java.util.regex.Pattern.compile(expression).matcher(text).find()
            ? text.replaceAll(expression, java.util.regex.Matcher.quoteReplacement(assignment))
            : text + "\n" + assignment + "\n";
    }

    private static void write(File destination, String text) throws IOException {
        AtomicFile file = new AtomicFile(destination);
        FileOutputStream stream = null;
        try {
            stream = file.startWrite();
            stream.write(text.getBytes(StandardCharsets.UTF_8));
            file.finishWrite(stream);
        } catch (IOException error) {
            if (stream != null) file.failWrite(stream);
            throw error;
        }
    }
}
