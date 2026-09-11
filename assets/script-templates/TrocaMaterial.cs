using Astra;
using System.Numerics;

// Troca a aparência do objeto quando algo acontece.
//
// Escreve nas propriedades do componente de Malha pela API comum — as mesmas
// propriedades que o Inspector edita, pelos mesmos identificadores. Não existe
// um caminho paralelo "de script" que aceite o que a interface recusa.
[ComponentId("project.TrocaMaterial")]
public sealed class TrocaMaterial : Behavior, IInteragivel
{
    [PropertyId("rotulo")] public string Etiqueta = "Alternar";
    [PropertyId("cor_ligada")] public Vector3 CorLigada = new(0.15f, 0.85f, 0.35f);
    [PropertyId("cor_desligada")] public Vector3 CorDesligada = new(0.6f, 0.6f, 0.6f);
    [PropertyId("emissao_ligada")] public float EmissaoLigada = 2;
    [PropertyId("comeca_ligado")] public bool ComecaLigado;
    // Quando vazio, muda a própria malha; senão, a malha do objeto referenciado.
    [PropertyId("alvo")] public ObjectReference Alvo;

    private Component? _malha;
    private bool _ligado;

    public string Rotulo => Etiqueta;

    public override void Start()
    {
        var alvo = Resolve(Alvo) ?? Object;
        _malha = alvo.GetComponent(ComponentIds.MeshRenderer);
        _ligado = ComecaLigado;
        Aplicar();
    }

    public void Interagir(GameObject autor)
    {
        _ligado = !_ligado;
        Aplicar();
    }

    public override void CollisionEnter(Collision collision)
    {
        // O mesmo efeito por contato sólido: encostar acende.
        if (_ligado) return;
        _ligado = true;
        Aplicar();
    }

    private void Aplicar()
    {
        if (_malha is not { } malha || !malha.IsAlive) return;
        var cor = _ligado ? CorLigada : CorDesligada;
        malha.SetFloat("base_color.r", cor.X);
        malha.SetFloat("base_color.g", cor.Y);
        malha.SetFloat("base_color.b", cor.Z);
        malha.SetFloat("emission.r", cor.X);
        malha.SetFloat("emission.g", cor.Y);
        malha.SetFloat("emission.b", cor.Z);
        malha.SetFloat("emission_strength", _ligado ? EmissaoLigada : 0);
    }
}
