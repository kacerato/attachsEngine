using System.Numerics;
using Astra;
using Astra.Components;

namespace Aether.Tests;

/// <summary>Managed contract tests; the small scene double does not prove native or device behavior.</summary>
public static class AstraExpandedApiTests
{
    private sealed class Scene : ISceneAccess
    {
        public readonly Dictionary<ulong, TransformValue> Poses = new() {
            [1] = new(new(10, 3, -2), Quaternion.CreateFromAxisAngle(Vector3.UnitY, .3f), new(2, 3, 4)),
            [2] = new(new(1, 2, 3), Quaternion.CreateFromAxisAngle(Vector3.UnitZ, .7f), Vector3.One),
            [3] = new(Vector3.Zero, Quaternion.Identity, Vector3.One)
        };
        public bool RejectWrites; public int Writes;
        public uint WorldId => 1;
        public uint GenerationOf(ulong id) => Exists(id) ? 1u : 0;
        public WorldStatus LastStatus => WorldStatus.TransformOwnedByPhysics;
        public bool Exists(ulong id) => Poses.ContainsKey(id);
        public TransformValue GetTransform(ulong id) => Poses[id];
        public bool SetTransform(ulong id, TransformValue value) { ++Writes; if (RejectWrites) return false; Poses[id] = value; return true; }
        public TransformValue GetWorldTransform(ulong id) => Poses[id];
        public bool SetWorldTransform(ulong id, TransformValue value) => SetTransform(id, value);
        public ulong ParentOf(ulong id) => id == 1 ? 0UL : 1UL;
        public int ChildCount(ulong id) => id == 1 ? 2 : 0;
        public ulong ChildAt(ulong id, uint index) => id == 1 && index < 2 ? index + 2 : 0;
        public int GetActive(ulong id) => id == 3 ? 0 : 1;
        public int ComponentCount(ulong id) => id == 2 ? 2 : 1;
        public (ulong Instance, string TypeId) ComponentAt(ulong id, uint index) => (id * 10 + index, ComponentIds.Collider);
        public bool SetBodyVelocity(ulong id, Vector3 value) => throw new NotSupportedException();
        public bool MoveKinematic(ulong id, Vector3 value, Quaternion rotation) => throw new NotSupportedException();
        public void Log(ulong id, string message) { }
    }
    private static void Near(Vector3 expected, Vector3 actual)
    {
        Assert.True(Vector3.Distance(expected, actual) < .0001f, $"{expected} != {actual}");
    }

    [Test] public static void AffineConversionsPreserveParentShearAndRejectSingularInverse()
    {
        var scene = new Scene(); var child = GameObject.Resolve(scene, 2);
        var point = new Vector3(.3f, 2, -.5f); var p = scene.Poses[2]; var parent = scene.Poses[1];
        var expected = parent.Position + Vector3.Transform(parent.Scale *
            (p.Position + Vector3.Transform(p.Scale * point, p.Rotation)), parent.Rotation);
        Near(expected, child.TransformPoint(point)); Near(point, child.InverseTransformPoint(expected));
        Near(point, child.InverseTransformVector(child.TransformVector(point)));
        Assert.Close(point.Length(), child.TransformDirection(point).Length());
        Near(Vector3.Transform(Vector3.Transform(point, p.Rotation), parent.Rotation), child.TransformDirection(point));
        Near(point, child.InverseTransformDirection(child.TransformDirection(point)));
        scene.Poses[1] = parent with { Scale = new(0, 3, 4) };
        Assert.Throws<InvalidOperationException>(() => child.InverseTransformPoint(Vector3.One));
    }

    [Test] public static void PoseOperationsPublishOnceAndKeepNativeOwnershipErrors()
    {
        var scene = new Scene(); var root = GameObject.Resolve(scene, 1); var before = root.WorldTransform;
        root.RotateAround(Vector3.Zero, Vector3.UnitY, MathF.PI / 2);
        Assert.Equal(1, scene.Writes);
        Near(Vector3.Transform(before.Position, Quaternion.CreateFromAxisAngle(Vector3.UnitY, MathF.PI / 2)), root.WorldPosition);
        root.LookAt(root.WorldPosition + Vector3.UnitX); Near(Vector3.UnitX, root.Forward);
        var lookWrites = scene.Writes;
        Assert.Throws<ArgumentOutOfRangeException>(() => root.LookAt(root.WorldPosition));
        Assert.Equal(lookWrites, scene.Writes);
        scene.RejectWrites = true;
        Assert.Throws<WorldException>(() => root.Translate(Vector3.One));
        var writes = scene.Writes;
        Assert.Throws<ArgumentOutOfRangeException>(() => root.Rotate(Vector3.Zero, 1));
        Assert.Throws<ArgumentOutOfRangeException>(() => root.Rotate(new(float.MaxValue, 0, 0), 1));
        Assert.Equal(writes, scene.Writes);
    }

    [Test] public static void HierarchyQueriesIncludeRepeatedInstancesAndInactiveSelf()
    {
        var scene = new Scene(); var root = GameObject.Resolve(scene, 1);
        var found = root.GetComponentsInChildren<Collider>();
        Assert.Equal(3, found.Length); Assert.Equal(10UL, found[0].InstanceId);
        Assert.Equal(20UL, found[1].InstanceId); Assert.Equal(21UL, found[2].InstanceId);
        Assert.Equal(4, root.GetComponentsInChildren<Collider>(true).Length);
        var inactive = GameObject.Resolve(scene, 3);
        Assert.Equal(1, inactive.GetComponentsInChildren<Collider>().Length);
        Assert.Equal(2, inactive.GetComponentsInParent<Collider>().Length);
        var result = new List<Collider>(); root.GetComponentsInChildren(false, result); root.GetComponentsInChildren(false, result);
        Assert.Equal(3, result.Count);
    }

    [Test] public static void RandomStateReplayAndDampingBoundariesRemainStable()
    {
        var random = new RandomStream(42, 54); var saved = random.Capture();
        Assert.Equal(0xa15c02b7u, random.NextUInt(), "PCG reference vector, seed42/stream54");
        Assert.Equal(0x7b47f409u, random.NextUInt()); random.Restore(saved);
        var samples = Enumerable.Range(0, 100).Select(_ => random.NextUInt()).ToArray();
        random.Restore(saved); foreach (var sample in samples) Assert.Equal(sample, random.NextUInt());
        for (var i = 0; i < 256; ++i) {
            Assert.True(random.Range(int.MinValue, int.MaxValue) < int.MaxValue);
            Assert.True(random.InsideUnitSphere().LengthSquared() <= 1.00001f);
            Assert.Close(1, random.OnUnitSphere().Length(), .00001f);
        }
        var velocity = 2f; Assert.Close(3, Mathf.SmoothDamp(3, 10, ref velocity, 1, 0)); Assert.Close(2, velocity);
        var current = 0f; velocity = 0;
        for (var i = 0; i < 300; ++i) { current = Mathf.SmoothDamp(current, 1, ref velocity, .2f, 1f / 60); Assert.True(current <= 1); }
        Assert.Close(1, current);
        Assert.Close(.2f, Mathf.DeltaAngle(MathF.Tau - .1f, .1f), .00001f);
        Assert.Close(.5f, Mathf.Damp(0, 1, 1, 1));
        Assert.Close(.75f, Mathf.Repeat(-.25f, 1));
        Assert.True(float.IsFinite(Mathf.DeltaAngle(-float.MaxValue, float.MaxValue)));
        Assert.True(float.IsFinite(Mathf.Repeat(float.MaxValue, .1f)));
        velocity = 0;
        Assert.True(float.IsFinite(Mathf.SmoothDamp(-float.MaxValue, float.MaxValue, ref velocity, 10, .016f)));
    }
}
