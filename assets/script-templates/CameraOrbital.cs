using Astra;
using System;
using System.Numerics;

// Câmera de órbita que não atravessa parede.
//
// A distância é reduzida até o primeiro corpo entre o alvo e a câmera, medido
// por uma ESFERA varrida: um raio fino entraria por frestas que a câmera não
// atravessa, e o resultado seria a câmera encostando na quina.
//
// A rotação vem da ação de olhar que o projeto configurou — não de um nome fixo
// no núcleo. Um jogo que chame essa ação de outra coisa continua funcionando.
[ComponentId("project.CameraOrbital")]
public sealed class CameraOrbital : Behavior
{
    [PropertyId("alvo")] public ObjectReference Alvo;
    [PropertyId("distancia")] public float Distancia = 6;
    [PropertyId("altura")] public float Altura = 1.6f;
    [PropertyId("raio_da_camera")] public float RaioDaCamera = .3f;
    [PropertyId("distancia_minima")] public float DistanciaMinima = 1;
    [PropertyId("graus_por_tela")] public float GrausPorTela = 180;
    [PropertyId("limite_vertical")] public float LimiteVertical = 60;

    private GameObject? _alvo;
    private float _yaw, _pitch;

    public override void Start()
    {
        if (Resolve(Alvo) is { } encontrado) _alvo = encontrado;
    }

    public override void Update(float deltaTime)
    {
        if (_alvo is not { IsAlive: true }) return;

        var olhar = Input.Look;
        _yaw += olhar.X * GrausPorTela;
        _pitch = Math.Clamp(_pitch + olhar.Y * GrausPorTela, -LimiteVertical, LimiteVertical);

        const float paraRadianos = MathF.PI / 180;
        var rotacao = Quaternion.CreateFromYawPitchRoll(_yaw * paraRadianos, _pitch * paraRadianos, 0);
        // A câmera da engine olha ao longo de +Z; a posição fica atrás do foco.
        var frente = Vector3.Transform(Vector3.UnitZ, rotacao);
        var foco = _alvo.WorldTransform.Position + new Vector3(0, Altura, 0);

        var distancia = Distancia;
        var filtro = QueryFilter.Default;
        // Encostar no próprio personagem não é obstáculo.
        filtro.Ignore = _alvo.ObjectId;
        var caminho = -frente * Distancia;
        if (Physics.ShapeCast(ShapeQuery.Sphere(RaioDaCamera), foco, caminho, filtro) is { } obstaculo)
            distancia = Math.Max(DistanciaMinima, obstaculo.Distance);

        Object.WorldTransform = new TransformValue(foco - frente * distancia, rotacao, Vector3.One);
    }
}
