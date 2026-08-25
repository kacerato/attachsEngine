namespace Aether.Tests;

public static class MemoryTests
{
    // ---------- FrameArena ----------

    [Test] public static void FrameArena_AlocacaoRespeitaAlinhamento()
    {
        using var arena = new FrameArena(blockSize: 1024, alignment: 32);
        var a = arena.Alloc<byte>(3);   // desalinha o cursor de propósito
        var b = arena.Alloc<long>(4);
        unsafe
        {
            fixed (long* p = b)
                Assert.Equal(0L, (nint)p % 32, "endereço devolvido precisa ser múltiplo do alinhamento pedido");
        }
        Assert.True(a.Length == 3);
    }

    [Test] public static void FrameArena_ResetReusaAMesmaMemoria()
    {
        using var arena = new FrameArena(blockSize: 4096);
        var first = arena.Alloc<int>(16);
        unsafe { fixed (int* p1 = first) { nint addr1 = (nint)p1;
            arena.Reset();
            var second = arena.Alloc<int>(16);
            fixed (int* p2 = second)
                Assert.Equal(addr1, (nint)p2, "depois do reset, a próxima alocação começa do mesmo endereço");
        }}
    }

    [Test] public static void FrameArena_CresceComBlocosQuandoEstoura()
    {
        using var arena = new FrameArena(blockSize: 64);
        Assert.Equal(1, arena.BlockCount, "começa com um único bloco");
        arena.Alloc<byte>(60);
        arena.Alloc<byte>(60);   // não cabe mais no primeiro bloco de 64 bytes: precisa encadear
        Assert.True(arena.BlockCount >= 2, "estourar o bloco encadeia um novo em vez de falhar");
    }

    [Test] public static void FrameArena_PeakBytesReflete_O_MaiorUsoJaVisto()
    {
        using var arena = new FrameArena(blockSize: 4096, alignment: 16);
        arena.Alloc<byte>(1000);
        nuint peakApos1000 = arena.PeakBytes;
        Assert.True(peakApos1000 >= 1000, "marca d'água cobre a primeira alocação");

        arena.Reset();
        arena.Alloc<byte>(10);
        Assert.Equal(peakApos1000, arena.PeakBytes, "reset não derruba a marca d'água — ela é o pico histórico, não o uso atual");
    }

    [Test] public static void FrameArena_AlocacaoDeTamanhoZeroDevolveSpanVazio()
    {
        using var arena = new FrameArena(blockSize: 256);
        var span = arena.Alloc<float>(0);
        Assert.Equal(0, span.Length);
        Assert.Equal((nuint)0, arena.PeakBytes, "alocação de tamanho zero não consome orçamento");
    }

    [Test] public static void FrameArena_ContagemNegativaLanca()
    {
        using var arena = new FrameArena(blockSize: 256);
        Assert.Throws<ArgumentOutOfRangeException>(() => arena.Alloc<int>(-1));
    }

    [Test] public static void FrameArena_NaoAlocaNoHeapGerenciadoEmAllocEReset()
    {
        using var arena = new FrameArena(blockSize: 1 << 16);
        Assert.NoAlloc(() =>
        {
            for (int i = 0; i < 200; i++)
            {
                var span = arena.Alloc<float>(16);
                span[0] = 1f;
            }
            arena.Reset();
        }, "alloc + reset é o caminho de todo frame — não pode tocar o GC");
    }

    // ---------- PoolAllocator ----------

    [Test] public static unsafe void PoolAllocator_RentReturnRent_DevolveOMesmoBloco()
    {
        using var pool = new PoolAllocator<int>(itemsPerChunk: 8);
        int idx = pool.Rent(out int* p1);
        pool.Return(idx);
        int idx2 = pool.Rent(out int* p2);
        Assert.Equal(idx, idx2, "free-list devolve o índice mais recentemente liberado");
        Assert.Equal((nint)p1, (nint)p2);
    }

    [Test] public static unsafe void PoolAllocator_PonteirosContinuamValidosAposCrescer()
    {
        using var pool = new PoolAllocator<long>(itemsPerChunk: 4);
        int idxA = pool.Rent(out long* pA);
        *pA = 0x1234;
        // esgota o resto do primeiro bloco e força crescimento
        for (int i = 0; i < 10; i++) pool.Rent(out _);

        Assert.Equal(0x1234L, *pA, "ponteiro entregue antes do pool crescer continua válido e com o mesmo conteúdo");
        _ = idxA;
    }

    [Test] public static unsafe void PoolAllocator_DoubleFreeEhDetectado()
    {
        using var pool = new PoolAllocator<int>(itemsPerChunk: 4);
        int idx = pool.Rent(out _);
        pool.Return(idx);
        Assert.Throws<InvalidOperationException>(() => pool.Return(idx), "devolver o mesmo índice duas vezes é double-free");
    }

    [Test] public static void PoolAllocator_IndiceForaDeFaixaLanca()
    {
        using var pool = new PoolAllocator<int>(itemsPerChunk: 4);
        Assert.Throws<IndexOutOfRangeException>(() => pool.Return(999));
    }

    // ---------- NativeList / NativeArray ----------

    [Test] public static void NativeList_Cresce_E_PreservaConteudo()
    {
        using var list = new NativeList<int>(initialCapacity: 2);
        for (int i = 0; i < 50; i++) list.Add(i * i);
        Assert.Equal(50, list.Count);
        for (int i = 0; i < 50; i++) Assert.Equal(i * i, list[i]);
    }

    [Test] public static void NativeList_IndexadorRef_AlteraOConteudoReal()
    {
        using var list = new NativeList<int>();
        list.Add(10);
        list[0] = 99;
        Assert.Equal(99, list[0], "indexador por ref permite mutação in-place");
    }

    [Test] public static void NativeList_IndiceForaDeFaixaLancaComMensagemClara()
    {
        using var list = new NativeList<int>();
        list.Add(1);
        Assert.Throws<IndexOutOfRangeException>(() => _ = list[5]);
        Assert.Throws<IndexOutOfRangeException>(() => _ = list[-1]);
    }

    [Test] public static void NativeList_DisposeDuploEhSeguro()
    {
        var list = new NativeList<int>();
        list.Add(1);
        list.Dispose();
        list.Dispose(); // não pode lançar nem dar segfault
    }

    [Test] public static void NativeArray_IndexadorRef_AlteraOConteudoReal()
    {
        using var arr = new NativeArray<float>(4);
        arr[2] = 3.5f;
        Assert.Close(3.5f, arr[2]);
    }

    [Test] public static void NativeArray_IndiceForaDeFaixaLanca()
    {
        using var arr = new NativeArray<int>(3);
        Assert.Throws<IndexOutOfRangeException>(() => _ = arr[3]);
    }

    [Test] public static void NativeArray_TamanhoZeroFuncionaSemLancar()
    {
        using var arr = new NativeArray<int>(0);
        Assert.Equal(0, arr.Length);
        Assert.Equal(0, arr.AsSpan().Length);
    }

    [Test] public static void NativeArray_DisposeDuploEhSeguro()
    {
        var arr = new NativeArray<int>(4);
        arr.Dispose();
        arr.Dispose();
    }

    // ---------- MemoryBudget ----------

    [Test] public static void MemoryBudget_SomaAlocacoesELiberacoesPorCategoria()
    {
        var budget = new MemoryBudget();
        budget.SetLimit(MemoryCategory.TexturesGpu, 1000);
        budget.Allocate(MemoryCategory.TexturesGpu, 400);
        budget.Allocate(MemoryCategory.TexturesGpu, 100);
        budget.Free(MemoryCategory.TexturesGpu, 200);
        Assert.Equal(300L, budget.Used(MemoryCategory.TexturesGpu));
        Assert.Equal(700L, budget.Remaining(MemoryCategory.TexturesGpu));

        Assert.Equal(0L, budget.Used(MemoryCategory.MeshesGpu), "categorias não se misturam");
    }

    [Test] public static void MemoryBudget_CallbackDeLimiarDisparaUmaVezSoNaoARepeteCadaByte()
    {
        var budget = new MemoryBudget();
        budget.SetLimit(MemoryCategory.Scene, 1000, thresholdFraction: 0.8f);
        int fireCount = 0;
        budget.ThresholdCrossed += _ => fireCount++;

        budget.Allocate(MemoryCategory.Scene, 500);          // 50%: abaixo do limiar
        Assert.Equal(0, fireCount);

        budget.Allocate(MemoryCategory.Scene, 350);          // 85%: cruza o limiar
        Assert.Equal(1, fireCount);

        for (int i = 0; i < 20; i++) budget.Allocate(MemoryCategory.Scene, 1); // continua acima: não deve disparar de novo
        Assert.Equal(1, fireCount, "uma vez por cruzamento, não a cada byte");

        budget.Free(MemoryCategory.Scene, 900);              // volta pra baixo do limiar
        budget.Allocate(MemoryCategory.Scene, 900);          // cruza de novo
        Assert.Equal(2, fireCount, "um novo cruzamento depois de recuar dispara de novo");
    }

    [Test] public static void MemoryBudget_RemainingNuncaFicaNegativo()
    {
        var budget = new MemoryBudget();
        budget.SetLimit(MemoryCategory.UndoHistory, 100);
        budget.Allocate(MemoryCategory.UndoHistory, 500);
        Assert.Equal(0L, budget.Remaining(MemoryCategory.UndoHistory));
    }
}
