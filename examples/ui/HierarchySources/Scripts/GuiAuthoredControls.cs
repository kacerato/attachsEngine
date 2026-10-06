using Astra;
using System;
using System.Numerics;
using Astra.Components;

[ComponentId("example.gui.authored-controls")]
public sealed class GuiAuthoredControls : Behavior
{
    private GuiAccess hud = null!;
    private GameObject? player;
    private Vector2 movement;
    private int jumps;
    private int impulses;
    public override void Start()
    {
        hud = Gui.ForCanvas(Object);
    }
    public override void Update(float seconds)
    {
        movement = hud.Input.Axis2("Mover");
        if (hud.Input.JustPressed("Saltar")) ++jumps;
        if (player is { IsAlive: true } && player.GetComponent<DynamicBodyMotor>() is not null && hud.Input.JustPressed("Impulso"))
        {
            var body=player.PhysicsBody();
            var settings=player.GetComponent<PhysicsBody>() ?? throw new InvalidOperationException("Motor sem corpo físico");
            // Same velocity change for an arbitrary configured object, whose
            // mass is independent of the original cylinder example.
            body.AddImpulseAtPosition(new Vector3(settings.Mass*5,0,0),body.CenterOfMass);
            ++impulses;
        }
    }
    public override void LateUpdate(float seconds)
    {
        var receiver = Object.GetComponent<UiCanvas>()?.InputReceiver.ObjectId ?? 0;
        if (receiver == 0)
        {
            hud.Find("Instrucoes").Text = "Escolha Jogador / receptor no componente Canvas";
            return;
        }
        if (player is null || !player.IsAlive || player.ObjectId != receiver)
            player = GameObject.Resolve(Scene, receiver);
        var p = player.WorldTransform.Position;
        hud.Find("Instrucoes").Text = FormattableString.Invariant(
            $"Player X {p.X:F2} Y {p.Y:F2} Z {p.Z:F2} | vetor {movement.X:F2}, {movement.Y:F2} | saltos {jumps}");
        if (player.GetComponent<DynamicBodyMotor>() is not null)
        {
            var state=player.DynamicMotor().ReadBodyState();
            hud.Find("Instrucoes").Text += FormattableString.Invariant($" | vx {state.LinearVelocity.X:F2} | impulsos {impulses}");
        }
    }
}
