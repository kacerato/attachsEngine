using System.Globalization;
using System.Runtime.CompilerServices;

namespace Aether;

/// <summary>Vetor 2D (8 bytes, blittable).</summary>
public struct float2 : IEquatable<float2>
{
    public float X, Y;
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public float2(float x, float y) { X = x; Y = y; }
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public float2(float s) { X = s; Y = s; }

    public static readonly float2 Zero = default;
    public static readonly float2 One  = new(1f);

    public static float2 operator +(float2 a, float2 b) => new(a.X + b.X, a.Y + b.Y);
    public static float2 operator -(float2 a, float2 b) => new(a.X - b.X, a.Y - b.Y);
    public static float2 operator *(float2 a, float2 b) => new(a.X * b.X, a.Y * b.Y);
    public static float2 operator *(float2 a, float s)  => new(a.X * s, a.Y * s);
    public static float2 operator *(float s, float2 a)  => a * s;
    public static float2 operator /(float2 a, float s)  => a * (1f / s);
    public static float2 operator -(float2 a) => new(-a.X, -a.Y);
    public static bool operator ==(float2 a, float2 b) => a.Equals(b);
    public static bool operator !=(float2 a, float2 b) => !a.Equals(b);

    public readonly bool Equals(float2 o) => X == o.X && Y == o.Y;
    public readonly override bool Equals(object? o) => o is float2 v && Equals(v);
    public readonly override int GetHashCode() => HashCode.Combine(X, Y);
    public readonly override string ToString() =>
        string.Create(CultureInfo.InvariantCulture, $"({X:0.###}, {Y:0.###})");

    public readonly float LengthSquared => X * X + Y * Y;
    public readonly float Length => MathF.Sqrt(LengthSquared);
    public readonly float2 Normalized
    { get { float l2 = LengthSquared; return l2 > 1e-12f ? this * (1f / MathF.Sqrt(l2)) : Zero; } }
}

/// <summary>Vetor 4D (16 bytes, blittable). Também usado como linha de matriz e como plano homogêneo.</summary>
public struct float4 : IEquatable<float4>
{
    public float X, Y, Z, W;
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public float4(float x, float y, float z, float w) { X = x; Y = y; Z = z; W = w; }
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public float4(float3 xyz, float w) { X = xyz.X; Y = xyz.Y; Z = xyz.Z; W = w; }
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public float4(float s) { X = s; Y = s; Z = s; W = s; }

    public static readonly float4 Zero = default;
    public static readonly float4 One  = new(1f);

    public readonly float3 XYZ { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => new(X, Y, Z); }

    public float this[int i]
    {
        readonly get => i switch { 0 => X, 1 => Y, 2 => Z, 3 => W, _ => throw new ArgumentOutOfRangeException(nameof(i)) };
        set { switch (i) { case 0: X = value; break; case 1: Y = value; break; case 2: Z = value; break; case 3: W = value; break;
                           default: throw new ArgumentOutOfRangeException(nameof(i)); } }
    }

    public static float4 operator +(float4 a, float4 b) => new(a.X + b.X, a.Y + b.Y, a.Z + b.Z, a.W + b.W);
    public static float4 operator -(float4 a, float4 b) => new(a.X - b.X, a.Y - b.Y, a.Z - b.Z, a.W - b.W);
    public static float4 operator *(float4 a, float s)  => new(a.X * s, a.Y * s, a.Z * s, a.W * s);
    public static float4 operator *(float s, float4 a)  => a * s;
    public static float4 operator /(float4 a, float s)  => a * (1f / s);
    public static bool operator ==(float4 a, float4 b) => a.Equals(b);
    public static bool operator !=(float4 a, float4 b) => !a.Equals(b);

    public readonly bool Equals(float4 o) => X == o.X && Y == o.Y && Z == o.Z && W == o.W;
    public readonly override bool Equals(object? o) => o is float4 v && Equals(v);
    public readonly override int GetHashCode() => HashCode.Combine(X, Y, Z, W);
    public readonly override string ToString() =>
        string.Create(CultureInfo.InvariantCulture, $"({X:0.###}, {Y:0.###}, {Z:0.###}, {W:0.###})");
}
