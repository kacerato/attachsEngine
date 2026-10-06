using System.Numerics;
using System.Runtime.InteropServices;
using System.Text.Json;
using Astra;
using Astra.Compilation;
using Astra.Runtime;

namespace Aether.Tests;

// Rota gerenciada do bloco C: View/Debug/Haptics sobre as interfaces das
// famílias, e ParentChanged/ChildrenChanged pelo BehaviorWorld real. O lado
// nativo (ABI42, GameWorld e EditorPlayScene) está em test_game_services.cpp.
public static class GameServicesTests
{
    private sealed class Scene : ISceneAccess, IGameViewAccess, IDebugDrawAccess, IHierarchyChangeAccess, IHapticsAccess
    {
        public uint WorldId => 7;
        public uint GenerationOf(ulong id) => id == 2 ? 5u : 1u;
        public bool Exists(ulong id) => id is 1 or 2;
        public int GetActive(ulong id) => 1;
        public WorldStatus LastStatus => WorldStatus.InvalidArgument;
        public TransformValue GetTransform(ulong id) => new(Vector3.Zero, Quaternion.Identity, Vector3.One);
        public bool SetTransform(ulong id, TransformValue value) => true;
        public bool SetBodyVelocity(ulong id, Vector3 value) => true;
        public bool MoveKinematic(ulong id, Vector3 value, Quaternion rotation) => true;
        public readonly List<string> Log_ = [];
        public void Log(ulong id, string message) => Log_.Add(message);
        public GameViewState View = new(800, 400, 420, false, new(0, 0, 800, 400), GameViewPlatform.Android, 0);
        public bool ReadGameView(out GameViewState state) { state = View; return true; }
        public (float X, float Y) LastRay;
        public bool ScreenRay(float x, float y, out Astra.Ray ray) { LastRay = (x, y); ray = new(Vector3.Zero, Vector3.UnitZ); return true; }
        public bool WorldToScreen(Vector3 world, out Vector3 screen) { screen = new(400, 200, world.Z); return true; }
        public readonly List<(Vector3, Vector3, uint, float)> Lines = [];
        public bool DrawLine(Vector3 from, Vector3 to, uint argb, float seconds) { Lines.Add((from, to, argb, seconds)); return seconds >= 0; }
        public readonly Queue<HierarchyChange> Changes = new();
        public int PollHierarchyChanges(Span<HierarchyChange> destination)
        {
            var count = 0;
            while (count < destination.Length && Changes.TryDequeue(out var change)) destination[count++] = change;
            return count;
        }
        public bool HapticsAvailable { get; set; }
        public readonly List<(uint, float)> Vibrations = [];
        public bool Vibrate(uint milliseconds, float amplitude) { Vibrations.Add((milliseconds, amplitude)); return true; }
    }
    private sealed class Project : IDisposable
    {
        private readonly string root = Path.Combine(Path.GetTempPath(), "astra-services-" + Guid.NewGuid().ToString("N"));
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

    [Test] public static void GameServices_NativeLayoutsMatchExtensionTables()
    {
        Assert.Equal(56, Marshal.SizeOf<NativeBehaviorRuntime.NativeViewState>());
        Assert.Equal(16, Marshal.SizeOf<NativeBehaviorRuntime.NativeHierarchyChange>());
        Assert.Equal(8 + 3 * IntPtr.Size, Marshal.SizeOf<NativeBehaviorRuntime.NativeViewOperations>());
        Assert.Equal(8 + IntPtr.Size, Marshal.SizeOf<NativeBehaviorRuntime.NativeDebugOperations>());
        Assert.Equal(8 + IntPtr.Size, Marshal.SizeOf<NativeBehaviorRuntime.NativeHapticsOperations>());
        // Cor linear -> sRGB da interface, alfa preservado.
        Assert.Equal(0xFFFFFFFFu, DebugDraw.ToArgb(Color.White));
        Assert.Equal(0x80000000u, DebugDraw.ToArgb(new Color(0, 0, 0, .5f)));
        Assert.Equal(0xFFBC0000u, DebugDraw.ToArgb(new Color(.5f, 0, 0)));
    }

    [Test] public static void GameServices_BehaviorReachesViewDebugHapticsAndHierarchyCallbacks()
    {
        using var project = new Project("""
            using Astra; using System.Numerics;
            [ComponentId("test.services")] public sealed class Probe : Behavior {
                public override void Start() {
                    var ray = View.ViewportPointToRay(new Vector2(.5f, .5f));
                    Scene.Log(ObjectId, $"view:{View.Width}x{View.Height}@{View.Dpi}:{ray.Direction.Z}");
                    Scene.Log(ObjectId, "safe:" + View.TryGetSafeArea(out _));
                    Debug.DrawRay(Vector3.Zero, Vector3.UnitY, Color.White, .5f);
                    try { Haptics.Vibrate(40); } catch (System.NotSupportedException) { Scene.Log(ObjectId, "no-vibrator"); }
                }
                public override void ParentChanged() => Scene.Log(ObjectId, "parent");
                public override void ChildrenChanged() => Scene.Log(ObjectId, "children");
            }
            """);
        var scene = new Scene();
        using var world = new BehaviorWorld();
        world.Start(project.Compiled, scene, [new BehaviorAttachment(1, 8, "test.services", true, new Dictionary<string, JsonElement>())]);
        Assert.True(scene.Log_.Contains("view:800x400@420:1"), string.Join(",", scene.Log_));
        Assert.Equal((400f, 200f), scene.LastRay);
        Assert.True(scene.Log_.Contains("safe:False"), "unreported safe area is not claimed");
        Assert.Equal(1, scene.Lines.Count);
        Assert.Equal(new Vector3(0, 1, 0), scene.Lines[0].Item2);
        Assert.True(scene.Log_.Contains("no-vibrator"), "absent haptics family refused explicitly");
        scene.Log_.Clear();
        scene.Changes.Enqueue(new(1, 1, true));
        scene.Changes.Enqueue(new(1, 1, false));
        scene.Changes.Enqueue(new(2, 4, true));   // geração vencida: outro objeto no mesmo id
        world.Update(.016f);
        Assert.Equal("parent,children", string.Join(",", scene.Log_));
        Assert.Equal(0, world.Failures.Count);
    }
}
