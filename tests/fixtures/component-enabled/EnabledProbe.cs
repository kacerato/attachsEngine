using System;
using System.Numerics;
using Astra;
using Astra.Components;

[ComponentId("acceptance.enabled.worker")]
public sealed class EnabledWorker : Behavior
{
    [HideInInspector] public int Awakes, Starts, Enables, Disables, Ticks;
    public override void Awake() => ++Awakes;
    public override void Start() => ++Starts;
    public override void Enable() => ++Enables;
    public override void Disable() => ++Disables;
    public override void Update(float dt) => ++Ticks;
}

[ComponentId("acceptance.enabled.driver")]
public sealed class EnabledProbe : Behavior
{
    private int phase, ticks;
    private GameObject workerObject = null!;
    private EnabledWorker worker = null!;
    private Component script, collider;
    private static void Require(bool ok, string message)
    {
        if (!ok) throw new InvalidOperationException("ENABLED FAIL: " + message);
    }
    public override void Start()
    {
        var root = Object.Parent!;
        workerObject = root.Find("Worker")!;
        worker = FindBehavior<EnabledWorker>(workerObject)!;
        script = workerObject.GetComponent(ComponentIds.ScriptBehavior)!.Value;
        collider = root.Find("Body")!.GetComponent(ComponentIds.Collider)!.Value;
        var camera = root.Find("Camera")!;
        var look = camera.GetComponent<CameraLook>()!.Value;
        var animation = camera.GetComponent<Animation>()!.Value;
        var lod = camera.GetComponent<LodGroup>()!.Value;
        Require(!look.Enabled && !animation.Enabled && !lod.Enabled, "valores autorados persistidos");
        look.Enabled = true; animation.Enabled = true; lod.Enabled = true;
        Require(look.Enabled && animation.Enabled && lod.Enabled, "fachadas geradas atravessam a ABI");
        look.Enabled = false; animation.Enabled = false; lod.Enabled = false;
    }
    public override void Update(float dt)
    {
        switch (phase++)
        {
            case 0:
                Require(Physics.RayCast(Vector3.Zero, new Vector3(10, 0, 0)) is not null, "colisor inicialmente presente");
                script.Enabled = false; collider.Enabled = false; ticks = worker.Ticks;
                break;
            case 1:
                Require(!worker.Enabled && worker.Ticks == ticks, "Component.Enabled suspende callbacks");
                Require(Physics.RayCast(Vector3.Zero, new Vector3(10, 0, 0)) is null, "último colisor desligado sem parar Play");
                collider.Enabled = true; script.Enabled = true;
                break;
            case 2:
                Require(worker.Enabled && worker.Ticks > ticks, "reativação retoma callbacks");
                Require(Physics.RayCast(Vector3.Zero, new Vector3(10, 0, 0)) is not null, "colisor reativado");
                worker.Enabled = false; ticks = worker.Ticks;
                Require(!script.Enabled, "Behavior.Enabled escreve no componente nativo");
                break;
            case 3:
                Require(worker.Ticks == ticks, "desligamento direto suspende Update");
                workerObject.SetActive(false); worker.Enabled = true;
                break;
            case 4:
                Require(script.Enabled && worker.Ticks == ticks, "Enabled local não supera inatividade");
                workerObject.SetActive(true);
                break;
            case 5:
                Require(worker.Ticks > ticks && worker.Awakes == 1 && worker.Starts == 1, "retoma a mesma instância");
                Enabled = false;
                Require(!Object.GetComponent(ComponentIds.ScriptBehavior)!.Value.Enabled, "autodesativação sincronizada");
                Scene.Log(ObjectId, "ENABLED PASS: C#/ABI; componentes; callbacks; hierarquia; colisor; persistencia");
                break;
        }
    }
}
