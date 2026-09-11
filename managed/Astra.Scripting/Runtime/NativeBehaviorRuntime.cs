using System.Numerics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using Astra.Compilation;

namespace Astra.Runtime;

public static unsafe class NativeBehaviorRuntime
{
    /// <summary>Espelho de <c>ae::scene::ScriptQueryFilter</c>.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeQueryFilter
    {
        public uint Size, GameplayLayerMask, Flags, Reserved;
        public ulong Ignore;
    }

    /// <summary>Espelho de <c>ae::scene::ScriptShapeQuery</c>.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeShapeQuery
    {
        public uint Kind;
        public float HalfX, HalfY, HalfZ;
        public float Radius, HalfHeight;
        public float RotationX, RotationY, RotationZ, RotationW;
    }

    /// <summary>
    /// Espelho exato de <c>ae::scene::ScriptSceneAccess</c> (ABI v5). A ordem dos
    /// campos É o contrato: acrescentar só no fim, e conferir <c>Size</c> antes de
    /// ler qualquer ponteiro — uma struct maior do que a acordada seria lida além
    /// do fim do que o nativo alocou.
    /// </summary>
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
        // v3 — identidade
        public delegate* unmanaged<void*, uint> WorldId;
        public delegate* unmanaged<void*, ulong, uint> Generation;
        public delegate* unmanaged<void*, uint> LastStatus;
        // v3 — hierarquia
        public delegate* unmanaged<void*, ulong, ulong> ParentOf;
        public delegate* unmanaged<void*, ulong, int> ChildCount;
        public delegate* unmanaged<void*, ulong, uint, ulong> ChildAt;
        public delegate* unmanaged<void*, ulong, byte*, int, int, ulong> FindChild;
        public delegate* unmanaged<void*, ulong, byte*, int, int> GetName;
        public delegate* unmanaged<void*, ulong, byte*, int, int> SetName;
        public delegate* unmanaged<void*, ulong, int> GetActive;
        public delegate* unmanaged<void*, ulong, int, int> SetActive;
        // v3 — ciclo de vida
        public delegate* unmanaged<void*, ulong, byte*, int, ulong> CreateObject;
        public delegate* unmanaged<void*, ulong, int> DestroyObject;
        public delegate* unmanaged<void*, ulong, ulong, uint, int> SetParent;
        // v3 — componentes
        public delegate* unmanaged<void*, ulong, int> ComponentCount;
        public delegate* unmanaged<void*, ulong, uint, byte*, int, ulong> ComponentAt;
        public delegate* unmanaged<void*, ulong, byte*, int, uint, ulong> FindComponent;
        public delegate* unmanaged<void*, ulong, byte*, int, ulong> AddComponent;
        public delegate* unmanaged<void*, ulong, ulong, int> RemoveComponent;
        public delegate* unmanaged<void*, ulong, ulong, byte*, int, uint*, ulong*, int> GetProperty;
        public delegate* unmanaged<void*, ulong, ulong, byte*, int, uint, ulong, int> SetProperty;
        // v3 — transform de mundo
        public delegate* unmanaged<void*, ulong, float*, int> GetWorldTransform;
        public delegate* unmanaged<void*, ulong, float*, int> SetWorldTransform;
        // v4 — consultas físicas
        public delegate* unmanaged<void*, float*, float*, NativeQueryFilter*, RawQueryHit*, int, int> RayCast;
        public delegate* unmanaged<void*, NativeShapeQuery*, float*, float*, NativeQueryFilter*, RawQueryHit*, int> ShapeCast;
        public delegate* unmanaged<void*, NativeShapeQuery*, float*, NativeQueryFilter*, RawQueryHit*, int, int> Overlap;
        public delegate* unmanaged<void*, byte*, int, int> LayerByName;
        public delegate* unmanaged<void*, uint, byte*, int, int> LayerName;
        // v5 — entrada por ações
        public delegate* unmanaged<void*, byte*, int, float*, int> InputAxis;
        public delegate* unmanaged<void*, byte*, int, uint, int> InputButton;
        public delegate* unmanaged<void*, byte*, int, int, int> InputContext;
        public delegate* unmanaged<void*, uint, byte*, int, int> InputRole;

        public bool Complete => Exists != null && GetTransform != null && SetTransform != null && SetVelocity != null &&
            MoveKinematic != null && Log != null && BodyForce != null && GetVelocity != null && WorldId != null &&
            Generation != null && LastStatus != null && ParentOf != null && ChildCount != null && ChildAt != null &&
            FindChild != null && GetName != null && SetName != null && GetActive != null && SetActive != null &&
            CreateObject != null && DestroyObject != null && SetParent != null && ComponentCount != null &&
            ComponentAt != null && FindComponent != null && AddComponent != null && RemoveComponent != null &&
            GetProperty != null && SetProperty != null && GetWorldTransform != null && SetWorldTransform != null &&
            RayCast != null && ShapeCast != null && Overlap != null && LayerByName != null && LayerName != null &&
            InputAxis != null && InputButton != null && InputContext != null && InputRole != null;
    }

    private sealed class SceneAdapter(SceneAccess access) : ISceneAccess
    {
        // Nomes cabem em 63 bytes e ids de tipo em 256; os buffers são o teto do
        // contrato nativo, não uma estimativa.
        private const int NameCapacity = 64;
        private const int TypeIdCapacity = 257;
        private bool _active = true;
        private readonly int _ownerThread = Environment.CurrentManagedThreadId;
        private bool Accessible => _active && Environment.CurrentManagedThreadId == _ownerThread;
        public void Invalidate() => _active = false;

        private static byte[] Utf8(string text, string what)
        {
            ArgumentNullException.ThrowIfNull(text);
            var bytes = Encoding.UTF8.GetBytes(text);
            if (bytes.Length == 0 || bytes.Length > 1024)
                throw new ArgumentException($"{what} inválido: {text.Length} caracteres", nameof(text));
            return bytes;
        }

        public bool Exists(ulong objectId) => Accessible && access.Exists(access.Context, objectId) != 0;

        public uint WorldId => Accessible ? access.WorldId(access.Context) : 0;
        public uint GenerationOf(ulong objectId) => Accessible ? access.Generation(access.Context, objectId) : 0;
        public WorldStatus LastStatus => Accessible ? (WorldStatus)access.LastStatus(access.Context) : WorldStatus.NotRunning;

        public TransformValue GetTransform(ulong objectId)
        {
            float* value = stackalloc float[10];
            if (!Accessible || access.GetTransform(access.Context, objectId, value) == 0)
                throw new WorldException(LastStatus, "ler transform");
            return Decode(value);
        }
        public bool SetTransform(ulong objectId, TransformValue value)
        {
            float* data = stackalloc float[10];
            Encode(value, data);
            return Accessible && access.SetTransform(access.Context, objectId, data) != 0;
        }
        public TransformValue GetWorldTransform(ulong objectId)
        {
            float* value = stackalloc float[10];
            if (!Accessible || access.GetWorldTransform(access.Context, objectId, value) == 0)
                throw new WorldException(LastStatus, "ler transform de mundo");
            return Decode(value);
        }
        public bool SetWorldTransform(ulong objectId, TransformValue value)
        {
            float* data = stackalloc float[10];
            Encode(value, data);
            return Accessible && access.SetWorldTransform(access.Context, objectId, data) != 0;
        }
        private static TransformValue Decode(float* v) =>
            new(new(v[0], v[1], v[2]), new(v[3], v[4], v[5], v[6]), new(v[7], v[8], v[9]));
        private static void Encode(TransformValue value, float* data)
        {
            data[0] = value.Position.X; data[1] = value.Position.Y; data[2] = value.Position.Z;
            data[3] = value.Rotation.X; data[4] = value.Rotation.Y; data[5] = value.Rotation.Z; data[6] = value.Rotation.W;
            data[7] = value.Scale.X; data[8] = value.Scale.Y; data[9] = value.Scale.Z;
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

        // --- hierarquia -----------------------------------------------------
        public ulong ParentOf(ulong objectId) => Accessible ? access.ParentOf(access.Context, objectId) : 0;
        public int ChildCount(ulong objectId) => Accessible ? access.ChildCount(access.Context, objectId) : -1;
        public ulong ChildAt(ulong objectId, uint index) => Accessible ? access.ChildAt(access.Context, objectId, index) : 0;
        public ulong FindChild(ulong objectId, string name, bool recursive)
        {
            if (!Accessible) return 0;
            var bytes = Utf8(name, "nome");
            fixed (byte* pointer = bytes)
                return access.FindChild(access.Context, objectId, pointer, bytes.Length, recursive ? 1 : 0);
        }
        public string GetName(ulong objectId)
        {
            if (!Accessible) throw new WorldException(WorldStatus.NotRunning, "ler nome");
            byte* buffer = stackalloc byte[NameCapacity];
            var size = access.GetName(access.Context, objectId, buffer, NameCapacity);
            if (size <= 0 || size > NameCapacity) throw new WorldException(LastStatus, "ler nome");
            return Encoding.UTF8.GetString(buffer, size);
        }
        public bool SetName(ulong objectId, string name)
        {
            if (!Accessible) return false;
            var bytes = Utf8(name, "nome");
            fixed (byte* pointer = bytes) return access.SetName(access.Context, objectId, pointer, bytes.Length) != 0;
        }
        public int GetActive(ulong objectId) => Accessible ? access.GetActive(access.Context, objectId) : -1;
        public bool SetActive(ulong objectId, bool active) =>
            Accessible && access.SetActive(access.Context, objectId, active ? 1 : 0) != 0;

        // --- ciclo de vida --------------------------------------------------
        public ulong CreateObject(ulong parent, string name)
        {
            if (!Accessible) return 0;
            var bytes = Utf8(name, "nome");
            fixed (byte* pointer = bytes) return access.CreateObject(access.Context, parent, pointer, bytes.Length);
        }
        public bool DestroyObject(ulong objectId) => Accessible && access.DestroyObject(access.Context, objectId) != 0;
        public bool SetParent(ulong objectId, ulong parent, uint childIndex) =>
            Accessible && access.SetParent(access.Context, objectId, parent, childIndex) != 0;

        // --- componentes ----------------------------------------------------
        public int ComponentCount(ulong objectId) => Accessible ? access.ComponentCount(access.Context, objectId) : -1;
        public (ulong Instance, string TypeId) ComponentAt(ulong objectId, uint index)
        {
            if (!Accessible) return (0, string.Empty);
            byte* buffer = stackalloc byte[TypeIdCapacity];
            var instance = access.ComponentAt(access.Context, objectId, index, buffer, TypeIdCapacity);
            if (instance == 0) return (0, string.Empty);
            var length = 0;
            while (length < TypeIdCapacity && buffer[length] != 0) ++length;
            return (instance, Encoding.UTF8.GetString(buffer, length));
        }
        public ulong FindComponent(ulong objectId, string typeId, uint ordinal)
        {
            if (!Accessible) return 0;
            var bytes = Utf8(typeId, "tipo de componente");
            fixed (byte* pointer = bytes)
                return access.FindComponent(access.Context, objectId, pointer, bytes.Length, ordinal);
        }
        public ulong AddComponent(ulong objectId, string typeId)
        {
            if (!Accessible) return 0;
            var bytes = Utf8(typeId, "tipo de componente");
            fixed (byte* pointer = bytes) return access.AddComponent(access.Context, objectId, pointer, bytes.Length);
        }
        public bool RemoveComponent(ulong objectId, ulong instanceId) =>
            Accessible && access.RemoveComponent(access.Context, objectId, instanceId) != 0;
        public bool TryGetProperty(ulong objectId, ulong instanceId, string propertyId, out uint kind, out ulong bits)
        {
            kind = 0; bits = 0;
            if (!Accessible) return false;
            var bytes = Utf8(propertyId, "propriedade");
            fixed (byte* pointer = bytes)
            fixed (uint* kindOut = &kind)
            fixed (ulong* bitsOut = &bits)
                return access.GetProperty(access.Context, objectId, instanceId, pointer, bytes.Length, kindOut, bitsOut) != 0;
        }
        public bool SetProperty(ulong objectId, ulong instanceId, string propertyId, uint kind, ulong bits)
        {
            if (!Accessible) return false;
            var bytes = Utf8(propertyId, "propriedade");
            fixed (byte* pointer = bytes)
                return access.SetProperty(access.Context, objectId, instanceId, pointer, bytes.Length, kind, bits) != 0;
        }

        // --- consultas físicas ----------------------------------------------
        private static NativeQueryFilter Encode(in QueryFilter filter) => new()
        {
            Size = (uint)sizeof(NativeQueryFilter),
            GameplayLayerMask = filter.LayerMask,
            Flags = (filter.IncludeStatic ? 1u : 0u) | (filter.IncludeDynamic ? 2u : 0u) | (filter.IncludeSensors ? 4u : 0u),
            Reserved = 0,
            Ignore = filter.Ignore,
        };
        private static NativeShapeQuery Encode(in ShapeQuery shape) => new()
        {
            Kind = (uint)shape.Kind,
            HalfX = shape.HalfExtent.X, HalfY = shape.HalfExtent.Y, HalfZ = shape.HalfExtent.Z,
            Radius = shape.Radius, HalfHeight = shape.HalfHeight,
            RotationX = shape.Rotation.X, RotationY = shape.Rotation.Y,
            RotationZ = shape.Rotation.Z, RotationW = shape.Rotation.W,
        };

        public int RayCast(Vector3 origin, Vector3 direction, in QueryFilter filter, Span<RawQueryHit> results)
        {
            if (!Accessible) return 0;
            var nativeFilter = Encode(filter);
            float* from = stackalloc float[3] { origin.X, origin.Y, origin.Z };
            float* along = stackalloc float[3] { direction.X, direction.Y, direction.Z };
            fixed (RawQueryHit* hits = results)
                return access.RayCast(access.Context, from, along, &nativeFilter, hits, results.Length);
        }
        public int ShapeCast(in ShapeQuery shape, Vector3 origin, Vector3 direction, in QueryFilter filter, out RawQueryHit hit)
        {
            hit = default;
            if (!Accessible) return 0;
            var nativeFilter = Encode(filter);
            var nativeShape = Encode(shape);
            float* from = stackalloc float[3] { origin.X, origin.Y, origin.Z };
            float* along = stackalloc float[3] { direction.X, direction.Y, direction.Z };
            fixed (RawQueryHit* single = &hit)
                return access.ShapeCast(access.Context, &nativeShape, from, along, &nativeFilter, single);
        }
        public int Overlap(in ShapeQuery shape, Vector3 origin, in QueryFilter filter, Span<RawQueryHit> results)
        {
            if (!Accessible) return 0;
            var nativeFilter = Encode(filter);
            var nativeShape = Encode(shape);
            float* from = stackalloc float[3] { origin.X, origin.Y, origin.Z };
            fixed (RawQueryHit* hits = results)
                return access.Overlap(access.Context, &nativeShape, from, &nativeFilter, hits, results.Length);
        }
        public int LayerByName(string name)
        {
            if (!Accessible) return -1;
            var bytes = Utf8(name, "camada");
            fixed (byte* pointer = bytes) return access.LayerByName(access.Context, pointer, bytes.Length);
        }
        public string LayerName(uint layer)
        {
            if (!Accessible) return string.Empty;
            byte* buffer = stackalloc byte[NameCapacity];
            var size = access.LayerName(access.Context, layer, buffer, NameCapacity);
            return size <= 0 || size > NameCapacity ? string.Empty : Encoding.UTF8.GetString(buffer, size);
        }

        // --- entrada por ações ----------------------------------------------
        public bool InputAxis(string action, out Vector2 value)
        {
            value = default;
            if (!Accessible) return false;
            var bytes = Utf8(action, "ação");
            float* raw = stackalloc float[2];
            int ok;
            fixed (byte* pointer = bytes) ok = access.InputAxis(access.Context, pointer, bytes.Length, raw);
            if (ok == 0) return false;
            value = new Vector2(raw[0], raw[1]);
            return true;
        }
        public int InputButton(string action, uint query)
        {
            if (!Accessible) return -1;
            var bytes = Utf8(action, "ação");
            fixed (byte* pointer = bytes) return access.InputButton(access.Context, pointer, bytes.Length, query);
        }
        public bool InputContext(string context, int enabled)
        {
            if (!Accessible) return false;
            var bytes = Utf8(context, "contexto");
            fixed (byte* pointer = bytes) return access.InputContext(access.Context, pointer, bytes.Length, enabled) != 0;
        }
        public string InputRole(uint role)
        {
            if (!Accessible) return string.Empty;
            byte* buffer = stackalloc byte[NameCapacity];
            var size = access.InputRole(access.Context, role, buffer, NameCapacity);
            return size <= 0 || size > NameCapacity ? string.Empty : Encoding.UTF8.GetString(buffer, size);
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
                jsonLength <= 0 || jsonLength > 32 * 1024 * 1024 || access == null || access->Version != 5 ||
                access->Size != sizeof(SceneAccess) || !access->Complete) return 1;
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
    /// <summary>
    /// Contato sólido vindo do passo físico. `normal` é nulo quando o backend não
    /// tem geometria a informar — o fim de um contato não traz.
    /// </summary>
    [UnmanagedCallersOnly]
    public static int Contact(ulong self, ulong other, uint phase, float* normal)
    {
        try
        {
            if (phase > 2) return 1;
            Vector3? direction = normal == null ? null : new Vector3(normal[0], normal[1], normal[2]);
            _world?.Contact(self, other, phase, direction);
            RefreshDiagnostics();
            return 0;
        }
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
