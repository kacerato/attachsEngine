using System;
using System.Numerics;
using Astra;
using Astra.Components;
[ComponentId("acceptance.character.platform.carry")]
public sealed class CharacterPlatformCarryProbe : Behavior {
 private GameObject platform=null!;private Character character;private double start,jumped;private float distance;private int phase;private Vector3 origin,disabledAt;
 public override void Start(){platform=Object.FindInWorld("Piso")??throw new Exception("Kinematic floor missing");character=new(Object.GetComponent(Character.TypeId)??throw new Exception("Character missing"));if(!character.InheritPlatformHorizontal)throw new Exception("Carry policy not authored");start=Time.TimeSinceStart;Scene.Log(ObjectId,"CHARACTER PLATFORM CARRY READY");}
 public override void Update(float delta){if(phase!=0||distance<1)return;var state=Object.ReadCharacterState();if(!state.IsGrounded||!Object.TryJumpCharacter())throw new Exception("Supported jump refused");origin=state.Position;jumped=Time.TimeSinceStart;phase=1;}
 public override void FixedUpdate(float delta){if(phase==3||Time.TimeSinceStart-start<2)return;distance+=delta;if(!Scene.MoveKinematic(platform.ObjectId,new Vector3(distance,-.5f,0),Quaternion.Identity))throw new WorldException(Scene.LastStatus,"move support");}
 public override void LateUpdate(float delta){
  if(phase==1&&Time.TimeSinceStart-jumped>=.5){var state=Object.ReadCharacterState();if(state.IsGrounded||state.Position.X-origin.X<.4f)throw new Exception("Horizontal carry absent in air");character.InheritPlatformHorizontal=false;disabledAt=state.Position;phase=2;}
  else if(phase==2&&Time.TimeSinceStart-jumped>=.55){var state=Object.ReadCharacterState();if(MathF.Abs(state.Position.X-disabledAt.X)>.001f||state.IsGrounded)throw new Exception("Live disable did not remove horizontal carry");phase=3;Scene.Log(ObjectId,"CHARACTER PLATFORM CARRY PASS: authored inheritance and live SDK disable affect real airborne movement");}
 }
}
