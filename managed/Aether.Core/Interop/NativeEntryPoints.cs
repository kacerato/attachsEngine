using System.Runtime.InteropServices;

namespace Aether.Interop;

/// <summary>
/// Pontos de entrada que o C++ chama do lado gerenciado (item 0.1.4 do plano: "chamar C# do C++
/// e vice-versa"). Cada método aqui precisa ser <c>public static</c> e marcado com
/// <see cref="UnmanagedCallersOnlyAttribute"/> — é o contrato que <c>DotNetHost::getManagedFunctionPointer</c>
/// (native/platform/android/dotnet_host.cpp) exige ao resolver via
/// <c>load_assembly_and_get_function_pointer</c> com <c>UNMANAGEDCALLERSONLY_METHOD</c>.
/// <para>
/// Só tipos blittable cruzam esta fronteira — mesma regra de <c>jolt_bridge.h</c>
/// (docs/CONVENCOES.md §2): sem <c>string</c>, sem marshaling automático, sem exceção
/// atravessando para o C++ (um método aqui que deixar uma exceção escapar derruba o processo
/// nativo inteiro — CoreCLR não tem como devolver isso ao chamador C++ de forma segura).
/// </para>
/// </summary>
public static class NativeEntryPoints
{
    private static readonly Core.Diagnostics.InstancingWorkload Workload = new(5000);
    private static readonly Aether.Scripting.HotReloadHost HotReload = new();
    /// <summary>
    /// Primeira prova de vida do host CoreCLR embutido: soma dois inteiros do lado gerenciado.
    /// Sem efeito colateral de propósito — é a fatia mínima que prova que
    /// hostfxr_initialize_for_runtime_config → hostfxr_get_runtime_delegate →
    /// load_assembly_and_get_function_pointer resolveu este método e o C++ conseguiu chamá-lo
    /// com o resultado certo. Sistemas reais (ECS, física, render) são conectados depois, uma
    /// vez que este caminho mínimo esteja provado — não faz sentido cablear a engine inteira
    /// antes de confirmar que a fronteira em si funciona.
    /// </summary>
    [UnmanagedCallersOnly]
    public static int Ping(int a, int b) => a + b;

    /// <summary>
    /// PoC-A (item 0.2 do plano): "o overhead de interop C#↔Vulkan mata o desempenho?". Preenche
    /// <paramref name="outBuffer"/> — já alocado pelo C++ (ver InstancedRenderer::drawFrame em
    /// android_triangle_renderer.cpp) — com <paramref name="instanceCount"/> registros de 5
    /// floats (posição XY + cor RGB, o layout que instanced.vert espera). A lista inteira
    /// atravessa a fronteira num ÚNICO crossing por frame, nunca um crossing por instância —
    /// exatamente a regra de "chamadas nativas sempre em lote" de docs/CONVENCOES.md §2, e o que
    /// esta PoC existe para provar que não mata o desempenho.
    /// <para>
    /// Animação simples (posição orbital dependente de <paramref name="timeSeconds"/>) para a
    /// medição refletir uma cena que muda a cada frame, não uma lista estática recalculada à
    /// toa — overhead de interop só é interessante medir sob carga de trabalho real, senão o
    /// compilador/JIT poderia teoricamente otimizar o caminho de forma que uma cena parada nunca
    /// exercitaria.
    /// </para>
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe void FillInstanceBuffer(float *outBuffer, int instanceCount, float timeSeconds)
    {
        if (outBuffer == null || instanceCount <= 0) return;

        if (instanceCount == Workload.InstanceCount)
        {
            Workload.Fill(new Span<float>(outBuffer, instanceCount * Core.Diagnostics.InstancingWorkload.FloatsPerInstance), timeSeconds);
            return;
        }

        // Compatibility path for callers with a different count; no unbounded
        // cache or allocation when the workload changes at runtime.
        int gridSize = (int)MathF.Ceiling(MathF.Sqrt(instanceCount));

        for (int i = 0; i < instanceCount; i++)
        {
            // Distribuição determinística em grade normalizada [-0.9, 0.9], com uma oscilação
            // orbital por instância para o quadro mudar a cada chamada — sem alocar (sem LINQ,
            // sem coleção intermediária), lendo/escrevendo direto no ponteiro recebido.
            int row = i / gridSize;
            int col = i % gridSize;
            float baseX = gridSize == 1 ? 0 : (col / (float)(gridSize - 1) - 0.5f) * 1.8f;
            float baseY = gridSize == 1 ? 0 : (row / (float)(gridSize - 1) - 0.5f) * 1.8f;

            float phase = i * 0.017f;
            float orbitRadius = 0.01f;
            float x = baseX + MathF.Cos(timeSeconds * 2f + phase) * orbitRadius;
            float y = baseY + MathF.Sin(timeSeconds * 2f + phase) * orbitRadius;

            int offset = i * 5;
            outBuffer[offset + 0] = x;
            outBuffer[offset + 1] = y;
            outBuffer[offset + 2] = 0.5f + 0.5f * MathF.Sin(phase);
            outBuffer[offset + 3] = 0.5f + 0.5f * MathF.Sin(phase + 2.094f); // +120°
            outBuffer[offset + 4] = 0.5f + 0.5f * MathF.Sin(phase + 4.188f); // +240°
        }
    }

    // ---------------------------------------------------------------- PoC-D: hot reload (item 0.2)

    /// <summary>
    /// Carrega o assembly de script em <paramref name="utf8Path"/> (caminho de arquivo, não bytes
    /// diretos — mais simples de produzir do lado nativo/ferramenta de desenvolvimento, que já tem
    /// o `.dll` recém-compilado em disco no dispositivo) via <see cref="Aether.Scripting.HotReloadHost"/>.
    /// Path como (ponteiro UTF-8, comprimento), nunca <c>string</c> — mesma disciplina de
    /// <c>native/resources/sqlite_bridge.h</c>. Devolve 1 em sucesso, 0 em falha (arquivo ausente,
    /// assembly já carregado, etc.) — nenhuma exceção atravessa esta fronteira.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int HotReload_Load(byte* utf8Path, int pathLength)
    {
        if (utf8Path == null || pathLength <= 0) return 0;
        try
        {
            string path = System.Text.Encoding.UTF8.GetString(utf8Path, pathLength);
            byte[] bytes = File.ReadAllBytes(path);
            HotReload.Load(bytes);
            return 1;
        }
        catch (Exception ex)
        {
            LogHotReloadFailure(ex);
            return 0;
        }
    }

    /// <summary>
    /// Invoca <c>AetherScript.Script.Compute(int)</c> no assembly de script atualmente carregado —
    /// contrato fixo para este PoC (o pipeline de produto do AetherFlow, Fase 5/9, não terá essa
    /// limitação; aqui existe só para medir o ciclo completo load→invoke→unload em hardware real
    /// sem precisar generalizar assinatura de método pela fronteira C ABI). Devolve o resultado, ou
    /// <see cref="int.MinValue"/> como sentinela de falha (nenhum assembly carregado, método
    /// ausente, exceção do próprio script) — escolhido por não ser um resultado plausível dos
    /// scripts de exemplo deste PoC.
    /// </summary>
    [UnmanagedCallersOnly]
    public static int HotReload_InvokeCompute(int x)
    {
        try
        {
            object? result = HotReload.InvokeStaticMethod("AetherScript.Script", "Compute", x);
            return result is int i ? i : int.MinValue;
        }
        catch (Exception ex)
        {
            LogHotReloadFailure(ex);
            return int.MinValue;
        }
    }

    /// <summary>Descarrega o assembly de script atual. Devolve 1 se o <see
    /// cref="System.Runtime.Loader.AssemblyLoadContext"/> foi totalmente coletado após as
    /// tentativas de GC (mesma checagem de <see cref="Aether.Scripting.HotReloadHost.LastContextFullyCollected"/>),
    /// 0 caso contrário — permite ao lado nativo/ferramenta de teste confirmar que o ciclo não
    /// vazou memória, não só que "não lançou".</summary>
    [UnmanagedCallersOnly]
    public static int HotReload_Unload()
    {
        HotReload.Unload();
        return HotReload.LastContextFullyCollected ? 1 : 0;
    }

    /// <summary>Bytes atualmente alocados no heap gerenciado deste processo (<see
    /// cref="GC.GetTotalMemory"/> com coleta forçada) — usado pelo probe do PoC-D (item 0.2) para
    /// medir crescimento de memória ao longo de múltiplos ciclos de reload de forma direta, em vez
    /// de depender só de <see cref="HotReload_Unload"/> confirmar cada contexto individualmente
    /// (que pode ficar temporariamente pendurado sem indicar vazamento real — ver
    /// docs/ESTADO.md, item 0.2/PoC-D, para a medição completa).</summary>
    [UnmanagedCallersOnly]
    public static long HotReload_GetManagedHeapBytes() => GC.GetTotalMemory(forceFullCollection: true);

    private static void LogHotReloadFailure(Exception ex)
    {
        // Nunca deixa a exceção atravessar para o C++ (comentário de classe acima) — loga em
        // stderr, visível via logcat/adb quando este processo roda em Android, para diagnóstico
        // sem derrubar o processo nativo.
        Console.Error.WriteLine($"[HotReload] {ex.GetType().Name}: {ex.Message}");
    }
}
