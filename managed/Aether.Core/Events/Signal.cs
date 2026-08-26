namespace Aether;

/// <summary>Alça geracional para uma inscrição em <see cref="Signal{T}"/> — mesmo padrão de
/// índice+geração que <c>PhysicsBodyHandle</c>/<c>PhysicsJointHandle</c> usam do lado nativo:
/// remove a inscrição errada é impossível mesmo se o slot for reciclado para outro assinante
/// entre a inscrição e a remoção (a geração não bate, <see cref="Signal{T}.Unsubscribe"/> vira
/// no-op em vez de desinscrever quem não devia).</summary>
public readonly struct SignalSubscription : IEquatable<SignalSubscription>
{
    internal readonly int Slot;
    internal readonly uint Generation;

    internal SignalSubscription(int slot, uint generation)
    {
        Slot = slot;
        Generation = generation;
    }

    public static readonly SignalSubscription Invalid = new(-1, 0);
    public bool IsValid => Slot >= 0;

    public bool Equals(SignalSubscription o) => Slot == o.Slot && Generation == o.Generation;
    public override bool Equals(object? o) => o is SignalSubscription s && Equals(s);
    public override int GetHashCode() => HashCode.Combine(Slot, Generation);
    public static bool operator ==(SignalSubscription a, SignalSubscription b) => a.Equals(b);
    public static bool operator !=(SignalSubscription a, SignalSubscription b) => !a.Equals(b);
}

/// <summary>
/// Item 1.5.3 do plano: "eventos e sinais tipados". Publicador/assinante em processo, não uma
/// fila persistida nem cruzando a fronteira nativa (isso é <c>PhysicsWorld.GetTriggerEvents</c>,
/// um padrão de polling em lote deliberadamente diferente — ver comentário lá). <typeparamref
/// name="T"/> é tipicamente um <c>readonly struct</c> de evento (payload dos dados publicados).
/// <para>
/// Estrutura de dados: array de slots pré-alocado (sem <c>List&lt;T&gt;</c>/dicionário), cada
/// slot com uma geração que invalida remoções tardias/erradas. <see cref="Publish"/> varre os
/// slots ocupados sem alocar — mas o próprio delegate do assinante pode alocar se capturar
/// estado (uma lambda capturante é responsabilidade de QUEM assina, não deste tipo; código no
/// caminho de frame deve assinar métodos estáticos ou instância sem closure, mesma disciplina de
/// docs/CONVENCOES.md §3). <see cref="Subscribe"/> e <see cref="Unsubscribe"/> podem realocar o
/// array de slots (crescimento amortizado) — não são operações de caminho de frame, são
/// setup/teardown, o mesmo enquadramento que o projeto já aplica a `CreateBody`/`DestroyBody`.
/// </para>
/// </summary>
public struct Signal<T>
{
    private struct Slot
    {
        public Action<T>? Listener;
        public uint Generation;
    }

    // Deliberadamente sem inicializador de campo: `default(Signal<T>)`/`new Signal<T>()` (o
    // construtor implícito de struct, que tem prioridade sobre qualquer construtor definido
    // aqui quando chamado sem argumentos posicionais explícitos) zera _slots para null — testado
    // e confirmado, não é uma suposição. Todo acesso trata null como "nenhum slot ainda", tanto
    // faz se o Signal veio do construtor explícito com capacidade 0 ou do default do struct.
    private Slot[]? _slots;
    private int _count;

    /// <summary>Cria um sinal com capacidade inicial de slots. Zero é válido — a primeira
    /// inscrição aloca a capacidade mínima sob demanda. Usar este construtor é só uma otimização
    /// para pular o primeiro Array.Resize quando o número de assinantes é conhecido de
    /// antemão — `default(Signal&lt;T&gt;)`/`new Signal&lt;T&gt;()` sem capacidade funcionam
    /// identicamente, só com uma realocação a mais na primeira inscrição.</summary>
    public Signal(int initialCapacity)
    {
        _slots = initialCapacity > 0 ? new Slot[initialCapacity] : null;
        _count = 0;
    }

    public readonly int SubscriberCount => _count;

    /// <summary>Registra <paramref name="listener"/> e devolve a alça para removê-lo depois.
    /// Reusa o primeiro slot livre (geração já incrementada de uma remoção anterior) antes de
    /// crescer o array — mesma disciplina de reciclagem de índice do resto do projeto.</summary>
    public SignalSubscription Subscribe(Action<T> listener)
    {
        ArgumentNullException.ThrowIfNull(listener);

        if (_slots is not null)
        {
            for (int i = 0; i < _slots.Length; i++)
            {
                if (_slots[i].Listener is null)
                {
                    _slots[i].Listener = listener;
                    _count++;
                    return new SignalSubscription(i, _slots[i].Generation);
                }
            }
        }

        int newSlot = _slots?.Length ?? 0;
        Array.Resize(ref _slots, Math.Max(4, newSlot * 2));
        _slots![newSlot].Listener = listener;
        _slots[newSlot].Generation = 0;
        _count++;
        return new SignalSubscription(newSlot, 0);
    }

    /// <summary>Remove a inscrição. No-op silencioso se <paramref name="subscription"/> já foi
    /// removida, é inválida, ou pertence a um slot reciclado para outro assinante (geração não
    /// bate) — mesmo contrato de idempotência de <c>PhysicsWorld.DestroyBody</c>.</summary>
    public void Unsubscribe(SignalSubscription subscription)
    {
        if (_slots is null || !subscription.IsValid || subscription.Slot >= _slots.Length) return;
        ref Slot slot = ref _slots[subscription.Slot];
        if (slot.Listener is null || slot.Generation != subscription.Generation) return;

        slot.Listener = null;
        slot.Generation++; // invalida qualquer alça antiga que ainda aponte para este slot
        _count--;
    }

    /// <summary>
    /// Notifica todos os assinantes ativos com <paramref name="value"/>, na ordem dos slots.
    /// Um assinante que se desinscreve durante o próprio callback (do slot atual ou de outro)
    /// não corrompe a varredura: a checagem de geração a cada iteração garante que um slot
    /// reaproveitado no meio da publicação nunca seja notificado com o listener errado — mas
    /// não há garantia de que um slot recém-inscrito DURANTE a publicação seja ou não
    /// notificado nesta mesma chamada (depende de estar antes ou depois do cursor atual).
    /// </summary>
    public readonly void Publish(T value)
    {
        if (_slots is null) return;
        for (int i = 0; i < _slots.Length; i++)
        {
            var listener = _slots[i].Listener;
            listener?.Invoke(value);
        }
    }
}
