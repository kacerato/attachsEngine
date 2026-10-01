using System.Numerics;
using System.Runtime.InteropServices;
using Astra;

namespace Aether.Tests;

public static class AstraInputProfileTests
{
    [Test]
    public static void ProfileFile_ReplacesAtomicallyBoundsReadsAndPreservesPreviousOnExportFailure()
    {
        var root=Path.Combine(Path.GetTempPath(),"astra-profile-"+Guid.NewGuid().ToString("N"));
        var path=Path.Combine(root,"input.profile");var scene=new FileBoundaryScene();var input=new InputAccess(scene);
        try {
            input.SaveProfile(path);Assert.Equal(scene.Export,File.ReadAllText(path));
            scene.Export="second persisted profile";input.SaveProfile(path);
            Assert.Equal(scene.Export,File.ReadAllText(path));
            Assert.True(input.LoadProfile(path));Assert.Equal(scene.Export,scene.Imported);
            scene.FailExport=true;Assert.Throws<InvalidOperationException>(()=>input.SaveProfile(path));
            Assert.Equal("second persisted profile",File.ReadAllText(path));
            Assert.Equal(1,Directory.GetFiles(root).Length,"failed save leaves no pending file");
            File.WriteAllBytes(path,new byte[262145]);scene.Imported="unchanged";
            Assert.True(!input.LoadProfile(path));Assert.Equal("unchanged",scene.Imported);
            Assert.Equal(24,Marshal.SizeOf<InputBindingValue>(),"native binding layout");
            Assert.Equal(16,Marshal.OffsetOf<InputBindingValue>(nameof(InputBindingValue.Scale)).ToInt32());
        } finally {if(Directory.Exists(root))Directory.Delete(root,true);}
    }

    // This double exercises only the managed filesystem boundary. Native tests
    // separately exercise actual profile parsing and effective input consumption.
    private sealed class FileBoundaryScene : ISceneAccess
    {
        public string Export="first persisted profile",Imported="";
        public bool FailExport;
        public string ExportInputProfile() => FailExport?throw new InvalidOperationException("export failed"):Export;
        public bool ImportInputProfile(string profile) {Imported=profile;return true;}
        public bool Exists(ulong id)=>true;
        public TransformValue GetTransform(ulong id)=>default;
        public bool SetTransform(ulong id,TransformValue value)=>false;
        public bool SetBodyVelocity(ulong id,Vector3 value)=>false;
        public bool MoveKinematic(ulong id,Vector3 position,Quaternion rotation)=>false;
        public void Log(ulong id,string message){}
    }
}
