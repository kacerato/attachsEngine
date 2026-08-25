namespace Aether.Editing;

/// <summary>
/// Pilha clássica de undo/redo. <see cref="Execute"/> roda o comando e o empilha; <see cref="Undo"/>
/// e <see cref="Redo"/> movem entre as duas pilhas.
///
/// Executar um novo comando depois de um <see cref="Undo"/> descarta o "futuro" de redo —
/// comportamento padrão de qualquer editor (Blender, Photoshop, ...): uma vez que o usuário edita
/// a partir de um ponto no passado, o que tinha sido desfeito deixa de fazer sentido como
/// continuação e é abandonado.
///
/// Profundidade limitada de propósito: sem limite, uma sessão de edição longa (milhares de
/// pequenos ajustes de posição, por exemplo) faria a pilha de undo crescer sem parar — memória sem
/// teto numa engine com orçamento de 4 W não é aceitável. Ao estourar o limite, o comando mais
/// antigo do fundo da pilha é descartado silenciosamente (não pode mais ser desfeito): é a mesma
/// escolha que editores mainstream fazem — perder o undo mais velho, nunca travar nem vazar
/// memória. O WAL (<see cref="WriteAheadLog"/>) é o mecanismo separado que garante que nenhum
/// TRABALHO é perdido mesmo quando o HISTÓRICO de undo é podado; os dois resolvem problemas
/// diferentes.
/// </summary>
public sealed class UndoStack
{
    private readonly List<IEditCommand> _undo = new();
    private readonly List<IEditCommand> _redo = new();
    private readonly int _maxDepth;

    public UndoStack(int maxDepth = 200)
    {
        if (maxDepth <= 0) throw new ArgumentOutOfRangeException(nameof(maxDepth), "profundidade máxima precisa ser positiva");
        _maxDepth = maxDepth;
    }

    public int UndoCount => _undo.Count;
    public int RedoCount => _redo.Count;
    public bool CanUndo => _undo.Count > 0;
    public bool CanRedo => _redo.Count > 0;

    /// <summary>Comando no topo da pilha de undo (o próximo que <see cref="Undo"/> desfaria), sem
    /// desempilhar. Nulo se não há nada para desfazer.</summary>
    public IEditCommand? PeekUndo => _undo.Count > 0 ? _undo[^1] : null;

    /// <summary>Comando no topo da pilha de redo (o próximo que <see cref="Redo"/> reaplicaria),
    /// sem desempilhar. Nulo se não há nada para refazer.</summary>
    public IEditCommand? PeekRedo => _redo.Count > 0 ? _redo[^1] : null;

    /// <summary>
    /// Executa <paramref name="command"/> e o empilha para undo. Esta pilha, sozinha, não grava
    /// nada em disco — para a garantia de recuperação de crash, use <see cref="ExecuteWithWal"/>,
    /// que grava no <see cref="WriteAheadLog"/> antes de chamar este método.
    /// </summary>
    public void Execute(IEditCommand command)
    {
        ArgumentNullException.ThrowIfNull(command);
        command.Do();

        _undo.Add(command);
        if (_undo.Count > _maxDepth) _undo.RemoveAt(0); // descarta o undo mais antigo: não trava, não vaza

        _redo.Clear(); // novo comando depois de um Undo apaga o futuro de redo
    }

    /// <summary>
    /// Variante de <see cref="Execute"/> que primeiro grava o comando no <see cref="WriteAheadLog"/>
    /// (com fsync — ver <see cref="WriteAheadLog.Append"/>) e só DEPOIS chama <see
    /// cref="IEditCommand.Do"/>. Esta ordem é o contrato do WAL e não pode ser invertida: se o
    /// processo morrer entre as duas linhas abaixo, o comando já está gravado em disco e é
    /// reaplicado na próxima abertura (<see cref="WriteAheadLog.Recover"/>); se a ordem fosse
    /// "aplica, depois grava" e o processo morresse no meio, o comando teria efeito na sessão que
    /// morreu mas nenhum registro em disco — exatamente o tipo de perda que a regra 1 do item 8 do
    /// CONVENCOES.md ("nunca perder trabalho do usuário") proíbe.
    /// </summary>
    public void ExecuteWithWal(WriteAheadLog wal, IEditCommand command)
    {
        ArgumentNullException.ThrowIfNull(wal);
        ArgumentNullException.ThrowIfNull(command);
        wal.Append(command); // grava primeiro — precisa estar durável em disco antes de qualquer efeito
        Execute(command);    // só agora aplica de verdade e entra no histórico de undo
    }

    /// <summary>Desfaz o comando do topo. Devolve <c>false</c> (não lança) se não há nada para
    /// desfazer — um Undo "vazio" é uma ação normal de UI (botão desabilitado, atalho sem efeito),
    /// não um erro.</summary>
    public bool Undo()
    {
        if (_undo.Count == 0) return false;
        var command = _undo[^1];
        _undo.RemoveAt(_undo.Count - 1);
        command.Undo();
        _redo.Add(command);
        return true;
    }

    /// <summary>Reaplica o comando do topo do redo. Devolve <c>false</c> se não há nada para
    /// refazer.</summary>
    public bool Redo()
    {
        if (_redo.Count == 0) return false;
        var command = _redo[^1];
        _redo.RemoveAt(_redo.Count - 1);
        command.Do();
        _undo.Add(command); // redo só existe se antes coube no undo, então nunca estoura o limite aqui
        return true;
    }

    public void Clear()
    {
        _undo.Clear();
        _redo.Clear();
    }
}
