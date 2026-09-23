using Astra;
using System;
using System.Numerics;

[ComponentId("project.AbyssDiver")]
public sealed class AbyssDiver : Behavior
{
    [PropertyId("pressaoInicial")] public float PressaoInicial = 100;
    [PropertyId("distanciaSegurar")] public float DistanciaSegurar = 2.1f;
    private GameObject? _camera, _held, _emergency, _restored;
    private float _pressure, _pulse;
    private int _modules;
    private bool _finished;

    public override void Start()
    {
        _camera = Object.Find("Câmera dos olhos"); _pressure = PressaoInicial;
        var world = Object.Parent;
        _emergency = world?.Find("Iluminação de emergência");
        _restored = world?.Find("Iluminação restaurada"); _restored?.SetActive(false);
        for (var index = 1; index <= 4; index++)
            world?.Find("Módulo de pressão " + index)?.Find("Indicador azul")?.SetActive(false);
        Scene.Log(ObjectId, "FAROL ABISSAL · instale 4 células · PULSO atordoa predadores");
    }

    public override void Update(float dt)
    {
        if (_finished) return;
        _pulse = MathF.Max(0, _pulse - dt);
        if (_pressure <= 0) { _finished = true; Scene.Log(ObjectId, "PRESSÃO CRÍTICA · reinicie Play"); return; }
        if (_modules == 4 && Object.Position.Z > 22) { _finished = true; Scene.Log(ObjectId, "FAROL REATIVADO · pressão " + MathF.Ceiling(_pressure)); return; }
        if (!Input.JustPressed("Pulso") || _camera is not { IsAlive: true }) return;
        var target = Aim(_held, _held is null ? 11 : 4.5f);
        if (_held is { IsAlive: true } carried)
        {
            if (target?.Name.StartsWith("Módulo de pressão ", StringComparison.Ordinal) == true &&
                target.Find("Indicador azul")?.ActiveInHierarchy == false)
            {
                target.Find("Indicador vermelho")?.SetActive(false); target.Find("Indicador azul")?.SetActive(true);
                carried.Destroy(); _held = null; _modules++;
                if (_modules == 4) { _emergency?.SetActive(false); _restored?.SetActive(true); Scene.Log(ObjectId, "PRESSÃO ESTÁVEL · alcance o núcleo do farol"); }
                else Scene.Log(ObjectId, "MÓDULOS " + _modules + "/4 · pressão " + MathF.Ceiling(_pressure));
            }
            else { Scene.SetBodyVelocity(carried.ObjectId, Vector3.Zero); _held = null; Scene.Log(ObjectId, "CÉLULA SOLTA"); }
            return;
        }
        if (target?.Name.StartsWith("Célula de pressão ", StringComparison.Ordinal) == true)
        { _held = target; Scene.Log(ObjectId, "CÉLULA ACOPLADA À MANOPLA · leve a um módulo vermelho"); return; }
        var stalker = FindBehavior<AbyssStalker>(target);
        if (stalker != null && _pulse <= 0) { stalker.Stun(); _pulse = 2.2f; Scene.Log(ObjectId, "PULSO ELETROMAGNÉTICO · predador atordoado"); }
    }

    public override void FixedUpdate(float dt)
    {
        if (_held is not { IsAlive: true } carried || _camera is not { IsAlive: true }) return;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var delta = pose.Position + forward * DistanciaSegurar - carried.WorldTransform.Position;
        if (delta.LengthSquared() > 42) { _held = null; return; }
        var velocity = delta * 12.5f - Scene.GetBodyVelocity(carried.ObjectId) * 2.6f;
        if (velocity.LengthSquared() > 144) velocity = Vector3.Normalize(velocity) * 12;
        if (!Scene.SetBodyVelocity(carried.ObjectId, velocity)) _held = null;
    }

    public void TakePressure(float amount)
    {
        if (_finished) return; _pressure = MathF.Max(0, _pressure - amount);
        Scene.Log(ObjectId, "IMPACTO ABISSAL · pressão " + MathF.Ceiling(_pressure) + " · módulos " + _modules + "/4");
    }

    private GameObject? Aim(GameObject? ignored, float range)
    {
        if (_camera is not { IsAlive: true }) return null;
        var pose = _camera.WorldTransform; var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var hits = Physics.RayCastAll(pose.Position, forward * range, out _, QueryFilter.Default.Ignoring(Object), 16);
        foreach (var hit in hits) if (ignored is null || hit.Object.ObjectId != ignored.ObjectId) return hit.Object;
        return null;
    }
}
