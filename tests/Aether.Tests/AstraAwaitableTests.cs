using System.Numerics;
using System.Text.Json;
using Astra;
using Astra.Compilation;
using Astra.Runtime;

namespace Aether.Tests;

public static class AstraAwaitableTests
{
    private sealed class Scene : ISceneAccess
    {
        public readonly List<string> Events = [];
        public bool Alive = true, Active = true;
        public uint WorldId => 77;
        public uint GenerationOf(ulong id) => Exists(id) ? 1u : 0;
        public bool Exists(ulong id) => id == 1 && Alive;
        public int GetActive(ulong id) => Exists(id) ? (Active ? 1 : 0) : -1;
        public TransformValue GetTransform(ulong id) => new(Vector3.Zero, Quaternion.Identity, Vector3.One);
        public bool SetTransform(ulong id, TransformValue value) => Exists(id);
        public bool SetBodyVelocity(ulong id, Vector3 value) => Exists(id);
        public bool MoveKinematic(ulong id, Vector3 value, Quaternion rotation) => Exists(id);
        public void Log(ulong id, string message) => Events.Add(message);
    }
    private sealed class Project : IDisposable
    {
        private readonly string _root = Path.Combine(Path.GetTempPath(), "astra-await-" + Guid.NewGuid().ToString("N"));
        public CompiledProject Compiled { get; }
        public Project(string source)
        {
            Directory.CreateDirectory(_root); File.WriteAllText(Path.Combine(_root, "Probe.cs"), source);
            var result = new ProjectCompiler().Build(_root);
            Assert.True(result.Success, string.Join("\n", result.Diagnostics.Select(d => d.Message)));
            Compiled = result.Project!;
        }
        public void Dispose() => Directory.Delete(_root, recursive: true);
    }
    private static void Start(BehaviorWorld world, Project project, Scene scene) => world.Start(project.Compiled, scene,
        [new BehaviorAttachment(1, 8, "test.await", true, new Dictionary<string, JsonElement>())]);

    [Test] public static void AwaitablesUseNativeDispatchTimePhysicalStepsAndMainThread()
    {
        using var project = new Project("""
            using Astra; using System; using System.Threading; using System.Threading.Tasks;
            [ComponentId("test.await")] public sealed class Probe : Behavior
            {
                private int thread;
                private bool requestedFixed;
                public override void Start() { thread = Environment.CurrentManagedThreadId; StartAsync(Run); }
                public override void FixedUpdate(float deltaTime) {
                    if(requestedFixed) return; requestedFixed = true;
                    StartAsync(async cancellation => { await Awaitable.FixedUpdate(cancellation); Record("fixed-from-callback"); });
                }
                private void Record(string phase) {
                    if(thread != Environment.CurrentManagedThreadId) throw new Exception("wrong thread");
                    Scene.Log(ObjectId, phase);
                }
                private async Task Run(CancellationToken cancellation) {
                    Record("begin"); await Awaitable.NextFrame(cancellation); Record("frame");
                    await Awaitable.Seconds(.05, cancellation); Record("seconds");
                    await Awaitable.FixedUpdate(cancellation); Record("fixed");
                    await Awaitable.SecondsRealtime(.005, cancellation); Record("real");
                }
            }
            """);
        var scene = new Scene(); using var world = new BehaviorWorld(); Start(world, project, scene);
        Assert.Equal("begin", string.Join(",", scene.Events));
        world.FixedUpdate(.01f); Assert.Equal(1, scene.Events.Count);
        Assert.False(scene.Events.Contains("fixed-from-callback"));
        world.Update(.01f); Assert.Equal("begin,frame", string.Join(",", scene.Events));
        world.Update(.02f); Assert.Equal(2, scene.Events.Count);
        world.Update(.04f); Assert.Equal("seconds", scene.Events[^1]);
        world.Update(0); Assert.Equal(3, scene.Events.Count);
        world.FixedUpdate(.016f); Assert.Equal("fixed", scene.Events[^1]);
        Assert.True(scene.Events.Contains("fixed-from-callback"));
        Thread.Sleep(10); world.Update(0); Assert.Equal("real", scene.Events[^1]);
        world.Update(0); Assert.Equal(0, world.Failures.Count);
    }

    [Test] public static void NestedCoroutinesDisposeOnceWithReentrancyAndIsolatedFailure()
    {
        using var project = new Project("""
            using Astra; using System; using System.Collections;
            [ComponentId("test.await")] public sealed class Probe : Behavior
            {
                public override void Start() { StartCoroutine(Parent()); StartCoroutine(Bad()); StartCoroutine(Sibling()); }
                private IEnumerator Parent() { try { yield return Child(); Scene.Log(ObjectId,"unreachable"); }
                    finally { Scene.Log(ObjectId,"parent-dispose"); } }
                private IEnumerator Child() { try {
                    Scene.Log(ObjectId,"child-start"); yield return null; Scene.Log(ObjectId,"child-frame");
                    yield return null; StopAllCoroutines(); yield return null;
                } finally { Scene.Log(ObjectId,"child-dispose"); } }
                private IEnumerator Bad() { try { yield return null; throw new Exception("bad coroutine"); }
                    finally { Scene.Log(ObjectId,"bad-dispose"); } }
                private IEnumerator Sibling() { try { while(true) { yield return null; Scene.Log(ObjectId,"sibling-frame"); } }
                    finally { Scene.Log(ObjectId,"sibling-dispose"); } }
            }
            """);
        var scene = new Scene(); using var world = new BehaviorWorld(); Start(world, project, scene);
        world.Edit(1, 8, new BehaviorEdit(false, null)); // enabled=false does not cancel ongoing work.
        world.Update(.01f); Assert.True(scene.Events.Contains("sibling-frame"));
        Assert.Equal(1, world.Failures.Count); Assert.True(world.Failures[0].Phase.StartsWith("Coroutine", StringComparison.Ordinal));
        world.Update(.01f); world.Update(.01f);
        foreach (var name in new[] { "child-dispose", "parent-dispose", "bad-dispose", "sibling-dispose" })
            Assert.Equal(1, scene.Events.Count(e => e == name));
        Assert.False(scene.Events.Contains("unreachable"));
    }

    [Test] public static void SessionAndObjectLifecycleCancelAsyncAndRejectForeignHandles()
    {
        using var project = new Project("""
            using Astra; using System; using System.Collections; using System.Threading; using System.Threading.Tasks;
            [ComponentId("test.await")] public sealed class Probe : Behavior
            {
                private readonly CancellationTokenSource cancel = new();
                public override void Start() { StartAsync(Run); StartAsync(Bad); StartAsync(_ => TokenWait(cancel.Token)); }
                public override void Update(float deltaTime) => cancel.Cancel();
                private async Task TokenWait(CancellationToken cancellation) {
                    try { await Awaitable.SecondsRealtime(1000, cancellation); Scene.Log(ObjectId,"token-unreachable"); }
                    finally { Scene.Log(ObjectId,"token-dispose"); }
                }
                private async Task Run(CancellationToken cancellation) {
                    try { await Awaitable.Seconds(1000, cancellation); Scene.Log(ObjectId,"unreachable"); }
                    finally { Scene.Log(ObjectId,"async-dispose"); }
                }
                private async Task Bad(CancellationToken cancellation) {
                    await Awaitable.NextFrame(cancellation); throw new Exception("async fault");
                }
                public override void Stop() {
                    try { StartCoroutine(Forever()); Scene.Log(ObjectId,"restart-bug"); }
                    catch(InvalidOperationException) { Scene.Log(ObjectId,"restart-blocked"); }
                }
                public IEnumerator Forever() { try { while(true) yield return null; }
                    finally { Scene.Log(ObjectId,"foreign-dispose"); } }
            }
            """);
        var scene = new Scene(); using var world = new BehaviorWorld(); Start(world, project, scene);
        world.Update(.01f); world.Update(.01f); Assert.Equal(1, world.Failures.Count);
        Assert.Equal(1, scene.Events.Count(e => e == "token-dispose")); Assert.False(scene.Events.Contains("token-unreachable"));
        var behavior = (Behavior)world.FindBehavior(1, typeof(Behavior))!;
        using var second = new BehaviorWorld(); var secondScene = new Scene(); Start(second, project, secondScene);
        var other = (Behavior)second.FindBehavior(1, typeof(Behavior))!;
        var forever = (System.Collections.IEnumerator)other.GetType().GetMethod("Forever")!.Invoke(other, null)!;
        var handle = other.StartCoroutine(forever);
        Assert.Throws<ArgumentException>(() => behavior.StopCoroutine(handle));
        var offThread = (System.Collections.IEnumerator)behavior.GetType().GetMethod("Forever")!.Invoke(behavior, null)!;
        Assert.Throws<InvalidOperationException>(() => Task.Run(() => behavior.StartCoroutine(offThread)).GetAwaiter().GetResult());
        scene.Active = false; world.Update(0); Assert.Equal(1, scene.Events.Count(e => e == "async-dispose"));
        scene.Active = true; world.Update(0); Assert.False(scene.Events.Contains("unreachable"));
        world.Dispose(); Assert.True(scene.Events.Contains("restart-blocked")); Assert.False(scene.Events.Contains("restart-bug"));
        second.Dispose(); Assert.Equal(CoroutineStatus.Cancelled, handle.Status);
        Assert.Equal(1, secondScene.Events.Count(e => e == "async-dispose"));
        Assert.Equal(1, secondScene.Events.Count(e => e == "foreign-dispose"));
    }
}
