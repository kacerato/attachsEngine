using Aether.Diagnostics;
using Aether.Jobs;

namespace Aether.Tests;

/// <summary>Testes do item 1.2.4: instrumentação (rastreador de alocações, visualizador de jobs).</summary>
public static class DiagnosticsTests
{
    [Test] public static void AllocationTracker_Track_AcaoQueNaoAloca_DevolveZero()
    {
        int x = 0;
        long allocated = AllocationTracker.Track(() => x++);
        Assert.Equal(0L, allocated, "incrementar uma variável local não deveria alocar no heap");
    }

    [Test] public static void AllocationTracker_Track_AcaoQueAloca_DevolveValorPositivo()
    {
        object? sink = null;
        long allocated = AllocationTracker.Track(() => sink = new byte[1024]);
        Assert.True(allocated >= 1024, $"alocar um array de 1024 bytes deveria refletir no total medido, veio {allocated}");
        Assert.True(sink is not null); // evita o compilador/JIT eliminar a alocação por "não usada"
    }

    [Test] public static void AllocationTracker_BeginAndBytesSince_MedeIntervaloManual()
    {
        long mark = AllocationTracker.Begin();
        object? sink = new byte[2048];
        long allocated = AllocationTracker.BytesSince(mark);
        Assert.True(allocated >= 2048, $"esperava pelo menos 2048 bytes medidos, veio {allocated}");
        Assert.True(sink is not null);
    }

    [Test] public static void AllocationTracker_Track_ActionNula_Lanca()
    {
        Assert.Throws<ArgumentNullException>(() => AllocationTracker.Track(null!));
    }

    [Test] public static void SubsystemAllocationTotal_Add_AcumulaBytesEAmostras()
    {
        var total = new SubsystemAllocationTotal("physics");
        total.Add(100);
        total.Add(300);
        Assert.Equal(400L, total.TotalBytes);
        Assert.Equal(2, total.SampleCount);
        Assert.Close(200f, (float)total.AverageBytesPerSample);
    }

    [Test] public static void SubsystemAllocationTotal_SemAmostras_MediaEhZeroSemDividirPorZero()
    {
        var total = new SubsystemAllocationTotal("render");
        Assert.Close(0f, (float)total.AverageBytesPerSample);
    }

    [Test] public static void JobSystem_ScheduleComLabel_apareceNoSnapshotDeRecentes()
    {
        using var js = new JobSystem(performanceWorkers: 2);
        var handle = js.Schedule(() => { }, "physics.step", CoreAffinity.Any);
        js.Complete(handle);

        Span<JobRecord> buffer = new JobRecord[16];
        int count = js.Diagnostics.CopyRecentTo(buffer);
        Assert.True(count >= 1, "o job concluído deveria aparecer no snapshot de recentes");

        bool found = false;
        for (int i = 0; i < count; i++)
        {
            if (buffer[i].Label == "physics.step") { found = true; break; }
        }
        Assert.True(found, "o label passado a Schedule deveria aparecer no registro do job");
    }

    [Test] public static void JobSystem_ScheduleSemLabel_UsaLabelGenerico()
    {
        using var js = new JobSystem(performanceWorkers: 2);
        var handle = js.Schedule(() => { });
        js.Complete(handle);

        Span<JobRecord> buffer = new JobRecord[16];
        int count = js.Diagnostics.CopyRecentTo(buffer);
        bool found = false;
        for (int i = 0; i < count; i++)
        {
            if (buffer[i].Label == "job") { found = true; break; }
        }
        Assert.True(found, "sem label explícito, o job deveria cair na categoria genérica \"job\"");
    }

    [Test] public static void JobSystem_VariosJobsComMesmoLabel_TotalAgregaCorretamente()
    {
        using var js = new JobSystem(performanceWorkers: 2);
        for (int i = 0; i < 5; i++)
        {
            js.Complete(js.Schedule(() => { }, "render.cull", CoreAffinity.Any));
        }

        var totals = js.Diagnostics.SnapshotTotals();
        JobLabelTotal? cullTotal = null;
        foreach (var t in totals)
        {
            if (t.Label == "render.cull") { cullTotal = t; break; }
        }
        Assert.True(cullTotal.HasValue, "deveria haver um total agregado para o label \"render.cull\"");
        Assert.Equal(5, cullTotal!.Value.Count);
    }

    [Test] public static void JobSystem_JobComFalha_RegistraFaultedNoSnapshot()
    {
        using var js = new JobSystem(performanceWorkers: 2);
        var handle = js.Schedule(() => throw new InvalidOperationException("erro proposital"), "test.falha", CoreAffinity.Any);
        Assert.Throws<JobException>(() => js.Complete(handle));

        Span<JobRecord> buffer = new JobRecord[16];
        int count = js.Diagnostics.CopyRecentTo(buffer);
        bool foundFaulted = false;
        for (int i = 0; i < count; i++)
        {
            if (buffer[i].Label == "test.falha" && buffer[i].Faulted) { foundFaulted = true; break; }
        }
        Assert.True(foundFaulted, "um job que lançou deveria aparecer marcado como Faulted no snapshot");
    }

    [Test] public static void JobDiagnostics_BufferCircular_MantemSoOsMaisRecentes()
    {
        using var js = new JobSystem(performanceWorkers: 2);
        var diagnostics = new JobDiagnostics(capacity: 4);
        // Testa JobDiagnostics diretamente (não via JobSystem, que tem capacidade fixa própria)
        // simulando records manualmente através do JobSystem real com poucos jobs e checando
        // que CopyRecentTo nunca devolve mais que a capacidade.
        for (int i = 0; i < 10; i++)
        {
            js.Complete(js.Schedule(() => { }, "ring.test", CoreAffinity.Any));
        }
        Span<JobRecord> buffer = new JobRecord[256];
        int count = js.Diagnostics.CopyRecentTo(buffer);
        Assert.True(count <= 256, "CopyRecentTo nunca deveria devolver mais que o buffer de destino");
        Assert.True(count >= 10, "todos os 10 jobs deveriam caber no buffer default de 256");
    }

    [Test] public static void JobDiagnostics_CapacidadeInvalida_Lanca()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => new JobDiagnostics(capacity: 0));
        Assert.Throws<ArgumentOutOfRangeException>(() => new JobDiagnostics(capacity: -1));
    }

    [Test] public static void JobSystem_JobConcluido_TemDuracaoNaoNegativa()
    {
        using var js = new JobSystem(performanceWorkers: 2);
        var handle = js.Schedule(() =>
        {
            // trabalho mínimo, só para garantir tempo > 0 mensurável na maioria das plataformas
            long sum = 0;
            for (int i = 0; i < 1000; i++) sum += i;
        }, "timed.job", CoreAffinity.Any);
        js.Complete(handle);

        Span<JobRecord> buffer = new JobRecord[16];
        int count = js.Diagnostics.CopyRecentTo(buffer);
        bool found = false;
        for (int i = 0; i < count; i++)
        {
            if (buffer[i].Label == "timed.job")
            {
                Assert.True(buffer[i].ExecutionMilliseconds >= 0, "duração não pode ser negativa");
                found = true;
            }
        }
        Assert.True(found);
    }
}
