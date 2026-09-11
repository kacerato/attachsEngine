using Astra;
using System;
using System.Numerics;

// Uma porta configurável: gira até um ângulo aberto e volta.
//
// Ela cumpre o contrato `IInteragivel`, então qualquer coisa que saiba interagir
// a aciona. O eixo, o ângulo e a velocidade são campos — a mesma porta serve
// para um portão que desliza, bastando trocar o eixo e usar deslocamento.
[ComponentId("project.Porta")]
public sealed class Porta : Behavior, IInteragivel
{
    [PropertyId("rotulo")] public string Etiqueta = "Abrir";
    [PropertyId("angulo_aberto")] public float AnguloAberto = 95;
    [PropertyId("graus_por_segundo")] public float GrausPorSegundo = 220;
    [PropertyId("comeca_aberta")] public bool ComecaAberta;
    [PropertyId("tranca")] public bool Trancada;

    private float _angulo;
    private float _fechado;
    private bool _aberta;

    public string Rotulo => Trancada ? "Trancada" : _aberta ? "Fechar" : Etiqueta;

    public override void Start()
    {
        // O ângulo fechado é o que o usuário AUTOROU na cena: a porta não
        // assume que nasce alinhada com o eixo do mundo.
        _fechado = Object.LocalTransform.Rotation.ToEulerY();
        _aberta = ComecaAberta;
        _angulo = _aberta ? AnguloAberto : 0;
    }

    public void Interagir(GameObject autor)
    {
        if (Trancada) return;
        _aberta = !_aberta;
    }

    public override void Update(float deltaTime)
    {
        var destino = _aberta ? AnguloAberto : 0;
        if (Math.Abs(_angulo - destino) < .01f) return;
        var passo = GrausPorSegundo * deltaTime;
        _angulo = Math.Abs(destino - _angulo) <= passo ? destino
                : _angulo + (destino > _angulo ? passo : -passo);
        var atual = Object.LocalTransform;
        const float paraRadianos = MathF.PI / 180;
        Object.LocalTransform = atual with
        {
            Rotation = Quaternion.CreateFromYawPitchRoll((_fechado + _angulo) * paraRadianos, 0, 0),
        };
    }
}

internal static class PortaMatematica
{
    /// <summary>Guinada em graus de um quatérnio, na convenção da engine.</summary>
    public static float ToEulerY(this Quaternion rotation)
    {
        var seno = 2 * (rotation.W * rotation.Y + rotation.X * rotation.Z);
        var cosseno = 1 - 2 * (rotation.Y * rotation.Y + rotation.X * rotation.X);
        return MathF.Atan2(seno, cosseno) * 180 / MathF.PI;
    }
}
