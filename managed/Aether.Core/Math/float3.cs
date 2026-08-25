using System.Diagnostics;
using System.Globalization;
using System.Runtime.CompilerServices;

namespace Aether;

/// <summary>Vetor 3D. Layout idêntico ao lado nativo (12 bytes, blittable).</summary>
[DebuggerDisplay("({X}, {Y}, {Z})")]
public struct float3 : IEquatable<float3>
{
    public float X, Y, Z;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public float3(float x, float y, float z) { X = x; Y = y; Z = z; }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public float3(float s) { X = s; Y = s; Z = s; }

    public static readonly float3 Zero    = default;
    public static readonly float3 One     = new(1f);
    public static readonly float3 Right   = new(1f, 0f, 0f);
    public static readonly float3 Up      = new(0f, 1f, 0f);
    public static readonly float3 Forward = new(0f, 0f, 1f);

    public float this[int i]
    {
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        readonly get => i switch { 0 => X, 1 => Y, 2 => Z, _ => throw new ArgumentOutOfRangeException(nameof(i)) };
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        set { switch (i) { case 0: X = value; break; case 1: Y = value; break; case 2: Z = value; break;
                           default: throw new ArgumentOutOfRangeException(nameof(i)); } }
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float3 operator +(float3 a, float3 b) => new(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float3 operator -(float3 a, float3 b) => new(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float3 operator *(float3 a, float3 b) => new(a.X * b.X, a.Y * b.Y, a.Z * b.Z);
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float3 operator /(float3 a, float3 b) => new(a.X / b.X, a.Y / b.Y, a.Z / b.Z);
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float3 operator *(float3 a, float s) => new(a.X * s, a.Y * s, a.Z * s);
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float3 operator *(float s, float3 a) => a * s;
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float3 operator /(float3 a, float s) => a * (1f / s);
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float3 operator -(float3 a) => new(-a.X, -a.Y, -a.Z);

    public static bool operator ==(float3 a, float3 b) => a.Equals(b);
    public static bool operator !=(float3 a, float3 b) => !a.Equals(b);

    public readonly bool Equals(float3 o) => X == o.X && Y == o.Y && Z == o.Z;
    public readonly override bool Equals(object? o) => o is float3 v && Equals(v);
    public readonly override int GetHashCode() => HashCode.Combine(X, Y, Z);
    public readonly override string ToString() =>
        string.Create(CultureInfo.InvariantCulture, $"({X:0.###}, {Y:0.###}, {Z:0.###})");

    public readonly float LengthSquared { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => X * X + Y * Y + Z * Z; }
    public readonly float Length { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => MathF.Sqrt(LengthSquared); }

    /// <summary>Normaliza; devolve <see cref="Zero"/> se o comprimento for desprezível (nunca produz NaN).</summary>
    public readonly float3 Normalized
    {
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        get { float l2 = LengthSquared; return l2 > 1e-12f ? this * (1f / MathF.Sqrt(l2)) : Zero; }
    }
}
