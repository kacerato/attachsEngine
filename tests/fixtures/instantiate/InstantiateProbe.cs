using System;
using System.Collections.Generic;
using Astra;
using Astra.Components;

public abstract class CloneBase : Behavior
{
    [SerializeField, PropertyId("inherited")] private int inherited;
    public int ReadInherited() => inherited;
    public void SetInherited(int value) => inherited = value;
}
[ComponentId("acceptance.clone.receiver")]
public sealed class CloneReceiver : CloneBase
{
    [PropertyId("child")] public ObjectReference Child;
    [PropertyId("external")] public ObjectReference External;
    [PropertyId("timer")] public GameTimer? Timer;
    [PropertyId("route")] public List<ObjectReference> Route = new();
    [PropertyId("values")] public List<int> Values = new() { 3, 8 };
    [PropertyId("curve")] public AnimationCurve Curve = AnimationCurve.Linear(0, 0, 1, 1);
    [HideInInspector] public int Awakes;
    public override void Awake()
    {
        ++Awakes;
        if (Object.Find("Child") != Resolve(Child) || Timer?.Component.Object != Resolve(Child) ||
            Resolve(Route[0]) != Resolve(Child) || Resolve(External)?.Name != "External")
            throw new Exception("CLONE FAIL: references were not ready before Awake");
    }
}
[ComponentId("acceptance.clone.destroyer")]
public sealed class CloneDestroyer : Behavior
{
    public static bool Armed;
    public override void Awake() { if (Armed) Object.Destroy(); }
}
[ComponentId("acceptance.clone.driver")]
public sealed class InstantiateProbe : Behavior
{
    private GameObject source = null!, copy = null!;
    private CloneReceiver receiver = null!;
    private void Require(bool value, string message)
    {
        if (value) return;
        Scene.Log(ObjectId, "CLONE FAIL: " + message);
        throw new Exception(message);
    }
    public override void Start()
    {
        var root = Object.Parent!;
        source = root.CreateChild("Source"); source.SetActive(false);
        var child = source.CreateChild("Child");
        var external = root.CreateChild("External");
        var original = source.AddBehavior<CloneReceiver>();
        original.Child = child.AsReference(); original.External = external.AsReference();
        original.Timer = child.AddComponent<GameTimer>(); original.Route.Add(child.AsReference());
        original.SetInherited(71);
        original.Enabled = false;
        copy = source.Instantiate(root); copy.Name = "Copy";
        receiver = copy.GetBehavior<CloneReceiver>()!;
        Require(receiver.ReadInherited() == 71, "private serialized base field keeps its current value");
        Require(!copy.ActiveSelf && !receiver.Enabled && receiver.Awakes == 0, "inactive clone stays asleep");
        Require(receiver.Child.ObjectId == copy.Find("Child")!.ObjectId && receiver.External.ObjectId == original.External.ObjectId,
            "internal and external reference policy: child=" + receiver.Child.ObjectId + "/" + copy.Find("Child")!.ObjectId + "; external=" + receiver.External.ObjectId + "/" + original.External.ObjectId);
        receiver.Values[0] = 99; receiver.Curve.Keys[0].Value = 12;
        Require(original.Values[0] == 3 && original.Curve.Keys[0].Value == 0, "mutable serialized values are independent");
        source.Destroy(); copy.SetActive(true); receiver.Enabled = true;
        var template = root.CreateChild("DestroyOnClone"); template.AddBehavior<CloneDestroyer>();
        CloneDestroyer.Armed = true;
        var destroyed = template.Instantiate(root);
        CloneDestroyer.Armed = false;
        Require(!destroyed.IsAlive, "Awake may destroy the clone before Instantiate returns");
        template.Destroy();
        var invalid = root.CreateChild("RejectCopy"); invalid.AddBehavior<RejectClone>();
        var count = root.ChildCount;
        RejectClone.Reject = true;
        try { invalid.Instantiate(root); throw new Exception("CLONE FAIL: rejected field was accepted"); }
        catch (System.Reflection.TargetInvocationException) { }
        finally { RejectClone.Reject = false; }
        Require(root.ChildCount == count, "failed field application rolls back the entire native clone");
        invalid.Destroy();
        copy.AddBehavior<InspectionProbe>();
        Scene.Log(ObjectId, "CLONE READY: hierarchy and current fields copied");
    }
    public override void Update(float dt)
    {
        if (receiver.Awakes == 0) return;
        Require(receiver.Awakes == 1 && copy.IsAlive && !source.IsAlive, "clone lifecycle and independent destruction");
        Require(receiver.Timer is { } timer && timer.Component.IsAlive, "component reference resolves in clone");
        Scene.Log(ObjectId, "CLONE PASS: hierarchy; fields; references; lifecycle; independent ownership");
        Enabled = false;
    }
}

[ComponentId("acceptance.clone.inspection")]
public sealed class InspectionProbe : Behavior
{
    [PropertyId("optional")] public List<int> Optional = null!;
    [PropertyId("bad")] public int Bad
    {
        get => throw new InvalidOperationException("getter deliberado");
        set { }
    }
}

[ComponentId("acceptance.clone.reject")]
public sealed class RejectClone : Behavior
{
    public static bool Reject;
    [PropertyId("value")] public int Value
    {
        get => 42;
        set { if (Reject) throw new InvalidOperationException("expected clone property failure"); }
    }
}
