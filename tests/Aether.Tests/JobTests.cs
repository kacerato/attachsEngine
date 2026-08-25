using Aether.Jobs;

namespace Aether.Tests;

public static class JobTests
{
    private static JobSystem UmWorker() => new(performanceWorkers: 1, efficiencyWorkers: 0);
    private static JobSystem VariosWorkers() => new(performanceWorkers: 2, efficiencyWorkers: 2);

    [Test] public static void Schedule_NJobsIndependentes_TodosExecutam()
    {
        using var js = VariosWorkers();
        const int n = 64;
        int[] executado = new int[n];
        var handles = new JobHandle[n];
        for (int i = 0; i < n; i++)
        {
            int captured = i;
            handles[i] = js.Schedule(() => Interlocked.Exchange(ref executado[captured], 1));
        }
        foreach (var h in handles) js.Complete(h);

        for (int i = 0; i < n; i++) Assert.Equal(1, executado[i], $"job {i} deveria ter executado");
    }

    [Test] public static void Schedule_CadeiaDeDependencias_ExecutaNaOrdemCerta()
    {
        using var js = VariosWorkers();
        var ordem = new List<int>();
        var trava = new object();

        var h1 = js.Schedule(() => { lock (trava) ordem.Add(1); });
        var h2 = js.Schedule(() => { lock (trava) ordem.Add(2); }, h1);
        var h3 = js.Schedule(() => { lock (trava) ordem.Add(3); }, h2);
        js.Complete(h3);

        Assert.Equal(3, ordem.Count);
        Assert.Equal(1, ordem[0]);
        Assert.Equal(2, ordem[1]);
        Assert.Equal(3, ordem[2]);
    }

    [Test] public static void ScheduleParallel_CobreExatamenteORangeSemSobreporENemDeixarBuraco()
    {
        using var js = VariosWorkers();
        const int count = 997; // proposital: não é múltiplo do batchSize
        const int batchSize = 16;
        int[] contadores = new int[count];

        var h = js.ScheduleParallel(count, batchSize, (start, end) =>
        {
            for (int i = start; i < end; i++) Interlocked.Increment(ref contadores[i]);
        });
        js.Complete(h);

        for (int i = 0; i < count; i++)
            Assert.Equal(1, contadores[i], $"índice {i} precisa ser tocado exatamente uma vez");
    }

    [Test] public static void ScheduleParallel_ComCountZero_NaoFazNada()
    {
        using var js = UmWorker();
        bool chamou = false;
        var h = js.ScheduleParallel(0, 8, (_, _) => chamou = true);
        js.Complete(h);
        Assert.False(chamou, "count=0 não deveria disparar nenhum lote");
    }

    [Test] public static void Complete_HandleDefault_NaoFazNada()
    {
        using var js = UmWorker();
        js.Complete(default); // não pode lançar nem travar
    }

    [Test] public static void Complete_RelancaExcecaoDoJobComContexto()
    {
        using var js = VariosWorkers();
        var h = js.Schedule(() => throw new InvalidOperationException("falha proposital"));
        var lancou = false;
        try { js.Complete(h); }
        catch (JobException je)
        {
            lancou = true;
            Assert.True(je.InnerException is InvalidOperationException, "a exceção original vem embrulhada como InnerException");
        }
        Assert.True(lancou, "Complete precisa relançar a falha do job, não engolir");
    }

    [Test] public static void JobSystem_FuncionaComUmUnicoWorker()
    {
        using var js = UmWorker();
        int total = 0;
        var h1 = js.Schedule(() => Interlocked.Increment(ref total));
        var h2 = js.Schedule(() => Interlocked.Increment(ref total), h1);
        js.Complete(h2);
        Assert.Equal(2, total);
    }

    [Test] public static void StressDeDezMilJobsPequenos()
    {
        using var js = VariosWorkers();
        const int n = 10_000;
        long soma = 0;
        var handles = new JobHandle[n];
        for (int i = 0; i < n; i++)
        {
            int v = i;
            handles[i] = js.Schedule(() => Interlocked.Add(ref soma, v));
        }
        foreach (var h in handles) js.Complete(h);

        long esperado = (long)(n - 1) * n / 2;
        Assert.Equal(esperado, soma);
    }

    [Test] public static void CombineDependencies_JuncaoSoConcluiQuandoTodasTerminam()
    {
        using var js = VariosWorkers();
        int contador = 0;
        var h1 = js.Schedule(() => { Thread.Sleep(10); Interlocked.Increment(ref contador); });
        var h2 = js.Schedule(() => { Thread.Sleep(30); Interlocked.Increment(ref contador); });
        var h3 = js.Schedule(() => Interlocked.Increment(ref contador));

        var combinado = JobHandle.CombineDependencies(h1, h2, h3);
        js.Complete(combinado);

        Assert.Equal(3, contador, "junção só conclui depois que as três dependências rodaram");
    }

    [Test] public static void Complete_DentroDoProprioJob_DetectaCicloEmVezDeTravar()
    {
        using var js = UmWorker();
        JobHandle h = default;
        var jobConcluiu = false;
        h = js.Schedule(() =>
        {
            // Esperar pelo próprio handle de dentro da execução do job é um ciclo:
            // ele nunca vai "terminar" enquanto está esperando por si mesmo.
            js.Complete(h);
            jobConcluiu = true;
        });

        var lancou = false;
        try { js.Complete(h); }
        catch (JobException je) when (je.InnerException is InvalidOperationException)
        {
            lancou = true;
        }
        Assert.True(lancou, "ciclo de dependência precisa lançar, não travar o runner");
        Assert.False(jobConcluiu, "a linha depois do auto-Complete nunca deveria rodar");
    }
}
