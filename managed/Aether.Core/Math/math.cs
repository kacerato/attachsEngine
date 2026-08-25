using System.Runtime.CompilerServices;

namespace Aether;

/// <summary>Funções matemáticas livres. Nome minúsculo por convenção de shader.</summary>
public static class math
{
    public const float PI      = 3.14159265358979f;
    public const float TAU     = 6.28318530717959f;
    public const float Deg2Rad = PI / 180f;
    public const float Rad2Deg = 180f / PI;
    public const float Epsilon = 1e-6f;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float Dot(float3 a, float3 b) => a.X * b.X + a.Y * b.Y + a.Z * b.Z;
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float Dot(float2 a, float2 b) => a.X * b.X + a.Y * b.Y;
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float Dot(float4 a, float4 b) => a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float3 Cross(float3 a, float3 b) => new(
        a.Y * b.Z - a.Z * b.Y,
        a.Z * b.X - a.X * b.Z,
        a.X * b.Y - a.Y * b.X);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float Clamp(float v, float lo, float hi) => v < lo ? lo : (v > hi ? hi : v);
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int Clamp(int v, int lo, int hi) => v < lo ? lo : (v > hi ? hi : v);
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float Saturate(float v) => Clamp(v, 0f, 1f);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float Lerp(float a, float b, float t) => a + (b - a) * t;
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float3 Lerp(float3 a, float3 b, float t) => a + (b - a) * t;
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float2 Lerp(float2 a, float2 b, float t) => a + (b - a) * t;

    /// <summary>Interpolação independente da taxa de quadros. <paramref name="halfLife"/> em segundos.</summary>
    public static float Damp(float a, float b, float halfLife, float dt)
        => b + (a - b) * MathF.Exp(-0.6931472f * dt / MathF.Max(halfLife, 1e-6f));
    public static float3 Damp(float3 a, float3 b, float halfLife, float dt)
        => b + (a - b) * MathF.Exp(-0.6931472f * dt / MathF.Max(halfLife, 1e-6f));

    public static float SmoothStep(float edge0, float edge1, float x)
    { float t = Saturate((x - edge0) / (edge1 - edge0)); return t * t * (3f - 2f * t); }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static float Abs(float v) => MathF.Abs(v);
    public static float3 Abs(float3 v) => new(MathF.Abs(v.X), MathF.Abs(v.Y), MathF.Abs(v.Z));
    public static float3 Min(float3 a, float3 b) => new(MathF.Min(a.X, b.X), MathF.Min(a.Y, b.Y), MathF.Min(a.Z, b.Z));
    public static float3 Max(float3 a, float3 b) => new(MathF.Max(a.X, b.X), MathF.Max(a.Y, b.Y), MathF.Max(a.Z, b.Z));
    public static float MinComponent(float3 v) => MathF.Min(v.X, MathF.Min(v.Y, v.Z));
    public static float MaxComponent(float3 v) => MathF.Max(v.X, MathF.Max(v.Y, v.Z));

    public static float Distance(float3 a, float3 b) => (a - b).Length;
    public static float DistanceSquared(float3 a, float3 b) => (a - b).LengthSquared;

    public static bool Approximately(float a, float b, float eps = Epsilon) => MathF.Abs(a - b) <= eps;
    public static bool Approximately(float3 a, float3 b, float eps = Epsilon)
        => Approximately(a.X, b.X, eps) && Approximately(a.Y, b.Y, eps) && Approximately(a.Z, b.Z, eps);

    /// <summary>Arredonda para o múltiplo mais próximo — base do snap de grade do editor.</summary>
    public static float Snap(float v, float step) => step <= 0f ? v : MathF.Round(v / step) * step;
    public static float3 Snap(float3 v, float step) => new(Snap(v.X, step), Snap(v.Y, step), Snap(v.Z, step));

    /// <summary>Próxima potência de dois &gt;= v. Usado por alocadores e atlas.</summary>
    public static int NextPowerOfTwo(int v)
    {
        if (v <= 1) return 1;
        v--; v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16;
        return v + 1;
    }

    /// <summary>Alinha um valor para cima ao múltiplo de <paramref name="alignment"/> (potência de dois).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int AlignUp(int value, int alignment) => (value + alignment - 1) & ~(alignment - 1);
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static nint AlignUp(nint value, nint alignment) => (value + alignment - 1) & ~(alignment - 1);
}
