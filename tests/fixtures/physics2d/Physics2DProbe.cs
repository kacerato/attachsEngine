using System;
using System.Numerics;
using Astra;
using Astra.Components;

[ComponentId("acceptance.physics2d")]
public sealed class Physics2DProbe : Behavior
{
    private readonly PhysicsHit2D[] hits = new PhysicsHit2D[16];
    private GameObject? wall;
    private Body2DAccess body;
    private Vector3 initialPosition, wallPosition;
    private double nextSearch, started;
    private bool ready, finished, moved, contacted;
    private string waitReason = "";
    private void Log(string message) => Scene.Log(ObjectId, "PHY2D " + message);
    private void Wait(string reason) { if (waitReason == reason) return; waitReason = reason; Log("WAIT: " + reason); }

    public override void Start() => Wait("attach one probe to authored PHY2D Body; waiting for PHY2D Wall");
    public override void CollisionEnter(Collision collision)
    {
        if (!ready || finished || wall is null || collision.Other.ObjectId != wall.ObjectId) return;
        if (collision.Normal is not Vector3 normal || !float.IsFinite(normal.X) || !float.IsFinite(normal.Y) || MathF.Abs(normal.Z) > .0001f)
        { Log("FAIL: contact did not provide finite XY/Z0 normal"); finished = true; return; }
        if (!contacted) Log("CONTACT: authored wall=" + wall.ObjectId + " normal=" + normal);
        contacted = true;
    }

    public override void Update(float dt)
    {
        if (finished) return;
        try
        {
            var now = Time.TimeSinceStart;
            if (!ready)
            {
                if (now < nextSearch) return;
                nextSearch = now + .5; Prepare(now); return;
            }
            if (wall is null || !wall.IsAlive || !Object.IsAlive) { Wait("authored body/wall removed before acceptance"); finished = true; return; }
            var position = Object.WorldTransform.Position;
            var velocity = body.Velocity;
            if (!float.IsFinite(velocity.Linear.X) || !float.IsFinite(velocity.Linear.Y) || !float.IsFinite(velocity.AngularDegrees)) throw new Exception("nonfinite solver velocity");
            if (Vector3.Distance(wall.WorldTransform.Position, wallPosition) > .001f) throw new Exception("Static wall moved");
            if (!moved && position.X - initialPosition.X > .1f)
            {
                moved = true; Log("ADVANCE: pose=" + position + " velocity=" + velocity.Linear);
            }
            if (moved && contacted && MathF.Abs(velocity.Linear.X) < .25f)
            {
                if (position.X >= wallPosition.X) throw new Exception("body crossed wall center despite contact");
                body.Velocity = new BodyVelocity2D(Vector2.Zero, 0);
                Log("PASS: real Body2D set/get velocity, XY pose advance, ray/overlap collider identity, contact normal and Static wall response"); finished = true; return;
            }
            if (now - started > 12) { Wait("movement/contact/stop criteria not met; inspect runtime diagnostics and authored placement"); finished = true; }
        }
        catch (Exception failure) { Log("FAIL: " + failure.GetType().Name + ": " + failure.Message); finished = true; }
    }

    private void Prepare(double now)
    {
        var authorBody = Object.GetComponent<Body2D>();
        var authorCollider = Object.GetComponent<Collider2D>();
        if (authorBody is null || authorCollider is null) { Wait("probe owner needs real Body2D and Collider2D"); return; }
        var root = Object; while (root.Parent is GameObject parent) root = parent;
        wall = root.Find("PHY2D Wall");
        if (wall is null) { Wait("create authored object named PHY2D Wall"); return; }
        var wallBody = wall.GetComponent<Body2D>(); var wallCollider = wall.GetComponent<Collider2D>();
        if (wallBody is null || wallCollider is null) { Wait("wall needs real Body2D and Collider2D"); return; }
        var b = authorBody.Value; var c = authorCollider.Value; var wb = wallBody.Value; var wc = wallCollider.Value;
        if (!Object.ActiveInHierarchy || !wall.ActiveInHierarchy) { Wait("body and wall must be active in hierarchy"); return; }
        if (b.Motion != Body2D.MotionOption.Dinamico || wb.Motion != Body2D.MotionOption.Estatico || b.GravityScale != 0)
        { Wait("body Dynamic gravityScale0; wall Static"); return; }
        if (c.Sensor || wc.Sensor || c.Shape != Collider2D.ShapeOption.Caixa || wc.Shape != Collider2D.ShapeOption.Caixa || wc.Restitution != 0)
        { Wait("both colliders Box/non-sensor; wall restitution0"); return; }
        initialPosition = Object.WorldTransform.Position; wallPosition = wall.WorldTransform.Position;
        var gap = wallPosition.X - initialPosition.X;
        if (gap < 2 || gap > 8 || MathF.Abs(wallPosition.Y - initialPosition.Y) > .05f || c.OffsetX != 0 || c.OffsetY != 0 || wc.OffsetX != 0 || wc.OffsetY != 0)
        { Wait("wall 2–8 units to the right at same Y; collider offsets0"); return; }
        var filter = QueryFilter.Default.Ignoring(Object);
        var rayCount = Physics2D.RayCast(new Vector2(initialPosition.X,initialPosition.Y),new Vector2(gap + wc.HalfX + 1,0),hits,filter);
        bool rayWall = false;
        for (var i = 0; i < Math.Min(rayCount,hits.Length); ++i) if (hits[i].Object == wall && hits[i].ColliderInstance == wc.InstanceId)
        {
            var hit = hits[i];
            if (hit.Normal is not Vector2 normal || !float.IsFinite(normal.X) || !float.IsFinite(normal.Y) || MathF.Abs(normal.Length()-1)>.02f || hit.Fraction<0 || hit.Fraction>1 || !float.IsFinite(hit.Distance)) throw new Exception("invalid XY ray hit");
            rayWall = true;
        }
        if (!rayWall) { Wait("ray does not reach authored wall collider; inspect solver diagnostics"); return; }
        var overlapCount = Physics2D.OverlapCircle(new Vector2(wallPosition.X,wallPosition.Y),.1f,hits,filter);bool overlapWall=false;
        for(var i=0;i<Math.Min(overlapCount,hits.Length);++i)if(hits[i].Object==wall&&hits[i].ColliderInstance==wc.InstanceId){if(hits[i].Normal is not null)throw new Exception("overlap invented a surface normal");overlapWall=true;}
        if(!overlapWall){Wait("overlap does not find authored wall; inspect solver diagnostics");return;}
        body=Physics2D.Body(Object);body.Velocity=new BodyVelocity2D(new Vector2(2,0),0);var commanded=body.Velocity;
        if(Vector2.Distance(commanded.Linear,new Vector2(2,0))>.001f||MathF.Abs(commanded.AngularDegrees)>.001f)throw new Exception("set/get velocity did not reach Body2D solver");
        ready=true;started=now;
        Log("READY: body="+ObjectId+" instance="+b.InstanceId+" collider="+c.InstanceId+" wall="+wall.ObjectId+" collider="+wc.InstanceId+" rayHits="+rayCount+" overlapHits="+overlapCount+" velocity="+commanded.Linear);
    }
}
