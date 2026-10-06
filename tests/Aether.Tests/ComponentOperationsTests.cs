using System.Numerics;
using System.Runtime.InteropServices;
using System.Text.Json;
using Astra;
using Astra.Compilation;
using Astra.Components;
using Astra.Runtime;

namespace Aether.Tests;

// Rota gerenciada da família astra.component.operations: fachada gerada ->
// Component.Invoke -> IComponentOperationAccess, e fila de eventos -> assinatura
// do Behavior -> isolamento de falha. O lado nativo (ABI42, fila e serviços
// reais) é coberto por tests/native/test_component_operations.cpp.
public static class ComponentOperationsTests
{
    private sealed class Scene : ISceneAccess, IComponentOperationAccess
    {
        public uint WorldId => 31;
        public uint GenerationOf(ulong id) => 2;
        public bool Exists(ulong id) => id is 1 or 2;
        public int GetActive(ulong id) => 1;
        public WorldStatus Status = WorldStatus.Ok;
        public WorldStatus LastStatus => Status;
        public TransformValue GetTransform(ulong id) => new(Vector3.Zero, Quaternion.Identity, Vector3.One);
        public bool SetTransform(ulong id, TransformValue value) => true;
        public bool SetBodyVelocity(ulong id, Vector3 value) => true;
        public bool MoveKinematic(ulong id, Vector3 value, Quaternion rotation) => true;
        public readonly List<string> Log_ = [];
        public void Log(ulong id, string message) => Log_.Add(message);
        public int ComponentCount(ulong id) => id == 1 ? 2 : 0;
        public (ulong Instance, string TypeId) ComponentAt(ulong id, uint index) =>
            index == 0 ? (8UL, "astra.script.behavior") : (5UL, "astra.time.timer");
        public ulong FindComponent(ulong id, string type, uint ordinal) => id == 1 && type == "astra.time.timer" && ordinal == 0 ? 5UL : 0UL;
        public readonly List<(ulong Object, uint World, uint Generation, ulong Instance, string Method, ComponentValue[] Args)> Calls = [];
        public ComponentValue Result;
        public bool InvokeComponentMethod(ulong objectId, uint world, uint generation, ulong instanceId, string method,
            ReadOnlySpan<ComponentValue> arguments, out ComponentValue result)
        {
            Calls.Add((objectId, world, generation, instanceId, method, arguments.ToArray()));
            result = Result; return Status == WorldStatus.Ok;
        }
        public readonly Queue<ComponentEventRecord> Pending = new();
        public int Polls;
        public int PollComponentEvents(Span<ComponentEventRecord> destination)
        {
            ++Polls; var count = 0;
            while (count < destination.Length && Pending.TryDequeue(out var record)) destination[count++] = record;
            return count;
        }
        public bool DeclaresComponentEvent(string typeId, string eventId) => typeId == "astra.time.timer" && eventId == "elapsed";
        public void Elapsed(ulong instance, long count, uint generation = 2) => Pending.Enqueue(new(1, 31, generation, instance,
            "astra.time.timer", "elapsed", 1, 0, ComponentValue.Integer(count), default, default));
    }
    private sealed class Project : IDisposable
    {
        private readonly string root = Path.Combine(Path.GetTempPath(), "astra-ops-" + Guid.NewGuid().ToString("N"));
        public CompiledProject Compiled { get; }
        public Project(string source)
        {
            Directory.CreateDirectory(root); File.WriteAllText(Path.Combine(root, "Probe.cs"), source);
            var result = new ProjectCompiler().Build(root);
            Assert.True(result.Success, string.Join("\n", result.Diagnostics.Select(d => d.Message)));
            Compiled = result.Project!;
        }
        public void Dispose() => Directory.Delete(root, true);
    }

    [Test] public static void ComponentValue_LayoutMatchesNativeAndRefusesWrongReads()
    {
        Assert.Equal(24, Marshal.SizeOf<ComponentValue>());
        Assert.Equal(112, Marshal.SizeOf<NativeBehaviorRuntime.NativeComponentEvent>());
        Assert.Equal(8 + 4 * IntPtr.Size, Marshal.SizeOf<NativeBehaviorRuntime.NativeComponentOperations>());
        var extension = Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("Extension").ToInt64();
        Assert.Equal(Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("GuiTransitions").ToInt64() + IntPtr.Size, extension);
        Assert.Equal(extension + IntPtr.Size, (long)Marshal.SizeOf<NativeBehaviorRuntime.SceneAccess>());
        Assert.Equal(2.5, ComponentValue.Number(2.5).AsNumber());
        Assert.Equal(new Vector3(1, 2, 3), ComponentValue.Vector(new(1, 2, 3)).AsVector3());
        Assert.True(ComponentValue.Boolean(true).AsBoolean());
        Assert.Throws<InvalidOperationException>(() => ComponentValue.Integer(3).AsNumber());
        Assert.Equal(ComponentValueKind.None, default(ComponentValue).Kind);
    }

    [Test] public static void GeneratedFacade_InvokesDeclaredMethodsWithIdentityAndTypedValues()
    {
        var scene = new Scene();
        var timer = GameObject.Resolve(scene, 1).GetComponent<GameTimer>()!.Value;
        timer.Start(.75);
        scene.Result = ComponentValue.Number(.5);
        Assert.Equal(.5, timer.Remaining());
        var call = scene.Calls[0];
        Assert.Equal((1UL, 31u, 2u, 5UL, "start"), (call.Object, call.World, call.Generation, call.Instance, call.Method));
        Assert.Equal(.75, call.Args.Single().AsNumber());
        Assert.Equal(0, scene.Calls[1].Args.Length);
        scene.Result = ComponentValue.Integer(1);
        Assert.Throws<InvalidOperationException>(() => timer.Remaining());
        scene.Status = WorldStatus.UnknownOperation;
        WorldStatus refused = WorldStatus.Ok;
        try { timer.Stop(); } catch (WorldException error) { refused = error.Status; }
        Assert.Equal(WorldStatus.UnknownOperation, refused);
    }

    [Test] public static void Subscriptions_DeliverInPhaseOrderFilterIdentityAndIsolateFailures()
    {
        using var project = new Project("""
            using Astra; using Astra.Components;
            [ComponentId("test.ops")] public sealed class Probe : Behavior {
                public static int Received; public static long Total;
                ComponentSubscription subscription;
                public override void Start() {
                    var timer = Object.GetComponent<GameTimer>().Value;
                    subscription = timer.OnElapsed(this, e => {
                        Received++; Total += e[0].AsInteger();
                        Scene.Log(ObjectId, "event:" + e.EventId + ":" + e.Count);
                        if (Total >= 10) throw new System.Exception("handler failure");
                    });
                    try { Connect(timer.Component, "finished", e => { }); } catch (WorldException error) { Scene.Log(ObjectId, "refused:" + error.Status); }
                }
                public override void Update(float dt) { Scene.Log(ObjectId, "update"); if (Received == 2) subscription.Dispose(); }
            }
            """);
        var scene = new Scene();
        using var world = new BehaviorWorld();
        world.Start(project.Compiled, scene, [new BehaviorAttachment(1, 8, "test.ops", true, new Dictionary<string, JsonElement>())]);
        Assert.True(scene.Log_.Contains("refused:UnknownOperation"), "undeclared event refused on subscription");
        scene.Elapsed(5, 1);
        scene.Elapsed(6, 4);           // outra instância: não é a assinada
        scene.Elapsed(5, 2, generation: 9); // outra geração do objeto: handle vencido
        scene.Log_.Clear(); world.Update(.016f);
        Assert.Equal("event:elapsed:1,update", string.Join(",", scene.Log_));
        scene.Elapsed(5, 2); scene.Log_.Clear(); world.FixedUpdate(.02f);
        Assert.Equal("event:elapsed:1", string.Join(",", scene.Log_));
        world.Update(.016f);               // Received == 2: o próprio script encerra a assinatura
        scene.Elapsed(5, 100); scene.Log_.Clear(); world.LateUpdate(.016f);
        Assert.Equal(0, scene.Log_.Count);
        Assert.Equal(0, world.Failures.Count);
        Assert.True(scene.Polls >= 4, "queue drained at Update, FixedUpdate and LateUpdate even without matches");
    }

    [Test] public static void Subscriptions_HandlerFailureIsolatesOwnerAndStopEndsAll()
    {
        using var project = new Project("""
            using Astra; using Astra.Components;
            [ComponentId("test.ops")] public sealed class Probe : Behavior {
                public static ComponentSubscription Kept;
                public override void Start() {
                    Kept = Object.GetComponent<GameTimer>().Value.OnElapsed(this, e => throw new System.Exception("boom"));
                }
            }
            """);
        var scene = new Scene();
        var world = new BehaviorWorld();
        world.Start(project.Compiled, scene, [new BehaviorAttachment(1, 8, "test.ops", true, new Dictionary<string, JsonElement>())]);
        scene.Elapsed(5, 1); world.Update(.016f);
        Assert.Equal(1, world.Failures.Count);
        Assert.True(world.Failures[0].Phase == "Event astra.time.timer/elapsed", world.Failures[0].Phase);
        scene.Elapsed(5, 1); world.Update(.016f);
        Assert.Equal(1, world.Failures.Count);
        world.Dispose();
    }
}
