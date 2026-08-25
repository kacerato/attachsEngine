using System.Runtime.CompilerServices;

namespace Aether;

/// <summary>
/// Matriz 4x4, armazenamento column-major (compatível com SPIR-V/GLSL sem transposição).
/// C0..C3 são as colunas; a translação vive em C3.
/// </summary>
public struct float4x4 : IEquatable<float4x4>
{
    public float4 C0, C1, C2, C3;

    public float4x4(float4 c0, float4 c1, float4 c2, float4 c3) { C0 = c0; C1 = c1; C2 = c2; C3 = c3; }

    public static readonly float4x4 Identity = new(
        new float4(1, 0, 0, 0), new float4(0, 1, 0, 0),
        new float4(0, 0, 1, 0), new float4(0, 0, 0, 1));

    public float4 this[int col]
    {
        readonly get => col switch { 0 => C0, 1 => C1, 2 => C2, 3 => C3, _ => throw new ArgumentOutOfRangeException(nameof(col)) };
        set { switch (col) { case 0: C0 = value; break; case 1: C1 = value; break; case 2: C2 = value; break; case 3: C3 = value; break;
                             default: throw new ArgumentOutOfRangeException(nameof(col)); } }
    }

    public static float4x4 Translate(float3 t) => new(
        new float4(1, 0, 0, 0), new float4(0, 1, 0, 0), new float4(0, 0, 1, 0), new float4(t, 1f));

    public static float4x4 Scale(float3 s) => new(
        new float4(s.X, 0, 0, 0), new float4(0, s.Y, 0, 0), new float4(0, 0, s.Z, 0), new float4(0, 0, 0, 1));

    public static float4x4 Rotate(quaternion q)
    {
        float x = q.X, y = q.Y, z = q.Z, w = q.W;
        float x2 = x + x, y2 = y + y, z2 = z + z;
        float xx = x * x2, xy = x * y2, xz = x * z2;
        float yy = y * y2, yz = y * z2, zz = z * z2;
        float wx = w * x2, wy = w * y2, wz = w * z2;
        return new float4x4(
            new float4(1f - (yy + zz), xy + wz, xz - wy, 0f),
            new float4(xy - wz, 1f - (xx + zz), yz + wx, 0f),
            new float4(xz + wy, yz - wx, 1f - (xx + yy), 0f),
            new float4(0f, 0f, 0f, 1f));
    }

    public static float4x4 TRS(float3 t, quaternion r, float3 s)
    {
        float4x4 m = Rotate(r);
        m.C0 *= s.X; m.C1 *= s.Y; m.C2 *= s.Z;
        m.C3 = new float4(t, 1f);
        return m;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float4x4 operator *(float4x4 a, float4x4 b) => new(
        a.Transform(b.C0), a.Transform(b.C1), a.Transform(b.C2), a.Transform(b.C3));

    /// <summary>Multiplica esta matriz por um vetor homogêneo.</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public readonly float4 Transform(float4 v) =>
        C0 * v.X + C1 * v.Y + C2 * v.Z + C3 * v.W;

    /// <summary>Transforma um ponto (w = 1).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public readonly float3 TransformPoint(float3 p) => Transform(new float4(p, 1f)).XYZ;

    /// <summary>Transforma uma direção (w = 0, ignora translação).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public readonly float3 TransformDirection(float3 d) => Transform(new float4(d, 0f)).XYZ;

    public readonly float4x4 Transposed => new(
        new float4(C0.X, C1.X, C2.X, C3.X), new float4(C0.Y, C1.Y, C2.Y, C3.Y),
        new float4(C0.Z, C1.Z, C2.Z, C3.Z), new float4(C0.W, C1.W, C2.W, C3.W));

    /// <summary>Inversa geral (cofatores). Devolve <see cref="Identity"/> se singular.</summary>
    public readonly float4x4 Inverse
    {
        get
        {
            Span<float> m = stackalloc float[16];
            for (int c = 0; c < 4; c++) { float4 col = this[c]; m[c * 4 + 0] = col.X; m[c * 4 + 1] = col.Y; m[c * 4 + 2] = col.Z; m[c * 4 + 3] = col.W; }
            Span<float> inv = stackalloc float[16];

            inv[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
            inv[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
            inv[8]  =  m[4]*m[9]*m[15]  - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
            inv[12] = -m[4]*m[9]*m[14]  + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
            inv[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
            inv[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
            inv[9]  = -m[0]*m[9]*m[15]  + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
            inv[13] =  m[0]*m[9]*m[14]  - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
            inv[2]  =  m[1]*m[6]*m[15]  - m[1]*m[7]*m[14]  - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7]  - m[13]*m[3]*m[6];
            inv[6]  = -m[0]*m[6]*m[15]  + m[0]*m[7]*m[14]  + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7]  + m[12]*m[3]*m[6];
            inv[10] =  m[0]*m[5]*m[15]  - m[0]*m[7]*m[13]  - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7]  - m[12]*m[3]*m[5];
            inv[14] = -m[0]*m[5]*m[14]  + m[0]*m[6]*m[13]  + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6]  + m[12]*m[2]*m[5];
            inv[3]  = -m[1]*m[6]*m[11]  + m[1]*m[7]*m[10]  + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7]   + m[9]*m[3]*m[6];
            inv[7]  =  m[0]*m[6]*m[11]  - m[0]*m[7]*m[10]  - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7]   - m[8]*m[3]*m[6];
            inv[11] = -m[0]*m[5]*m[11]  + m[0]*m[7]*m[9]   + m[4]*m[1]*m[11] - m[4]*m[3]*m[9]  - m[8]*m[1]*m[7]   + m[8]*m[3]*m[5];
            inv[15] =  m[0]*m[5]*m[10]  - m[0]*m[6]*m[9]   - m[4]*m[1]*m[10] + m[4]*m[2]*m[9]  + m[8]*m[1]*m[6]   - m[8]*m[2]*m[5];

            float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
            if (MathF.Abs(det) < 1e-20f) return Identity;
            float d = 1f / det;
            return new float4x4(
                new float4(inv[0]*d,  inv[1]*d,  inv[2]*d,  inv[3]*d),
                new float4(inv[4]*d,  inv[5]*d,  inv[6]*d,  inv[7]*d),
                new float4(inv[8]*d,  inv[9]*d,  inv[10]*d, inv[11]*d),
                new float4(inv[12]*d, inv[13]*d, inv[14]*d, inv[15]*d));
        }
    }

    /// <summary>Projeção perspectiva para Vulkan: profundidade em [0,1], Y invertido no clip space.</summary>
    public static float4x4 PerspectiveVulkan(float fovYRadians, float aspect, float near, float far)
    {
        float t = 1f / MathF.Tan(fovYRadians * 0.5f);
        float range = far - near;
        return new float4x4(
            new float4(t / aspect, 0f, 0f, 0f),
            new float4(0f, -t, 0f, 0f),                       // Y invertido: convenção do Vulkan
            new float4(0f, 0f, far / range, 1f),              // w = +z (mão-esquerda)
            new float4(0f, 0f, -(near * far) / range, 0f));
    }

    /// <summary>Perspectiva com plano distante infinito e Z invertido (melhor precisão de depth buffer).</summary>
    public static float4x4 PerspectiveReverseZ(float fovYRadians, float aspect, float near)
    {
        float t = 1f / MathF.Tan(fovYRadians * 0.5f);
        return new float4x4(
            new float4(t / aspect, 0f, 0f, 0f),
            new float4(0f, -t, 0f, 0f),
            new float4(0f, 0f, 0f, 1f),
            new float4(0f, 0f, near, 0f));
    }

    public static float4x4 OrthographicVulkan(float width, float height, float near, float far)
    {
        return new float4x4(
            new float4(2f / width, 0f, 0f, 0f),
            new float4(0f, -2f / height, 0f, 0f),
            new float4(0f, 0f, 1f / (far - near), 0f),
            new float4(0f, 0f, -near / (far - near), 1f));
    }

    /// <summary>Matriz de visão (mundo → câmera).</summary>
    public static float4x4 LookAt(float3 eye, float3 target, float3 up)
    {
        float3 f = (target - eye).Normalized;
        float3 r = math.Cross(up, f).Normalized;
        float3 u = math.Cross(f, r);
        return new float4x4(
            new float4(r.X, u.X, f.X, 0f),
            new float4(r.Y, u.Y, f.Y, 0f),
            new float4(r.Z, u.Z, f.Z, 0f),
            new float4(-math.Dot(r, eye), -math.Dot(u, eye), -math.Dot(f, eye), 1f));
    }

    public readonly bool Equals(float4x4 o) => C0 == o.C0 && C1 == o.C1 && C2 == o.C2 && C3 == o.C3;
    public readonly override bool Equals(object? o) => o is float4x4 m && Equals(m);
    public readonly override int GetHashCode() => HashCode.Combine(C0, C1, C2, C3);
    public static bool operator ==(float4x4 a, float4x4 b) => a.Equals(b);
    public static bool operator !=(float4x4 a, float4x4 b) => !a.Equals(b);
    public readonly override string ToString() => $"[{C0}\n {C1}\n {C2}\n {C3}]";
}
