namespace Aether.Editing;

/// <summary>
/// Registro de (de)serialização de comandos de edição para o <see cref="WriteAheadLog"/> — no
/// mesmo espírito do registro de metadados de componente do ECS: cada tipo de comando se registra
/// explicitamente com uma "tag" estável e a função que sabe ler/escrever seus próprios campos, sem
/// reflexão mágica (sem percorrer campos via <c>System.Reflection</c>, sem serializador genérico
/// adivinhando o formato).
///
/// É uma INSTÂNCIA, não um registro estático global de processo: um registro estático mutável seria
/// estado compartilhado entre testes (o que o CONVENCOES.md item 6 proíbe explicitamente) — dois
/// testes que registrassem tags parecidas em qualquer ordem de execução poderiam colidir. Cada dono
/// de um <see cref="WriteAheadLog"/> (o editor de verdade, ou um teste) cria e possui seu próprio
/// <see cref="WalCommandRegistry"/>.
///
/// A tag é gravada em disco dentro de cada registro do WAL — ela, não o nome do tipo .NET, é o que
/// precisa permanecer estável entre versões do editor: o nome de uma classe pode mudar num
/// refactor sem quebrar a leitura de logs antigos, desde que a tag registrada continue a mesma.
/// </summary>
public sealed class WalCommandRegistry
{
    private readonly Dictionary<string, (Type CommandType, Action<IEditCommand, BinaryWriter> Serialize, Func<BinaryReader, IEditCommand> Deserialize)> _byTag = new();
    private readonly Dictionary<Type, string> _tagByType = new();

    /// <summary>
    /// Registra como serializar/desserializar um tipo de comando. Chamar de novo com exatamente a
    /// mesma tag para o mesmo <typeparamref name="TCommand"/> é um no-op (idempotente) — permite
    /// que código de inicialização registre tipos sem se preocupar se já rodou antes. Registrar a
    /// mesma tag para um tipo DIFERENTE lança: isso seria ambíguo na hora de recuperar o log.
    /// </summary>
    public void Register<TCommand>(
        string tag,
        Action<TCommand, BinaryWriter> serialize,
        Func<BinaryReader, TCommand> deserialize)
        where TCommand : IEditCommand
    {
        ArgumentException.ThrowIfNullOrEmpty(tag);
        ArgumentNullException.ThrowIfNull(serialize);
        ArgumentNullException.ThrowIfNull(deserialize);

        if (_byTag.TryGetValue(tag, out var existing))
        {
            if (existing.CommandType == typeof(TCommand)) return; // mesmo par (tag, tipo) de novo: ok
            throw new InvalidOperationException(
                $"tag de WAL '{tag}' já registrada para o tipo {existing.CommandType.Name}; não pode ser reusada para {typeof(TCommand).Name}");
        }

        _byTag[tag] = (typeof(TCommand),
            (cmd, writer) => serialize((TCommand)cmd, writer),
            reader => deserialize(reader));
        _tagByType[typeof(TCommand)] = tag;
    }

    internal string TagFor(IEditCommand command)
    {
        if (!_tagByType.TryGetValue(command.GetType(), out var tag))
            throw new InvalidOperationException(
                $"tipo de comando '{command.GetType().Name}' não foi registrado no WalCommandRegistry — chame Register<T> antes de gravar no WAL");
        return tag;
    }

    internal void Serialize(string tag, IEditCommand command, BinaryWriter writer) => _byTag[tag].Serialize(command, writer);

    /// <summary>Devolve <c>false</c> (em vez de lançar) para uma tag desconhecida — pode acontecer
    /// se o log foi gravado por uma versão mais nova do editor, com tipos de comando que esta
    /// versão não conhece. É um caso de recuperação legítimo, não um bug.</summary>
    internal bool TryDeserialize(string tag, BinaryReader reader, out IEditCommand command)
    {
        if (_byTag.TryGetValue(tag, out var entry))
        {
            command = entry.Deserialize(reader);
            return true;
        }
        command = null!;
        return false;
    }
}
