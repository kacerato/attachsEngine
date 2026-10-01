using System;
using System.Numerics;
using Astra;
using Astra.Components;
using NativePath = Astra.Components.PathComponent;

[ComponentId("acceptance.path-follow")]
public sealed class PathProbe : Behavior
{
    private CurvePath curve;
    private PathFollower playback;
    private GameObject? followerObject;
    private string waitReason = "";
    private double nextSearch, stageStarted, baselineProgress;
    private Vector3 baselinePosition;
    private int stage;
    private bool ready, finished;

    private void Log(string message) => Scene.Log(ObjectId, "PATH " + message);
    private void Wait(string reason)
    {
        if (waitReason == reason) return;
        waitReason = reason;
        Log("WAIT: " + reason);
    }

    public override void Start() => Wait("finding authored Path and PathFollow; no objects will be created");

    public override void Update(float dt)
    {
        if (finished) return;
        try
        {
            var now = Time.TimeSinceStart;
            if (!ready)
            {
                if (now < nextSearch) return;
                nextSearch = now + .5;
                FindAuthoredPair(now);
                return;
            }
            if (followerObject is null || !followerObject.IsAlive)
            {
                Wait("selected follower was removed; acceptance did not finish"); finished = true; return;
            }
            var progress = playback.Progress;
            var position = followerObject.WorldTransform.Position;
            if (stage == 0)
            {
                if (now - stageStarted < .3) return;
                if (Math.Abs(progress - baselineProgress) < .001 || Vector3.Distance(position, baselinePosition) < .001)
                {
                    if (now - stageStarted > 10) { Wait("no real progress/pose advance; inspect PathFollow runtime diagnostics"); finished = true; }
                    return;
                }
                Log("ADVANCE: progress=" + progress + " playing=" + playback.IsPlaying + " pose=" + position);
                playback.Stop();
                if (playback.IsPlaying) throw new Exception("Stop left playback request enabled");
                baselineProgress = playback.Progress; baselinePosition = position;
                stage = 1; stageStarted = now;
                Log("STOP: progress=" + baselineProgress + " playing=" + playback.IsPlaying);
            }
            else if (stage == 1)
            {
                if (Math.Abs(progress - baselineProgress) > .0001 || Vector3.Distance(position, baselinePosition) > .0001)
                    throw new Exception("stopped follower changed progress or pose");
                if (now - stageStarted < .4) return;
                playback.Restart();
                if (!playback.IsPlaying) throw new Exception("Restart did not enable playback request");
                baselineProgress = playback.Progress;
                stage = 2; stageStarted = now;
                Log("RESTART: progress=" + baselineProgress + " playing=" + playback.IsPlaying);
            }
            else if (stage == 2)
            {
                if (now - stageStarted < .3) return;
                if (Math.Abs(progress - baselineProgress) < .001)
                {
                    if (now - stageStarted > 10) { Wait("restart did not produce real advance; inspect diagnostics"); finished = true; }
                    return;
                }
                Log("PASS: ABI22 point identities/world sample/advance/Stop hold/Restart advance; progress=" + progress + " playing=" + playback.IsPlaying);
                finished = true;
            }
        }
        catch (Exception failure)
        {
            Log("FAIL: " + failure.GetType().Name + ": " + failure.Message);
            finished = true;
        }
    }

    private void FindAuthoredPair(double now)
    {
        var root = Object;
        while (root.Parent is GameObject parent) root = parent;
        var followers = root.GetComponentsInChildren<PathFollow>(true);
        if (followers.Length == 0) { Wait("author a PathFollow and assign a Path target, then restart Play"); return; }
        var reason = "no usable authored PathFollow→Path pair; configure target and inspect diagnostics";
        foreach (var follow in followers)
        {
            var target = Resolve(follow.Target);
            if (target is null) continue;
            var path = target.GetComponent<NativePath>();
            if (path is null) continue;
            if (!follow.Enabled || !follow.Object.ActiveInHierarchy) { reason = "PathFollow must be enabled and active"; continue; }
            if (follow.ProgressDistance != 0 || (follow.Mode == PathFollow.ModeOption.VelocidadeMundial && follow.Speed <= 0))
            { reason = "set initial distance=0 and positive speed, or use Duration mode"; continue; }
            curve = new CurvePath(path.Value.Component);
            if (curve.Count < 2) { reason = "Path needs at least two noncoincident points"; continue; }
            var first = curve.At(0);
            var firstById = curve.ById(first.Id);
            if (first.Id == 0 || first != firstById) throw new Exception("point lookup by durable ID disagrees with index");
            var start = curve.Sample(0);
            if (start.Length <= .01) { reason = "world curve length must exceed .01 units"; continue; }
            var middle = curve.Sample(start.Length * .5);
            if (!float.IsFinite(middle.Position.X) || !float.IsFinite(middle.Position.Y) || !float.IsFinite(middle.Position.Z)
                || MathF.Abs(middle.Tangent.Length() - 1) > .02f) throw new Exception("invalid world sample/tangent");
            var ids = "";
            for (var i = 0; i < curve.Count; ++i)
            {
                var point = curve.At(i);
                if (point.Id == 0 || curve.ById(point.Id) != point) throw new Exception("invalid point identity");
                ids += (i == 0 ? "" : ",") + point.Id;
            }
            playback = new PathFollower(follow.Component);
            followerObject = follow.Object;
            Log("READY: path=" + target.ObjectId + " instance=" + path.Value.InstanceId + " follower=" + follow.Object.ObjectId
                + " instance=" + follow.InstanceId + " points=" + curve.Count + " ids=[" + ids + "] worldLength=" + start.Length
                + " middle=" + middle.Position + " tangent=" + middle.Tangent + " progress=" + playback.Progress + " playing=" + playback.IsPlaying);
            // Commands affect only the Play GameWorld. Authored points and properties are never written.
            playback.Restart();
            baselineProgress = playback.Progress;
            baselinePosition = followerObject.WorldTransform.Position;
            stage = 0; stageStarted = now; ready = true;
            return;
        }
        Wait(reason);
    }
}
