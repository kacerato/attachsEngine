using Astra;

// Os contratos que os comportamentos deste projeto usam para conversar entre si.
//
// São INTERFACES, não componentes: nada aqui aparece no menu "Adicionar". Quem
// interage procura por capacidade — "este objeto sabe ser interagido?" — em vez
// de procurar por um tipo concreto. É o que permite acrescentar um baú, uma
// alavanca ou um elevador depois sem tocar no script que interage.

/// <summary>Algo que responde a uma interação do jogador.</summary>
public interface IInteragivel
{
    /// <summary>Texto curto para a interface de jogo mostrar ao mirar.</summary>
    string Rotulo { get; }

    /// <summary>Executa a interação. `autor` é quem interagiu.</summary>
    void Interagir(GameObject autor);
}

/// <summary>Algo que pode ser coletado e contado.</summary>
public interface IColetavel
{
    int Valor { get; }
    void Coletar(GameObject autor);
}
