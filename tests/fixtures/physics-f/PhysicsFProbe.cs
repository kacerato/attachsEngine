using System;
using System.Numerics;
using Astra;
using Astra.Components;

// Aceite no aparelho do bloco F: material por forma (chão composto), superfície
// lida por script, quebra de junta, colisão do personagem, Raio e Braço de mola.
[ComponentId("acceptance.physics_f")]
public sealed class PhysicsFProbe : Behavior
{
    private GameObject? actor, wall, ballLeft, ballRight, sensor, arm;
    private float bestLeft, bestRight;
    private bool landedLeft, landedRight, broken, wallHit, done;
    private double brokenForce;
    private int stayFrames, ghostEnters, zoneEnters;
    private int lastStayFrame = -1, frame;

    private void Log(string message) => Scene.Log(ObjectId, "PHYSF " + message);
    private GameObject? Find(string name) => Object.FindInWorld(name);

    public override void Start()
    {
        actor = Find("Personagem"); wall = Find("Parede"); ballLeft = Find("Bola esquerda"); ballRight = Find("Bola direita");
        sensor = Find("Sensor de chão"); arm = Find("Braço");
        Find("Peso")!.GetComponent<Joint>()!.Value.OnBroken(this, e => { broken = true; brokenForce = e[0].AsNumber(); });
        actor!.GetComponent<Character>()!.Value.OnColliderHit(this, e =>
        {
            if (e.GetObject(0).ObjectId == wall!.ObjectId && e[2].AsVector3().X < -.5f) wallHit = true;
        });
        var zone = Find("Zona")!.GetComponent<Collider>()!.Value;
        var ghost = Find("Fantasma")!;
        zone.OnTriggerEnter(this, e => { if (e.GetObject(0).ObjectId == ghost.ObjectId) ++ghostEnters; else ++zoneEnters; });
        zone.OnTriggerStay(this, _ => { if (lastStayFrame != frame) { lastStayFrame = frame; ++stayFrames; } });
        Log("start");
    }

    // Ângulo, em graus, entre o +Y da caixa e o +Y do mundo.
    private static float Tilt(GameObject o)
    {
        var up = Vector3.Transform(Vector3.UnitY, o.LocalTransform.Rotation);
        return MathF.Acos(Math.Clamp(up.Y, -1f, 1f)) * 180f / MathF.PI;
    }

    private static void Track(GameObject? o, ref bool landed, ref float best)
    {
        var y = o!.LocalTransform.Position.Y;
        if (y < .6f) landed = true;
        if (landed && y > best) best = y;
    }

    public override void Update(float deltaTime)
    {
        if (done) return;
        ++frame;
        actor!.MoveCharacter(new Vector2(1, 0), 0);
        Track(ballLeft, ref landedLeft, ref bestLeft);
        Track(ballRight, ref landedRight, ref bestRight);
        if (Time.TimeSinceStart < 4) return;
        done = true;
        var ray = sensor!.GetComponent<RayCast>()!.Value;
        var floorLeft = Find("Chão esquerdo")!;
        var rayPass = ray.Colliding() && Math.Abs(ray.Distance() - 2) < .05;
        var left = Physics.RayCast(new Vector3(-3, 2, 3), new Vector3(0, -5, 0));
        var right = Physics.RayCast(new Vector3(3, 2, 3), new Vector3(0, -5, 0));
        var surfacePass = left is { } l && l.Surface() == PhysicsSurface.Rubber && right is { } r && r.Surface() == PhysicsSurface.Concrete;
        var armLength = arm!.GetComponent<SpringArm>()!.Value.HitLength();
        var camera = Find("Câmera do braço")!.LocalTransform.Position.Z;
        var armPass = Math.Abs(armLength - 2.65) < .05 && Math.Abs(camera - 2.65f) < .05f;
        var materialPass = bestLeft > 1f && landedRight && bestRight < .6f;
        Log($"material esquerda={bestLeft:F2} direita={bestRight:F2} {(materialPass ? "PASS" : "FAIL")}");
        Log($"superficie esquerda={left?.Surface()} direita={right?.Surface()} {(surfacePass ? "PASS" : "FAIL")}");
        Log($"junta quebrou={broken} forca={brokenForce:F0} {(broken && brokenForce > 50 ? "PASS" : "FAIL")}");
        Log($"personagem bateu na parede={wallHit} {(wallHit ? "PASS" : "FAIL")}");
        Log($"raio acerta={ray.Colliding()} distancia={ray.Distance():F2} chao={(ray.Collider().ObjectId == floorLeft.ObjectId || ray.Collider().ObjectId == Find("Chão")!.ObjectId)} {(rayPass ? "PASS" : "FAIL")}");
        Log($"braco comprimento={armLength:F2} camera={camera:F2} {(armPass ? "PASS" : "FAIL")}");
        var area = zoneEnters == 1 && ghostEnters == 0 && stayFrames > 30;
        Log($"sensor entradas={zoneEnters} fantasma={ghostEnters} quadros-dentro={stayFrames} {(area ? "PASS" : "FAIL")}");
        var tilted = Tilt(Find("Caixa desequilibrada")!); var upright = Tilt(Find("Caixa equilibrada")!);
        Log($"centro-de-massa inclinacao={tilted:F0} equilibrada={upright:F0} {(tilted > 30 && upright < 5 ? "PASS" : "FAIL")}");
        var smooth = Find("Caixa interpolada")!.LocalTransform.Position.Y;
        Log($"interpolacao y={smooth:F2} {(smooth < 2.9f && smooth > 0 ? "PASS" : "FAIL")}");
    }
}
