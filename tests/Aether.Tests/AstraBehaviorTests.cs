using System.Numerics;
using System.Text;
using System.Text.Json;
using Astra;
using Astra.Compilation;
using Astra.Runtime;

namespace Aether.Tests;

public static class AstraBehaviorTests
{
    private sealed class Project : IDisposable
    {
        public string Root { get; } = Path.Combine(Path.GetTempPath(), "astra-behavior-" + Guid.NewGuid().ToString("N"));
        public Project(string source) { Directory.CreateDirectory(Root); File.WriteAllText(Path.Combine(Root, "Behavior.cs"), source); }
        public CompiledProject Compile()
        {
            var result = new ProjectCompiler().Build(Root);
            Assert.True(result.Success, string.Join("\n", result.Diagnostics.Select(d => d.Code + ": " + d.Message)));
            return result.Project!;
        }
        public void Dispose() { Directory.Delete(Root, recursive: true); }
    }
    private sealed class Scene : ISceneAccess
    {
        public readonly List<string> Events = [];
        public Vector3 Force, Impulse, Torque, AngularImpulse;
        public bool Exists(ulong id) => id is 1 or 2;
        public TransformValue GetTransform(ulong id) => new(Vector3.Zero, Quaternion.Identity, Vector3.One);
        public bool SetTransform(ulong id, TransformValue value) => Exists(id);
        public bool SetBodyVelocity(ulong id, Vector3 velocity) => Exists(id);
        public bool MoveKinematic(ulong id, Vector3 position, Quaternion rotation) => Exists(id);
        public bool AddForce(ulong id, Vector3 value) { Force += value; return Exists(id); }
        public bool AddImpulse(ulong id, Vector3 value) { Impulse += value; return Exists(id); }
        public bool AddTorque(ulong id, Vector3 value) { Torque += value; return Exists(id); }
        public bool AddAngularImpulse(ulong id, Vector3 value) { AngularImpulse += value; return Exists(id); }
        public Vector3 GetBodyVelocity(ulong id) => new(2, 0, 0);
        public void Log(ulong id, string message) => Events.Add(id + ":" + message);
    }
    private const string Source = """
        using Astra;
        using System;
        using System.Numerics;
        [ComponentId("test.force")]
        public sealed class ForceBehavior : Behavior
        {
            [PropertyId("strength")] public float Strength = 1;
            [PropertyId("target")] public ObjectReference Target;
            public override void Start() => Scene.Log(ObjectId, "start:" + InstanceId);
            public override void FixedUpdate(float dt) { Scene.AddForce(ObjectId, new Vector3(Strength,0,0)); Scene.AddTorque(ObjectId, Vector3.UnitY); }
            public override void TriggerEnter(ObjectReference other) { Scene.AddImpulse(Target.ObjectId, Vector3.UnitY); Scene.Log(ObjectId,"enter:"+other.ObjectId); }
            public override void TriggerStay(ObjectReference other) => Scene.Log(ObjectId,"stay");
            public override void TriggerExit(ObjectReference other) { Scene.AddAngularImpulse(Target.ObjectId, Vector3.UnitZ); Scene.Log(ObjectId,"exit"); }
            public override void Stop() => Scene.Log(ObjectId,"stop:"+InstanceId);
        }
        [ComponentId("test.failure")]
        public sealed class FailingBehavior : Behavior
        {
            public override void TriggerEnter(ObjectReference other) => throw new InvalidOperationException("isolated trigger failure");
            public override void FixedUpdate(float dt) => Scene.Log(ObjectId,"bad-fixed");
        }
        """;
    private static BehaviorAttachment Attach(ulong objectId, ulong instance, string type, float strength = 1) =>
        new(objectId, instance, type, true, new Dictionary<string, JsonElement>
        {
            ["strength"] = JsonSerializer.SerializeToElement(strength),
            ["target"] = JsonSerializer.SerializeToElement(new ObjectReference(2))
        });

    [Test]
    public static void CompileAttachFixedTriggerStop_PreservesInstancesAndIsolatesFailure()
    {
        using var project = new Project(Source); var compiled = project.Compile(); var scene = new Scene();
        Assert.Equal(2, compiled.Types.Length);
        using var world = new BehaviorWorld();
        world.Start(compiled, scene, [Attach(1, 10, "test.force", 3), Attach(1, 11, "test.force", 7), Attach(1, 12, "test.failure"), Attach(2, 10, "test.force", 2)]);
        world.FixedUpdate(1f / 60); Assert.Close(12, scene.Force.X);
        world.Trigger(1, 2, 0); world.Trigger(1, 2, 1); world.Trigger(1, 2, 2);
        Assert.Close(2, scene.Impulse.Y, what: "only sensor owner's two instances receive enter");
        Assert.Close(2, scene.AngularImpulse.Z);
        Assert.Equal(1, world.Failures.Count); Assert.Equal("TriggerEnter", world.Failures[0].Phase);
        world.FixedUpdate(1f / 60); Assert.Close(24, scene.Force.X);
        Assert.Equal(1, scene.Events.Count(e => e == "1:bad-fixed"), "failed behavior stays disabled");
        world.Dispose(); Assert.False(world.Running);
        Assert.Equal(3, scene.Events.Count(e => e.Contains(":stop:")));
        world.Start(compiled, scene, [Attach(1, 10, "test.force")]);
        Assert.Equal(0, world.Failures.Count, "new Play resets runtime failure state");
    }
    [Test]
    public static void SchemaTypeChangeAndDuplicateIdentity_RejectBeforeStartingWorld()
    {
        using var project = new Project(Source); var compiled = project.Compile(); var scene = new Scene();
        using var world = new BehaviorWorld();
        Assert.Throws<InvalidOperationException>(() => world.Start(compiled, scene, [Attach(1, 1, "test.force"), Attach(1, 1, "test.force")]));
        Assert.False(world.Running); Assert.Equal(0, scene.Events.Count);
        var changed = Attach(1, 1, "test.force") with { PropertyTypes = new Dictionary<string, string> { ["strength"] = "string" } };
        Assert.Throws<InvalidDataException>(() => world.Start(compiled, scene, [changed]));
        Assert.False(world.Running); Assert.Equal(0, scene.Events.Count);
        world.Start(compiled, scene, [Attach(1, 1, "test.force")]); Assert.True(world.Running, "failed preparation can recover");
    }
    [Test]
    public static unsafe void ApplyPublicationAndCompilerDiagnostics_PreserveLastGoodGeneration()
    {
        using var project = new Project(Source); var bytes = Encoding.UTF8.GetBytes(project.Root);
        delegate* unmanaged<byte*, int, int> build = &NativeCompiler.Build;
        delegate* unmanaged<int> commit = &NativeCompiler.Commit;
        fixed (byte* root = bytes) Assert.Equal(0, build(root, bytes.Length));
        Assert.False(Directory.Exists(Path.Combine(project.Root, ".astra")), "build does not publish or run code");
        Assert.Equal(0, commit()); var applied = NativeCompiler.LoadApplied(project.Root);
        var pointer = Path.Combine(project.Root, ".astra", "code", "current");
        Assert.Equal(applied.Id, File.ReadAllText(pointer));
        File.WriteAllText(Path.Combine(project.Root, "Behavior.cs"), "public class Broken { syntax error }");
        fixed (byte* root = bytes) Assert.Equal(1, build(root, bytes.Length));
        Assert.Equal(1, commit(), "invalid candidate cannot replace applied generation");
        Assert.Equal(applied.Id, File.ReadAllText(pointer));
        var failed = new ProjectCompiler().Build(project.Root);
        Assert.True(failed.Diagnostics.Any(d => d.Error && d.File == "Behavior.cs" && d.Line == 1));
    }
    [Test]
    public static void DocumentationExamples_CompileWithTheShippedApi()
    {
        var directory = new DirectoryInfo(AppContext.BaseDirectory);
        while (directory is not null && !File.Exists(Path.Combine(directory.FullName, "docs", "componentes", "fisica-codigo.md"))) directory = directory.Parent;
        Assert.True(directory is not null, "repository documentation available");
        var markdown = File.ReadAllText(Path.Combine(directory!.FullName, "docs", "componentes", "fisica-codigo.md"));
        using var project = new Project("");
        var blocks = markdown.Split("```csharp").Skip(1).Select(s => s[..s.IndexOf("```", StringComparison.Ordinal)]).ToArray();
        Assert.Equal(2, blocks.Length);
        for (var i = 0; i < blocks.Length; ++i) File.WriteAllText(Path.Combine(project.Root, "Example" + i + ".cs"), blocks[i]);
        Assert.Equal(2, project.Compile().Types.Length, "both documented examples emit attachable schemas");
    }
}
