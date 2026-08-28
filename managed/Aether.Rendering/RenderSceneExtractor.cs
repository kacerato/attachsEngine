using System.Runtime.InteropServices;
using Aether.Resources;

namespace Aether.Rendering;

/// <summary>ABI v1: matriz column-major, tint RGBA e identificação transitória da entidade.
/// O buffer pertence ao chamador; não é retido nem serializado pelo renderer.</summary>
[StructLayout(LayoutKind.Sequential, Pack = 4)]
public struct RenderInstance
{
    public float4x4 Model;
    public float4 Tint;
    public EntityId Entity;
    public const int Stride = 88;
    public const int AbiVersion = 1;
}

public enum RenderExtractionStatus
{
    Ok = 0, BufferTooSmall = -1, ResourceUnavailable = -2, InvalidTransform = -3,
    InvalidTint = -4, InvalidArgument = -5, NotInitialized = -6, InternalError = -7
}

/// <summary>Extrai um lote de malha/material já disponíveis no backend. A primeira fatia
/// aceita um par por cena: outro recurso produz erro explícito, nunca desenha a malha errada.
/// O ECS deve ser acessado pela mesma thread, sem mudanças estruturais durante Extract.
/// Nenhum snapshot gerenciado é alocado; a validação antecede qualquer escrita no destino.</summary>
public sealed class RenderSceneExtractor
{
    private readonly World _world;
    private readonly CompiledQuery _query;
    private readonly ResourceId _mesh;
    private readonly ResourceId _material;
    private readonly bool _requireInvertibleTransform;

    public RenderSceneExtractor(World world, ResourceId mesh, ResourceId material, bool requireInvertibleTransform = false)
    {
        ArgumentNullException.ThrowIfNull(world);
        if (mesh.IsEmpty || material.IsEmpty) throw new ArgumentException("Recursos do lote devem ter identidade válida.");
        _world = world; _mesh = mesh; _material = material;
        _requireInvertibleTransform = requireInvertibleTransform;
        _query = world.Query().With<MeshRenderer>().Compile();
    }

    public RenderExtractionStatus Extract(Span<RenderInstance> destination, out int count)
    {
        count = 0;
        foreach (var chunk in _query)
        {
            var renderers = chunk.GetReadOnlySpan<MeshRenderer>();
            for (int i = 0; i < chunk.Count; ++i)
            {
                ref readonly var renderer = ref renderers[i];
                if (!renderer.Enabled) continue;
                if (renderer.Mesh != _mesh || renderer.Material != _material) return RenderExtractionStatus.ResourceUnavailable;
                if (!TransformSystem.TryGetWorldMatrix(_world, chunk.Entities[i], out var model) ||
                    !Finite(model.C0) || !Finite(model.C1) || !Finite(model.C2) || !Finite(model.C3))
                    return RenderExtractionStatus.InvalidTransform;
                if (_requireInvertibleTransform && !Invertible(model)) return RenderExtractionStatus.InvalidTransform;
                if (!Finite(renderer.Tint) || renderer.Tint.W != 1f) return RenderExtractionStatus.InvalidTint;
                ++count;
            }
        }
        if (count > destination.Length) return RenderExtractionStatus.BufferTooSmall;
        int index = 0;
        foreach (var chunk in _query)
        {
            var renderers = chunk.GetReadOnlySpan<MeshRenderer>();
            for (int i = 0; i < chunk.Count; ++i)
            {
                if (!renderers[i].Enabled) continue;
                TransformSystem.TryGetWorldMatrix(_world, chunk.Entities[i], out var model);
                destination[index++] = new RenderInstance { Model = model, Tint = renderers[i].Tint, Entity = chunk.Entities[i] };
            }
        }
        return RenderExtractionStatus.Ok;
    }

    private static bool Finite(float4 v) => float.IsFinite(v.X) && float.IsFinite(v.Y) && float.IsFinite(v.Z) && float.IsFinite(v.W);
    private static bool Invertible(float4x4 m)
    {
        float determinant=m.C0.X*(m.C1.Y*m.C2.Z-m.C2.Y*m.C1.Z) -
            m.C1.X*(m.C0.Y*m.C2.Z-m.C2.Y*m.C0.Z)+m.C2.X*(m.C0.Y*m.C1.Z-m.C1.Y*m.C0.Z);
        return float.IsFinite(determinant) && MathF.Abs(determinant)>1e-8f;
    }
}
