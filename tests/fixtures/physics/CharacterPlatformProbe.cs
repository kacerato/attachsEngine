using System;
using System.Numerics;
using Astra;
[ComponentId("acceptance.character.platform")]
public sealed class CharacterPlatformProbe : Behavior {
 private GameObject platform=null!;private double start;private float distance;private bool done;
 public override void Start(){platform=Object.FindInWorld("Piso")??throw new Exception("Kinematic floor missing");start=Time.TimeSinceStart;Scene.Log(ObjectId,"CHARACTER PLATFORM READY");}
 public override void FixedUpdate(float delta){if(done||Time.TimeSinceStart-start<2)return;distance+=delta;if(!Scene.MoveKinematic(platform.ObjectId,new Vector3(distance,-.5f,0),Quaternion.Identity))throw new WorldException(Scene.LastStatus,"move real kinematic support");}
 public override void LateUpdate(float delta){if(done||distance<2)return;var state=Object.ReadCharacterState();if(!state.IsGrounded||state.Position.X<1.8f||MathF.Abs(state.GroundVelocity.X-1)>.05f)throw new Exception("Support transport/snapshot mismatch");done=true;Scene.Log(ObjectId,"CHARACTER PLATFORM PASS: FixedUpdate kinematic support carries real capsule");}
}
