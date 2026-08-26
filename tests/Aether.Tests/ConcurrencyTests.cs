namespace Aether.Tests;

/// <summary>Testes do item 1.2.3: primitivas sem trava (filas SPSC, contadores atômicos).</summary>
public static class ConcurrencyTests
{
    [Test] public static void AtomicCounter_IncrementDecrement_RefletemNoValue()
    {
        var counter = new AtomicCounter(10);
        counter.Increment();
        counter.Increment();
        counter.Decrement();
        Assert.Equal(11, counter.Value);
    }

    [Test] public static void AtomicCounter_Add_SomaODelta()
    {
        var counter = new AtomicCounter(5);
        counter.Add(7);
        Assert.Equal(12, counter.Value);
        counter.Add(-3);
        Assert.Equal(9, counter.Value);
    }

    [Test] public static void AtomicCounter_Exchange_DevolveValorAnteriorEDefineONovo()
    {
        var counter = new AtomicCounter(100);
        long previous = counter.Exchange(200);
        Assert.Equal(100, previous);
        Assert.Equal(200, counter.Value);
    }

    [Test] public static void AtomicCounter_CompareExchange_TrocaSoQuandoComparandoBate()
    {
        var counter = new AtomicCounter(1);
        long before = counter.CompareExchange(2, comparand: 1);
        Assert.Equal(1, before, "deveria devolver o valor anterior à tentativa");
        Assert.Equal(2, counter.Value, "com comparand correto, a troca deveria ter acontecido");

        before = counter.CompareExchange(99, comparand: 1); // comparand errado agora
        Assert.Equal(2, before);
        Assert.Equal(2, counter.Value, "com comparand incorreto, o valor não deveria mudar");
    }

    [Test] public static void AtomicCounter_IncrementConcorrente_NaoPerdeIncrementos()
    {
        // Prova real de atomicidade: 8 threads incrementando 10.000 vezes cada, sem trava
        // externa — se Interlocked não fosse de fato atômico, o total final ficaria abaixo do
        // esperado por causa de leituras/escritas intercaladas (a classe race condition que
        // "contador sem trava" existe para evitar).
        var counter = new AtomicCounter(0);
        const int threadCount = 8;
        const int incrementsPerThread = 10_000;

        var threads = new Thread[threadCount];
        for (int t = 0; t < threadCount; t++)
        {
            threads[t] = new Thread(() =>
            {
                for (int i = 0; i < incrementsPerThread; i++) counter.Increment();
            });
        }
        foreach (var thread in threads) thread.Start();
        foreach (var thread in threads) thread.Join();

        Assert.Equal((long)threadCount * incrementsPerThread, counter.Value,
            "nenhum incremento deveria se perder mesmo sob concorrência real de múltiplas threads");
    }

    [Test] public static void SpscRingBuffer_EnqueueDequeue_PreservaOrdemFIFO()
    {
        using var buffer = new SpscRingBuffer<int>(capacity: 8);
        Assert.True(buffer.TryEnqueue(1));
        Assert.True(buffer.TryEnqueue(2));
        Assert.True(buffer.TryEnqueue(3));

        Assert.True(buffer.TryDequeue(out int a));
        Assert.True(buffer.TryDequeue(out int b));
        Assert.True(buffer.TryDequeue(out int c));
        Assert.Equal(1, a);
        Assert.Equal(2, b);
        Assert.Equal(3, c);
    }

    [Test] public static void SpscRingBuffer_DequeueVazia_DevolveFalse()
    {
        using var buffer = new SpscRingBuffer<int>(capacity: 4);
        Assert.False(buffer.TryDequeue(out _));
    }

    [Test] public static void SpscRingBuffer_EnqueueAteEncher_UltimoFalha()
    {
        // Capacidade 4 comporta no máximo 3 itens (1 slot sempre vazio por design, ver
        // comentário do construtor) — o quarto Enqueue deveria falhar, não sobrescrever.
        using var buffer = new SpscRingBuffer<int>(capacity: 4);
        Assert.Equal(3, buffer.Capacity);
        Assert.True(buffer.TryEnqueue(1));
        Assert.True(buffer.TryEnqueue(2));
        Assert.True(buffer.TryEnqueue(3));
        Assert.False(buffer.TryEnqueue(4), "a fila cheia deveria recusar mais um item, não sobrescrever");
    }

    [Test] public static void SpscRingBuffer_CicloCompletoDeEnqueueDequeue_ReusaSlots()
    {
        // Prova que o índice circular (módulo capacity) funciona: encher, esvaziar, encher de
        // novo várias vezes deveria continuar funcionando indefinidamente, não só na primeira
        // volta do buffer.
        using var buffer = new SpscRingBuffer<int>(capacity: 4);
        for (int cycle = 0; cycle < 5; cycle++)
        {
            Assert.True(buffer.TryEnqueue(cycle * 10 + 1));
            Assert.True(buffer.TryEnqueue(cycle * 10 + 2));
            Assert.True(buffer.TryEnqueue(cycle * 10 + 3));
            Assert.True(buffer.TryDequeue(out int a));
            Assert.True(buffer.TryDequeue(out int b));
            Assert.True(buffer.TryDequeue(out int c));
            Assert.Equal(cycle * 10 + 1, a);
            Assert.Equal(cycle * 10 + 2, b);
            Assert.Equal(cycle * 10 + 3, c);
        }
    }

    [Test] public static void SpscRingBuffer_CountEstimate_RefleteOcupacaoReal()
    {
        using var buffer = new SpscRingBuffer<int>(capacity: 8);
        Assert.Equal(0, buffer.CountEstimate);
        buffer.TryEnqueue(1);
        buffer.TryEnqueue(2);
        Assert.Equal(2, buffer.CountEstimate);
        buffer.TryDequeue(out _);
        Assert.Equal(1, buffer.CountEstimate);
    }

    [Test] public static void Constructor_CapacidadeMenorQueDois_Lanca()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => new SpscRingBuffer<int>(capacity: 1));
        Assert.Throws<ArgumentOutOfRangeException>(() => new SpscRingBuffer<int>(capacity: 0));
    }

    [Test] public static void SpscRingBuffer_ProdutorEConsumidorReaisEmThreadsSeparadas_NaoPerdeNemDuplicaItens()
    {
        // O teste que realmente prova a garantia de visibilidade de memória documentada no
        // tipo: produtor e consumidor rodando em threads de verdade (não simulados
        // sequencialmente na mesma thread, o que não exercitaria nenhuma condição de corrida
        // real nem a necessidade de Volatile.Read/Write). Se a ordem de memória estivesse
        // errada, o consumidor poderia ler um slot que o produtor ainda não terminou de
        // escrever, ou perder itens por causa de reordenação de CPU/JIT.
        using var buffer = new SpscRingBuffer<int>(capacity: 64);
        const int itemCount = 200_000;
        var received = new int[itemCount];
        int receivedCount = 0;

        var producer = new Thread(() =>
        {
            for (int i = 0; i < itemCount; i++)
            {
                while (!buffer.TryEnqueue(i)) Thread.SpinWait(1); // espera a fila abrir espaço
            }
        });

        var consumer = new Thread(() =>
        {
            while (receivedCount < itemCount)
            {
                if (buffer.TryDequeue(out int value))
                {
                    received[receivedCount] = value;
                    receivedCount++;
                }
                else
                {
                    Thread.SpinWait(1);
                }
            }
        });

        consumer.Start();
        producer.Start();
        producer.Join();
        consumer.Join();

        Assert.Equal(itemCount, receivedCount, "nenhum item deveria se perder na travessia entre threads");
        for (int i = 0; i < itemCount; i++)
        {
            if (received[i] != i)
                throw new AssertException($"item na posição {i} deveria ser {i}, veio {received[i]} — ordem FIFO violada ou dado corrompido sob concorrência real");
        }
    }
}
