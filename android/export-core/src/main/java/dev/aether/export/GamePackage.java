package dev.aether.export;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.zip.*;

/** Versioned, hashed project payload shared by the desktop exporter and Android player. */
public final class GamePackage {
    public static final String PREFIX = "assets/astra-game/";
    public static final String INDEX = PREFIX + "files.tsv";
    public static final String MARKER = "assets/astra-player-template-v1";
    public static final long MAX_BYTES = 8L * 1024 * 1024 * 1024;
    public static final int MAX_FILES = 100000;
    private GamePackage() {}

    public static final class Entry {
        public final String path, hash;
        public final long size;
        Entry(String path, String hash, long size) { this.path=path; this.hash=hash; this.size=size; }
    }

    public static String sha256(File file) throws IOException {
        try (InputStream in=new FileInputStream(file)) { return hash(in, null, MAX_BYTES); }
    }
    private static MessageDigest digest() {
        try { return MessageDigest.getInstance("SHA-256"); }
        catch (NoSuchAlgorithmException impossible) { throw new AssertionError(impossible); }
    }
    private static String hex(byte[] bytes) {
        StringBuilder out=new StringBuilder(); for(byte b:bytes)out.append(String.format(Locale.ROOT,"%02x",b&255)); return out.toString();
    }
    private static String hash(InputStream in, OutputStream out, long limit) throws IOException {
        MessageDigest digest=digest(); byte[] buffer=new byte[65536]; long total=0; int count;
        while((count=in.read(buffer))!=-1) {
            total+=count; if(total>limit)throw new IOException("Project file exceeds its declared size");
            if(Thread.currentThread().isInterrupted())throw new InterruptedIOException("Export cancelled");
            digest.update(buffer,0,count); if(out!=null)out.write(buffer,0,count);
        }
        return hex(digest.digest());
    }
    private static boolean excluded(String path) {
        String name=path.substring(path.lastIndexOf('/')+1).toLowerCase(Locale.ROOT);
        if(name.endsWith(".jks")||name.endsWith(".keystore")||name.endsWith(".p12")||name.endsWith(".pem")||name.equals(".env")||name.startsWith(".env."))return true;
        for(String segment:path.split("/"))if(Arrays.asList(".git",".gradle",".idea","bin","obj","node_modules").contains(segment))return true;
        return path.equals(".astra/code-drafts.astra")||path.equals(".astra/save")||path.startsWith(".astra/save/");
    }
    static String validPath(String path) throws IOException {
        if(path.isEmpty()||path.length()>2048||path.startsWith("/")||path.indexOf('\\')>=0||path.indexOf(':')>=0||path.indexOf('\0')>=0)
            throw new IOException("Invalid project path: "+path);
        for(String part:path.split("/",-1))if(part.isEmpty()||part.equals(".")||part.equals(".."))throw new IOException("Unsafe project path: "+path);
        return path;
    }

    /** Copies a stable snapshot. A source file changed during copying aborts publication. */
    public static List<Entry> snapshot(File project, File destination) throws IOException {
        Path root=project.toPath().toRealPath(); Path target=destination.toPath().toAbsolutePath().normalize();
        if(target.startsWith(root))throw new IOException("Export staging must be outside the project");
        if(!Files.isRegularFile(root.resolve("project.json"))||!Files.isRegularFile(root.resolve("scenes/editor.aescene")))
            throw new IOException("Project must contain project.json and scenes/editor.aescene");
        List<Path> paths=new ArrayList<>();
        try(java.util.stream.Stream<Path> walk=Files.walk(root)) {
            Iterator<Path> iterator=walk.iterator();
            while(iterator.hasNext()) {
                Path file=iterator.next(); if(file.equals(root))continue;
                String relative=root.relativize(file).toString().replace(File.separatorChar,'/');
                if(excluded(relative))continue;
                if(Files.isSymbolicLink(file))throw new IOException("Linked project file is unsupported: "+relative);
                if(Files.isRegularFile(file,LinkOption.NOFOLLOW_LINKS)) {paths.add(file);if(paths.size()>MAX_FILES)throw new IOException("Too many project files");}
            }
        }
        paths.sort(Comparator.comparing(path->root.relativize(path).toString()));
        List<Entry> result=new ArrayList<>();long total=0;
        for(Path file:paths) {
            String relative=validPath(root.relativize(file).toString().replace(File.separatorChar,'/'));
            long size=Files.size(file);total+=size;if(total>MAX_BYTES)throw new IOException("Project exceeds 8 GiB export limit");
            Path copy=target.resolve(relative);Files.createDirectories(copy.getParent());
            String hash;
            try(InputStream in=Files.newInputStream(file);OutputStream out=Files.newOutputStream(copy,StandardOpenOption.CREATE_NEW)) {hash=hash(in,out,size);}
            if(Files.size(copy)!=size||!hash.equals(sha256(file.toFile())))throw new IOException("Project changed during export: "+relative);
            result.add(new Entry(relative,hash,size));
        }
        return result;
    }
    public static byte[] index(List<Entry> entries) {
        StringBuilder text=new StringBuilder("ASTRA_GAME\t1\n");
        for(Entry entry:entries)text.append(entry.hash).append('\t').append(entry.size).append('\t')
            .append(Base64.getEncoder().encodeToString(entry.path.getBytes(StandardCharsets.UTF_8))).append('\n');
        return text.toString().getBytes(StandardCharsets.UTF_8);
    }
    private static List<Entry> readIndex(InputStream stream) throws IOException {
        List<Entry> result=new ArrayList<>();Set<String> paths=new HashSet<>();long total=0;
        BufferedReader reader=new BufferedReader(new InputStreamReader(stream,StandardCharsets.UTF_8));
        if(!"ASTRA_GAME\t1".equals(reader.readLine()))throw new IOException("Unsupported game package version");
        String line;while((line=reader.readLine())!=null) {
            if(line.length()>4096)throw new IOException("Oversized package index entry");
            String[] parts=line.split("\t",-1);if(parts.length!=3||!parts[0].matches("[0-9a-f]{64}"))throw new IOException("Invalid package index");
            try {
                long size=Long.parseLong(parts[1]);String path=validPath(new String(Base64.getDecoder().decode(parts[2]),StandardCharsets.UTF_8));
                if(size<0||size>MAX_BYTES-total||!paths.add(path)||result.size()>=MAX_FILES)throw new IOException("Invalid package size or duplicate path");
                total+=size;result.add(new Entry(path,parts[0],size));
            } catch(IllegalArgumentException failure) {throw new IOException("Invalid package index",failure);}
        }
        if(!paths.contains("project.json")||!paths.contains("scenes/editor.aescene"))throw new IOException("Game entry scene missing");
        return result;
    }
    /** Destination must be a newly created staging folder; caller publishes it after success. */
    public static void extract(File apk, File destination) throws IOException {
        if(destination.exists())throw new IOException("Game extraction destination already exists");
        if(!destination.mkdirs())throw new IOException("Cannot create game directory");
        boolean complete=false;
        try(ZipFile zip=new ZipFile(apk)) {
            ZipEntry index=zip.getEntry(INDEX);if(index==null||index.getSize()>32*1024*1024)throw new IOException("Game payload missing");
            List<Entry> entries;try(InputStream in=zip.getInputStream(index)){entries=readIndex(in);}
            for(Entry entry:entries) {
                ZipEntry source=zip.getEntry(PREFIX+"project/"+entry.path);
                if(source==null||source.getSize()!=entry.size)throw new IOException("Missing or incomplete game asset: "+entry.path);
                File target=new File(destination,entry.path);Files.createDirectories(target.toPath().getParent());
                String actual;try(InputStream in=zip.getInputStream(source);OutputStream out=new FileOutputStream(target)){actual=hash(in,out,entry.size);}
                if(!entry.hash.equals(actual))throw new IOException("Game asset checksum mismatch: "+entry.path);
            }
            complete=true;
        } finally {if(!complete)deleteTree(destination);}
    }
    public static void deleteTree(File directory) throws IOException {
        if(!directory.exists())return;
        try(java.util.stream.Stream<Path> paths=Files.walk(directory.toPath())) {
            for(Path path:(Iterable<Path>)paths.sorted(Comparator.reverseOrder())::iterator)Files.delete(path);
        }
    }
}
