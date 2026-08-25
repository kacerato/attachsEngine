namespace Aether;

/// <summary>Caixa alinhada aos eixos.</summary>
public struct Bounds : IEquatable<Bounds>
{
    public float3 Min, Max;

    public Bounds(float3 min, float3 max) { Min = min; Max = max; }

    /// <summary>Bounds vazio: qualquer <see cref="Encapsulate(float3)"/> o torna válido.</summary>
    public static readonly Bounds Empty = new(new float3(float.PositiveInfinity), new float3(float.NegativeInfinity));

    public static Bounds FromCenterExtents(float3 center, float3 extents) => new(center - extents, center + extents);

    public readonly float3 Center  => (Min + Max) * 0.5f;
    public readonly float3 Size    => Max - Min;
    public readonly float3 Extents => (Max - Min) * 0.5f;
    public readonly bool IsValid   => Min.X <= Max.X && Min.Y <= Max.Y && Min.Z <= Max.Z;
    public readonly float Radius   => Extents.Length;

    public void Encapsulate(float3 p) { Min = math.Min(Min, p); Max = math.Max(Max, p); }
    public void Encapsulate(in Bounds b) { Min = math.Min(Min, b.Min); Max = math.Max(Max, b.Max); }
    public void Expand(float amount) { float3 e = new(amount); Min -= e; Max += e; }

    public readonly bool Contains(float3 p) =>
        p.X >= Min.X && p.X <= Max.X && p.Y >= Min.Y && p.Y <= Max.Y && p.Z >= Min.Z && p.Z <= Max.Z;

    public readonly bool Intersects(in Bounds b) =>
        Min.X <= b.Max.X && Max.X >= b.Min.X &&
        Min.Y <= b.Max.Y && Max.Y >= b.Min.Y &&
        Min.Z <= b.Max.Z && Max.Z >= b.Min.Z;

    /// <summary>AABB do bounds transformado (método dos eixos absolutos — correto e barato).</summary>
    public readonly Bounds Transform(in float4x4 m)
    {
        if (!IsValid) return Empty;
        float3 c = Center, e = Extents;
        float3 nc = m.TransformPoint(c);
        float3 ne = new(
            MathF.Abs(m.C0.X) * e.X + MathF.Abs(m.C1.X) * e.Y + MathF.Abs(m.C2.X) * e.Z,
            MathF.Abs(m.C0.Y) * e.X + MathF.Abs(m.C1.Y) * e.Y + MathF.Abs(m.C2.Y) * e.Z,
            MathF.Abs(m.C0.Z) * e.X + MathF.Abs(m.C1.Z) * e.Y + MathF.Abs(m.C2.Z) * e.Z);
        return FromCenterExtents(nc, ne);
    }

    public readonly bool Equals(Bounds o) => Min == o.Min && Max == o.Max;
    public readonly override bool Equals(object? o) => o is Bounds b && Equals(b);
    public readonly override int GetHashCode() => HashCode.Combine(Min, Max);
    public static bool operator ==(Bounds a, Bounds b) => a.Equals(b);
    public static bool operator !=(Bounds a, Bounds b) => !a.Equals(b);
    public readonly override string ToString() => $"Bounds({Min} .. {Max})";
}

/// <summary>Raio com origem e direção normalizada. Base de todo picking por toque.</summary>
public readonly struct Ray
{
    public readonly float3 Origin;
    public readonly float3 Direction;

    public Ray(float3 origin, float3 direction) { Origin = origin; Direction = direction.Normalized; }

    public float3 At(float t) => Origin + Direction * t;

    /// <summary>Interseção slab com AABB. <paramref name="tMin"/> é a distância de entrada.</summary>
    public bool Intersects(in Bounds b, out float tMin)
    {
        float t0 = 0f, t1 = float.PositiveInfinity;
        for (int i = 0; i < 3; i++)
        {
            float d = Direction[i];
            float o = Origin[i];
            float bmin = b.Min[i], bmax = b.Max[i];
            if (MathF.Abs(d) < 1e-9f)
            {
                if (o < bmin || o > bmax) { tMin = 0f; return false; }
                continue;
            }
            float inv = 1f / d;
            float ta = (bmin - o) * inv, tb = (bmax - o) * inv;
            if (ta > tb) (ta, tb) = (tb, ta);
            t0 = MathF.Max(t0, ta);
            t1 = MathF.Min(t1, tb);
            if (t0 > t1) { tMin = 0f; return false; }
        }
        tMin = t0;
        return true;
    }

    /// <summary>Interseção com plano infinito. Usada pelos gizmos de arrasto em plano.</summary>
    public bool Intersects(in Plane p, out float t)
    {
        float denom = math.Dot(p.Normal, Direction);
        if (MathF.Abs(denom) < 1e-9f) { t = 0f; return false; }
        t = -(math.Dot(p.Normal, Origin) + p.Distance) / denom;
        return t >= 0f;
    }

    /// <summary>
    /// Ponto do raio mais próximo de um eixo (origem + direção). Base do gizmo de translação
    /// travada em eixo: converte o arrasto do dedo em deslocamento ao longo do eixo.
    /// </summary>
    public float ClosestPointOnAxis(float3 axisOrigin, float3 axisDir)
    {
        float3 w0 = axisOrigin - Origin;
        float a = math.Dot(axisDir, axisDir);
        float b = math.Dot(axisDir, Direction);
        float c = math.Dot(Direction, Direction);
        float d = math.Dot(axisDir, w0);
        float e = math.Dot(Direction, w0);
        float denom = a * c - b * b;
        if (MathF.Abs(denom) < 1e-9f) return 0f;   // paralelos
        return (b * e - c * d) / denom;
    }

    public override string ToString() => $"Ray({Origin} → {Direction})";
}

/// <summary>Plano na forma Normal·X + Distance = 0, com normal unitária.</summary>
public readonly struct Plane
{
    public readonly float3 Normal;
    public readonly float Distance;

    public Plane(float3 normal, float distance)
    {
        float len = normal.Length;
        if (len > 1e-12f) { Normal = normal * (1f / len); Distance = distance / len; }
        else { Normal = float3.Up; Distance = 0f; }
    }

    public Plane(float3 normal, float3 point) : this(normal, 0f)
        => Distance = -math.Dot(Normal, point);

    public float SignedDistance(float3 p) => math.Dot(Normal, p) + Distance;
}

/// <summary>Frustum de 6 planos com normais apontando para dentro. Base do culling.</summary>
public struct Frustum
{
    public Plane P0, P1, P2, P3, P4, P5;   // esquerda, direita, baixo, cima, perto, longe

    /// <summary>Extrai os planos de uma matriz view-projection (método de Gribb-Hartmann).</summary>
    public static Frustum FromViewProjection(in float4x4 vp)
    {
        // Linhas da matriz (a matriz é column-major, então as linhas são componentes das colunas).
        float4 r0 = new(vp.C0.X, vp.C1.X, vp.C2.X, vp.C3.X);
        float4 r1 = new(vp.C0.Y, vp.C1.Y, vp.C2.Y, vp.C3.Y);
        float4 r2 = new(vp.C0.Z, vp.C1.Z, vp.C2.Z, vp.C3.Z);
        float4 r3 = new(vp.C0.W, vp.C1.W, vp.C2.W, vp.C3.W);

        static Plane Make(float4 v) => new(new float3(v.X, v.Y, v.Z), v.W);

        return new Frustum
        {
            P0 = Make(r3 + r0),   // esquerda
            P1 = Make(r3 - r0),   // direita
            P2 = Make(r3 + r1),   // baixo
            P3 = Make(r3 - r1),   // cima
            P4 = Make(r2),        // perto  (Vulkan: z em [0,1])
            P5 = Make(r3 - r2),   // longe
        };
    }

    /// <summary>Teste conservador: falso apenas se o bounds estiver totalmente fora.</summary>
    public readonly bool Intersects(in Bounds b)
    {
        float3 c = b.Center, e = b.Extents;
        return Test(P0, c, e) && Test(P1, c, e) && Test(P2, c, e) && Test(P3, c, e) && Test(P4, c, e) && Test(P5, c, e);

        static bool Test(in Plane p, float3 c, float3 e)
        {
            float r = e.X * MathF.Abs(p.Normal.X) + e.Y * MathF.Abs(p.Normal.Y) + e.Z * MathF.Abs(p.Normal.Z);
            return p.SignedDistance(c) >= -r;
        }
    }
}
