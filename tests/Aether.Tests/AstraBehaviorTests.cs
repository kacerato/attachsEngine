using System.Numerics;
using System.Text;
using System.Text.Json;
using Astra;
using Astra.Compilation;
using Astra.Runtime;

namespace Aether.Tests;

public static class AstraBehaviorTests
{
    [Test]
    public static void TagsAcceptanceFixture_CompilesWithTheProjectCompiler()
    {
        var root = new DirectoryInfo(AppContext.BaseDirectory);
        while (root is not null && !Directory.Exists(Path.Combine(root.FullName, "tests", "fixtures", "object-tags")))
            root = root.Parent;
        Assert.True(root is not null, "fixture de aceite no repositório");
        var source = File.ReadAllText(Path.Combine(root!.FullName, "tests", "fixtures", "object-tags", "TagProbe.cs"));
        using var project = new Project(source);
        var compiled = project.Compile();
        Assert.Equal("acceptance.tags.driver", compiled.Types.Single().Id);
    }

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
        public int SlotWrites;
        public readonly List<string> SlotPropertyIds = [];
        public bool Exists(ulong id) => id is 1 or 2;
        public bool HierarchyActive = true;
        public int GetActive(ulong id) => Exists(id) ? (HierarchyActive ? 1 : 0) : -1;
        public int GetActiveSelf(ulong id) => Exists(id) ? 1 : -1;
        public uint WorldId => 7;
        public uint GenerationOf(ulong id) => Exists(id) ? 1u : 0u;
        public WorldStatus LastStatus => WorldStatus.Ok;
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
        public ulong FindComponent(ulong objectId, string typeId, uint ordinal) =>
            Exists(objectId) && typeId == ComponentIds.MeshRenderer && ordinal == 0 ? 17ul : 0;
        public bool SetSlotProperty(ulong objectId, ulong instanceId, string propertyId, uint slot, uint kind, ulong bits)
        { ++SlotWrites; SlotPropertyIds.Add(propertyId); return Exists(objectId) && instanceId == 17; }
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
    // Inspector em Play: o campo e o Enabled chegam à instância viva, sem reiniciar.
    [Test]
    public static void PlayModeEdit_ChangesFieldAndEnabledOfTheLiveInstance()
    {
        using var project = new Project(Source); var compiled = project.Compile(); var scene = new Scene();
        using var world = new BehaviorWorld();
        world.Start(compiled, scene, [Attach(1, 10, "test.force", 3)]);
        world.FixedUpdate(1f / 60); Assert.Close(3, scene.Force.X);
        Assert.True(world.Edit(1, 10, new BehaviorEdit(true, new Dictionary<string, JsonElement>
            { ["strength"] = JsonSerializer.SerializeToElement(5f) })), "campo aceito");
        world.FixedUpdate(1f / 60); Assert.Close(8, scene.Force.X, what: "o valor novo vale no passo seguinte");
        Assert.Equal(1, scene.Events.Count(e => e.Contains("start:")), "editar não reinicia a instância");
        Assert.True(world.Edit(1, 10, new BehaviorEdit(false, null)), "desligar aceito");
        world.FixedUpdate(1f / 60); Assert.Close(8, scene.Force.X, what: "desligada pelo Inspector não recebe FixedUpdate");
        Assert.False(world.Edit(1, 99, new BehaviorEdit(true, null)), "instância inexistente é recusada");
    }
    private sealed class ComponentScene : ISceneAccess
    {
        public bool Exists(ulong id) => id is 1 or 2;
        public int GetActive(ulong id) => Exists(id) ? 1 : -1;
        public uint WorldId => 7;
        public uint GenerationOf(ulong id) => Exists(id) ? 1u : 0u;
        public WorldStatus LastStatus => WorldStatus.Ok;
        public TransformValue GetTransform(ulong id) => new(Vector3.Zero, Quaternion.Identity, Vector3.One);
        public bool SetTransform(ulong id, TransformValue value) => Exists(id);
        public bool SetBodyVelocity(ulong id, Vector3 velocity) => Exists(id);
        public bool MoveKinematic(ulong id, Vector3 position, Quaternion rotation) => Exists(id);
        public bool AddForce(ulong id, Vector3 value) => Exists(id);
        public bool AddImpulse(ulong id, Vector3 value) => Exists(id);
        public bool AddTorque(ulong id, Vector3 value) => Exists(id);
        public bool AddAngularImpulse(ulong id, Vector3 value) => Exists(id);
        public Vector3 GetBodyVelocity(ulong id) => Vector3.Zero;
        public readonly List<string> Events = [];
        public void Log(ulong id, string message) => Events.Add(message);
        // O objeto 2 tem dois componentes: um colisor (40) e um corpo físico (41).
        public int ComponentCount(ulong objectId) => objectId == 2 ? 2 : 0;
        public (ulong Instance, string TypeId) ComponentAt(ulong objectId, uint index) =>
            index == 0 ? (40ul, "astra.physics.collider") : (41ul, "astra.physics.body");
    }
    private const string ComponentFieldSource = """
        using Astra;
        using Astra.Components;
        [ComponentId("test.follow")]
        public sealed class Follow : Behavior
        {
            [PropertyId("body")] public PhysicsBody? Body;
            [PropertyId("wrong")] public PhysicsBody? Wrong;
            public override void Start() => Scene.Log(ObjectId, "body:" + (Body?.InstanceId ?? 0) + " wrong:" + (Wrong?.InstanceId ?? 0));
        }
        """;

    // Unity: campo `public Rigidbody body;`. O compilador publica o tipo exigido e
    // o mundo entrega a fachada daquela instância; instância de outro tipo ou
    // inexistente deixa o campo vazio (o "Missing" da Unity).
    [Test]
    public static void ComponentField_ResolvesTheAuthoredInstanceToItsFacade()
    {
        using var project = new Project(ComponentFieldSource); var compiled = project.Compile();
        var schema = compiled.Types.Single();
        Assert.Equal("component:astra.physics.body", schema.Properties.Single(p => p.Id == "body").ValueType);
        using var world = new BehaviorWorld(); var scene = new ComponentScene();
        world.Start(compiled, scene, [new(1, 10, "test.follow", true, new Dictionary<string, JsonElement>
        {
            ["body"] = JsonSerializer.SerializeToElement(new { ObjectId = 2ul, InstanceId = 41ul }),
            ["wrong"] = JsonSerializer.SerializeToElement(new { ObjectId = 2ul, InstanceId = 40ul })
        })]);
        Assert.Equal("body:41 wrong:0", string.Join(' ', scene.Events),
                     "instância do tipo certo vira fachada; a de outro tipo fica vazia");
        Assert.Equal(0, world.Failures.Count, "campo de componente aceito no Start");
    }
    private const string ListFieldSource = """
        using Astra;
        using Astra.Components;
        using System.Collections.Generic;
        [ComponentId("test.list")]
        public sealed class Patrol : Behavior
        {
            [PropertyId("waits")] public float[] Waits = [];
            [PropertyId("names")] public List<string> Names = new();
            [PropertyId("bodies")] public PhysicsBody?[] Bodies = [];
            [PropertyId("seed"), HideInInspector] public int Seed;
            [PropertyId("secret"), SerializeField] private int secret;
            public override void Start() => Scene.Log(ObjectId, string.Join(",", Waits) + "|" + string.Join(",", Names) + "|" +
                string.Join(",", System.Linq.Enumerable.Select(Bodies, b => b?.InstanceId ?? 0)) + "|" + Seed + "|" + secret);
        }
        """;

    // Unity Manual/InspectorArray e [HideInInspector]/[SerializeField]: listas e
    // campos ocultos/privados chegam ao esquema e à instância no Play.
    [Test]
    public static void ListAndAttributeFields_ReachTheSchemaAndTheInstance()
    {
        using var project = new Project(ListFieldSource); var compiled = project.Compile();
        var properties = compiled.Types.Single().Properties.ToDictionary(p => p.Id);
        Assert.Equal("array:float", properties["waits"].ValueType);
        Assert.Equal("array:string", properties["names"].ValueType);
        Assert.Equal("array:component:astra.physics.body", properties["bodies"].ValueType);
        Assert.True(properties["seed"].Hidden && !properties["waits"].Hidden, "HideInInspector marca só o campo dele");
        Assert.Equal("int32", properties["secret"].ValueType, "SerializeField expõe o campo privado");
        using var world = new BehaviorWorld(); var scene = new ComponentScene();
        world.Start(compiled, scene, [new(1, 10, "test.list", true, new Dictionary<string, JsonElement>
        {
            ["waits"] = JsonSerializer.SerializeToElement(new[] { 1.5f, 2f }),
            ["names"] = JsonSerializer.SerializeToElement(new[] { "a", "b" }),
            ["bodies"] = JsonSerializer.SerializeToElement(new[] { new { ObjectId = 2ul, InstanceId = 41ul }, new { ObjectId = 2ul, InstanceId = 40ul } }),
            ["seed"] = JsonSerializer.SerializeToElement(9),
            ["secret"] = JsonSerializer.SerializeToElement(4)
        })]);
        Assert.Equal("1.5,2|a,b|41,0|9|4", string.Join(' ', scene.Events), "listas, oculto e privado aplicados");
    }
    private const string ColorFieldSource = """
        using Astra;
        [ComponentId("test.tint")]
        public sealed class Tint : Behavior
        {
            [PropertyId("plain")] public Color Plain = Color.White;
            [PropertyId("glow"), ColorUsage(false, true)] public Color Glow;
            [PropertyId("palette")] public Color[] Palette = [];
            public override void Start() => Scene.Log(ObjectId, $"{Plain.R},{Plain.A}|{Glow.R}|{Palette.Length}");
        }
        """;

    // Unity: campo Color e [ColorUsage(showAlpha, hdr)].
    [Test]
    public static void ColorFields_CarryUsageFlagsAndReachTheInstance()
    {
        using var project = new Project(ColorFieldSource); var compiled = project.Compile();
        var properties = compiled.Types.Single().Properties.ToDictionary(p => p.Id);
        Assert.Equal("color", properties["plain"].ValueType);
        Assert.Equal("color:hdr:noalpha", properties["glow"].ValueType);
        Assert.Equal("array:color", properties["palette"].ValueType);
        using var world = new BehaviorWorld(); var scene = new ComponentScene();
        world.Start(compiled, scene, [new(1, 10, "test.tint", true, new Dictionary<string, JsonElement>
        {
            ["plain"] = JsonSerializer.SerializeToElement(new { R = 0.25f, G = 0f, B = 0f, A = 0.5f }),
            ["glow"] = JsonSerializer.SerializeToElement(new { R = 4f, G = 0f, B = 0f, A = 1f }),
            ["palette"] = JsonSerializer.SerializeToElement(new[] { new { R = 1f, G = 1f, B = 1f, A = 1f } })
        })]);
        Assert.Equal("0.25,0.5|4|1", string.Join(' ', scene.Events), "cores aplicadas à instância");
    }
    private const string GradientFieldSource = """
        using Astra;
        [ComponentId("test.sky")]
        public sealed class Sky : Behavior
        {
            [PropertyId("tones")] public Gradient Tones = new();
            [PropertyId("glow"), GradientUsage(true)] public Gradient Glow = new();
            public override void Start()
            {
                var mid = Tones.Evaluate(0.5f);
                Scene.Log(ObjectId, $"{mid.R:0.00},{mid.G:0.00},{mid.A:0.00}|{Tones.Mode}");
            }
        }
        """;

    // Unity: campo Gradient e [GradientUsage(hdr)]; Evaluate segue o editor.
    [Test]
    public static void GradientFields_ReachTheInstanceAndEvaluate()
    {
        using var project = new Project(GradientFieldSource); var compiled = project.Compile();
        var properties = compiled.Types.Single().Properties.ToDictionary(p => p.Id);
        Assert.Equal("gradient", properties["tones"].ValueType);
        Assert.Equal("gradient:hdr", properties["glow"].ValueType);
        using var world = new BehaviorWorld(); var scene = new ComponentScene();
        world.Start(compiled, scene, [new(1, 10, "test.sky", true, new Dictionary<string, JsonElement>
        {
            ["tones"] = JsonSerializer.SerializeToElement(new
            {
                Mode = 0,
                ColorKeys = new[] { new { Time = 0f, Color = new { R = 1f, G = 0f, B = 0f, A = 1f } }, new { Time = 1f, Color = new { R = 0f, G = 1f, B = 0f, A = 1f } } },
                AlphaKeys = new[] { new { Time = 0f, Alpha = 1f }, new { Time = 1f, Alpha = 0f } }
            })
        })]);
        Assert.Equal("0.50,0.50,0.50|Blend", string.Join(' ', scene.Events), "gradiente aplicado e avaliado");
    }
    private const string CurveFieldSource = """
        using Astra;
        [ComponentId("test.jump")]
        public sealed class Jump : Behavior
        {
            [PropertyId("height")] public AnimationCurve Height = new();
            public override void Start() =>
                Scene.Log(ObjectId, $"{Height.Evaluate(0.5f):0.00}|{Height.Evaluate(1.5f):0.00}|{Height.Evaluate(2.5f):0.00}");
        }
        """;

    // Unity: campo AnimationCurve; Evaluate segue o editor (linear e Loop).
    [Test]
    public static void CurveFields_ReachTheInstanceAndEvaluateWithWrap()
    {
        using var project = new Project(CurveFieldSource); var compiled = project.Compile();
        Assert.Equal("curve", compiled.Types.Single().Properties.Single().ValueType);
        using var world = new BehaviorWorld(); var scene = new ComponentScene();
        world.Start(compiled, scene, [new(1, 10, "test.jump", true, new Dictionary<string, JsonElement>
        {
            ["height"] = JsonSerializer.SerializeToElement(new
            {
                PreWrapMode = 0, PostWrapMode = 1,
                Keys = new[]
                {
                    new { Time = 0f, Value = 0f, InTangent = 2f, OutTangent = 2f, LeftMode = 2, RightMode = 2, Broken = false },
                    new { Time = 1f, Value = 2f, InTangent = 2f, OutTangent = 2f, LeftMode = 2, RightMode = 2, Broken = false }
                }
            })
        })]);
        Assert.Equal("1.00|1.00|1.00", string.Join(' ', scene.Events), "linear no meio e Loop depois do fim");
    }
    private const string LifecycleSource = """
        using Astra;
        [ComponentId("test.life")]
        public sealed class LifeBehavior : Behavior
        {
            public override void Awake() => Scene.Log(ObjectId, "awake:" + InstanceId);
            public override void Enable() => Scene.Log(ObjectId, "enable:" + InstanceId);
            public override void Start() => Scene.Log(ObjectId, "start:" + InstanceId);
            public override void Update(float dt)
            {
                Scene.Log(ObjectId, "update:" + InstanceId);
                // A instância 21 se desliga no primeiro quadro: Disable vem no seguinte.
                if (InstanceId == 21) Enabled = false;
            }
            public override void LateUpdate(float dt) => Scene.Log(ObjectId, "late:" + InstanceId);
            public override void ApplicationPause(bool paused) => Scene.Log(ObjectId, "pause:" + paused);
            public override void ApplicationFocus(bool focused) => Scene.Log(ObjectId, "focus:" + focused);
            public override void Disable() => Scene.Log(ObjectId, "disable:" + InstanceId);
            public override void Stop() => Scene.Log(ObjectId, "stop:" + InstanceId);
        }
        """;
    private static BehaviorAttachment Life(ulong instance, bool enabled = true) =>
        new(1, instance, "test.life", enabled, new Dictionary<string, JsonElement>());

    [Test]
    public static void Lifecycle_AwakeEnableStartUpdateLateDisableStop_InUnityOrder()
    {
        using var project = new Project(LifecycleSource); var compiled = project.Compile(); var scene = new Scene();
        using var world = new BehaviorWorld();
        world.Start(compiled, scene, [Life(20), Life(21), Life(22, enabled: false)]);
        // Awake de todas (até a desativada) antes do primeiro Enable/Start.
        Assert.Equal("1:awake:20 1:awake:21 1:awake:22 1:enable:20 1:start:20 1:enable:21 1:start:21",
                     string.Join(' ', scene.Events));
        scene.Events.Clear();
        world.Update(1f / 60); world.LateUpdate(1f / 60);
        // Enabled=false no Update: o Disable sai no mesmo quadro, no primeiro ponto
        // em que o mundo percorre as instâncias (aqui, a fase LateUpdate).
        Assert.Equal("1:update:20 1:update:21 1:late:20 1:disable:21", string.Join(' ', scene.Events),
                     "LateUpdate depois de todos os Update, só para quem continua ativo");
        scene.Events.Clear();
        world.Application(BehaviorWorld.ApplicationEvent.Pause, true);
        Assert.Equal("1:pause:True", string.Join(' ', scene.Events), "Disable não se repete; pausa só às ativas");
        scene.Events.Clear();
        world.Dispose();
        Assert.Equal("1:stop:21 1:disable:20 1:stop:20", string.Join(' ', scene.Events),
                     "fim do Play: Disable das ativas e Stop de quem começou");
    }

    [Test]
    public static void Lifecycle_InactiveHierarchyDefersAwakeAndResumesTheSameInstance()
    {
        using var project = new Project(LifecycleSource); var compiled = project.Compile();
        var scene = new Scene { HierarchyActive = false };
        using var world = new BehaviorWorld();
        world.Start(compiled, scene, [Life(20), Life(22, enabled: false)]);
        var instance = world.FindBehaviors(1, typeof(Behavior)).Cast<Behavior>().First();
        Assert.Equal(0, scene.Events.Count, "objeto inativo não recebe Awake nem Start");
        world.Update(.01f); world.FixedUpdate(.01f); world.LateUpdate(.01f);
        world.Application(BehaviorWorld.ApplicationEvent.Pause, true);
        Assert.Equal(0, scene.Events.Count, "nenhuma fase executa o objeto inativo");
        scene.HierarchyActive = true;
        world.Update(.01f);
        Assert.Equal("1:awake:20 1:enable:20 1:start:20 1:update:20 1:awake:22", string.Join(' ', scene.Events));
        scene.Events.Clear(); scene.HierarchyActive = false;
        world.LateUpdate(.01f); world.Update(.01f); world.FixedUpdate(.01f);
        Assert.Equal("1:disable:20", string.Join(' ', scene.Events), "uma transição, sem callbacks de quadro");
        Assert.True(instance.Enabled, "inatividade herdada não apaga Enabled");
        scene.Events.Clear(); scene.HierarchyActive = true;
        world.Update(.01f);
        Assert.Equal("1:enable:20 1:update:20", string.Join(' ', scene.Events), "Awake e Start não repetem");
        Assert.True(ReferenceEquals(instance, world.FindBehaviors(1, typeof(Behavior)).First()), "mesma instância e campos");
        Assert.Equal(0, world.Failures.Count);
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
    public static void GraphicsAbiStructs_MatchTheNativeLayout()
    {
        // Os mesmos números do static_assert em native/scene/script_runtime.h:
        // a ponte recusa tamanho diferente, e no aparelho isso vira "gráficos
        // indisponíveis" em vez de um erro de compilação.
        Assert.Equal(224, System.Runtime.CompilerServices.Unsafe.SizeOf<GraphicsSettings>(), "GraphicsSettings");
        Assert.Equal(272, System.Runtime.CompilerServices.Unsafe.SizeOf<ResolvedGraphicsSettings>(), "ResolvedGraphicsSettings");
        Assert.Equal(44, System.Runtime.CompilerServices.Unsafe.SizeOf<GraphicsCapabilities>(), "GraphicsCapabilities");
        Assert.Equal(88, System.Runtime.CompilerServices.Unsafe.SizeOf<TextureStreamingStats>(), "TextureStreamingStats");
        Assert.Equal(56, System.Runtime.CompilerServices.Unsafe.SizeOf<FrameStatistics>(), "FrameStatistics");
        Assert.Equal(728, System.Runtime.CompilerServices.Unsafe.SizeOf<NativeGraphicsState>(), "NativeGraphicsState");
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
        // Cada bloco `csharp` deste documento e um exemplo COMPLETO: se um deles
        // deixar de compilar, a documentacao passou a ensinar uma API que nao existe.
        Assert.True(blocks.Length >= 2, "documento tem exemplos");
        for (var i = 0; i < blocks.Length; ++i) File.WriteAllText(Path.Combine(project.Root, "Example" + i + ".cs"), blocks[i]);
        Assert.Equal(blocks.Length, project.Compile().Types.Length, "todo exemplo documentado emite um schema anexável");
    }

    [Test]
    public static void MaterialSlotAndAuthoredTextureReference_CompileThroughTheProjectCompiler()
    {
        const string source = """
            using Astra;
            using System.Numerics;
            [ComponentId("test.material-driver")]
            public sealed class MaterialDriver : Behavior
            {
                [PropertyId("albedo")] public AssetReference Albedo;
                public override void Start()
                {
                    var component = Object.GetComponent(ComponentIds.MeshRenderer) ??
                        throw new System.InvalidOperationException("MeshRenderer ausente");
                    var material = component.Material(1);
                    material.BaseColor = new Vector3(0.8f, 0.7f, 0.6f);
                    material.Roughness = 0.35f;
                    material.AlphaMode = MaterialAlphaMode.Mask;
                    material.SetTexture(MaterialTextureBinding.BaseColor, Albedo);
                    var normal = material.TextureSampling(MaterialTextureBinding.Normal);
                    normal.Scale = new Vector2(2, 2);
                    normal.Wrap = MaterialWrap.Repeat;
                }
            }
            """;
        using var project = new Project(source);
        Assert.Equal(1, project.Compile().Types.Length, "fachada tipada compila no pipeline real de scripts");
    }

    [Test]
    public static void MaterialSlotCompositeValues_ValidateBeforeWritingAnyAxis()
    {
        var scene = new Scene();
        var component = GameObject.Resolve(scene, 1).GetComponent(ComponentIds.MeshRenderer) ??
            throw new InvalidOperationException("fixture sem MeshRenderer");
        var material = component.Material();
        Assert.Throws<ArgumentOutOfRangeException>(() => material.BaseColor = new Vector3(.5f, float.NaN, .5f));
        Assert.Throws<ArgumentOutOfRangeException>(() => material.Scale = new Vector2(1, 0));
        Assert.Throws<ArgumentOutOfRangeException>(() => material.TextureSampling(MaterialTextureBinding.Normal).Offset =
            new Vector2(0, float.PositiveInfinity));
        Assert.Equal(0, scene.SlotWrites, "vetor inválido não deixa primeiro eixo aplicado");
        material.TextureSampling(MaterialTextureBinding.Normal).Scale = new Vector2(2, 3);
        Assert.Equal("sampling.normal.scale_u", scene.SlotPropertyIds[0]);
        Assert.Equal("sampling.normal.scale_v", scene.SlotPropertyIds[1]);
        Assert.Throws<ArgumentOutOfRangeException>(() => material.TextureSampling((MaterialTextureBinding)99));
    }
}
