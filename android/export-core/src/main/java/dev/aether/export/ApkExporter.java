package dev.aether.export;

import com.android.apksig.ApkSigner;
import com.android.apksig.ApkVerifier;
import com.reandroid.apk.ApkModule;
import com.reandroid.archive.ByteInputSource;
import com.reandroid.archive.FileInputSource;
import com.reandroid.arsc.chunk.xml.AndroidManifestBlock;
import com.reandroid.arsc.chunk.xml.ResXmlElement;
import java.io.*;
import java.nio.file.*;
import java.security.*;
import java.security.cert.X509Certificate;
import java.util.*;
import java.util.zip.ZipFile;

/** One packaging/signing pipeline for Android and desktop; never edits its input APK/project. */
public final class ApkExporter {
    public interface Progress { void report(String stage); }
    public static final class Settings {
        public final String applicationId, label, versionName;
        public final int versionCode;
        public Settings(String id,String label,int code,String version) {
            if(id==null||!id.matches("[a-z][a-z0-9_]*(\\.[a-z][a-z0-9_]*)+")||id.length()>180)
                throw new IllegalArgumentException("Use an application ID such as com.studio.game");
            if(id.startsWith("dev.aether.editor"))throw new IllegalArgumentException("The game must have its own application ID");
            if(label==null||label.trim().isEmpty()||label.length()>80||code<1||version==null||version.trim().isEmpty()||version.length()>80)
                throw new IllegalArgumentException("Invalid game name or version");
            applicationId=id;this.label=label;versionCode=code;versionName=version;
        }
    }
    public static ApkSigner.SignerConfig signer(File keyStore,String type,char[] storePassword,String alias,char[] keyPassword) throws Exception {
        KeyStore store=KeyStore.getInstance(type);
        try(InputStream in=new FileInputStream(keyStore)){store.load(in,storePassword);}
        Key key=store.getKey(alias,keyPassword);
        if(!(key instanceof PrivateKey))throw new IOException("Signing alias has no private key");
        java.security.cert.Certificate[] chain=store.getCertificateChain(alias);
        if(chain==null||chain.length==0)throw new IOException("Signing certificate is missing");
        List<X509Certificate> certificates=new ArrayList<>();
        for(java.security.cert.Certificate certificate:chain)certificates.add((X509Certificate)certificate);
        return new ApkSigner.SignerConfig.Builder(alias,(PrivateKey)key,certificates).build();
    }
    public static void export(File template,File project,File output,File temporaryRoot,Settings settings,
                              ApkSigner.SignerConfig signer,Progress progress) throws Exception {
        if(output.exists())throw new IOException("Output already exists; choose a new file");
        if(output.getCanonicalFile().toPath().startsWith(project.getCanonicalFile().toPath()))
            throw new IOException("The APK destination must be outside the source project");
        try(ZipFile zip=new ZipFile(template)) {
            if(zip.getEntry(GamePackage.MARKER)==null)throw new IOException("This APK is not a compatible Astra player template");
            if(zip.getEntry(GamePackage.INDEX)!=null)throw new IOException("An exported game cannot be used as an editor template");
        }
        Files.createDirectories(temporaryRoot.toPath());
        File work=Files.createTempDirectory(temporaryRoot.toPath(),"astra-export-").toFile();
        try {
            progress.report("Copying saved project and checking assets");
            File snapshot=new File(work,"project");List<GamePackage.Entry> files=GamePackage.snapshot(project,snapshot);
            progress.report("Writing Android identity and game payload");
            File unsigned=new File(work,"unsigned.apk"),signed=new File(work,"signed.apk");
            try(ApkModule apk=ApkModule.loadApkFile(template)) {
                AndroidManifestBlock manifest=apk.getAndroidManifestBlock();
                manifest.setPackageName(settings.applicationId);
                manifest.setVersionCode(settings.versionCode);manifest.setVersionName(settings.versionName);manifest.setApplicationLabel(settings.label);
                ResXmlElement application=manifest.getApplicationElement();
                // The template contract has only these two activity classes. No editor launcher, aliases or providers are carried over.
                application.removeElementsIf(element->Arrays.asList("activity","activity-alias","provider","service","receiver","profileable").contains(element.getName()));
                ResXmlElement boot=manifest.getOrCreateMainActivity("dev.aether.editor.GameBootstrapActivity");
                boot.getOrCreateAndroidAttribute("exported",0x01010010).setValueAsBoolean(true);
                ResXmlElement player=manifest.getOrCreateActivity("dev.aether.editor.AetherActivity",false);
                player.getOrCreateAndroidAttribute("exported",0x01010010).setValueAsBoolean(false);
                // Preserve native surface across rotation/configuration and keep the game in landscape.
                androidInteger(player,"screenOrientation",0x0101001e,6);
                androidInteger(player,"configChanges",0x0101001f,0x00000fb0);
                androidInteger(boot,"screenOrientation",0x0101001e,6);
                androidInteger(boot,"configChanges",0x0101001f,0x00000fb0);
                androidInteger(boot,"launchMode",0x0101001d,2);
                androidInteger(player,"launchMode",0x0101001d,2);
                ResXmlElement metadata=player.newElement("meta-data");
                metadata.getOrCreateAndroidAttribute("name",0x01010003).setValueAsString("android.app.lib_name");
                metadata.getOrCreateAndroidAttribute("value",0x01010024).setValueAsString("aether_android");
                application.getOrCreateAndroidAttribute("debuggable",0x0101000f).setValueAsBoolean(false);
                apk.removeDir("META-INF");
                apk.add(new ByteInputSource(GamePackage.index(files),GamePackage.INDEX));
                for(GamePackage.Entry file:files)apk.add(new FileInputSource(new File(snapshot,file.path),GamePackage.PREFIX+"project/"+file.path));
                apk.writeApk(unsigned);
            }
            progress.report("Signing APK");
            new ApkSigner.Builder(Collections.singletonList(signer)).setInputApk(unsigned).setOutputApk(signed)
                .setMinSdkVersion(26).setV1SigningEnabled(false).setV2SigningEnabled(true).setV3SigningEnabled(true).build().sign();
            progress.report("Verifying signature and packaged assets");
            ApkVerifier.Result verification=new ApkVerifier.Builder(signed).setMinCheckedPlatformVersion(26).build().verify();
            if(!verification.isVerified())throw new IOException("APK signature verification failed: "+verification.getErrors());
            try(ApkModule verified=ApkModule.loadApkFile(signed)) {
                AndroidManifestBlock manifest=verified.getAndroidManifestBlock();
                if(!settings.applicationId.equals(manifest.getPackageName())||
                    !"dev.aether.editor.GameBootstrapActivity".equals(manifest.getMainActivityClassName())||
                    manifest.getActivity("dev.aether.editor.AetherActivity",false)==null||
                    manifest.listApplicationElementsByTag("activity").size()!=2||
                    !manifest.listApplicationElementsByTag("activity-alias").isEmpty())
                    throw new IOException("Exported Android entry points failed verification");
            }
            GamePackage.extract(signed,new File(work,"verification"));
            File parent=output.getAbsoluteFile().getParentFile();Files.createDirectories(parent.toPath());
            // Copy to a sibling first; readers never observe a partial final APK.
            Path staged=Files.createTempFile(parent.toPath(),".astra-apk-",".tmp");
            try {Files.copy(signed.toPath(),staged,StandardCopyOption.REPLACE_EXISTING);Files.move(staged,output.toPath());}
            finally {Files.deleteIfExists(staged);}
            progress.report("APK ready: "+output.getAbsolutePath());
        } finally {
            try {GamePackage.deleteTree(work);}
            catch(IOException cleanup){progress.report("Temporary export files could not be removed: "+work.getAbsolutePath());}
        }
    }
    private static void androidInteger(ResXmlElement element,String name,int id,int value) {
        com.reandroid.arsc.chunk.xml.ResXmlAttribute attribute=element.getOrCreateAndroidAttribute(name,id);
        attribute.setValueType(com.reandroid.arsc.value.ValueType.DEC);attribute.setData(value);
    }
}
