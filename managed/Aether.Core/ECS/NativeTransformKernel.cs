using System.Runtime.InteropServices;

namespace Aether;

/// <summary>Backend efetivamente usado pela última propagação de transforms.</summary>
public enum TransformPropagationBackend
{
    Managed,
    NativeBatch,
}

/// <summary>
/// Fachada da única chamada nativa em lote usada pelo sistema de transforms. A indisponibilidade
/// da biblioteca é detectada uma vez e não altera a API nem impede o fallback gerenciado.
/// </summary>
internal static partial class NativeTransformKernel
{
    private const string LibraryName = "aether_transform";
    private static int s_availability; // 0 desconhecido, 1 disponível, -1 indisponível

    internal static TransformPropagationBackend ActiveBackend { get; private set; }

    internal static unsafe bool TryPropagate(NativeTransformPlanEntry[] entries, int count)
    {
        if (count == 0)
        {
            ActiveBackend = TransformPropagationBackend.Managed;
            return false;
        }
        if (s_availability < 0)
        {
            ActiveBackend = TransformPropagationBackend.Managed;
            return false;
        }

        try
        {
            fixed (NativeTransformPlanEntry* entryPointer = entries)
            {
                if (PropagatePlanNative(entryPointer, count) == 0)
                    throw new InvalidOperationException("O kernel nativo rejeitou o plano topológico de transforms.");
            }
            s_availability = 1;
            ActiveBackend = TransformPropagationBackend.NativeBatch;
            return true;
        }
        catch (DllNotFoundException)
        {
            s_availability = -1;
        }
        catch (EntryPointNotFoundException)
        {
            s_availability = -1;
        }
        catch (BadImageFormatException)
        {
            s_availability = -1;
        }

        ActiveBackend = TransformPropagationBackend.Managed;
        return false;
    }

    [LibraryImport(LibraryName, EntryPoint = "AetherTransform_PropagatePlan")]
    private static unsafe partial int PropagatePlanNative(NativeTransformPlanEntry* entries, int count);
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeTransformPlanEntry(nint local, nint world, int parent)
{
    internal readonly nint Local = local;
    internal readonly nint World = world;
    internal readonly int Parent = parent;
    private readonly int _reserved = 0;
}
