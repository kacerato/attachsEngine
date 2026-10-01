using System.Numerics;
using System.Text.Json;
using Astra;
using Astra.Compilation;
using Astra.Runtime;

namespace Aether.Tests;
public static class AstraTimeTests
{
    [Test] public static void EditableTimeAcceptanceFixtureCompilesWithCurrentSdk()
    {
        var root=new DirectoryInfo(AppContext.BaseDirectory);
        while(root is not null && !File.Exists(Path.Combine(root.FullName,"tests","fixtures","time","TimeProbe.cs"))) root=root.Parent;
        Assert.True(root is not null,"editable acceptance fixture exists");
        using var project=new Project(File.ReadAllText(Path.Combine(root!.FullName,"tests","fixtures","time","TimeProbe.cs")));
        Assert.Equal("acceptance.time",project.Compiled.Types.Single().Id);
    }
    private class Scene : ISceneAccess
    {
        public uint WorldId => 81;
        public uint GenerationOf(ulong id) => 1;
        public bool Exists(ulong id) => id == 1;
        public int GetActive(ulong id) => 1;
        public TransformValue GetTransform(ulong id) => new(Vector3.Zero, Quaternion.Identity, Vector3.One);
        public bool SetTransform(ulong id, TransformValue value) => true;
        public bool SetBodyVelocity(ulong id, Vector3 value) => true;
        public bool MoveKinematic(ulong id, Vector3 value, Quaternion rotation) => true;
        public readonly List<string> Events = [];
        public void Log(ulong id, string message) => Events.Add(message);
    }
    private sealed class ClockScene : Scene, ITimeSceneAccess
    {
        private SimulationTimeState state=new(0,0,0,0,0,1,1);
        public SimulationTimeState ReadTime() => state;
        public WorldStatus SetTimeScale(float value) {
            if(!float.IsFinite(value) || value<0 || value>4) return WorldStatus.InvalidArgument;
            state=state with { Scale=value };return WorldStatus.Ok;
        }
        public float Advance(double realDelta) {
            var delta=(float)Math.Min(realDelta*state.Scale,.25);
            state=new(state.FrameCount+1,state.SimulationTime+delta,state.UnscaledTime+realDelta,
                delta,(float)realDelta,state.Scale,state.Scale);
            return delta;
        }
    }
    private sealed class Project : IDisposable
    {
        private readonly string root = Path.Combine(Path.GetTempPath(), "astra-time-" + Guid.NewGuid().ToString("N"));
        public CompiledProject Compiled { get; }
        public Project(string source) {
            Directory.CreateDirectory(root); File.WriteAllText(Path.Combine(root, "Probe.cs"), source);
            var result = new ProjectCompiler().Build(root);
            Assert.True(result.Success, string.Join("\n", result.Diagnostics.Select(d => d.Message)));
            Compiled = result.Project!;
        }
        public void Dispose() => Directory.Delete(root, true);
    }
    private static void Start(BehaviorWorld world, Project project, Scene scene) => world.Start(project.Compiled, scene,
        [new BehaviorAttachment(1, 8, "test.time", true, new Dictionary<string, JsonElement>())]);

    [Test] public static void TimePhasesAndAwaitedContinuationsUseSameWorldClock()
    {
        using var project = new Project("""
            using Astra;
            [ComponentId("test.time")] public sealed class Probe : Behavior {
                public override void Start() { StartAsync(async token => {
                    await Awaitable.NextFrame(token);
                    if(Time.FrameCount!=1 || Time.InFixedTimeStep || Time.DeltaTime!=.25f) throw new System.Exception("frame clock");
                    Scene.Log(ObjectId,"frame"); await Awaitable.FixedUpdate(token);
                    if(!Time.InFixedTimeStep || Time.FixedTime!=.125 || Time.TimeSinceStart!=.125 || Time.DeltaTime!=.125f) throw new System.Exception("fixed clock");
                    Scene.Log(ObjectId,"fixed");
                }); }
                public override void Update(float dt) { if(Time.DeltaTime!=dt || Time.TimeSinceStart!=.25) throw new System.Exception("update"); }
                public override void LateUpdate(float dt) { if(Time.FrameCount!=1 || Time.TimeSinceStart!=.25 || Time.DeltaTime!=.25f || Time.InFixedTimeStep) throw new System.Exception("late"); }
            }
            """);
        using var world = new BehaviorWorld(); var scene = new Scene(); Start(world, project, scene);
        world.Update(.25f); world.FixedUpdate(.125f); world.LateUpdate(.7f);
        Assert.Equal("frame,fixed", string.Join(",", scene.Events)); Assert.Equal(0, world.Failures.Count);
        world.Update(float.NaN); world.FixedUpdate(-1); world.LateUpdate(float.PositiveInfinity);
        Assert.Equal(1UL, world.Time.FrameCount); Assert.Equal(.25, world.Time.TimeSinceStart);
    }

    [Test] public static void PlayRestartInvalidatesOldClockAndWorldClocksStayIsolated()
    {
        using var project = new Project("""
            using Astra; [ComponentId("test.time")] public sealed class Probe : Behavior {
                public override void Start() { if(Time.FrameCount!=0 || Time.TimeSinceStart!=0 || Time.FixedTime!=0) throw new System.Exception("reset"); }
            }
            """);
        using var first = new BehaviorWorld(); using var second = new BehaviorWorld();
        Start(first, project, new Scene()); Start(second, project, new Scene());
        var old = first.Time; first.Update(.5f); second.Update(.125f);
        Assert.Equal(.5, first.Time.TimeSinceStart); Assert.Equal(.125, second.Time.TimeSinceStart);
        var rejected = Task.Run(() => { try { _ = old.FrameCount; return false; } catch(InvalidOperationException) { return true; } }).GetAwaiter().GetResult();
        Assert.True(rejected); first.Dispose();
        bool stale = false; try { _ = old.FrameCount; } catch(InvalidOperationException) { stale = true; }
        Assert.True(stale); Start(first, project, new Scene()); Assert.Equal(0UL, first.Time.FrameCount);
        Assert.Equal(0, first.Failures.Count); Assert.False(ReferenceEquals(old, first.Time));
    }

    [Test] public static void NativeClockScaleAndUnscaledTimeRemainCoherentAcrossFramePhases()
    {
        using var project = new Project("""
            using Astra;
            [ComponentId("test.time")] public sealed class Probe : Behavior {
                public override void Start() { Time.Scale=.5f; }
                public override void Update(float dt) {
                    if(Time.FrameCount==1) {
                        if(dt!=.05f || Time.UnscaledDeltaTime!=.1f) throw new System.Exception("scaled delta");
                        Time.Scale=0;
                    } else if(dt!=0 || Time.DeltaTime!=0 || Time.UnscaledDeltaTime!=.2f) throw new System.Exception("zero time");
                }
                public override void FixedUpdate(float dt) {
                    if(Time.UnscaledDeltaTime!=dt*2 || Time.Scale!=0) throw new System.Exception("fixed uses captured frame scale");
                }
            }
            """);
        using var world=new BehaviorWorld();var scene=new ClockScene();Start(world,project,scene);
        Assert.Equal(.5f,world.Time.Scale);
        world.Update(scene.Advance(.1));world.FixedUpdate(1f/60);world.LateUpdate(.05f);
        Assert.Equal(0f,world.Time.Scale);Assert.Equal(.1,world.Time.UnscaledTimeSinceStart);
        var simulation=world.Time.TimeSinceStart;
        world.Update(scene.Advance(.2));
        Assert.Equal(simulation,world.Time.TimeSinceStart);Assert.Equal(2UL,world.Time.FrameCount);
        Assert.True(Math.Abs(world.Time.UnscaledTimeSinceStart-.3)<1e-12);
        Assert.Equal(0,world.Failures.Count);
        Assert.Equal(48,System.Runtime.InteropServices.Marshal.SizeOf<NativeBehaviorRuntime.NativeTimeState>());
        Assert.Equal(16,System.Runtime.InteropServices.Marshal.SizeOf<NativeBehaviorRuntime.NativeTimerState>());
        Assert.Equal(24,System.Runtime.InteropServices.Marshal.SizeOf<NativeBehaviorRuntime.NativeTweenState>());
        Assert.Equal(24,System.Runtime.InteropServices.Marshal.SizeOf<NativeBehaviorRuntime.NativeNumberTweenParameters>());
        Assert.Equal(32,System.Runtime.InteropServices.Marshal.SizeOf<NativeBehaviorRuntime.NativeNumberTweenState>());
        Assert.Equal(80,System.Runtime.InteropServices.Marshal.SizeOf<NativeBehaviorRuntime.NativeCharacterState>());
    }

    [Test] public static void ClockHostWithoutSimulationControlRejectsScaleInsteadOfChangingOnlyScripts()
    {
        using var project=new Project("""
            using Astra; [ComponentId("test.time")] public sealed class Probe : Behavior { }
            """);
        using var world=new BehaviorWorld();Start(world,project,new Scene());
        bool rejected=false;try { world.Time.Scale=0; } catch(NotSupportedException) { rejected=true; }
        Assert.True(rejected);Assert.Equal(1f,world.Time.Scale);
    }
}
