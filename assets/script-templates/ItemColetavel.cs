using Astra;

// Item que some ao ser tocado ou interagido, somando ao total do coletor.
//
// Aceita os dois caminhos de propósito: o sensor (encostar) e o contrato de
// interação (apertar o botão mirando). Quem conta é o COLETOR, não este script:
// o item não sabe quem o pegou, só avisa.
[ComponentId("project.ItemColetavel")]
public sealed class ItemColetavel : Behavior, IInteragivel, IColetavel
{
    [PropertyId("rotulo")] public string Etiqueta = "Pegar";
    [PropertyId("valor")] public int Pontos = 1;
    [PropertyId("por_contato")] public bool PorContato = true;

    private bool _coletado;

    public string Rotulo => Etiqueta;
    public int Valor => Pontos;

    public void Interagir(GameObject autor) => Coletar(autor);

    public void Coletar(GameObject autor)
    {
        // Uma vez só: o sensor pode reportar Enter e o jogador apertar o botão
        // no mesmo quadro, e o item não pode valer dois pontos por isso.
        if (_coletado) return;
        _coletado = true;
        foreach (var contador in FindBehaviors<Coletor>(autor)) contador.Somar(Pontos);
        // A remoção entra na fila e é aplicada no ponto seguro; a referência a
        // este objeto já vence aqui, e ninguém mais consegue coletá-lo.
        Object.Destroy();
    }

    public override void TriggerEnter(ObjectReference other)
    {
        if (!PorContato || _coletado) return;
        if (Resolve(other) is { } autor) Coletar(autor);
    }
}

// O total de quem coleta. Fica no objeto do jogador, não no item.
[ComponentId("project.Coletor")]
public sealed class Coletor : Behavior
{
    [PropertyId("total")] public int Total;

    public void Somar(int valor)
    {
        Total += valor;
        Scene.Log(ObjectId, "coletado total=" + Total);
    }
}
