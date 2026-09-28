using System;
using System.Collections.Generic;
using Astra;

public interface ICounter { int Count { get; } }
[ComponentId("acceptance.dynamic.receiver")]
public sealed class DynamicReceiver : Behavior, ICounter
{
    public static bool InitiallyDisabled;
    public DynamicReceiver() { Enabled = !InitiallyDisabled; }
    public int Value = 7;
    [HideInInspector] public int Awakes, Starts, Updates;
    public int Count => Value;
    public static int Destroyed;
    public static readonly List<string> Calls = new();
    public override void Awake()
    {
        ++Awakes; Calls.Add(Object.Name + ":awake");
        if (Object.Name == "Nested" && Object.GetBehaviors<ICounter>().Length == 1) Object.AddBehavior<DynamicReceiver>();
    }
    public override void Start() => ++Starts;
    public override void Update(float dt) => ++Updates;
    public override void Destroy() { ++Destroyed; Calls.Add("destroy"); }
    private void Ping(int value) { Value += value; Calls.Add(Object.Name); }
    private void Flag(bool value) { if (value) ++Value; }
    private void RemoveSelf() => Remove();
    private void RemoveNext() { var all = Object.GetBehaviors<DynamicReceiver>(); if (all.Length > 1) all[1].Remove(); }
    private void Explode() { if (Value == 13) throw new InvalidOperationException("expected receiver failure"); ++Value; }
    private void Ambiguous(string value) { }
    private void Ambiguous(object value) { }
}
[ComponentId("acceptance.dynamic.driver")]
public sealed class DynamicProbe : Behavior
{
    private GameObject group = null!, delayed = null!;
    private DynamicReceiver survivor = null!;
    private int phase;
    private float elapsed;
    private void Require(bool value, string message)
    {
        if (value) return;
        Scene.Log(ObjectId, "DYNAMIC FAIL: " + message);
        throw new InvalidOperationException("DYNAMIC FAIL: " + message);
    }
    public override void Start()
    {
        Require(Object.FindInWorld("Driver") == Object, "global name lookup");
        var root = Object.Parent!;
        group = root.CreateChild("Group");
        var child = group.CreateChild("Child");
        var other = root.CreateChild("Other");
        DynamicReceiver.InitiallyDisabled = true;
        var disabled = other.AddBehavior<DynamicReceiver>();
        DynamicReceiver.InitiallyDisabled = false;
        Require(!disabled.Enabled && disabled.Awakes == 1 && disabled.Starts == 0, "constructor enabled state preserved");
        disabled.Remove();
        var parentReceiver = group.AddBehavior<DynamicReceiver>();
        var childReceiver = child.AddBehavior<DynamicReceiver>();
        var outside = other.AddBehavior<DynamicReceiver>();
        Require(parentReceiver.Awakes == 1 && parentReceiver.Starts == 0 && parentReceiver.Value == 7, "construction, fields, Awake before Start");
        Require(ReferenceEquals(group.GetBehavior<ICounter>(), parentReceiver), "lookup by contract");
        Require(group.SendMessage("Ping", 3) == 1 && parentReceiver.Value == 10, "local message");
        DynamicReceiver.Calls.Clear();
        Require(group.BroadcastMessage("Ping", 2) == 2, "descendants");
        Require(string.Join(",", DynamicReceiver.Calls) == "Group,Child", "preorder delivery");
        Require(child.SendMessageUpwards("Ping", 1) == 2 && parentReceiver.Value == 13 && childReceiver.Value == 10 && outside.Value == 7, "ancestors and branch isolation");
        parentReceiver.Enabled = false;
        Require(group.SendMessage("Ping", 0) == 1, "disabled behavior receives explicit message");
        parentReceiver.Enabled = true;
        group.SetActive(false);
        Require(group.BroadcastMessage("Ping", 1, MessageOptions.DontRequireReceiver) == 0 && Object.FindInWorld("Child") is null, "inactive hierarchy excluded");
        group.SetActive(true);
        Require(Object.Find("Child") is null && Object.FindInWorld("Child") == child, "subtree lookup unchanged");
        var nested = root.CreateChild("Nested"); nested.AddBehavior<DynamicReceiver>();
        Require(nested.GetBehaviors<ICounter>().Length == 2, "addition inside Awake");
        var first = root.CreateChild("Duplicate"); root.CreateChild("Duplicate");
        Require(Object.FindInWorld("Duplicate") == first, "duplicate name uses preorder");
        first.SetActive(false); Require(Object.FindInWorld("Duplicate") != first, "inactive duplicate skipped");
        try { group.SendMessage("Missing"); throw new Exception("DYNAMIC FAIL: missing receiver accepted"); }
        catch (MissingMethodException) { }
        try { group.SendMessage("Ambiguous", "text"); throw new Exception("DYNAMIC FAIL: ambiguous overload accepted"); }
        catch (System.Reflection.AmbiguousMatchException) { }
        child.SendMessage("Flag", true);
        Require(childReceiver.Value == 11, "boolean payload is not an option");
        child.SendMessage("RemoveSelf");
        Require(!childReceiver.IsAlive && child.GetBehavior<ICounter>() is null && child.ComponentCount == 0, "individual removal and stale reference");
        survivor = child.AddBehavior<DynamicReceiver>();
        Require(group.BroadcastMessage("Explode") == 2 && survivor.Value == 8, "failing receiver does not block next");
        var killer = other.AddBehavior<DynamicReceiver>();
        Require(other.SendMessage("RemoveNext") == 1 && !killer.IsAlive, "removed snapshot receiver skipped");
        delayed = root.CreateChild("Delayed"); delayed.AddBehavior<DynamicReceiver>();
        delayed.Destroy(10); delayed.Destroy(.15); delayed.Destroy(5);
        Require(delayed.IsAlive, "delay does not destroy immediately");
        Scene.Log(ObjectId, "DYNAMIC READY: runtime components and messages validated");
    }
    public override void Update(float dt)
    {
        elapsed += dt;
        if (++phase < 3 || delayed.IsAlive) { Require(elapsed < 3, "delayed destruction deadline"); return; }
        Require(survivor.Starts == 1 && survivor.Awakes == 1 && survivor.Updates > 0, "dynamic lifecycle resumes exactly once");
        Require(DynamicReceiver.Destroyed >= 3, "removed instances receive Destroy");
        Require(Object.FindInWorld("Delayed") is null, "destroyed object excluded from search");
        Require(Object.Parent!.BroadcastMessage("Ping", 0, MessageOptions.DontRequireReceiver) == 4, "messages skip destroyed hierarchy slots before flush");
        Scene.Log(ObjectId, "DYNAMIC PASS: scripts; lifecycle; messages; discovery; delayed destruction");
        Enabled = false;
    }
}
