package dev.aether.editor;

import android.app.Activity;
import android.content.Intent;
import android.content.ContentResolver;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.RejectedExecutionException;
import android.database.Cursor;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;

/**
 * O seletor de arquivos do sistema, para importar um modelo.
 *
 * <p>O URI do seletor é {@code content://}, não um caminho de arquivo, e não é
 * convertido em um: quem lê os bytes é o {@link android.content.ContentResolver},
 * que é o único que tem a permissão concedida junto com o URI. Em Android
 * moderno o caminho POSIX correspondente muitas vezes não existe e, quando
 * existe, não é legível pelo aplicativo.
 *
 * <p>O conteúdo atravessa para o nativo de uma vez, com o nome que o provedor
 * declara. Copiar para dentro do projeto é decisão do editor, não daqui.
 */
public final class ModelPicker {
    private static native long poll();
    private static native boolean allowMultiple(long token);
    private static native void submit(long token, byte[] bytes, byte[] name, byte[] diagnostic);
    /**
     * Vários arquivos escolhidos juntos: um .gltf (ou .glb) e as dependências dele.
     * O nativo decide qual é o principal; aqui só se lê conteúdo e nome.
     */
    private static native void submitMany(long token, byte[][] contents, byte[][] names, byte[] diagnostic);

    // --- Pasta de modelo (S0) ----------------------------------------------
    // Fase 1: o usuário escolhe uma pasta; aqui ela é LISTADA (caminhos e
    // tamanhos) e só os arquivos principais (.gltf, ou .glb sem .gltf) são lidos.
    // Fase 2: o nativo decide o que o glTF referencia e pede a cópia exata desses
    // arquivos para a pasta de preparo do projeto, com SHA-256 calculado junto.
    // O .max de 500 MB e as texturas que o modelo não usa nunca são copiados.
    private static native boolean pickFolder(long token);
    private static native void submitFolder(long token, byte[][] paths, long[] sizes, byte[][] mainPaths,
                                            byte[][] mainContents, byte[] folderName, byte[] diagnostic);
    private static native long pollCopy();
    private static native byte[][] copySources(long token);
    private static native byte[][] copyTargets(long token);
    private static native byte[] copyDestination(long token);
    private static native boolean copyCancelled(long token);
    private static native void copyProgress(long token, long done, long total, int files);
    private static native void submitCopy(long token, byte[][] sha256, byte[] diagnostic);

    private static final int MAXIMUM_FOLDER_FILES = 8192;
    private static final int MAXIMUM_FOLDER_DEPTH = 8;
    private static final int REQUEST_TREE = 0x6D_6F_65; // "moe"

    /** Máximo de arquivos numa seleção: um glTF com dezenas de texturas cabe, uma pasta inteira não. */
    private static final int MAXIMUM_FILES = 64;

    /** Teto de leitura. Acima disto a importação é recusada com motivo, não travada. */
    private static final int MAXIMUM_BYTES = 128 * 1024 * 1024;
    private static final int REQUEST = 0x6D_6F_64; // "mod"

    // One provider read at a time, no unbounded queue and no Activity retained
    // by the worker. JNI tokens reject completion after cancel/project closure.
    private static final ThreadPoolExecutor READER = new ThreadPoolExecutor(1, 1, 0L,
            TimeUnit.MILLISECONDS, new SynchronousQueue<>(), runnable -> {
                Thread thread = new Thread(runnable, "Astra-AssetReader");
                thread.setDaemon(true); return thread;
            });
    private final Activity activity;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private boolean running;
    private long openToken;
    private long copyToken;
    // Documentos da última pasta listada, por caminho relativo: é deles que a
    // fase de cópia lê. A permissão do seletor vale enquanto o processo vive.
    private static final java.util.Map<String, Uri> folderDocuments = new java.util.HashMap<>();

    public ModelPicker(Activity activity) { this.activity = activity; }

    private final Runnable tick = new Runnable() {
        @Override public void run() {
            if (!running) return;
            long token = poll();
            // Um pedido com token diferente do aberto significa que o nativo
            // desistiu do anterior — por exemplo porque o usuário pediu de novo
            // depois de o seletor ter sido morto junto com o processo dele.
            if (token != 0 && token != openToken) open(token);
            long copy = pollCopy();
            if (copy != 0 && copy != copyToken) startCopy(copy);
            handler.postDelayed(this, 120);
        }
    };

    public void start() { if (!running) { running = true; handler.post(tick); } }

    public void stop() {
        // Só o laço de consulta para. O pedido aberto NÃO é cancelado aqui: abrir
        // o seletor pausa esta Activity, e cancelar em `onPause` cancelaria toda
        // importação no exato momento em que o usuário começa a escolher o
        // arquivo. O resultado chega em `onActivityResult`, já de volta.
        running = false;
        handler.removeCallbacks(tick);
    }

    private static byte[] encode(String value) {
        return value == null ? null : value.getBytes(StandardCharsets.UTF_8);
    }

    private void open(long token) {
        openToken = token;
        if (pickFolder(token)) {
            try {
                activity.startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), REQUEST_TREE);
            } catch (RuntimeException error) {
                submitFolder(token, null, null, null, null, null, encode("Nenhum aplicativo de arquivos oferece escolha de pasta."));
                openToken = 0;
            }
            return;
        }
        try {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            // `*/*` de propósito: provedores declaram GLB como octet-stream, como
            // model/gltf-binary ou como nada. Filtrar por tipo esconderia o
            // arquivo que o usuário quer; a extensão é conferida depois, e o
            // conteúdo é validado pelo leitor nativo de qualquer forma.
            intent.setType("*/*");
            // Seleção múltipla: um .gltf chega com o .bin e as imagens escolhidos
            // junto. Não há acesso à pasta — só ao que o usuário marcou.
            intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, allowMultiple(token));
            activity.startActivityForResult(intent, REQUEST);
        } catch (RuntimeException error) {
            submit(token, null, null, encode("Nenhum aplicativo de arquivos respondeu ao pedido."));
            openToken = 0;
        }
    }

    /** Chamado pela Activity. Devolve verdadeiro quando o resultado era deste seletor. */
    public boolean onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQUEST_TREE && openToken != 0) {
            long token = openToken;
            Uri tree = resultCode == Activity.RESULT_OK && data != null ? data.getData() : null;
            if (tree == null) {
                submitFolder(token, null, null, null, null, null, null); // cancelado
                return true;
            }
            ContentResolver resolver = activity.getApplicationContext().getContentResolver();
            try {
                READER.execute(() -> listFolder(token, tree, resolver));
            } catch (RejectedExecutionException busy) {
                submitFolder(token, null, null, null, null, null, encode("A leitura anterior ainda está terminando; tente novamente."));
            }
            return true;
        }
        if (requestCode != REQUEST || openToken == 0) return false;
        long token = openToken;
        // Keep this token while the asynchronous reader runs. The polling tick
        // must not reopen the picker for the same still-pending native request.
        java.util.ArrayList<Uri> uris = new java.util.ArrayList<>();
        if (resultCode == Activity.RESULT_OK && data != null) {
            android.content.ClipData clip = data.getClipData();
            if (clip != null) {
                for (int i = 0; i < clip.getItemCount(); ++i) {
                    Uri item = clip.getItemAt(i).getUri();
                    if (item != null) uris.add(item);
                }
            } else if (data.getData() != null) {
                uris.add(data.getData());
            }
        }
        if (uris.isEmpty()) {
            // Cancelar não é erro: o nativo distingue as duas coisas.
            submit(token, null, null, null);
            return true;
        }
        if (uris.size() > MAXIMUM_FILES) {
            submit(token, null, null, encode("Arquivos demais numa seleção; escolha o principal e as dependências dele."));
            return true;
        }
        ContentResolver resolver = activity.getApplicationContext().getContentResolver();
        try {
            if (uris.size() == 1) {
                Uri uri = uris.get(0);
                READER.execute(() -> readSource(token, uri, resolver));
            } else {
                READER.execute(() -> readSources(token, uris, resolver));
            }
        } catch (RejectedExecutionException busy) {
            submit(token, null, null, encode("A leitura anterior ainda está terminando; tente novamente."));
        }
        return true;
    }

    private static void readSources(long token, java.util.List<Uri> uris, ContentResolver resolver) {
        byte[][] contents = new byte[uris.size()][];
        byte[][] names = new byte[uris.size()][];
        long total = 0;
        try {
            for (int i = 0; i < uris.size(); ++i) {
                Uri uri = uris.get(i);
                try (InputStream stream = resolver.openInputStream(uri)) {
                    if (stream == null) {
                        submitMany(token, null, null, encode("Um dos arquivos escolhidos não pôde ser aberto."));
                        return;
                    }
                    ByteArrayOutputStream buffer = new ByteArrayOutputStream();
                    byte[] chunk = new byte[64 * 1024];
                    int read;
                    while ((read = stream.read(chunk)) > 0) {
                        if (poll() != token) return;
                        total += read;
                        if (total > MAXIMUM_BYTES) {
                            submitMany(token, null, null, encode("Os arquivos escolhidos juntos são grandes demais para esta importação."));
                            return;
                        }
                        buffer.write(chunk, 0, read);
                    }
                    contents[i] = buffer.toByteArray();
                    names[i] = encode(displayName(uri, resolver));
                }
            }
            submitMany(token, contents, names, null);
        } catch (Exception error) {
            submitMany(token, null, null, encode("Não foi possível ler os arquivos escolhidos."));
        }
    }

    private static void readSource(long token, Uri uri, ContentResolver resolver) {
        try (InputStream stream = resolver.openInputStream(uri)) {
            if (stream == null) {
                submit(token, null, null, encode("O arquivo escolhido não pôde ser aberto."));
                return;
            }
            ByteArrayOutputStream buffer = new ByteArrayOutputStream();
            byte[] chunk = new byte[64 * 1024];
            int read;
            while ((read = stream.read(chunk)) > 0) {
                if (poll() != token) return;
                if (buffer.size() + read > MAXIMUM_BYTES) {
                    submit(token, null, null, encode("O arquivo é grande demais para esta importação."));
                    return;
                }
                buffer.write(chunk, 0, read);
            }
            submit(token, buffer.toByteArray(), encode(displayName(uri, resolver)), null);
        } catch (Exception error) {
            submit(token, null, null, encode("Não foi possível ler o arquivo escolhido."));
        }
        return;
    }

    private static void listFolder(long token, Uri tree, ContentResolver resolver) {
        java.util.ArrayList<String> paths = new java.util.ArrayList<>();
        java.util.ArrayList<Long> sizes = new java.util.ArrayList<>();
        java.util.ArrayList<Integer> depths = new java.util.ArrayList<>();
        synchronized (folderDocuments) { folderDocuments.clear(); }
        String folderName = "Modelo";
        try {
            String rootId = DocumentsContract.getTreeDocumentId(tree);
            Uri rootDocument = DocumentsContract.buildDocumentUriUsingTree(tree, rootId);
            String rootName = displayName(rootDocument, resolver);
            if (rootName != null && !rootName.isEmpty()) folderName = rootName;
            java.util.ArrayDeque<String[]> pending = new java.util.ArrayDeque<>(); // {documentId, prefixo}
            pending.add(new String[] {rootId, ""});
            String[] columns = {DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                                DocumentsContract.Document.COLUMN_MIME_TYPE, DocumentsContract.Document.COLUMN_SIZE};
            while (!pending.isEmpty()) {
                if (poll() != token) return;
                String[] entry = pending.poll();
                int depth = entry[1].isEmpty() ? 0 : entry[1].split("/").length;
                Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, entry[0]);
                try (Cursor cursor = resolver.query(children, columns, null, null, null)) {
                    while (cursor != null && cursor.moveToNext()) {
                        String id = cursor.getString(0), name = cursor.getString(1), mime = cursor.getString(2);
                        if (id == null || name == null || name.isEmpty() || name.contains("/")) continue;
                        String relative = entry[1].isEmpty() ? name : entry[1] + "/" + name;
                        if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                            if (depth + 1 < MAXIMUM_FOLDER_DEPTH) pending.add(new String[] {id, relative});
                            continue;
                        }
                        if (paths.size() >= MAXIMUM_FOLDER_FILES) {
                            submitFolder(token, null, null, null, null, null,
                                         encode("A pasta tem arquivos demais; escolha a pasta do modelo, não uma pasta acima dela."));
                            return;
                        }
                        paths.add(relative);
                        sizes.add(cursor.isNull(3) ? -1L : cursor.getLong(3));
                        depths.add(depth);
                        synchronized (folderDocuments) {
                            folderDocuments.put(relative, DocumentsContract.buildDocumentUriUsingTree(tree, id));
                        }
                    }
                }
            }
            // Principais: todo .gltf (texto, pequeno); .glb só quando não há .gltf.
            boolean anyGltf = false;
            for (String path : paths) anyGltf |= path.toLowerCase(java.util.Locale.ROOT).endsWith(".gltf");
            java.util.ArrayList<byte[]> mainPaths = new java.util.ArrayList<>(), mainContents = new java.util.ArrayList<>();
            long mainBytes = 0;
            for (int i = 0; i < paths.size(); ++i) {
                String lower = paths.get(i).toLowerCase(java.util.Locale.ROOT);
                if (!(anyGltf ? lower.endsWith(".gltf") : lower.endsWith(".glb"))) continue;
                Uri document;
                synchronized (folderDocuments) { document = folderDocuments.get(paths.get(i)); }
                byte[] content = readAll(document, resolver, token, MAXIMUM_BYTES);
                if (content == null) {
                    if (poll() != token) return;
                    continue;
                }
                mainBytes += content.length;
                if (mainBytes > MAXIMUM_BYTES) {
                    submitFolder(token, null, null, null, null, null,
                                 encode("Arquivos principais grandes demais nesta pasta; escolha a pasta de um único modelo."));
                    return;
                }
                mainPaths.add(encode(paths.get(i)));
                mainContents.add(content);
            }
            byte[][] encodedPaths = new byte[paths.size()][];
            long[] sizeArray = new long[paths.size()];
            for (int i = 0; i < paths.size(); ++i) { encodedPaths[i] = encode(paths.get(i)); sizeArray[i] = sizes.get(i); }
            submitFolder(token, encodedPaths, sizeArray, mainPaths.toArray(new byte[0][]), mainContents.toArray(new byte[0][]),
                         encode(folderName), null);
        } catch (Exception error) {
            submitFolder(token, null, null, null, null, null, encode("Não foi possível listar a pasta escolhida."));
        }
    }

    private static byte[] readAll(Uri uri, ContentResolver resolver, long token, int maximum) {
        if (uri == null) return null;
        try (InputStream stream = resolver.openInputStream(uri)) {
            if (stream == null) return null;
            ByteArrayOutputStream buffer = new ByteArrayOutputStream();
            byte[] chunk = new byte[64 * 1024];
            int read;
            while ((read = stream.read(chunk)) > 0) {
                if (poll() != token) return null;
                if (buffer.size() + read > maximum) return null;
                buffer.write(chunk, 0, read);
            }
            return buffer.toByteArray();
        } catch (Exception error) {
            return null;
        }
    }

    private void startCopy(long token) {
        copyToken = token;
        ContentResolver resolver = activity.getApplicationContext().getContentResolver();
        try {
            READER.execute(() -> copyFolder(token, resolver));
        } catch (RejectedExecutionException busy) {
            // O leitor ainda termina a listagem; o próximo tique tenta de novo.
            copyToken = 0;
        }
    }

    private static void copyFolder(long token, ContentResolver resolver) {
        byte[][] sources = copySources(token), targets = copyTargets(token);
        byte[] destinationBytes = copyDestination(token);
        if (sources == null || targets == null || destinationBytes == null || sources.length != targets.length) {
            submitCopy(token, null, encode("Pedido de cópia inválido."));
            return;
        }
        File destination = new File(new String(destinationBytes, StandardCharsets.UTF_8));
        long total = 0;
        Uri[] documents = new Uri[sources.length];
        for (int i = 0; i < sources.length; ++i) {
            String source = new String(sources[i], StandardCharsets.UTF_8);
            synchronized (folderDocuments) { documents[i] = folderDocuments.get(source); }
            if (documents[i] == null) {
                submitCopy(token, null, encode("A pasta escolhida não tem mais o arquivo " + source + "."));
                return;
            }
        }
        byte[][] hashes = new byte[sources.length][];
        long done = 0;
        byte[] chunk = new byte[1 << 20];
        try {
            String root = destination.getCanonicalPath() + File.separator;
            for (int i = 0; i < sources.length; ++i) {
                File target = new File(destination, new String(targets[i], StandardCharsets.UTF_8));
                // O nativo já normalizou o caminho; conferir aqui fecha a pasta de preparo.
                if (!target.getCanonicalPath().startsWith(root)) {
                    submitCopy(token, null, encode("Destino de cópia fora da pasta de preparo."));
                    return;
                }
                File parent = target.getParentFile();
                if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
                    submitCopy(token, null, encode("Não foi possível criar a pasta de preparo."));
                    return;
                }
                MessageDigest digest = MessageDigest.getInstance("SHA-256");
                try (InputStream in = resolver.openInputStream(documents[i]); FileOutputStream out = new FileOutputStream(target)) {
                    if (in == null) {
                        submitCopy(token, null, encode("Um arquivo da pasta não pôde ser aberto."));
                        return;
                    }
                    int read;
                    long sinceReport = 0;
                    while ((read = in.read(chunk)) > 0) {
                        if (copyCancelled(token)) {
                            submitCopy(token, null, null);
                            return;
                        }
                        out.write(chunk, 0, read);
                        digest.update(chunk, 0, read);
                        done += read;
                        sinceReport += read;
                        if (sinceReport >= (8 << 20)) { copyProgress(token, done, total, i); sinceReport = 0; }
                    }
                    out.getFD().sync();
                }
                StringBuilder hex = new StringBuilder(64);
                for (byte value : digest.digest()) hex.append(String.format(java.util.Locale.ROOT, "%02x", value));
                hashes[i] = encode(hex.toString());
                copyProgress(token, done, total, i + 1);
            }
            submitCopy(token, hashes, null);
        } catch (Exception error) {
            submitCopy(token, null, encode("Falha ao copiar a pasta do modelo para o projeto."));
        }
    }

    private static String displayName(Uri uri, ContentResolver resolver) {
        try (Cursor cursor = resolver.query(uri, null, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int column = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (column >= 0) {
                    String value = cursor.getString(column);
                    if (value != null && !value.isEmpty()) return value;
                }
            }
        } catch (Exception ignored) {
            // Sem nome utilizável: o editor usa um padrão. Falhar a importação
            // por causa do nome seria trocar um problema pequeno por um grande.
        }
        String path = uri.getLastPathSegment();
        if (path == null || path.isEmpty()) return "modelo.glb";
        int slash = path.lastIndexOf('/');
        return slash >= 0 && slash + 1 < path.length() ? path.substring(slash + 1) : path;
    }
}
