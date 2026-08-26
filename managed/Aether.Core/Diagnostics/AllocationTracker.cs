namespace Aether.Diagnostics;

/// <summary>
/// Item 1.2.4 do plano ("rastreador de alocações com atribuição por subsistema"). Mede bytes
/// alocados no heap gerenciado — mesma API de base que <c>Assert.NoAlloc</c> já usa
/// (<see cref="GC.GetAllocatedBytesForCurrentThread"/>) — mas acumulando por rótulo de
/// subsistema (ex.: <c>"physics"</c>, <c>"render"</c>, <c>"ecs"</c>) em vez de só comparar contra
/// zero num teste isolado. Existe para responder "quem está alocando?" quando o orçamento de
/// zero-alloc por frame (docs/CONVENCOES.md §3) é violado em produção, não só em teste.
/// <para>
/// <b>Por que thread-local, não um dicionário global travado:</b> <see cref="GetAllocatedBytesForCurrentThread"/>
/// só mede a THREAD CHAMADORA — não existe uma versão "para toda thread" na BCL. Um
/// <see cref="Track"/> teria que agregar entre threads via alguma trava/atômico se quisesse
/// somar workers diferentes, o que este tipo delega para o CHAMADOR (ex.: <c>JobSystem</c>
/// registrando o total por job já rodado até o fim naquela thread, ver <c>JobDiagnostics</c>) —
/// medir e agregar são responsabilidades separadas de propósito.
/// </para>
/// </summary>
public static class AllocationTracker
{
    /// <summary>
    /// Executa <paramref name="action"/> e devolve quantos bytes o heap gerenciado cresceu NESTA
    /// THREAD durante a chamada — não conta alocação de outras threads rodando em paralelo, nem
    /// GC de outra geração rodando concorrentemente (mesma limitação documentada da API
    /// subjacente). Não força coleta antes/depois (diferente de <c>Assert.NoAlloc</c>, que já
    /// aquece e força GC antes de medir para eliminar ruído de warm-up de um teste isolado) —
    /// aqui o objetivo é medir o custo real de produção, incluindo qualquer alocação de
    /// primeira-chamada, não um número artificialmente baixo por causa de aquecimento prévio.
    /// </summary>
    public static long Track(Action action)
    {
        ArgumentNullException.ThrowIfNull(action);
        long before = GC.GetAllocatedBytesForCurrentThread();
        action();
        return GC.GetAllocatedBytesForCurrentThread() - before;
    }

    /// <summary>Início de uma medição manual, para o caso em que o trabalho medido não cabe
    /// dentro de um único <see cref="Action"/> (ex.: um job cujo início/fim são callbacks
    /// separados de um framework externo — o caso real de <c>JobSystem.RunEntry</c>). Combine
    /// com <see cref="BytesSince"/>.</summary>
    public static long Begin() => GC.GetAllocatedBytesForCurrentThread();

    /// <summary>Bytes alocados NESTA THREAD desde o valor devolvido por <see cref="Begin"/>.</summary>
    public static long BytesSince(long beginMark) => GC.GetAllocatedBytesForCurrentThread() - beginMark;
}

/// <summary>Total acumulado de bytes alocados atribuídos a um subsistema nomeado — a metade
/// "agregação" que <see cref="AllocationTracker"/> deixa para o chamador (ver comentário de
/// classe). <c>struct</c> mutável simples: quem quiser agregação thread-safe entre múltiplas
/// threads escrevendo o MESMO <see cref="SubsystemAllocationTotal"/> deve envolver com
/// <see cref="AtomicCounter"/> (ex.: um <c>Dictionary&lt;string, AtomicCounter&gt;</c> por
/// subsystem) — este tipo não impõe essa decisão, só carrega o par nome+total.</summary>
public struct SubsystemAllocationTotal(string subsystem)
{
    public readonly string Subsystem = subsystem;
    public long TotalBytes;
    public int SampleCount;

    public void Add(long bytes)
    {
        TotalBytes += bytes;
        SampleCount++;
    }

    public readonly double AverageBytesPerSample => SampleCount == 0 ? 0.0 : (double)TotalBytes / SampleCount;
}
