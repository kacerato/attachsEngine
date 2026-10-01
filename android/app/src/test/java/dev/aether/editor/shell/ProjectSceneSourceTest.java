package dev.aether.editor.shell;

import org.junit.Test;
import org.junit.Rule;
import org.junit.rules.TemporaryFolder;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.charset.StandardCharsets;
import static org.junit.Assert.*;

public class ProjectSceneSourceTest {
    @Rule public TemporaryFolder temporary = new TemporaryFolder();
    private Project project(String template) throws Exception {
        File root=temporary.newFolder();
        assertTrue(new File(root,"scenes").mkdir());
        return new Project("Arbitrary name",root.getAbsolutePath(),template,null,1,0);
    }
    private void archive(Project p,String header) throws Exception {
        Files.write(new File(p.path,"scenes/editor.aescene").toPath(),header.getBytes(StandardCharsets.UTF_8));
    }
    @Test public void newEmptyProjectHasNoPackage() throws Exception {
        Project p=project("empty");
        Files.write(new File(p.path,"project.json").toPath(),
                "{\"resourceSource\":\"independent\"}".getBytes(StandardCharsets.UTF_8));
        assertTrue(ProjectSceneSource.isIndependent(p));
    }
    @Test public void legacyWithoutPublicArchiveKeepsPrivateMigrationPath() throws Exception {
        assertFalse(ProjectSceneSource.isIndependent(project("empty")));
    }
    @Test public void independentArchiveReopensWithoutPackage() throws Exception {
        Project p=project("empty");archive(p,"AETHER_EDITOR 6 0 1\n");
        assertTrue(ProjectSceneSource.isIndependent(p));
    }
    @Test public void sparseArchiveReopensWithoutPackage() throws Exception {
        Project p=project("empty");archive(p,"AETHER_EDITOR 7 0 1\n");
        assertTrue(ProjectSceneSource.isIndependent(p));
    }
    @Test public void componentArchiveReopensWithoutPackage() throws Exception {
        Project p=project("empty");archive(p,"AETHER_EDITOR 8 0 1\n");
        assertTrue(ProjectSceneSource.isIndependent(p));
    }
    @Test public void futureArchiveIsRejected() throws Exception {
        Project p=project("empty");archive(p,"AETHER_EDITOR 18 0 1\n");
        try { ProjectSceneSource.isIndependent(p); fail("future version accepted"); }
        catch(IOException expected) { }
    }
    @Test public void allNativeVersionsRouteByFingerprintIncludingInstanceArchive() throws Exception {
        Project p=project("empty");
        for(int version=1;version<=17;++version) {
            archive(p,"AETHER_EDITOR "+version+" 0 1\n");
            assertTrue("independent version "+version,ProjectSceneSource.isIndependent(p));
            archive(p,"AETHER_EDITOR "+version+" 9651248282590934415 1\n");
            assertFalse("packaged version "+version,ProjectSceneSource.isIndependent(p));
        }
    }
    @Test public void legacyEmptyArchiveRetainsItsPackage() throws Exception {
        Project p=project("empty");archive(p,"AETHER_EDITOR 6 9651248282590934415 1\n");
        assertFalse(ProjectSceneSource.isIndependent(p));
        assertFalse(ProjectSceneSource.isIndependent(project("ocean")));
    }
    @Test public void malformedArchiveNeverFallsBackToDemo() throws Exception {
        Project p=project("empty");archive(p,"broken\n");
        try { ProjectSceneSource.isIndependent(p); fail("corrupt archive accepted"); }
        catch(IOException expected) { }
        assertEquals("broken\n",new String(Files.readAllBytes(new File(p.path,"scenes/editor.aescene").toPath()),StandardCharsets.UTF_8));
    }
    @Test public void unboundedHeaderIsRejected() throws Exception {
        Project p=project("empty");archive(p,new String(new char[129]).replace('\0','1'));
        try { ProjectSceneSource.isIndependent(p); fail("oversized header accepted"); }
        catch(IOException expected) { }
    }
}
