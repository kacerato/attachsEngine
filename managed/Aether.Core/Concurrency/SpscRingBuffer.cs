namespace Aether;

/// <summary>
/// Item 1.2.3 do plano: fila circular single-producer/single-consumer, sem alocação após a
/// construção, wait-free (nenhuma thread bloqueia a outra nem espera em loop de CAS — o algoritmo
/// clássico de Lamport para SPSC, onde produtor e consumidor só disputam DUAS variáveis inteiras,
/// nunca a fila em si). Existe como alternativa mais barata que <c>ConcurrentQueue&lt;T&gt;</c>
/// (que já é usada em <c>JobSystem</c>, MPMC genérico do BCL) para o caso específico onde só há
/// exatamente um produtor e um consumidor — <c>ConcurrentQueue&lt;T&gt;</c> aloca um nó por item
/// enfileirado; este tipo aloca só o buffer de tamanho fixo, uma vez.
/// <para>
/// <b>Contrato de threading, sem exceção:</b> chamar <see cref="TryEnqueue"/> de mais de uma
/// thread ao mesmo tempo, ou <see cref="TryDequeue"/> de mais de uma thread ao mesmo tempo, é
/// comportamento indefinido — o tipo não detecta nem impede isso (detectar exigiria a própria
/// trava que o tipo existe para evitar). É responsabilidade do chamador garantir um único
/// produtor e um único consumidor, exatamente como o nome diz.
/// </para>
/// <para>
/// <b>Ordem de memória:</b> <see cref="_head"/> (posição de escrita, só o produtor mexe) e
/// <see cref="_tail"/> (posição de leitura, só o consumidor mexe) são lidos pela OUTRA
/// extremidade via <see cref="Volatile.Read(ref int)"/>/escritos via
/// <see cref="Volatile.Write(ref int, int)"/> — garante que quando o consumidor vê um `_head`
/// atualizado, o item que o produtor escreveu no slot correspondente já é visível também (a
/// escrita do item precede a escrita de `_head` na ordem do programa E na ordem observável por
/// outra thread, release/acquire semantics). Sem isso, a CPU/JIT poderiam reordenar a escrita do
/// slot para depois da escrita de `_head`, e o consumidor leria um slot ainda não escrito.
/// </para>
/// </summary>
public unsafe struct SpscRingBuffer<T> : IDisposable where T : unmanaged
{
    private NativeArray<T> _buffer;
    private int _head; // próximo índice a escrever (só o produtor escreve; consumidor só lê)
    private int _tail; // próximo índice a ler (só o consumidor escreve; produtor só lê)

    /// <param name="capacity">Quantidade de slots. A fila comporta no máximo <paramref
    /// name="capacity"/> - 1 itens simultâneos — um slot fica sempre vazio de propósito, para
    /// distinguir "cheio" de "vazio" só comparando <c>head</c>/<c>tail</c>, sem precisar de um
    /// contador extra (a mesma técnica clássica de ring buffer, trade-off aceito por simplicidade
    /// e por não precisar de mais uma variável para o produtor/consumidor sincronizarem).</param>
    public SpscRingBuffer(int capacity)
    {
        if (capacity < 2)
            throw new ArgumentOutOfRangeException(nameof(capacity), "capacidade mínima é 2 (1 slot fica sempre vazio por design).");
        _buffer = new NativeArray<T>(capacity);
        _head = 0;
        _tail = 0;
    }

    /// <summary>Chamar só da thread produtora. Devolve false se a fila estiver cheia — o
    /// chamador decide se descarta o item, espera, ou aumenta a capacidade (este tipo nunca
    /// realoca sozinho: tamanho fixo é o que garante nenhuma alocação após a construção).</summary>
    public bool TryEnqueue(in T item)
    {
        int capacity = _buffer.Length;
        int head = _head; // só o produtor escreve _head — leitura direta é segura aqui
        int nextHead = (head + 1) % capacity;

        // Lê a posição do consumidor com acquire semantics: se _tail já avançou o bastante para
        // haver espaço, esta leitura também garante que o produtor não vai sobrescrever um slot
        // que o consumidor ainda não drenou (o consumidor escreve _tail só DEPOIS de terminar de
        // ler o item do slot antigo).
        int tail = Volatile.Read(ref _tail);
        if (nextHead == tail) return false; // cheia

        _buffer[head] = item;
        // Publica o item ANTES de publicar o novo _head — release semantics: qualquer thread que
        // observe o _head atualizado também observa o item já escrito no slot.
        Volatile.Write(ref _head, nextHead);
        return true;
    }

    /// <summary>Chamar só da thread consumidora. Devolve false se a fila estiver vazia.</summary>
    public bool TryDequeue(out T item)
    {
        int tail = _tail; // só o consumidor escreve _tail — leitura direta é segura aqui
        int head = Volatile.Read(ref _head); // acquire: também torna visível o item que o produtor escreveu
        if (tail == head)
        {
            item = default;
            return false; // vazia
        }

        item = _buffer[tail];
        int capacity = _buffer.Length;
        Volatile.Write(ref _tail, (tail + 1) % capacity); // release: libera o slot para o produtor reusar
        return true;
    }

    /// <summary>Estimativa de ocupação — só é exata se chamada da thread produtora OU
    /// consumidora (a outra extremidade pode estar avançando concorrentemente). Útil para
    /// telemetria/diagnóstico (item 1.2.4), não para decisão de controle de fluxo exata.</summary>
    public int CountEstimate
    {
        get
        {
            int capacity = _buffer.Length;
            int head = Volatile.Read(ref _head);
            int tail = Volatile.Read(ref _tail);
            int diff = head - tail;
            return diff >= 0 ? diff : diff + capacity;
        }
    }

    public readonly int Capacity => _buffer.Length - 1; // um slot é sempre reservado, ver construtor

    public void Dispose() => _buffer.Dispose();
}
