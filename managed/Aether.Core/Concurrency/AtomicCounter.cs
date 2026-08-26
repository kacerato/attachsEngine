namespace Aether;

/// <summary>
/// Item 1.2.3 do plano ("primitivas sem trava... contadores atômicos"): wrapper fino sobre
/// <see cref="System.Threading.Interlocked"/> — não reinventa a primitiva de hardware (não existe
/// "contador atômico melhor que Interlocked" em software puro), só dá um nome e uma API de
/// intenção clara para o padrão já usado informalmente em <c>JobSystem</c>/física
/// (<c>Interlocked.Increment(ref campo)</c> espalhado). <c>struct</c>, não <c>class</c>: o campo
/// <see cref="_value"/> precisa morar inline em quem possui o contador (ex.: um campo de
/// <c>NativeArray&lt;AtomicCounter&gt;</c> por subsistema, o caso de uso real de 1.2.4) — encapsular
/// atrás de uma classe forçaria uma alocação de heap por contador.
/// </summary>
public struct AtomicCounter
{
    private long _value;

    public AtomicCounter(long initialValue) => _value = initialValue;

    /// <summary>Leitura com barreira de memória completa (<c>Interlocked.Read</c>) — em x64 uma
    /// leitura de <c>long</c> alinhado já é atômica por si só, mas <c>Interlocked.Read</c> também
    /// impõe a ordem de memória (não deixa o compilador/CPU reordenar essa leitura para antes de
    /// um Increment anterior na mesma thread), o que uma leitura direta do campo não garante em
    /// toda arquitetura ARM64 é permissiva o bastante para importar aqui.</summary>
    public long Value => Interlocked.Read(ref _value);

    public long Increment() => Interlocked.Increment(ref _value);
    public long Decrement() => Interlocked.Decrement(ref _value);
    public long Add(long delta) => Interlocked.Add(ref _value, delta);

    /// <summary>Troca o valor e devolve o anterior — primitiva de composição para operações
    /// maiores (ex.: "zera e leia o total do frame anterior") sem reintroduzir uma janela de
    /// corrida entre leitura e escrita separadas.</summary>
    public long Exchange(long newValue) => Interlocked.Exchange(ref _value, newValue);

    /// <summary>Compare-and-swap: só escreve <paramref name="newValue"/> se o valor atual for
    /// <paramref name="comparand"/>. Devolve o valor que estava lá antes da tentativa (padrão do
    /// próprio <see cref="Interlocked.CompareExchange(ref long, long, long)"/>) — o chamador
    /// compara o retorno com <paramref name="comparand"/> para saber se a troca aconteceu.</summary>
    public long CompareExchange(long newValue, long comparand) =>
        Interlocked.CompareExchange(ref _value, newValue, comparand);
}
