using System.Numerics;
using System.Text.Json;
using Astra;
using Astra.Compilation;
using Astra.Runtime;
namespace Aether.Tests;
public static class SaveStoreTests
{
    private sealed class ProjectDirectory : IDisposable
    {
        public string Root { get; } = Path.Combine(Path.GetTempPath(), "astra-save-test-" + Guid.NewGuid().ToString("N"));
        public ProjectDirectory() => Directory.CreateDirectory(Root);
        public void Dispose() => Directory.Delete(Root, recursive: true);
    }
    [Test]
    public static void SaveSnapshots_PersistTypedValuesRecoverAndIsolateProjects()
    {
        using var a = new ProjectDirectory(); using var b = new ProjectDirectory();
        using (var store = new SaveStore(a.Root)) { store.SetString("name", "Maré"); store.SetBoolean("ready", true); store.SetInt64("score", long.MaxValue); store.SetDouble("speed", 1.25); store.Flush(); store.SetInt64("score", 7); store.Flush(); }
        using (var store = new SaveStore(a.Root)) { Assert.Equal(7L, store.GetInt64("score")); Assert.Throws<InvalidDataException>(() => store.GetString("score")); Assert.Throws<ArgumentOutOfRangeException>(() => store.SetDouble("bad", double.NaN)); }
        using (var store = new SaveStore(b.Root)) Assert.True(!store.Contains("score"));
        File.WriteAllText(Path.Combine(a.Root, ".astra/save/values.json"), "truncated");
        using (var recovered = new SaveStore(a.Root)) { Assert.Equal(SaveRecovery.PreviousSnapshot, recovered.Recovery); Assert.Equal(long.MaxValue, recovered.GetInt64("score")); Assert.Equal("Maré", recovered.GetString("name")); Assert.True(recovered.GetBoolean("ready")); Assert.Equal(1.25, recovered.GetDouble("speed")); }
        File.WriteAllText(Path.Combine(a.Root, ".astra/save/values.json"), "{\"Version\":2,\"Values\":{}}");
        Assert.Throws<SaveVersionException>(() => { using var incompatible = new SaveStore(a.Root); });
    }
    [Test]
    public static void SaveFiles_AtomicOverwriteContainmentQuotasAndBoundedReads()
    {
        using var project = new ProjectDirectory(); using var store = new SaveStore(project.Root);
        store.WriteText("slot/story.txt", "épisode"); store.WriteText("slot/story.txt", "continua"); store.WriteBytes("slot/state.bin", [0, 128, 255]);
        Assert.Equal("continua", store.ReadText("slot/story.txt")); Assert.True(store.ReadBytes("slot/state.bin").SequenceEqual(new byte[] { 0, 128, 255 }));
        Assert.Throws<ArgumentException>(() => store.WriteText("../outside", "escape")); Assert.Throws<ArgumentException>(() => store.ReadBytes("C:/outside")); Assert.Throws<ArgumentException>(() => store.WriteText("bad\\escape", "escape"));
        Assert.Throws<IOException>(() => store.WriteBytes("large.bin", new byte[SaveStore.MaxFileBytes + 1]));
        var path = Path.Combine(project.Root, ".astra/save/files/slot/state.bin"); File.WriteAllBytes(path, new byte[SaveStore.MaxFileBytes + 1]);
        Assert.Throws<InvalidDataException>(() => store.ReadBytes("slot/state.bin"));
        Assert.True(store.DeleteFile("slot/state.bin")); Assert.True(!store.DeleteFile("slot/state.bin"));
    }
    private sealed class Scene : ISceneAccess
    {
        public uint WorldId => 1; public uint GenerationOf(ulong id) => id == 1 ? 1u : 0;
        public bool Exists(ulong id) => id == 1; public int GetActive(ulong id) => id == 1 ? 1 : -1;
        public TransformValue GetTransform(ulong id) => new(Vector3.Zero, Quaternion.Identity, Vector3.One);
        public bool SetTransform(ulong id, TransformValue value) => false;
        public bool SetBodyVelocity(ulong id, Vector3 velocity) => false;
        public bool MoveKinematic(ulong id, Vector3 position, Quaternion rotation) => false;
        public void Log(ulong id, string message) { }
    }
    [Test]
    public static void BehaviorCoroutine_SaveSurvivesStopAndRealProjectReopen()
    {
        using var project = new ProjectDirectory();
        File.WriteAllText(Path.Combine(project.Root, "SaveProbe.cs"), """
            using Astra;
            using System.Collections;
            [ComponentId("test.save")]
            public sealed class SaveProbe : Behavior {
                public override void Start() { StartCoroutine(Write()); }
                IEnumerator Write() { yield return null; Save.SetInt64("runs", Save.GetInt64("runs") + 1); Save.WriteText("slot/checkpoint.txt", "completed"); Save.Flush(); }
            }
            """);
        var compiled = new ProjectCompiler().Build(project.Root); Assert.True(compiled.Success);
        for (var run = 0; run < 2; ++run) {
            using var store = new SaveStore(project.Root); using var world = new BehaviorWorld();
            world.Start(compiled.Project!, new Scene(), [new(1, 1, "test.save", true, new Dictionary<string, JsonElement>())], saveStore: store);
            world.Update(.016f); world.Update(.016f); world.Dispose(); Assert.Equal(0, world.Failures.Count);
        }
        using var reopened = new SaveStore(project.Root); Assert.Equal(2L, reopened.GetInt64("runs")); Assert.Equal("completed", reopened.ReadText("slot/checkpoint.txt"));
    }
}
