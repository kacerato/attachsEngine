using Aether.Jobs;
using Aether.Resources;

namespace Aether.Tests;

/// <summary>
/// Sistema de recursos (Etapa 1.4.3 do plano): GUID estável, referência forte/fraca, contagem de
/// uso e carregamento assíncrono via <see cref="JobSystem"/>.
/// </summary>
public static class ResourceTests
{
    private sealed class RecursoFalso
    {
        public int Valor;
    }

    /// <summary>Carregador fictício: nenhum I/O de verdade, só o suficiente para exercitar o
    /// caminho assíncrono (pode ser configurado para bloquear até um "portão" ser liberado, para
    /// provar que o carregamento roda em background sem depender de sorte de timing).</summary>
    private sealed class LoaderFalso : IResourceLoader<RecursoFalso>
    {
        private readonly Func<ResourceId, RecursoFalso> _fn;
        public LoaderFalso(Func<ResourceId, RecursoFalso> fn) => _fn = fn;
        public RecursoFalso Load(ResourceId id) => _fn(id);
    }

    private static JobSystem NovoJobSystem() => new(performanceWorkers: 2, efficiencyWorkers: 0);

    // ---------- ResourceId ----------

    [Test] public static void ResourceId_New_GeraIdsDiferentesENaoVazios()
    {
        var a = ResourceId.New();
        var b = ResourceId.New();
        Assert.NotEqual(a, b, "dois New() não podem colidir");
        Assert.False(a.IsEmpty, "um id recém-gerado não é vazio");
    }

    [Test] public static void ResourceId_Empty_EhODefaultEIgualANone()
    {
        Assert.True(default(ResourceId).IsEmpty, "default(ResourceId) é o id vazio");
        Assert.Equal(ResourceId.Empty, ResourceId.None, "Empty e None são o mesmo valor");
        Assert.True(ResourceId.Empty.IsEmpty);
    }

    // ---------- ResourceHandleTable: contagem de uso ----------

    [Test] public static void ResourceHandleTable_AddRefERelease_ContagemSobeEDesce()
    {
        var table = new ResourceHandleTable();
        var id = ResourceId.New();
        Assert.Equal(0, table.GetRefCount(id), "recurso nunca tocado começa com contagem zero");

        table.AddRef(id);
        table.AddRef(id);
        Assert.Equal(2, table.GetRefCount(id));

        table.Release(id);
        Assert.Equal(1, table.GetRefCount(id));
    }

    [Test] public static void ResourceHandleTable_Release_AlemDaContagemConcedida_Lanca()
    {
        var table = new ResourceHandleTable();
        var id = ResourceId.New();
        table.AddRef(id);
        table.Release(id);

        Assert.Throws<InvalidOperationException>(() => table.Release(id),
            "liberar mais vezes do que reservou é um bug de posse (double free lógico) e precisa ser detectado, não mentir silenciosamente");
    }

    [Test] public static void ResourceHandleTable_Release_IdNuncaRegistrado_Lanca()
    {
        var table = new ResourceHandleTable();
        Assert.Throws<InvalidOperationException>(() => table.Release(ResourceId.New()));
    }

    [Test] public static void ResourceHandleTable_TryGet_RecursoNuncaRegistrado_DevolveFalse()
    {
        var table = new ResourceHandleTable();
        bool achou = table.TryGet(ResourceId.New(), out var valor, out var estado);
        Assert.False(achou);
        Assert.Equal(ResourceState.NotLoaded, estado);
        Assert.True(valor is null);
    }

    [Test] public static void ResourceHandleTable_RecursoComContagemZero_FicaElegivelParaDescarregar()
    {
        using var jobs = NovoJobSystem();
        var manager = new ResourceManager(new ResourceHandleTable(), jobs);
        var id = ResourceId.New();

        var (referencia, handle) = manager.LoadAsync(id, new LoaderFalso(_ => new RecursoFalso { Valor = 42 }));
        jobs.Complete(handle);

        Assert.Equal(ResourceState.Loaded, manager.Table.GetState(id));
        Assert.Equal(1, manager.Table.GetRefCount(id));
        Assert.False(manager.Table.IsEligibleForUnload(id), "ainda existe uma referência forte viva");

        referencia.Dispose();

        Assert.Equal(0, manager.Table.GetRefCount(id));
        Assert.True(manager.Table.IsEligibleForUnload(id),
            "contagem chegou a zero e o recurso está carregado: elegível para descarregar (o descarregamento em si é de uma fase futura)");
    }

    [Test] public static void ResourceHandleTable_TryGet_CaminhoQuenteNaoAloca()
    {
        var table = new ResourceHandleTable();
        var id = ResourceId.New();
        using var jobs = NovoJobSystem();
        var manager = new ResourceManager(table, jobs);
        var (referencia, handle) = manager.LoadAsync(id, new LoaderFalso(_ => new RecursoFalso()));
        jobs.Complete(handle);

        Assert.NoAlloc(() =>
        {
            for (int i = 0; i < 200; i++) table.TryGet(id, out _, out _);
        }, "resolver um recurso já carregado é o caminho quente de todo sistema que usa esse recurso a cada frame");

        referencia.Dispose();
    }

    // ---------- ResourceRef<T>: posse forte ----------

    [Test] public static void ResourceRef_Dispose_LiberaAPosseNaTabela()
    {
        var table = new ResourceHandleTable();
        var id = ResourceId.New();
        var referencia = table.AcquireStrong<RecursoFalso>(id);
        Assert.Equal(1, table.GetRefCount(id));

        referencia.Dispose();
        Assert.Equal(0, table.GetRefCount(id));
    }

    [Test] public static void ResourceRef_AddRef_CriaPosseIndependenteDaOriginal()
    {
        var table = new ResourceHandleTable();
        var id = ResourceId.New();
        var r1 = table.AcquireStrong<RecursoFalso>(id);
        var r2 = r1.AddRef();
        Assert.Equal(2, table.GetRefCount(id));

        r1.Dispose();
        Assert.Equal(1, table.GetRefCount(id), "descartar uma posse não afeta a outra, independente");
        r2.Dispose();
        Assert.Equal(0, table.GetRefCount(id));
    }

    [Test] public static void ResourceRef_CopiaDeStruct_NaoIncrementaContagem()
    {
        // Documenta a decisão de design de ResourceRef<T>: é um struct copiável representando UMA
        // posse. Copiar por atribuição não chama AddRef — a cópia é só mais um "recibo" da mesma
        // posse, não uma posse nova. Só AddRef() cria uma posse de verdade.
        var table = new ResourceHandleTable();
        var id = ResourceId.New();
        var original = table.AcquireStrong<RecursoFalso>(id);
        var copia = original; // cópia de struct — não incrementa
        Assert.Equal(1, table.GetRefCount(id), "copiar o struct não duplica a posse");

        copia.Dispose();
        Assert.Equal(0, table.GetRefCount(id), "a única posse existente já foi liberada pela cópia");
        Assert.Throws<InvalidOperationException>(() => original.Dispose(),
            "'original' é a mesma posse que 'copia' já liberou — descartar de novo é double free lógico");
    }

    [Test] public static void ResourceRef_TryGetValue_AntesDeCarregarDevolveFalse_DepoisDevolveOValor()
    {
        using var jobs = NovoJobSystem();
        var manager = new ResourceManager(new ResourceHandleTable(), jobs);
        var id = ResourceId.New();
        using var portao = new ManualResetEventSlim(false);

        var (referencia, handle) = manager.LoadAsync(id, new LoaderFalso(_ => { portao.Wait(); return new RecursoFalso { Valor = 7 }; }));
        Assert.False(referencia.TryGetValue(out _), "ainda carregando: não há valor pronto ainda");

        portao.Set();
        jobs.Complete(handle);

        Assert.True(referencia.TryGetValue(out var valor));
        Assert.Equal(7, valor.Valor);
        referencia.Dispose();
    }

    // ---------- WeakResourceRef<T> ----------

    [Test] public static void WeakResourceRef_TryResolve_RecursoNuncaCarregado_DevolveFalse()
    {
        var table = new ResourceHandleTable();
        var fraca = table.CreateWeak<RecursoFalso>(ResourceId.New());
        Assert.False(fraca.TryResolve(out _), "id nunca carregado: não há nada para resolver");
    }

    [Test] public static void WeakResourceRef_TryResolve_EnquantoAindaCarregando_DevolveFalse()
    {
        using var jobs = NovoJobSystem();
        var manager = new ResourceManager(new ResourceHandleTable(), jobs);
        var id = ResourceId.New();
        using var portao = new ManualResetEventSlim(false);

        var (forte, handle) = manager.LoadAsync(id, new LoaderFalso(_ => { portao.Wait(); return new RecursoFalso(); }));
        var fraca = manager.Table.CreateWeak<RecursoFalso>(id);
        Assert.False(fraca.TryResolve(out _), "estado ainda é Loading: fraca não resolve antes de Loaded");

        portao.Set();
        jobs.Complete(handle);
        forte.Dispose();
    }

    [Test] public static void WeakResourceRef_TryResolve_RecursoCarregado_DevolveReferenciaFortePropria()
    {
        using var jobs = NovoJobSystem();
        var manager = new ResourceManager(new ResourceHandleTable(), jobs);
        var id = ResourceId.New();
        var (forte, handle) = manager.LoadAsync(id, new LoaderFalso(_ => new RecursoFalso { Valor = 9 }));
        jobs.Complete(handle);

        var fraca = forte.ToWeak();
        Assert.True(fraca.TryResolve(out var forte2), "recurso carregado: a fraca deve resolver");
        Assert.Equal(2, manager.Table.GetRefCount(id), "resolver a fraca concede uma posse forte de verdade, contada");
        Assert.True(forte2.TryGetValue(out var valor));
        Assert.Equal(9, valor.Valor);

        forte.Dispose();
        forte2.Dispose();
        Assert.Equal(0, manager.Table.GetRefCount(id));
    }

    // ---------- ResourceManager: carregamento assíncrono ----------

    [Test] public static void ResourceManager_LoadAsync_NaoBloqueiaAChamadoraEnquantoCarrega()
    {
        using var jobs = NovoJobSystem();
        var manager = new ResourceManager(new ResourceHandleTable(), jobs);
        var id = ResourceId.New();
        using var portao = new ManualResetEventSlim(false);

        var (referencia, handle) = manager.LoadAsync(id, new LoaderFalso(_ => { portao.Wait(); return new RecursoFalso(); }));

        // Se LoadAsync tivesse bloqueado até o carregador terminar, chegar até aqui seria
        // impossível com o portão ainda fechado — a prova de que ela devolveu o controle na hora.
        Assert.Equal(ResourceState.Loading, manager.Table.GetState(id));

        portao.Set();
        jobs.Complete(handle);
        Assert.Equal(ResourceState.Loaded, manager.Table.GetState(id));
        referencia.Dispose();
    }

    [Test] public static void ResourceManager_LoadAsync_ChamadaRepetidaNaoDisparaSegundoCarregamento()
    {
        using var jobs = NovoJobSystem();
        var manager = new ResourceManager(new ResourceHandleTable(), jobs);
        var id = ResourceId.New();
        int chamadasAoCarregador = 0;
        var loader = new LoaderFalso(_ => { Interlocked.Increment(ref chamadasAoCarregador); return new RecursoFalso(); });

        var (ref1, handle1) = manager.LoadAsync(id, loader);
        jobs.Complete(handle1);

        var (ref2, handle2) = manager.LoadAsync(id, loader);
        Assert.False(handle2.IsValid, "recurso já carregado: segunda chamada não deveria agendar uma nova job");
        jobs.Complete(handle2); // handle inválido: Complete é no-op

        Assert.Equal(1, chamadasAoCarregador, "o carregador só deveria ter sido invocado uma vez para o mesmo id");
        Assert.Equal(2, manager.Table.GetRefCount(id), "cada LoadAsync bem-sucedido devolve uma posse forte própria");

        ref1.Dispose();
        ref2.Dispose();
        Assert.Equal(0, manager.Table.GetRefCount(id));
    }

    [Test] public static void ResourceManager_LoadAsync_LoaderQueLancaMarcaFailedSemDerrubarAJob()
    {
        using var jobs = NovoJobSystem();
        var manager = new ResourceManager(new ResourceHandleTable(), jobs);
        var id = ResourceId.New();
        var loader = new LoaderFalso(_ => throw new InvalidOperationException("falha proposital de carregamento"));

        var (referencia, handle) = manager.LoadAsync(id, loader);
        jobs.Complete(handle); // não pode lançar: conteúdo do usuário nunca derruba o editor

        Assert.Equal(ResourceState.Failed, manager.Table.GetState(id));
        Assert.True(manager.Table.GetError(id) is InvalidOperationException, "o erro original fica acessível para diagnóstico/UI");
        referencia.Dispose();
    }
}
