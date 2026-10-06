using System.Numerics;
using System.Runtime.InteropServices;

namespace Astra;

/// <summary>Tipos que atravessam métodos e eventos de componente. Espelha
/// <c>ae::scene::ComponentValueKind</c>; não reordenar.</summary>
public enum ComponentValueKind : uint { None, Boolean, Integer, Number, Vector3, Object }

/// <summary>
/// Valor de argumento, retorno ou payload. Layout de 24 bytes idêntico a
/// <c>ae::scene::ComponentOperationValue</c> (native/scene/components.h).
/// </summary>
[StructLayout(LayoutKind.Explicit, Size = 24)]
public readonly struct ComponentValue
{
    [FieldOffset(0)] private readonly uint _kind;
    [FieldOffset(4)] private readonly uint _reserved;
    [FieldOffset(8)] private readonly double _number;
    [FieldOffset(8)] private readonly long _integer;
    [FieldOffset(8)] private readonly ulong _object;
    [FieldOffset(8)] private readonly uint _boolean;
    [FieldOffset(8)] private readonly float _x;
    [FieldOffset(12)] private readonly float _y;
    [FieldOffset(16)] private readonly float _z;
    [FieldOffset(20)] private readonly float _w;

    private ComponentValue(ComponentValueKind kind, double number = 0, long integer = 0, ulong obj = 0, uint boolean = 0, Vector3? vector = null)
    {
        _kind = (uint)kind; _reserved = 0; _w = 0;
        switch (kind)
        {
            case ComponentValueKind.Boolean: _boolean = boolean; break;
            case ComponentValueKind.Integer: _integer = integer; break;
            case ComponentValueKind.Number: _number = number; break;
            case ComponentValueKind.Object: _object = obj; break;
            case ComponentValueKind.Vector3: var v = vector ?? default; _x = v.X; _y = v.Y; _z = v.Z; break;
        }
    }
    public static ComponentValue Boolean(bool value) => new(ComponentValueKind.Boolean, boolean: value ? 1u : 0u);
    public static ComponentValue Integer(long value) => new(ComponentValueKind.Integer, integer: value);
    public static ComponentValue Number(double value) => new(ComponentValueKind.Number, number: value);
    public static ComponentValue Vector(Vector3 value) => new(ComponentValueKind.Vector3, vector: value);
    public static ComponentValue Object(ulong objectId) => new(ComponentValueKind.Object, obj: objectId);
    public static ComponentValue Object(ObjectReference reference) => Object(reference.ObjectId);

    public ComponentValueKind Kind => (ComponentValueKind)_kind;
    private void Expect(ComponentValueKind kind)
    {
        if (Kind != kind) throw new InvalidOperationException($"Valor de componente é {Kind}, não {kind}.");
    }
    public bool AsBoolean() { Expect(ComponentValueKind.Boolean); return _boolean != 0; }
    public long AsInteger() { Expect(ComponentValueKind.Integer); return _integer; }
    public double AsNumber() { Expect(ComponentValueKind.Number); return _number; }
    public Vector3 AsVector3() { Expect(ComponentValueKind.Vector3); return new(_x, _y, _z); }
    public ulong AsObjectId() { Expect(ComponentValueKind.Object); return _object; }
    public override string ToString() => Kind switch
    {
        ComponentValueKind.Boolean => (_boolean != 0).ToString(),
        ComponentValueKind.Integer => _integer.ToString(),
        ComponentValueKind.Number => _number.ToString(System.Globalization.CultureInfo.InvariantCulture),
        ComponentValueKind.Vector3 => new Vector3(_x, _y, _z).ToString(),
        ComponentValueKind.Object => "#" + _object,
        _ => "nada",
    };
}

/// <summary>Um acontecimento entregue pelo mundo de Play, com o tipo e o evento por nome.</summary>
public readonly record struct ComponentEventRecord(ulong ObjectId, uint World, uint Generation, ulong InstanceId,
    string TypeId, string EventId, uint Count, uint Lost, ComponentValue Value0, ComponentValue Value1, ComponentValue Value2)
{
    public ComponentValue this[int index] => (uint)index < Count
        ? index switch { 0 => Value0, 1 => Value1, _ => Value2 }
        : throw new ArgumentOutOfRangeException(nameof(index));
}

/// <summary>
/// Família <c>astra.component.operations</c> da ABI. Host sem ela não implementa
/// esta interface, e só as chamadas de método/evento são recusadas.
/// </summary>
public interface IComponentOperationAccess
{
    bool InvokeComponentMethod(ulong objectId, uint world, uint generation, ulong instanceId, string method,
        ReadOnlySpan<ComponentValue> arguments, out ComponentValue result);
    /// <summary>Copia eventos pendentes em ordem de emissão; devolve a quantidade copiada.</summary>
    int PollComponentEvents(Span<ComponentEventRecord> destination);
    /// <summary>Verdadeiro quando o tipo declara o evento nesta build.</summary>
    bool DeclaresComponentEvent(string typeId, string eventId);
}

/// <summary>Evento recebido por uma assinatura de componente.</summary>
public readonly struct ComponentEventArgs
{
    private readonly ComponentEventRecord _record;
    private readonly ISceneAccess _scene;
    internal ComponentEventArgs(ISceneAccess scene, Component source, ComponentEventRecord record)
    {
        _scene = scene; Source = source; _record = record;
    }
    public Component Source { get; }
    public string EventId => _record.EventId;
    /// <summary>Instância que emitiu; zero quando o backend identifica só o objeto (física 3D).</summary>
    public ulong EmitterInstanceId => _record.InstanceId;
    public int Count => (int)_record.Count;
    /// <summary>Eventos descartados por estouro da fila antes deste.</summary>
    public uint Lost => _record.Lost;
    public ComponentValue this[int index] => _record[index];
    public ObjectReference GetObject(int index) => ObjectReference.Capture(_scene, _record[index].AsObjectId());
}

/// <summary>Assinatura de um evento. Termina com Dispose, ao retirar o comportamento dono ou no fim do Play.</summary>
public sealed class ComponentSubscription : IDisposable
{
    internal ComponentSubscription(Behavior owner, Component source, string eventId, Action<ComponentEventArgs> handler)
    {
        Owner = owner; Source = source; EventId = eventId; Handler = handler;
    }
    internal Behavior Owner { get; }
    internal Action<ComponentEventArgs> Handler { get; }
    public Component Source { get; }
    public string EventId { get; }
    public bool Active { get; internal set; } = true;
    public void Dispose() => Active = false;
}

internal interface IComponentEventHost
{
    ComponentSubscription Connect(Behavior owner, Component source, string eventId, Action<ComponentEventArgs> handler);
}
