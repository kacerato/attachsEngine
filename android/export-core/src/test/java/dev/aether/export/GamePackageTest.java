package dev.aether.export;

import org.junit.Test;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;
import static org.junit.Assert.*;

public class GamePackageTest {
    @Test public void snapshotRoundTripAndTamperRollback() throws Exception {
        File root=Files.createTempDirectory("astra-package-test").toFile();
        try {
            File project=new File(root,"source");new File(project,"scenes").mkdirs();
            Files.writeString(new File(project,"project.json").toPath(),"{}");
            Files.writeString(new File(project,"scenes/editor.aescene").toPath(),"fixture");
            Files.writeString(new File(project,"private.p12").toPath(),"never export signing keys");
            new File(project,".astra/save").mkdirs();
            Files.writeString(new File(project,".astra/save/state.json").toPath(),"author's test progress must not ship");
            Files.writeString(new File(project,".astra/editor-preferences.astra").toPath(),"viewport_quality low\nviewport_animation off\n");
            final String gameGraphics="astra_rendering 3\nquality=s\nupscaling_filter=arm_asr\n";
            Files.writeString(new File(project,".astra/rendering.astra").toPath(),gameGraphics);
            File snapshot=new File(root,"snapshot");List<GamePackage.Entry> entries=GamePackage.snapshot(project,snapshot);
            assertEquals(3,entries.size());
            File apk=new File(root,"game.apk");writeApk(apk,snapshot,entries,false);
            File restored=new File(root,"restored");GamePackage.extract(apk,restored);
            assertEquals("fixture",Files.readString(new File(restored,"scenes/editor.aescene").toPath()));
            assertFalse(new File(restored,".astra/editor-preferences.astra").exists());
            assertEquals(gameGraphics,Files.readString(new File(restored,".astra/rendering.astra").toPath()));
            File tampered=new File(root,"tampered.apk");writeApk(tampered,snapshot,entries,true);
            File refused=new File(root,"refused");
            try {GamePackage.extract(tampered,refused);fail("corrupt payload accepted");}catch(IOException expected){assertFalse(refused.exists());}
            try {GamePackage.snapshot(project,new File(project,"nested"));fail("recursive export accepted");}catch(IOException expected){}
            try {GamePackage.validPath("../escape");fail("traversal accepted");}catch(IOException expected){}
        } finally {GamePackage.deleteTree(root);}
    }
    private static void writeApk(File file,File snapshot,List<GamePackage.Entry> entries,boolean tamper) throws IOException {
        try(ZipOutputStream zip=new ZipOutputStream(new FileOutputStream(file))) {
            zip.putNextEntry(new ZipEntry(GamePackage.INDEX));zip.write(GamePackage.index(entries));zip.closeEntry();
            for(GamePackage.Entry entry:entries) {
                zip.putNextEntry(new ZipEntry(GamePackage.PREFIX+"project/"+entry.path));
                byte[] bytes=Files.readAllBytes(new File(snapshot,entry.path).toPath());if(tamper&&bytes.length>0)bytes[0]^=1;
                zip.write(bytes);zip.closeEntry();
            }
        }
    }
    @Test public void portableSigningIdentitySurvivesReload() throws Exception {
        File root=Files.createTempDirectory("astra-signing-test").toFile();char[] password="test-only-password".toCharArray();
        try {
            File key=new File(root,"game.p12");GameSigningKey.create(key,"game",password);
            assertNotNull(ApkExporter.signer(key,"PKCS12",password,"game",password));
            try {GameSigningKey.create(key,"game",password);fail("existing identity overwritten");}catch(IOException expected){}
        } finally {Arrays.fill(password,'\0');GamePackage.deleteTree(root);}
    }
}
