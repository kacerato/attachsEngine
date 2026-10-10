using System.Globalization;
using System.Runtime.InteropServices;
using System.Text;

namespace Astra.Editor;

public enum ClipValueKind : uint { Number, Vector2, Vector3, LinearColor, Quaternion, Boolean, Enumeration, Object, Resource }
public enum ClipPropertyDomain : uint { Component, MaterialSlot, GuiNode }
public sealed record ClipPropertyAddress(ClipPropertyDomain Domain, ClipValueKind Kind, ulong Component,
    uint Version, string Type, string Property, uint Slot = 0, uint GuiNode = 0, AssetGuid Resource = default);
public sealed record ClipPropertyDescriptor(ClipPropertyAddress Address, string Label);
public sealed record ClipDriverParameter(ClipPropertyAddress Address, ulong Parameter, AssetGuid Controller, string Label);
/// <summary>Closed value types. Object identities and resource GUIDs retain all bits.</summary>
public readonly record struct ClipValue
{
    public ClipValueKind Kind { get; private init; }
    public float X { get; private init; }
    public float Y { get; private init; }
    public float Z { get; private init; }
    public float W { get; private init; }
    public uint Option { get; private init; }
    public ulong ObjectId { get; private init; }
    public AssetGuid ResourceId { get; private init; }
    public static ClipValue Number(float value) => new() { Kind = ClipValueKind.Number, X = value };
    public static ClipValue Vector2(float x, float y) => new() { Kind = ClipValueKind.Vector2, X = x, Y = y };
    public static ClipValue Vector3(float x, float y, float z) => new() { Kind = ClipValueKind.Vector3, X = x, Y = y, Z = z };
    public static ClipValue Color(float r, float g, float b) => new() { Kind = ClipValueKind.LinearColor, X = r, Y = g, Z = b };
    public static ClipValue Quaternion(float x, float y, float z, float w) => new() { Kind = ClipValueKind.Quaternion, X = x, Y = y, Z = z, W = w };
    public static ClipValue Boolean(bool value) => new() { Kind = ClipValueKind.Boolean, Option = value ? 1u : 0 };
    public static ClipValue Enumeration(uint value) => new() { Kind = ClipValueKind.Enumeration, Option = value };
    public static ClipValue Object(ulong value) => new() { Kind = ClipValueKind.Object, ObjectId = value };
    public static ClipValue Resource(AssetGuid value) => new() { Kind = ClipValueKind.Resource, ResourceId = value };
}
public sealed record ClipDiscreteKey(ulong Id, float Time, ClipValue Value);
public sealed record ClipTypedTrack(ulong Id, ulong Binding, ClipPropertyAddress Address, bool LeftContinuous,
    IReadOnlyList<ClipCurve> Curves, IReadOnlyList<ClipDiscreteKey> Keys);
public enum ClipDriverInputKind : uint { ClipTime, Property, AnimatorParameter }
public enum ClipDriverOperation : uint { Source, Constant, Add, Multiply, ScaleBias, Clamp, Remap, Curve }
public sealed record ClipDriverTarget(ulong Binding, ClipPropertyAddress Address);
public sealed record ClipDriverInput(ClipDriverInputKind Kind, ulong Binding = 0, ClipPropertyAddress? Address = null,
    ulong Parameter = 0, AssetGuid Controller = default);
public sealed record ClipDriverNode(ulong Id, ClipDriverOperation Operation, ulong First = 0, ulong Second = 0,
    uint Source = 0, double A = 0, double B = 1, double C = 0, double D = 1, ClipCurve? Curve = null);
/// <summary>Sources observe the composed clip; outputs publish atomically before physics. IDs share the clip allocator.</summary>
public sealed record ClipDrivers(IReadOnlyList<ClipDriverInput> Inputs, IReadOnlyList<ClipDriverNode> Nodes,
    IReadOnlyList<ClipDriverTarget> Targets, IReadOnlyList<ulong> Outputs)
{
    public static ClipDrivers Empty { get; } = new(Array.Empty<ClipDriverInput>(), Array.Empty<ClipDriverNode>(), Array.Empty<ClipDriverTarget>(), Array.Empty<ulong>());
    internal byte[] Encode()
    {
        if (Inputs.Count > 256 || Nodes.Count > 512 || Targets.Count > 128 || Outputs.Count != Targets.Count)
            throw new ArgumentException("Driver graph exceeds its budget or has unmatched outputs.");
        var text = new StringBuilder(); var keys = 0;
        void N<T>(T value) where T : IFormattable => text.Append(value.ToString(null, CultureInfo.InvariantCulture)).Append(' ');
        void Q(string value) { EditorContext.Text(value, 127); text.Append('"').Append(value.Replace("\\", "\\\\").Replace("\"", "\\\"")).Append("\" "); }
        void G(AssetGuid value) => text.Append(value == default ? "-" : value.ToString()).Append(' ');
        void Address(ulong binding, ClipPropertyAddress? a)
        {
            if (a == null || a.Kind != ClipValueKind.Number) throw new ArgumentException("Drivers require an effective scalar numeric property.");
            N(binding); N((uint)a.Domain); N(a.Component); N(a.Version); Q(a.Type); Q(a.Property); N(a.Slot); N(a.GuiNode); G(a.Resource);
        }
        N(Nodes.Count); N(Inputs.Count); N(Targets.Count); text.AppendLine();
        foreach (var input in Inputs)
        {
            N((uint)input.Kind); if (input.Kind != ClipDriverInputKind.ClipTime) Address(input.Binding, input.Address);
            else if (input.Binding != 0 || input.Address != null) throw new ArgumentException("Clip time has no component address.");
            N(input.Parameter); G(input.Controller); text.AppendLine();
        }
        foreach (var target in Targets) { Address(target.Binding, target.Address); text.AppendLine(); }
        foreach (var node in Nodes)
        {
            N(node.Id); N((uint)node.Operation); N(node.First); N(node.Second); N(node.Source);
            N(node.A); N(node.B); N(node.C); N(node.D); var curve = node.Curve?.Keys ?? Array.Empty<ClipKey>();
            keys += curve.Count; if (keys > 8192) throw new ArgumentException("Driver curves exceed their key budget.");
            N(curve.Count); text.AppendLine();
            foreach (var k in curve)
            {
                N(k.Id); N(k.Time); N(k.Value); N(k.IncomingSlope); N(k.OutgoingSlope); N(k.IncomingWeight); N(k.OutgoingWeight);
                N((uint)k.Incoming); N((uint)k.Outgoing); N(k.Broken ? 1 : 0); N(k.WeightedIncoming ? 1 : 0); N(k.WeightedOutgoing ? 1 : 0); text.AppendLine();
            }
        }
        foreach (var output in Outputs) N(output); text.AppendLine(); return Encoding.UTF8.GetBytes(text.ToString());
    }
}
[StructLayout(LayoutKind.Sequential)]
internal unsafe struct AuthorProperty
{
    public uint Domain, Kind, Version, Slot;
    public ulong Component;
    public AuthorGuid Resource;
    public uint GuiNode, Reserved;
    public fixed byte Type[128];
    public fixed byte Property[128];
    public AuthorProperty(ClipPropertyAddress value)
    {
        this = default; Domain = (uint)value.Domain; Kind = (uint)value.Kind; Version = value.Version; Slot = value.Slot;
        Component = value.Component; Resource = new(value.Resource); GuiNode = value.GuiNode;
        var type = EditorContext.Text(value.Type, 127); var property = EditorContext.Text(value.Property, 127);
        fixed (byte* p = Type) type.CopyTo(new Span<byte>(p, 128));
        fixed (byte* p = Property) property.CopyTo(new Span<byte>(p, 128));
    }
    public ClipPropertyAddress Value
    {
        get
        {
            string type, property;
            fixed (byte* p = Type) type = Read(p);
            fixed (byte* p = Property) property = Read(p);
            return new((ClipPropertyDomain)Domain, (ClipValueKind)Kind, Component, Version, type, property, Slot, GuiNode, Resource.Value);
        }
    }
    private static string Read(byte* p)
    {
        var bytes = new ReadOnlySpan<byte>(p, 128); var end = bytes.IndexOf((byte)0);
        if (end < 0) throw new InvalidDataException("Unterminated property address.");
        return new UTF8Encoding(false, true).GetString(bytes[..end]);
    }
}
[StructLayout(LayoutKind.Sequential)]
internal struct AuthorPropertyValue
{
    public uint Kind, Reserved;
    public float X, Y, Z, W;
    public ulong Object;
    public AuthorGuid Resource;
    public uint Option, Reserved2;
    public AuthorPropertyValue(ClipValue value)
    {
        this = default; Kind = (uint)value.Kind; X = value.X; Y = value.Y; Z = value.Z; W = value.W;
        Object = value.ObjectId; Resource = new(value.ResourceId); Option = value.Option;
    }
    public readonly ClipValue Value => (ClipValueKind)Kind switch
    {
        ClipValueKind.Number => ClipValue.Number(X), ClipValueKind.Vector2 => ClipValue.Vector2(X, Y),
        ClipValueKind.Vector3 => ClipValue.Vector3(X, Y, Z), ClipValueKind.LinearColor => ClipValue.Color(X, Y, Z),
        ClipValueKind.Quaternion => ClipValue.Quaternion(X, Y, Z, W), ClipValueKind.Boolean => ClipValue.Boolean(Option != 0),
        ClipValueKind.Enumeration => ClipValue.Enumeration(Option), ClipValueKind.Object => ClipValue.Object(Object),
        ClipValueKind.Resource => ClipValue.Resource(Resource.Value), _ => throw new InvalidDataException("Unknown property type.")
    };
}
[StructLayout(LayoutKind.Sequential)]
internal struct AuthorDriverParameter
{
    public AuthorProperty Property;
    public ulong Parameter;
    public AuthorGuid Controller;
}
