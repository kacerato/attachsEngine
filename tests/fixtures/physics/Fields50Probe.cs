using System;
using System.Numerics;
using Astra;
using Astra.Components;
[ComponentId("acceptance.fields50")]
public sealed class Fields50Probe : Behavior
{
    private PhysicsFieldRuntime field;
    private GameObject body=null!;
    private double began;
    private bool checkedResult;
    public override void Start()
    {
        field=Object.PhysicsField("astra.physics.field.wind");
        body=Object.FindInWorld("Body")??throw new Exception("Dynamic body missing");
        var point=body.PhysicsBody().CenterOfMass;
        if(!field.Contains(point)||!field.TrySample(point,0,out var sample)||sample.Weight<=0||sample.WindDrag<=0)
            throw new Exception("Current native field sample unavailable");
        if(field.AffectedBodyCount==0||field.AffectedMass<=0)throw new Exception("No real solver membership");
        var typed=Object.GetComponent<WindField>()??throw new Exception("Generated facade unavailable");
        typed.Vector=new Vector3(4,0,0);typed.Coefficient=2;
        if(field.Sample(point).WindVelocity.X!=4)throw new Exception("Reflected edit did not reach evaluator");
        began=Time.TimeSinceStart;
        Scene.Log(ObjectId,"FIELDS50 READY: all six APIs and live generated field properties");
    }
    public override void Update(float delta)
    {
        if(checkedResult||Time.TimeSinceStart-began<1)return;
        var observed=body.PhysicsBody().ReadState();
        if(observed.LinearVelocity.X<=.5f)throw new Exception("Wind failed to move native dynamic body");
        checkedResult=true;Scene.Log(ObjectId,"FIELDS50 PASS: authored wind reached solver velocity");
    }
}
