using System;
using System.Numerics;
using Astra;
using Astra.Components;
[ComponentId("acceptance.bulk50")]
public sealed class Bulk50Probe : Behavior {
 private GameObject follower=null!,rotation=null!,scale=null!,body=null!;
 private Vector3 start;private double begin;private bool done;
 public override void Start(){
  follower=Object.FindInWorld("Mola de posição")??throw new Exception("Position follower missing");
  rotation=Object.FindInWorld("Mola de rotação")??throw new Exception("Rotation follower missing");
  scale=Object.FindInWorld("Mola de escala")??throw new Exception("Scale follower missing");
  body=Object.FindInWorld("Corpo ABI32")??throw new Exception("Physical body missing");
  var physical=body.PhysicsBody();if(!physical.TryReadState(out _))throw new Exception("Native snapshot unavailable");
  physical.SetLinearAndAngularVelocity(Vector3.Zero,new Vector3(0,1,0));
  if(physical.AngularVelocity.Y<.9f)throw new Exception("Angular setter/solver mismatch");
  var center=physical.CenterOfMass;
  if(physical.GetPointVelocity(center+Vector3.UnitX).Z>-.9f)throw new Exception("Point velocity lacks angular lever arm");
  _=physical.GetRelativePointVelocity(Vector3.UnitX);physical.Sleep();
  if(!physical.IsSleeping)throw new Exception("Body did not sleep");physical.WakeUp();if(!physical.IsActive)throw new Exception("Body did not wake");
  physical.AddImpulseAtPosition(Vector3.UnitX,center+Vector3.UnitY);physical.AddForceAtPosition(Vector3.UnitX,center);
  physical.AddVelocityChange(new Vector3(.1f,0,0));physical.AddAngularVelocityChange(new Vector3(0,.1f,0));physical.AngularVelocity=Vector3.Zero;
  _=physical.ReadState();physical.SetLinearAndAngularVelocity(Vector3.Zero,Vector3.Zero);
  start=follower.WorldPosition;begin=Time.TimeSinceStart;Scene.Log(ObjectId,"BULK50 READY: real ABI32 commands accepted");
 }
 public override void Update(float delta){
  double t=Time.TimeSinceStart-begin;
  Object.WorldPosition=new Vector3((float)Math.Sin(t)*2,1,0);
  Object.WorldRotation=Quaternion.CreateFromAxisAngle(Vector3.UnitY,(float)t);
  Object.LocalScale=Vector3.One*(1+(float)Math.Sin(t*2)*.35f);
  if(done)return;
  if(t>8)throw new Exception("Bulk50 followers never produced runtime changes");
  if(t<2)return;
  if(Vector3.Distance(follower.WorldPosition,start)<.5f)throw new Exception("Position spring remained static");
  if(MathF.Abs(Quaternion.Dot(rotation.WorldRotation,Quaternion.Identity))>.999f)throw new Exception("Rotation spring remained static");
  if(MathF.Abs(scale.LocalScale.X-1)<.02f)throw new Exception("Scale spring remained static");
  var p=follower.GetComponent<SpringPositionConstraint>()??throw new Exception("Generated facade unavailable");
  p.Frequency=4;p.DampingRatio=.8f;p.MaxSpeed=8;p.Offset=new Vector3(2,0,0);p.Weight=.75f;
  done=true;Scene.Log(ObjectId,"BULK50 PASS: native body, moving position/rotation/scale, live reflected edits");
 }
}
