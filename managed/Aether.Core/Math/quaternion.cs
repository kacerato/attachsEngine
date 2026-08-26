using System.Globalization;
using System.Runtime.CompilerServices;

namespace Aether;

/// <summary>Quaternion unitário para rotação. Convenção: mão-esquerda, Y para cima, Z para frente.</summary>
public struct quaternion : IEquatable<quaternion>
{
    public float X, Y, Z, W;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public quaternion(float x, float y, float z, float w) { X = x; Y = y; Z = z; W = w; }

    public static readonly quaternion Identity = new(0f, 0f, 0f, 1f);

    /// <summary>Rotação em torno de um eixo (normalizado internamente) por um ângulo em radianos.</summary>
    public static quaternion AxisAngle(float3 axis, float radians)
    {
        float3 n = axis.Normalized;
        if (n == float3.Zero) return Identity;
        float h = radians * 0.5f;
        float s = MathF.Sin(h);
        return new quaternion(n.X * s, n.Y * s, n.Z * s, MathF.Cos(h));
    }

    /// <summary>Ângulos de Euler em radianos, ordem de aplicação Z (roll) → X (pitch) → Y (yaw).</summary>
    public static quaternion Euler(float pitchX, float yawY, float rollZ)
    {
        float hx = pitchX * 0.5f, hy = yawY * 0.5f, hz = rollZ * 0.5f;
        float sx = MathF.Sin(hx), cx = MathF.Cos(hx);
        float sy = MathF.Sin(hy), cy = MathF.Cos(hy);
        float sz = MathF.Sin(hz), cz = MathF.Cos(hz);
        return new quaternion(
            cy * sx * cz + sy * cx * sz,
            sy * cx * cz - cy * sx * sz,
            cy * cx * sz - sy * sx * cz,
            cy * cx * cz + sy * sx * sz);
    }

    public static quaternion Euler(float3 radians) => Euler(radians.X, radians.Y, radians.Z);

    /// <summary>Composição: aplica <paramref name="b"/> e depois <paramref name="a"/>.</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static quaternion operator *(quaternion a, quaternion b) => new(
        a.W * b.X + a.X * b.W + a.Y * b.Z - a.Z * b.Y,
        a.W * b.Y - a.X * b.Z + a.Y * b.W + a.Z * b.X,
        a.W * b.Z + a.X * b.Y - a.Y * b.X + a.Z * b.W,
        a.W * b.W - a.X * b.X - a.Y * b.Y - a.Z * b.Z);

    /// <summary>Rotaciona um vetor.</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float3 operator *(quaternion q, float3 v)
    {
        float3 u = new(q.X, q.Y, q.Z);
        float3 t = 2f * math.Cross(u, v);
        return v + q.W * t + math.Cross(u, t);
    }

    public static bool operator ==(quaternion a, quaternion b) => a.Equals(b);
    public static bool operator !=(quaternion a, quaternion b) => !a.Equals(b);

    public readonly quaternion Conjugate => new(-X, -Y, -Z, W);

    /// <summary>Inverso. Para quaternions unitários equivale ao conjugado.</summary>
    public readonly quaternion Inverse
    {
        get { float n = X * X + Y * Y + Z * Z + W * W;
              if (n < 1e-12f) return Identity;
              float inv = 1f / n;
              return new quaternion(-X * inv, -Y * inv, -Z * inv, W * inv); }
    }

    public readonly quaternion Normalized
    {
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        get { float n2 = X * X + Y * Y + Z * Z + W * W;
              if (n2 < 1e-24f) return Identity;
              // Quaternions produzidos pela API já são unitários. Evitar sqrt/divisão neste caso
              // é seguro dentro da tolerância numérica e crítico para milhares de composições.
              if (MathF.Abs(n2 - 1f) <= 1e-6f) return this;
              float inv = 1f / MathF.Sqrt(n2);
              return new quaternion(X * inv, Y * inv, Z * inv, W * inv); }
    }

    /// <summary>Interpolação esférica, pelo caminho curto.</summary>
    public static quaternion Slerp(quaternion a, quaternion b, float t)
    {
        float dot = a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;
        if (dot < 0f) { b = new quaternion(-b.X, -b.Y, -b.Z, -b.W); dot = -dot; }
        if (dot > 0.9995f)
        {
            return new quaternion(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t,
                                  a.Z + (b.Z - a.Z) * t, a.W + (b.W - a.W) * t).Normalized;
        }
        float theta0 = MathF.Acos(dot);
        float theta = theta0 * t;
        float sin0 = MathF.Sin(theta0);
        float s0 = MathF.Sin(theta0 - theta) / sin0;
        float s1 = MathF.Sin(theta) / sin0;
        return new quaternion(a.X * s0 + b.X * s1, a.Y * s0 + b.Y * s1,
                              a.Z * s0 + b.Z * s1, a.W * s0 + b.W * s1);
    }

    /// <summary>Rotação que olha na direção dada.</summary>
    public static quaternion LookRotation(float3 forward, float3 up)
    {
        float3 f = forward.Normalized;
        if (f == float3.Zero) return Identity;
        float3 r = math.Cross(up, f).Normalized;
        if (r == float3.Zero) { r = math.Cross(math.Abs(f.Y) > 0.99f ? float3.Right : float3.Up, f).Normalized; }
        float3 u = math.Cross(f, r);
        return FromBasis(r, u, f);
    }

    internal static quaternion FromBasis(float3 r, float3 u, float3 f)
    {
        float trace = r.X + u.Y + f.Z;
        if (trace > 0f)
        {
            float s = MathF.Sqrt(trace + 1f) * 2f;
            return new quaternion((u.Z - f.Y) / s, (f.X - r.Z) / s, (r.Y - u.X) / s, 0.25f * s);
        }
        if (r.X > u.Y && r.X > f.Z)
        {
            float s = MathF.Sqrt(1f + r.X - u.Y - f.Z) * 2f;
            return new quaternion(0.25f * s, (u.X + r.Y) / s, (f.X + r.Z) / s, (u.Z - f.Y) / s);
        }
        if (u.Y > f.Z)
        {
            float s = MathF.Sqrt(1f + u.Y - r.X - f.Z) * 2f;
            return new quaternion((u.X + r.Y) / s, 0.25f * s, (f.Y + u.Z) / s, (f.X - r.Z) / s);
        }
        {
            float s = MathF.Sqrt(1f + f.Z - r.X - u.Y) * 2f;
            return new quaternion((f.X + r.Z) / s, (f.Y + u.Z) / s, 0.25f * s, (r.Y - u.X) / s);
        }
    }

    public readonly bool Equals(quaternion o) => X == o.X && Y == o.Y && Z == o.Z && W == o.W;
    public readonly override bool Equals(object? o) => o is quaternion q && Equals(q);
    public readonly override int GetHashCode() => HashCode.Combine(X, Y, Z, W);
    public readonly override string ToString() =>
        string.Create(CultureInfo.InvariantCulture, $"({X:0.###}, {Y:0.###}, {Z:0.###}, {W:0.###})");
}
