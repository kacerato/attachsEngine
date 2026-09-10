using System.Numerics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using Astra.Compilation;

namespace Astra.Runtime;

public static unsafe class NativeBehaviorRuntime
{
    [StructLayout(LayoutKind.Sequential)]
    public struct SceneAccess
    {
        public uint Version, Size;
        public void* Context;
        public delegate* unmanaged<void*, ulong, int> Exists;
        public delegate* unmanaged<void*, ulong, float*, int> GetTransform, SetTransform, SetVelocity, MoveKinematic;
        public delegate* unmanaged<void*, ulong, byte*, int, void> Log;
        public delegate* unmanaged<void*, ulong, float*, uint, int> BodyForce;
        public delegate* unmanaged<void*, ulong, float*, int> GetVelocity;
    }
    private sealed class SceneAdapter(SceneAccess access) : ISceneAccess
    {
        private bool _active = true;
        private readonly int _ownerThread = Environment.CurrentManagedThreadId;
        private bool Accessible => _active && Environment.CurrentManagedThreadId == _ownerThread;
        public void Invalidate() => _active = false;
        public bool Exists(ulong objectId) => Accessible && access.Exists(access.Context, objectId) != 0;
        public TransformValue GetTransform(ulong objectId)
        {
            float* value = stackalloc float[10];
            if (!Accessible || access.GetTransform(access.Context, objectId, value) == 0)
                throw new InvalidOperationException("Object transform is unavailable.");
            return new(new(value[0], value[1], value[2]), new(value[3], value[4], value[5], value[6]), new(value[7], value[8], value[9]));
        }
        public bool SetTransform(ulong objectId, TransformValue value)
        {
            float* data = stackalloc float[10] { value.Position.X, value.Position.Y, value.Position.Z,
                value.Rotation.X, value.Rotation.Y, value.Rotation.Z, value.Rotation.W, value.Scale.X, value.Scale.Y, value.Scale.Z };
            return Accessible && access.SetTransform(access.Context, objectId, data) != 0;
        }
        public bool SetBodyVelocity(ulong objectId, Vector3 value)
        {
            float* data = stackalloc float[3] { value.X, value.Y, value.Z };
            return Accessible && access.SetVelocity(access.Context, objectId, data) != 0;
        }
        public bool MoveKinematic(ulong objectId, Vector3 position, Quaternion rotation)
        {
            float* data = stackalloc float[7] { position.X, position.Y, position.Z, rotation.X, rotation.Y, rotation.Z, rotation.W };
            return Accessible && access.MoveKinematic(access.Context, objectId, data) != 0;
        }
        private bool ApplyForce(ulong objectId, Vector3 value, uint kind)
        {
            float* data = stackalloc float[3] { value.X, value.Y, value.Z };
            return Accessible && access.BodyForce(access.Context, objectId, data, kind) != 0;
        }
        public bool AddForce(ulong objectId, Vector3 force) => ApplyForce(objectId, force, 0);
        public bool AddImpulse(ulong objectId, Vector3 impulse) => ApplyForce(objectId, impulse, 1);
        public bool AddTorque(ulong objectId, Vector3 torque) => ApplyForce(objectId, torque, 2);
        public bool AddAngularImpulse(ulong objectId, Vector3 impulse) => ApplyForce(objectId, impulse, 3);
        public Vector3 GetBodyVelocity(ulong objectId)
        {
            float* data = stackalloc float[3];
            if (!Accessible || access.GetVelocity(access.Context, objectId, data) == 0)
                throw new InvalidOperationException("Object has no active physics body.");
            return new(data[0], data[1], data[2]);
        }
        public void Log(ulong objectId, string message)
        {
            if (!Accessible) return;
            var bytes = Encoding.UTF8.GetBytes(message.Length > 8192 ? message[..8192] : message);
            fixed (byte* pointer = bytes) access.Log(access.Context, objectId, pointer, bytes.Length);
        }
    }
    private static BehaviorWorld? _world;
    private static SceneAdapter? _scene;
    private static byte[] _diagnostics = [];
    [UnmanagedCallersOnly]
    public static int Start(byte* root, int rootLength, byte* json, int jsonLength, SceneAccess* access)
    {
        try
        {
            if (_world is not null || root == null || json == null || rootLength <= 0 || rootLength > 32768 ||
                jsonLength <= 0 || jsonLength > 32 * 1024 * 1024 || access == null || access->Version != 2 ||
                access->Size != sizeof(SceneAccess) || access->Exists == null || access->GetTransform == null ||
                access->SetTransform == null || access->SetVelocity == null || access->MoveKinematic == null || access->Log == null || access->BodyForce == null || access->GetVelocity == null) return 1;
            var directory = new UTF8Encoding(false, true).GetString(new ReadOnlySpan<byte>(root, rootLength));
            var project = NativeCompiler.LoadApplied(directory);
            var attachments = JsonSerializer.Deserialize<BehaviorAttachment[]>(new ReadOnlySpan<byte>(json, jsonLength))
                ?? throw new InvalidDataException("Behavior attachment data is empty.");
            _scene = new(*access); _world = new(); _world.Start(project, _scene, attachments);
            RefreshDiagnostics(); return 0;
        }
        catch (Exception error) { StopWorld(); _diagnostics = Encoding.UTF8.GetBytes(error.ToString()); return 1; }
    }
    [UnmanagedCallersOnly]
    public static int Update(float deltaTime)
    {
        try { _world?.Update(deltaTime); RefreshDiagnostics(); return 0; }
        catch (Exception error) { _diagnostics = Encoding.UTF8.GetBytes(error.ToString()); return 1; }
    }
    [UnmanagedCallersOnly]
    public static int FixedUpdate(float deltaTime)
    {
        try { _world?.FixedUpdate(deltaTime); RefreshDiagnostics(); return 0; }
        catch (Exception error) { _diagnostics = Encoding.UTF8.GetBytes(error.ToString()); return 1; }
    }
    [UnmanagedCallersOnly]
    public static int Trigger(ulong sensor, ulong other, uint phase)
    {
        try { if (phase > 2) return 1; _world?.Trigger(sensor, other, phase); RefreshDiagnostics(); return 0; }
        catch (Exception error) { _diagnostics = Encoding.UTF8.GetBytes(error.ToString()); return 1; }
    }
    [UnmanagedCallersOnly]
    public static void Stop() { try { StopWorld(); } catch (Exception error) { _diagnostics = Encoding.UTF8.GetBytes(error.ToString()); } }
    private static void StopWorld()
    {
        try { _world?.Dispose(); }
        finally { _world = null; _scene?.Invalidate(); _scene = null; }
    }
    private static void RefreshDiagnostics()
    {
        _diagnostics = Encoding.UTF8.GetBytes(string.Join("\n", _world?.Failures.Select(f =>
            $"{f.ObjectId}/{f.InstanceId} {f.Phase}: {f.Message}") ?? []));
    }
    [UnmanagedCallersOnly]
    public static int CopyDiagnostics(byte* destination, int capacity)
    {
        if (destination == null || capacity < _diagnostics.Length) return _diagnostics.Length;
        _diagnostics.CopyTo(new Span<byte>(destination, capacity)); return _diagnostics.Length;
    }
}
