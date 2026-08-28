using System.Runtime.InteropServices;
using Aether.Rendering.Diagnostics;

namespace Aether.Rendering.Interop;

/// <summary>Fronteira da fixture Android. Inicialização exclusiva pelo worker; após join,
/// uso exclusivo pela thread nativa de render/eventos. Não admite chamadas concorrentes.
/// Recriar surface não reinicia o World; Shutdown encerra somente a sessão da cena.</summary>
public static class SceneEntryPoints
{
    private static ScenePreview? _preview;

    [UnmanagedCallersOnly]
    public static int Initialize()
    {
        try
        {
            if (_preview?.IsMaterialPreview == true) return (int)RenderExtractionStatus.InvalidArgument;
            _preview ??= new ScenePreview(); return 0;
        }
        catch (Exception error) { Console.Error.WriteLine($"[Scene] Inicialização: {error}"); return (int)RenderExtractionStatus.InternalError; }
    }

    [UnmanagedCallersOnly]
    public static int InitializeMaterialPreview()
    {
        try
        {
            if (_preview is not null && !_preview.IsMaterialPreview) return (int)RenderExtractionStatus.InvalidArgument;
            _preview ??= new ScenePreview(materialPreview: true); return 0;
        }
        catch (Exception error) { Console.Error.WriteLine($"[Scene] Material preview: {error}"); return (int)RenderExtractionStatus.InternalError; }
    }

    [UnmanagedCallersOnly]
    public static unsafe int Extract(RenderInstance* output, int capacity, int stride, int version)
    {
        if (capacity < 0 || (output == null && capacity != 0) || stride != RenderInstance.Stride || version != RenderInstance.AbiVersion)
            return (int)RenderExtractionStatus.InvalidArgument;
        if (_preview is null) return (int)RenderExtractionStatus.NotInitialized;
        try
        {
            var status = _preview.Extractor.Extract(new Span<RenderInstance>(output, capacity), out int count);
            return status == RenderExtractionStatus.Ok ? count : (int)status;
        }
        catch (Exception error) { Console.Error.WriteLine($"[Scene] Extração: {error}"); return (int)RenderExtractionStatus.InternalError; }
    }

    [UnmanagedCallersOnly]
    public static int ApplyValidationStep(int step)
    {
        if (_preview is null) return (int)RenderExtractionStatus.NotInitialized;
        try { _preview.ApplyValidationStep(step); return 0; }
        catch (Exception error) { Console.Error.WriteLine($"[Scene] Mutação: {error}"); return (int)RenderExtractionStatus.InternalError; }
    }

    [UnmanagedCallersOnly]
    public static void Shutdown() => _preview = null;

    [UnmanagedCallersOnly]
    public static unsafe int GetMaterialParameters(PbrMaterialParameters* output, int size)
    {
        if (output == null || size != sizeof(PbrMaterialParameters)) return (int)RenderExtractionStatus.InvalidArgument;
        if (_preview?.IsMaterialPreview != true) return (int)RenderExtractionStatus.NotInitialized;
        try { *output = PbrMaterial.MetalPlate.Parameters; return 0; }
        catch (Exception error) { Console.Error.WriteLine($"[Scene] Parâmetros do material: {error}"); return (int)RenderExtractionStatus.InternalError; }
    }
}
