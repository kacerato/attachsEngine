using System.Collections.Concurrent;

namespace Aether.Resources;

/// <summary>Estado de carregamento de um recurso na <see cref="ResourceHandleTable"/>.</summary>
public enum ResourceState : byte
{
    /// <summary>Nunca foi pedido, ou foi descarregado. Não há objeto nem erro guardado.</summary>
    NotLoaded,

    /// <summary>Carregamento em andamento numa job de background — ver <see cref="ResourceManager"/>.</summary>
    Loading,

    /// <summary>Carregado com sucesso; <see cref="ResourceSlot.Value"/> é o objeto de recurso.</summary>
    Loaded,

    /// <summary>O carregador lançou uma exceção. O recurso não trava o editor: o erro fica
    /// guardado em <see cref="ResourceSlot.Error"/> para diagnóstico/UI, e a falha não se propaga
    /// para fora da job (ver <see cref="ResourceManager.LoadAsync{T}"/>).</summary>
    Failed,
}

/// <summary>
/// Estado interno de um recurso na tabela. É uma classe (referência), não uma struct: fica
/// pendurada dentro do dicionário e é mutada in-place por AddRef/Release/conclusão de carregamento
/// — trocar de estado nunca precisa reconstruir ou reinserir a entrada no dicionário.
/// </summary>
internal sealed class ResourceSlot
{
    public object? Value;
    public ResourceState State;
    public Exception? Error;
    public int RefCount;

    /// <summary>Protege as trocas compostas de estado (State+Value+Error mudam juntos). A
    /// contagem de uso (<see cref="RefCount"/>) não precisa dele — é só um <c>int</c> mexido via
    /// <see cref="Interlocked"/>, mais barato que tomar este lock a cada AddRef/Release.</summary>
    public readonly object Lock = new();
}

/// <summary>
/// Tabela central de recursos do processo: mapeia <see cref="ResourceId"/> para o objeto carregado
/// (ou "carregando"/"não carregado"/"falhou"), mais a contagem de uso que decide quando um recurso
/// fica elegível para descarregamento.
///
/// <see cref="TryGet"/> — resolver um recurso já carregado — é o caminho quente: pode ser chamado
/// centenas de vezes por frame por sistemas de render/gameplay percorrendo <see
/// cref="ResourceRef{T}"/>s. Por isso o armazenamento é um <see cref="ConcurrentDictionary{TKey,TValue}"/>
/// (permite leituras concorrentes sem lock global, e uma consulta a uma entrada já existente não
/// aloca) em vez de um <c>Dictionary&lt;,&gt;</c> com lock manual.
///
/// Esta versão só mantém o SINAL de quando um recurso está elegível para descarregar (contagem
/// zero + carregado) — o descarregamento de verdade (liberar o objeto, invalidar handles nativos)
/// fica para quando o pipeline de assets existir (Fase 6); implementá-lo agressivamente agora seria
/// prematuro e sem onde testar de verdade.
/// </summary>
public sealed class ResourceHandleTable
{
    private readonly ConcurrentDictionary<ResourceId, ResourceSlot> _slots = new();

    internal ResourceSlot GetOrCreateSlot(ResourceId id) => _slots.GetOrAdd(id, static _ => new ResourceSlot());

    /// <summary>Consulta zero-alocação de um recurso já registrado. Devolve <c>true</c> só quando
    /// o estado é <see cref="ResourceState.Loaded"/> — para os outros estados, <paramref
    /// name="value"/> pode vir nulo mesmo com um slot existente (ex.: ainda carregando).</summary>
    public bool TryGet(ResourceId id, out object? value, out ResourceState state)
    {
        if (_slots.TryGetValue(id, out var slot))
        {
            value = slot.Value;
            state = slot.State;
            return state == ResourceState.Loaded;
        }
        value = null;
        state = ResourceState.NotLoaded;
        return false;
    }

    public ResourceState GetState(ResourceId id) => _slots.TryGetValue(id, out var slot) ? slot.State : ResourceState.NotLoaded;

    public int GetRefCount(ResourceId id) => _slots.TryGetValue(id, out var slot) ? slot.RefCount : 0;

    /// <summary>Erro do carregamento, quando o estado é <see cref="ResourceState.Failed"/>. Nulo em
    /// qualquer outro estado.</summary>
    public Exception? GetError(ResourceId id) => _slots.TryGetValue(id, out var slot) ? slot.Error : null;

    /// <summary>Um recurso é candidato a descarregamento quando ninguém mais o referencia
    /// fortemente e ele de fato terminou de carregar (não faz sentido descarregar algo que nem
    /// carregou, ou que está no meio do carregamento).</summary>
    public bool IsEligibleForUnload(ResourceId id) =>
        _slots.TryGetValue(id, out var slot) && slot.RefCount == 0 && slot.State == ResourceState.Loaded;

    /// <summary>Incrementa a contagem de uso "crua" — sem devolver um <see cref="ResourceRef{T}"/>.
    /// Exposto para composição de baixo nível e para testes; a via normal de obter posse é <see
    /// cref="AcquireStrong{T}"/>, que já embrulha o resultado numa referência forte.</summary>
    public int AddRef(ResourceId id)
    {
        var slot = GetOrCreateSlot(id);
        return Interlocked.Increment(ref slot.RefCount);
    }

    /// <summary>Decrementa a contagem de uso. Lançar quando a contagem ficaria negativa é
    /// deliberado: significa que <see cref="Release"/> foi chamado mais vezes que <see
    /// cref="AddRef"/> para o mesmo id — um bug de posse (double free lógico) que é muito melhor
    /// detectar na hora do que deixar a contagem mentir silenciosamente.</summary>
    public void Release(ResourceId id)
    {
        if (!_slots.TryGetValue(id, out var slot))
            throw new InvalidOperationException($"Release chamado para {id}, que nunca foi registrado nesta tabela");

        int updated = Interlocked.Decrement(ref slot.RefCount);
        if (updated < 0)
            throw new InvalidOperationException(
                $"contagem de uso de {id} ficou negativa — Release foi chamado mais vezes que AddRef (posse liberada duas vezes)");
    }

    /// <summary>Cria uma referência FORTE nova e independente para <paramref name="id"/>,
    /// incrementando a contagem de uso de verdade. É a via pública normal de obter um <see
    /// cref="ResourceRef{T}"/> — o construtor da struct é interno de propósito, para que nenhuma
    /// referência forte exista sem passar por aqui (e portanto sem contar).</summary>
    public ResourceRef<T> AcquireStrong<T>(ResourceId id) where T : class
    {
        AddRef(id);
        return new ResourceRef<T>(id, this);
    }

    /// <summary>Tenta transformar uma referência fraca numa forte: só funciona se o recurso está,
    /// neste exato instante, <see cref="ResourceState.Loaded"/> — checagem e incremento acontecem
    /// sob o mesmo lock do slot para não haver uma janela onde o recurso descarregaria entre os
    /// dois passos.</summary>
    internal bool TryAcquireStrongIfLoaded<T>(ResourceId id, out ResourceRef<T> reference) where T : class
    {
        if (_slots.TryGetValue(id, out var slot))
        {
            lock (slot.Lock)
            {
                if (slot.State == ResourceState.Loaded)
                {
                    Interlocked.Increment(ref slot.RefCount);
                    reference = new ResourceRef<T>(id, this);
                    return true;
                }
            }
        }
        reference = default;
        return false;
    }

    /// <summary>Cria uma referência FRACA: não altera a contagem de uso, então não há nada para
    /// "vazar" se ela nunca for resolvida ou descartada. É por isso que, ao contrário de <see
    /// cref="ResourceRef{T}"/>, <see cref="WeakResourceRef{T}"/> não precisa de disciplina de
    /// posse — pode ser criada livremente.</summary>
    public WeakResourceRef<T> CreateWeak<T>(ResourceId id) where T : class => new(id, this);

    /// <summary>Marca o início de um carregamento se, e só se, o recurso ainda não foi pedido.
    /// Devolve <c>false</c> quando outra chamada já iniciou (ou terminou) o carregamento deste id —
    /// sinal para quem chamou não agendar uma segunda job redundante.</summary>
    internal bool TryBeginLoad(ResourceId id)
    {
        var slot = GetOrCreateSlot(id);
        lock (slot.Lock)
        {
            if (slot.State != ResourceState.NotLoaded) return false;
            slot.State = ResourceState.Loading;
            return true;
        }
    }

    internal void SetLoaded(ResourceId id, object value)
    {
        var slot = GetOrCreateSlot(id);
        lock (slot.Lock)
        {
            slot.Value = value;
            slot.State = ResourceState.Loaded;
            slot.Error = null;
        }
    }

    internal void SetFailed(ResourceId id, Exception error)
    {
        var slot = GetOrCreateSlot(id);
        lock (slot.Lock)
        {
            slot.Value = null;
            slot.State = ResourceState.Failed;
            slot.Error = error;
        }
    }
}
