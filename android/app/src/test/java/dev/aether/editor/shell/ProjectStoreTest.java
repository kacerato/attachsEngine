package dev.aether.editor.shell;

import org.junit.Test;
import org.junit.Rule;
import org.junit.rules.TemporaryFolder;
import org.json.JSONArray;
import java.io.File;
import java.nio.file.Files;
import java.nio.charset.StandardCharsets;
import static org.junit.Assert.*;

public class ProjectStoreTest {
    @Rule public TemporaryFolder temporary = new TemporaryFolder();
    private ProjectStore store(File files, File projects) { return new ProjectStore(files, projects); }

    @Test public void retiresOnlyRecognizedBundledPackagesAndPreservesUserProjects() throws Exception {
        File files=temporary.newFolder("files"), projects=temporary.newFolder("Projects");
        ProjectStore initial=store(files,projects);
        Project example=initial.create("Cristais do Templo",SceneTemplate.byId("empty"));
        Project user=initial.create("Circuito Neon",SceneTemplate.byId("empty"));
        assertNotNull(example);assertNotNull(user);
        File packageRoot=new File(example.path);assertTrue(new File(packageRoot,".astra").mkdir());
        Files.write(new File(packageRoot,".astra/assets.astra").toPath(),new byte[]{42});
        Files.write(new File(packageRoot,"LEIA-ME.md").toPath(),new byte[]{42});
        File descriptor=new File(packageRoot,"project.json");
        org.json.JSONObject json=new org.json.JSONObject(new String(Files.readAllBytes(descriptor.toPath()),StandardCharsets.UTF_8));
        json.getJSONObject("project").put("thumbnail","example-cristais.png");
        Files.write(descriptor.toPath(),json.toString().getBytes(StandardCharsets.UTF_8));
        initial.retireBundledExamples();
        assertFalse(packageRoot.exists());assertTrue(new File(user.path,"project.json").isFile());
        ProjectStore reopened=store(files,projects);assertEquals(1,reopened.projects().size());
        assertEquals("Circuito Neon",reopened.projects().get(0).name);
    }

    @Test public void firstLaunchIsEmptyAndDoesNotSeedProjects() throws Exception {
        File files=temporary.newFolder("files"), projects=new File(temporary.getRoot(),"Projects");
        assertTrue(store(files,projects).projects().isEmpty());
        assertFalse(projects.exists());
        assertFalse(new File(files,"projects.json").exists());
    }
    @Test public void staleSeedCardsDisappearWithoutDeletingUserFolders() throws Exception {
        File files=temporary.newFolder("files"), projects=temporary.newFolder("Projects");
        File real=new File(projects,"Water Lab");assertTrue(real.mkdir());
        Files.write(new File(real,"user.txt").toPath(),new byte[]{42});
        JSONArray index=new JSONArray();
        index.put(new Project("Water Lab",real.getAbsolutePath(),"ocean",null,1,0).toJson());
        index.put(new Project("Backroom Demo",new File(projects,"Backroom Demo").getAbsolutePath(),"backroom",null,1,357).toJson());
        Files.write(new File(files,"projects.json").toPath(),index.toString().getBytes(StandardCharsets.UTF_8));
        ProjectStore loaded=store(files,projects);
        assertEquals(1,loaded.projects().size());
        assertEquals("Water Lab",loaded.projects().get(0).name);
        assertTrue(new File(real,"user.txt").isFile());
    }
    @Test public void createPersistsRealFilesAndReopensWithoutDuplicates() throws Exception {
        File files=temporary.newFolder("files"), projects=temporary.newFolder("Projects");
        ProjectStore initial=store(files,projects);
        Project p=initial.create("My Scene",SceneTemplate.byId("empty"));assertNotNull(p);
        assertTrue(new File(p.path,"project.json").isFile());
        assertTrue(new File(p.path,"scenes/main.ascene").isFile());
        assertEquals(0,p.assets);
        assertEquals(1,store(files,projects).projects().size());
        assertNull(initial.create("My Scene",SceneTemplate.byId("empty")));
        assertNull(initial.create("../escape",SceneTemplate.byId("empty")));
        assertNull(initial.create("Unavailable",SceneTemplate.byId("backroom")));
    }
    @Test public void corruptedIndexIsPreservedAndDescriptorsRecovered() throws Exception {
        File files=temporary.newFolder("files"), projects=temporary.newFolder("Projects");
        assertNotNull(store(files,projects).create("Real",SceneTemplate.byId("empty")));
        Files.write(new File(files,"projects.json").toPath(),"broken".getBytes(StandardCharsets.UTF_8));
        ProjectStore recovered=store(files,projects);
        assertEquals(1,recovered.projects().size());
        File[] backups=files.listFiles((dir,name)->name.startsWith("projects-corrupt-"));
        assertEquals(1,backups.length);
        assertEquals("broken",new String(Files.readAllBytes(backups[0].toPath()),StandardCharsets.UTF_8));
    }
    @Test public void failedDirectoryCreationDoesNotPublishProject() throws Exception {
        File files=temporary.newFolder("files"), projects=temporary.newFile("Projects");
        ProjectStore initial=store(files,projects);
        assertNull(initial.create("Failure",SceneTemplate.byId("empty")));
        assertTrue(initial.projects().isEmpty());
    }

    @Test public void creationOffersOnlyEmptyButPreservesLegacyResolution() throws Exception {
        assertEquals(1, SceneTemplate.all().size());
        assertEquals(SceneTemplate.EMPTY, SceneTemplate.all().get(0).id);
        assertEquals("aether.ocean_preview", SceneTemplate.byId("ocean").previewExtra);
        assertEquals("aether.map_preview", SceneTemplate.byId("forest").previewExtra);
        File files=temporary.newFolder("files"), projects=temporary.newFolder("Projects");
        ProjectStore initial=store(files,projects);
        assertNull(initial.create("Demo",SceneTemplate.byId("ocean")));
        assertFalse(new File(projects,"Demo").exists());
        Project empty=initial.create("Blank",SceneTemplate.all().get(0));
        assertNotNull(empty);
        org.json.JSONObject scene=new org.json.JSONObject(new String(Files.readAllBytes(
                new File(empty.path,"scenes/main.ascene").toPath()),StandardCharsets.UTF_8));
        assertEquals(0,scene.getJSONArray("nodes").length());
        assertEquals(SceneTemplate.EMPTY,scene.getString("template"));
    }
}
