namespace Aether.Editing;

/// <summary>
/// Uma ação de edição reversível: tudo que o usuário faz no editor (mover um objeto, criar uma
/// entidade, mudar uma propriedade) vira um <see cref="IEditCommand"/>. É a unidade que tanto o
/// <see cref="UndoStack"/> quanto o <see cref="WriteAheadLog"/> de recuperação entendem — undo/redo
/// e recuperação de crash são, no fundo, o mesmo problema: reproduzir ou desfazer uma sequência de
/// comandos.
/// </summary>
public interface IEditCommand
{
    /// <summary>Texto curto para mostrar na UI (histórico de undo, log de recuperação), ex.:
    /// "Mover Cubo". Não precisa ser único — é só para o usuário reconhecer a ação.</summary>
    string Description { get; }

    /// <summary>Aplica o efeito do comando. Chamado uma vez ao executar pela primeira vez, e de
    /// novo em cada <see cref="UndoStack.Redo"/>.</summary>
    void Do();

    /// <summary>Desfaz exatamente o efeito do <see cref="Do"/> anterior — precisa devolver o
    /// estado ao que era antes, byte a byte quando fizer sentido (posição, seleção, etc.).</summary>
    void Undo();
}
