using Astra;
using System;
using System.Numerics;

[ComponentId("project.RescuePlayer")]
public sealed class RescuePlayer : Behavior
{
    [PropertyId("oxigenioInicial")] public float OxigenioInicial = 240;
    [PropertyId("alcanceInteracao")] public float AlcanceInteracao = 4.1f;
    private GameObject? _camera, _world;
    private float _oxygen;
    private int _pumps, _survivors;
    private bool _finished;

    public override void Start()
    {
        _world = Object.Parent;
        _camera = Object.Find("Câmera dos olhos");
        _oxygen = OxigenioInicial;
        for (var index = 1; index <= 2; index++)
            _world?.Find("Bomba " + index)?.Find("Indicador verde")?.SetActive(false);
        Scene.Log(ObjectId, "RESGATE · 2 bombas · 3 sobreviventes · elevador | O2 240 s");
    }

    public override void Update(float dt)
    {
        if (_finished) return;
        _oxygen -= dt * (_pumps == 2 ? .55f : 1);
        if (_oxygen <= 0)
        {
            _finished = true;
            Scene.Log(ObjectId, "SEM OXIGÊNIO · reinicie Play");
            return;
        }
        if (_pumps == 2 && _survivors == 3 && Object.Position.Z > 26)
        {
            _finished = true;
            Scene.Log(ObjectId, "RESGATE CONCLUÍDO · equipe evacuada · O2 " + MathF.Ceiling(_oxygen) + " s");
            return;
        }
        if (!Input.JustPressed("Interagir") || _camera is not { IsAlive: true }) return;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var hit = Physics.RayCast(pose.Position, forward * AlcanceInteracao, QueryFilter.Default.Ignoring(Object));
        if (hit is null) { Scene.Log(ObjectId, "Interação: mire em bomba, sobrevivente ou bloco móvel"); return; }
        var target = hit.Value.Object;
        if (target.Name.StartsWith("Bomba ", StringComparison.Ordinal))
        {
            var green = target.Find("Indicador verde");
            if (green is { ActiveInHierarchy: true }) return;
            target.Find("Indicador vermelho")?.SetActive(false);
            green?.SetActive(true);
            _pumps++;
            Scene.Log(ObjectId, "BOMBAS " + _pumps + "/2 · drenagem ativada");
        }
        else if (target.Name.StartsWith("Sobrevivente ", StringComparison.Ordinal))
        {
            target.Destroy();
            _survivors++;
            Scene.Log(ObjectId, "SOBREVIVENTES " + _survivors + "/3 · siga até o elevador");
        }
        else if (target.Name.StartsWith("Bloco móvel ", StringComparison.Ordinal))
        {
            Scene.AddImpulse(target.ObjectId, forward * 20 + Vector3.UnitY * 3);
            Scene.Log(ObjectId, "Escombro deslocado · O2 " + MathF.Ceiling(_oxygen) + " s");
        }
    }
}
