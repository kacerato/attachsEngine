using Astra;
using System;
using System.Numerics;

// Acende, apaga e pulsa uma Luz.
//
// Usa a MESMA API de propriedades que o Inspector: `intensity`, `color.r/g/b`,
// `range`. Não existe um caminho paralelo de script para luz — se a interface
// recusa um valor fora do intervalo do descritor, aqui também recusa.
//
// A luz pode estar neste objeto ou em outro: quem quiser um interruptor na
// parede acendendo um poste referencia o poste em `alvo`.
[ComponentId("project.ControleDeLuz")]
public sealed class ControleDeLuz : Behavior, IInteragivel
{
    [PropertyId("rotulo")] public string Etiqueta = "Acender";
    [PropertyId("alvo")] public ObjectReference Alvo;
    [PropertyId("intensidade_acesa")] public float IntensidadeAcesa = 20;
    [PropertyId("cor")] public Vector3 Cor = new(1, 0.92f, 0.78f);
    [PropertyId("comeca_acesa")] public bool ComecaAcesa = true;
    // Zero deixa a luz parada. Acima disso ela pulsa nessa frequência, em Hz,
    // entre `variacao` e a intensidade cheia.
    [PropertyId("frequencia_pulso")] public float FrequenciaPulso;
    [PropertyId("variacao")] public float Variacao = 0.35f;

    private Component? _luz;
    private bool _acesa;
    private float _tempo;

    public string Rotulo => Etiqueta;

    public override void Start()
    {
        var alvo = Resolve(Alvo) ?? Object;
        _luz = alvo.GetComponent(ComponentIds.Light);
        _acesa = ComecaAcesa;
        if (_luz is { IsAlive: true } luz)
        {
            luz.SetFloat("color.r", Cor.X);
            luz.SetFloat("color.g", Cor.Y);
            luz.SetFloat("color.b", Cor.Z);
        }
        Aplicar();
    }

    public void Interagir(GameObject autor)
    {
        _acesa = !_acesa;
        Aplicar();
    }

    public override void Update(float deltaTime)
    {
        if (!_acesa || FrequenciaPulso <= 0) return;
        _tempo += deltaTime;
        Aplicar();
    }

    private void Aplicar()
    {
        if (_luz is not { IsAlive: true } luz) return;
        if (!_acesa) { luz.SetFloat("intensity", 0); return; }
        // Um seno em torno de 1 menos metade da variação: o pico continua sendo
        // a intensidade autorada, e o vale desce só o que `variacao` pedir.
        var fator = FrequenciaPulso <= 0
            ? 1
            : 1 - Math.Clamp(Variacao, 0, 1) * 0.5f *
                  (1 - MathF.Cos(_tempo * FrequenciaPulso * MathF.Tau));
        luz.SetFloat("intensity", MathF.Max(0, IntensidadeAcesa * fator));
    }
}
