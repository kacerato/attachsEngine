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

        for (int i = 0; i < instanceCount; i++)
        {
            // Distribuição determinística em grade normalizada [-0.9, 0.9], com uma oscilação
            // orbital por instância para o quadro mudar a cada chamada — sem alocar (sem LINQ,
            // sem coleção intermediária), lendo/escrevendo direto no ponteiro recebido.
            int gridSize = (int)MathF.Ceiling(MathF.Sqrt(instanceCount));
            int row = i / gridSize;
            int col = i % gridSize;
            float baseX = (col / (float)(gridSize - 1) - 0.5f) * 1.8f;
            float baseY = (row / (float)(gridSize - 1) - 0.5f) * 1.8f;

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
}
