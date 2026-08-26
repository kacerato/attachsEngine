using System.Runtime.CompilerServices;

namespace Aether;

/// <summary>
/// Transformação afim (translação, rotação, escala não-uniforme).
/// É o componente mais quente da engine: mantido pequeno e blittable (40 bytes).
/// </summary>
public struct Transform : IEquatable<Transform>
{
    public float3 Position;
    public quaternion Rotation;
    public float3 Scale;

    public Transform(float3 position, quaternion rotation, float3 scale)
    { Position = position; Rotation = rotation; Scale = scale; }

    public static readonly Transform Identity = new(float3.Zero, quaternion.Identity, float3.One);

    public static Transform FromPosition(float3 p) => new(p, quaternion.Identity, float3.One);

    public readonly float4x4 ToMatrix() => float4x4.TRS(Position, Rotation, Scale);

    public readonly float3 Right   => Rotation * float3.Right;
    public readonly float3 Up      => Rotation * float3.Up;
    public readonly float3 Forward => Rotation * float3.Forward;

    /// <summary>Compõe: aplica este transform sobre <paramref name="child"/> (pai * filho).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public readonly Transform TransformChild(in Transform child)
    {
        // Translação pura é extremamente comum em hierarquias de cena e animação. Além de evitar
        // multiplicações inúteis, este caminho impede que o JIT ARM64 materialize toda a álgebra
        // escalar de quaternion para uma identidade conhecida. Normalized preserva o contrato para
        // dados externos que tenham escrito um quaternion não unitário no filho.
        if (Rotation == quaternion.Identity)
        {
            if (Scale == float3.One)
                return new Transform(Position + child.Position, child.Rotation.Normalized, child.Scale);
            return new Transform(
                Position + Scale * child.Position,
                child.Rotation.Normalized,
                Scale * child.Scale);
        }

        return new Transform(
            Position + Rotation * (Scale * child.Position),
            (Rotation * child.Rotation).Normalized,
            Scale * child.Scale);
    }

    /// <summary>Inverso. Exato apenas para escala uniforme; para escala não-uniforme use a matriz.</summary>
    public readonly Transform Inverse()
    {
        quaternion invR = Rotation.Inverse;
        float3 invS = new(
            MathF.Abs(Scale.X) > 1e-9f ? 1f / Scale.X : 0f,
            MathF.Abs(Scale.Y) > 1e-9f ? 1f / Scale.Y : 0f,
            MathF.Abs(Scale.Z) > 1e-9f ? 1f / Scale.Z : 0f);
        return new Transform(invS * (invR * -Position), invR, invS);
    }

    public readonly float3 TransformPoint(float3 p) => Position + Rotation * (Scale * p);
    public readonly float3 TransformDirection(float3 d) => Rotation * d;

    public static Transform Lerp(in Transform a, in Transform b, float t) => new(
        math.Lerp(a.Position, b.Position, t),
        quaternion.Slerp(a.Rotation, b.Rotation, t),
        math.Lerp(a.Scale, b.Scale, t));

    public readonly bool Equals(Transform o) => Position == o.Position && Rotation == o.Rotation && Scale == o.Scale;
    public readonly override bool Equals(object? o) => o is Transform t && Equals(t);
    public readonly override int GetHashCode() => HashCode.Combine(Position, Rotation, Scale);
    public static bool operator ==(Transform a, Transform b) => a.Equals(b);
    public static bool operator !=(Transform a, Transform b) => !a.Equals(b);
    public readonly override string ToString() => $"T{Position} R{Rotation} S{Scale}";
}
