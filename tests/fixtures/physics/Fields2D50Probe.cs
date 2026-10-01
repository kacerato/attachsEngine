using System;
using System.Numerics;
using Astra;
using Astra.Components;
[ComponentId("acceptance.fields2d50")]
public sealed class Fields2D50Probe : Behavior
{
    private PhysicsField2DRuntime field;
    private GameObject body=null!;
    private double began;
    private bool checkedResult;
    public override void Start()
    {
        field=Object.PhysicsField2D("astra.physics2d.field.wind");
        body=Object.FindInWorld("Body2D")??throw new Exception("Dynamic 2D body missing");
        var position=body.WorldTransform.Position;var point=new Vector2(position.X,position.Y);
        if(!field.Contains(point)||!field.TrySample(point,0,out var sample)||sample.Weight<=0||sample.WindDrag<=0)
            throw new Exception("Current Box2D field sample unavailable");
        if(field.AffectedBodyCount==0||field.AffectedMass<=0)throw new Exception("No real 2D membership");
        var typed=Object.GetComponent<WindField2D>()??throw new Exception("Vector2 facade unavailable");
        typed.Vector=new Vector2(4,0);typed.Coefficient=2;
        if(field.Sample(point).WindVelocity.X!=4)throw new Exception("Vector2 edit did not reach evaluator");
        began=Time.TimeSinceStart;
        Scene.Log(ObjectId,"FIELDS2D50 READY: six queries and generated Vector2 authoring");
    }
    public override void Update(float delta)
    {
        if(checkedResult||Time.TimeSinceStart-began<1)return;
        var observed=Physics2D.Body(body).Velocity;
        if(observed.Linear.X<=.5f)throw new Exception("Wind failed to move actual Box2D body");
        checkedResult=true;Scene.Log(ObjectId,"FIELDS2D50 PASS: authored XY wind reached Box2D velocity");
    }
}
