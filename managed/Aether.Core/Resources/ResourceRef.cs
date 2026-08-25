namespace Aether.Resources;

/// <summary>
/// Referência FORTE a um recurso: enquanto uma instância viva existir, o recurso não é elegível
/// para descarregamento (<see cref="ResourceHandleTable.IsEligibleForUnload"/> só vira verdade com
/// a contagem de uso em zero). Criada por <see cref="ResourceHandleTable.AcquireStrong{T}"/> ou por
/// <see cref="ResourceManager.LoadAsync{T}"/>.
///
/// <para><b>Por que struct + <see cref="IDisposable"/>, e não uma classe com finalizador:</b>
/// recursos aqui tipicamente embrulham um handle nativo (textura Vulkan, buffer de malha, ...), e
/// não existe GC determinístico sobre esse lado nativo. Um finalizador rodaria numa thread do GC
/// em momento imprevisível — sob pressão de memória pode nem rodar a tempo — o que viola duas
/// regras não-negociáveis do CONVENCOES.md item 8: "nunca travar a thread principal" (finalizers
/// competem por ciclos de GC) e, indiretamente, "nunca esquentar o aparelho além do orçamento"
/// (recurso nativo pendurado até o próximo GC gasta memória/energia à toa). `IDisposable` com
/// `using` é determinístico: a posse é liberada na linha exata em que o código pede, do jeito que
/// C++ RAII faria — sem depender do GC nunca.</para>
///
/// <para><b>Atenção — posse "linear" representada por um struct copiável:</b> copiar uma instância
/// (atribuição, passar por valor, capturar numa closure) NÃO incrementa a contagem de uso — é
/// só mais uma cópia do mesmo "recibo" de uma única posse. Se duas cópias forem descartadas, a
/// contagem desce duas vezes para uma única posse concedida — um double-free lógico, que <see
/// cref="ResourceHandleTable.Release"/> detecta e lança. Para obter uma segunda posse independente
/// e válida, chame <see cref="AddRef"/>, que incrementa a contagem de verdade e devolve um novo
/// recibo. Esta é a mesma troca que um ponteiro bruto em C++ faz (rápido, sem alocação, mas sem
/// proteção de cópia) — deliberada aqui porque um wrapper com contagem própria (tipo
/// <c>shared_ptr</c>) exigiria um bloco de controle alocado no heap por referência, o que viola
/// "zero alocação" para algo tão comum quanto seguir um ponteiro de recurso.</para>
/// </summary>
public readonly struct ResourceRef<T> : IEquatable<ResourceRef<T>>, IDisposable where T : class
{
    public readonly ResourceId Id;
    private readonly ResourceHandleTable? _table;

    /// <summary>Interno de propósito — a única via pública de criar uma referência forte é <see
    /// cref="ResourceHandleTable.AcquireStrong{T}"/> (ou <see cref="AddRef"/> a partir de uma já
    /// existente), para que toda instância corresponda a um incremento de verdade na tabela.</summary>
    internal ResourceRef(ResourceId id, ResourceHandleTable table)
    {
        Id = id;
        _table = table;
    }

    /// <summary>Falso para <c>default(ResourceRef&lt;T&gt;)</c> — não aponta para nenhuma tabela.</summary>
    public bool IsValid => _table is not null && !Id.IsEmpty;

    /// <summary>Resolve o objeto de recurso, se e só se ele já terminou de carregar. Não lança para
    /// estados intermediários (Loading/Failed/NotLoaded) — devolve <c>false</c>, porque não achar o
    /// recurso ainda pronto é uma condição normal de carregamento assíncrono, não um erro.</summary>
    public bool TryGetValue(out T value)
    {
        if (_table is not null && _table.TryGet(Id, out var obj, out _) && obj is T typed)
        {
            value = typed;
            return true;
        }
        value = default!;
        return false;
    }

    /// <summary>Incrementa a contagem de uso de verdade e devolve uma posse forte nova e
    /// independente sobre o mesmo recurso — ver a nota de posse na documentação do tipo.</summary>
    public ResourceRef<T> AddRef()
    {
        if (_table is null) throw new InvalidOperationException("não é possível duplicar uma ResourceRef<T> inválida (default)");
        return _table.AcquireStrong<T>(Id);
    }

    /// <summary>Cria uma referência fraca para o mesmo recurso. Não altera a contagem de uso.</summary>
    public WeakResourceRef<T> ToWeak() => new(Id, _table);

    /// <summary>Libera esta posse: decrementa a contagem de uso na tabela. Depois de chamado, esta
    /// instância não deve ser usada de novo — descartá-la outra vez lança (ver nota de posse).</summary>
    public void Dispose() => _table?.Release(Id);

    public bool Equals(ResourceRef<T> other) => Id.Equals(other.Id) && ReferenceEquals(_table, other._table);
    public override bool Equals(object? obj) => obj is ResourceRef<T> other && Equals(other);
    public override int GetHashCode() => Id.GetHashCode();
    public override string ToString() => $"ResourceRef<{typeof(T).Name}>({Id})";
}

/// <summary>
/// Referência FRACA a um recurso: acompanha o <see cref="ResourceId"/> sem impedir descarregamento
/// (não participa da contagem de uso). Útil para caches, índices e referências "de conveniência"
/// que não devem, sozinhas, manter um recurso grande vivo na memória. Para usar o recurso de
/// verdade, resolva com <see cref="TryResolve"/> — se der certo, você recebe uma <see
/// cref="ResourceRef{T}"/> forte de verdade, que precisa ser descartada como qualquer outra.
/// </summary>
public readonly struct WeakResourceRef<T> : IEquatable<WeakResourceRef<T>> where T : class
{
    public readonly ResourceId Id;
    private readonly ResourceHandleTable? _table;

    internal WeakResourceRef(ResourceId id, ResourceHandleTable? table)
    {
        Id = id;
        _table = table;
    }

    public bool IsValid => _table is not null && !Id.IsEmpty;

    /// <summary>
    /// Tenta obter uma posse forte a partir da fraca. Só funciona se o recurso estiver, neste
    /// instante, carregado — devolve <c>false</c> se nunca foi carregado, ainda está carregando,
    /// falhou, ou (numa fase futura, quando descarregamento automático existir) já foi
    /// descarregado. Chamador precisa então recarregar via <see cref="ResourceManager"/>.
    /// </summary>
    public bool TryResolve(out ResourceRef<T> strong)
    {
        if (_table is not null && _table.TryAcquireStrongIfLoaded<T>(Id, out strong)) return true;
        strong = default;
        return false;
    }

    public bool Equals(WeakResourceRef<T> other) => Id.Equals(other.Id) && ReferenceEquals(_table, other._table);
    public override bool Equals(object? obj) => obj is WeakResourceRef<T> other && Equals(other);
    public override int GetHashCode() => Id.GetHashCode();
    public override string ToString() => $"WeakResourceRef<{typeof(T).Name}>({Id})";
}
