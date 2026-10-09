using System.Runtime.InteropServices;
using System.Text;
using Astra.Compilation;
using Astra.Editor;

namespace Aether.Tests;

public static unsafe class AnimationAuthoringSdkTests
{
    [Test]
    public static void AuthoringSdkCommandsCrossTheRealNativeResourceAndHistoryBoundary()
    {
        var libraryPath = Environment.GetEnvironmentVariable("ASTRA_AUTHORING_SDK_BRIDGE");
        if (!NativeInterop.AuthoringSdkBridgeAvailable(libraryPath)) return;
        var library = NativeLibrary.Load(libraryPath!);
        var create = (delegate* unmanaged<byte*, int, void*>)NativeLibrary.GetExport(library, "author_fixture_create");
        var access = (delegate* unmanaged<void*, void*>)NativeLibrary.GetExport(library, "author_fixture_access");
        var legacy = (delegate* unmanaged<void*, void*>)NativeLibrary.GetExport(library, "author_fixture_access_v1");
        var legacy2 = (delegate* unmanaged<void*, void*>)NativeLibrary.GetExport(library, "author_fixture_access_v2");
        var legacy3 = (delegate* unmanaged<void*, void*>)NativeLibrary.GetExport(library, "author_fixture_access_v3");
        var end = (delegate* unmanaged<void*, void>)NativeLibrary.GetExport(library, "author_fixture_end");
        var destroy = (delegate* unmanaged<void*, void>)NativeLibrary.GetExport(library, "author_fixture_destroy");
        var history = (delegate* unmanaged<void*, int, int>)NativeLibrary.GetExport(library, "author_fixture_history");
        var sample = (delegate* unmanaged<void*, float, float*, int>)NativeLibrary.GetExport(library, "author_fixture_sample");
        var layout = (delegate* unmanaged<int, int>)NativeLibrary.GetExport(library, "author_fixture_layout");
        var wire = typeof(EditorContext).Assembly.GetType("Astra.Editor.AnimationAuthorAccess", throwOnError: true)!;
        Assert.Equal(Marshal.SizeOf(wire), layout(0), "managed/native function-table layout");
        Assert.Equal(48, layout(1)); Assert.Equal(88, layout(2)); Assert.Equal(24, layout(3));
        Assert.Equal(32, layout(4)); Assert.Equal(24, layout(5));
        Assert.Equal(56, layout(6));
        var root = Path.Combine(Path.GetTempPath(), "astra-author-sdk-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root); void* fixture = null;
        try
        {
            File.WriteAllText(Path.Combine(root, "Tools.cs"), Source);
            Publish(root);
            var catalog = NativeEditorCommands.Discover(root);
            Assert.Equal(3, catalog.Count); Assert.True(catalog.Any(c => c.Id == "Tools::Author"));
            var rootBytes = Encoding.UTF8.GetBytes(root);
            fixed (byte* p = rootBytes) fixture = create(p, rootBytes.Length);
            Assert.True(fixture != null, "production editor fixture opens a real project");
            Assert.Equal(0, Run(root, "Tools::Author", access(fixture)), Report()); end(fixture);
            Assert.Equal(2, history(fixture, 0), "creation and a single committed draft, independent of operation count");
            var values = stackalloc float[2]; Assert.Equal(1, sample(fixture, .6f, values));
            Assert.Close(4, values[0]); Assert.Close(3, values[1], what: "escaped child path reaches the real compositor");
            Assert.Equal(1, history(fixture, 1)); Assert.Equal(1, sample(fixture, .6f, values)); Assert.Close(0, values[0]);
            Assert.Equal(1, history(fixture, 2)); Assert.Equal(1, sample(fixture, .6f, values)); Assert.Close(4, values[0]);
            Assert.Equal(0, Run(root, "Tools::Lifetime", access(fixture)), Report()); end(fixture);
            Assert.Equal(0, Run(root, "Tools::Lifetime", legacy(fixture)), Report()); end(fixture);
            Assert.Equal(0, Run(root, "Tools::Lifetime", legacy2(fixture)), Report()); end(fixture);
            Assert.Equal(0, Run(root, "Tools::Lifetime", legacy3(fixture)), Report()); end(fixture);
            Assert.Equal(2, history(fixture, 0), "lifetime probes do not publish edits");
            Assert.Equal(0, Run(root, "Tools::Consolidate", access(fixture)), Report()); end(fixture);
            Assert.Equal(3, history(fixture, 0), "new composed resource has one independent undo step");
            File.WriteAllText(Path.Combine(root, "Tools.cs"), "public class Broken { invalid syntax }");
            Assert.False(new ProjectCompiler().Build(root).Success);
            Assert.Equal(3, NativeEditorCommands.Discover(root).Count, "unpublished source cannot replace applied tools");
            File.WriteAllText(Path.Combine(root, "Tools.cs"), "using Astra.Editor; public static class Invalid { [EditorCommand(\"Async\")] public static async void Bad(EditorContext c) { await System.Threading.Tasks.Task.Yield(); } }");
            Publish(root);
            Assert.Throws<InvalidDataException>(() => NativeEditorCommands.Discover(root), "async void must be rejected before invocation");
            Assert.Equal("A%2FB/%2E%2E/çΩ", ClipBindingPath.Join("A/B", "..", "çΩ"));
        }
        finally
        {
            if (fixture != null) destroy(fixture); NativeLibrary.Free(library);
            Directory.Delete(root, recursive: true);
        }
    }
    private static void Publish(string root)
    {
        var bytes = Encoding.UTF8.GetBytes(root);
        delegate* unmanaged<byte*, int, int> build = &NativeCompiler.Build;
        delegate* unmanaged<int> commit = &NativeCompiler.Commit;
        fixed (byte* p = bytes) Assert.Equal(0, build(p, bytes.Length), "project command compiles with the real SDK");
        Assert.Equal(0, commit());
    }
    private static int Run(string root, string id, void* access)
    {
        var directory = Encoding.UTF8.GetBytes(root); var command = Encoding.UTF8.GetBytes(id);
        delegate* unmanaged<byte*, int, byte*, int, void*, int> run = &NativeEditorCommands.Run;
        fixed (byte* r = directory) fixed (byte* c = command) return run(r, directory.Length, c, command.Length, access);
    }
    private static string Report()
    {
        delegate* unmanaged<byte*, int, int> copy = &NativeEditorCommands.CopyReport;
        var bytes = new byte[copy(null, 0)]; fixed (byte* p = bytes) copy(p, bytes.Length);
        return Encoding.UTF8.GetString(bytes);
    }
    private const string Source = """
        using System;
        using System.Linq;
        using System.Threading.Tasks;
        using Astra.Editor;
        public static class Tools {
          [EditorCommand("Autorar mecanismo")]
          public static void Author(EditorContext context) {
            var id=context.CreateClip(context.SelectedObject,2);
            if(context.Clips.Count!=1 || context.ImportedClips.Count!=0) throw new Exception("Wrong native catalog");
            using var draft=context.BeginClip(id);
            var root=draft.Snapshot.Tracks.Single(t=>t.Property==ClipProperty.Translation).Id;
            var rotation=draft.Snapshot.Tracks.Single(t=>t.Property==ClipProperty.Rotation).Id;
            var scale=draft.Snapshot.Tracks.Single(t=>t.Property==ClipProperty.Scale).Id;
            draft.PutPose(root,.5f,new float[]{3,0,0});
            var key=draft.Snapshot.Tracks.Single(t=>t.Id==root).Curves[0].Keys.Single(k=>k.Time==.5f);
            draft.PutKey(root,0,key with { Incoming=ClipTangent.Free, Outgoing=ClipTangent.Free, IncomingSlope=1,
              OutgoingSlope=2, IncomingWeight=.2f, OutgoingWeight=.4f, Broken=true, WeightedIncoming=true, WeightedOutgoing=true });
            var edited=draft.Snapshot.Tracks.Single(t=>t.Id==root).Curves[0].Keys.Single(k=>k.Id==key.Id);
            if(!edited.WeightedIncoming || !edited.WeightedOutgoing || !edited.Broken || edited.OutgoingWeight!=.4f || edited.IncomingSlope!=1)
              throw new Exception("Weighted key ABI did not round-trip");
            var address=new ClipKeyAddress(root,0,key.Id);
            using var clipboard=draft.Copy(new[]{address}); draft.Paste(clipboard,.25f); clipboard.Dispose();
            try { draft.Paste(clipboard,.8f); throw new Exception("Disposed clipboard remained live"); } catch(ObjectDisposedException) {}
            var pasted=draft.Snapshot.Tracks.Single(t=>t.Id==root).Curves[0].Keys.Single(k=>k.Time==.25f);
            var selected=new[]{new ClipKeyAddress(root,0,pasted.Id)};
            draft.TransformSelection(selected,offset:.05);
            var split=draft.SplitKey(root,0,.75f); draft.EraseSelection(new[]{new ClipKeyAddress(root,0,split)});
            var splitAgain=draft.SplitKey(root,0,.8f); draft.EraseKey(new ClipKeyAddress(root,0,splitAgain));
            draft.AddTrack(ClipBindingPath.Join("A/B"),"A/B",ClipProperty.Translation,new float[]{3,0,0});
            var morph=draft.AddTrack("","Rotor",ClipProperty.MorphWeights,new float[]{0,0,0,0,0,0});
            draft.PutPose(morph,1,new float[]{.1f,.2f,.3f,.4f,.5f,.6f});
            var weights=draft.Sample(morph,1);
            if(weights.Length!=6 || weights[5]!=.6f) throw new Exception("Morph pose used a fixed four-component buffer");
            draft.RemoveTrack(morph);
            draft.SetRotation(rotation,ClipRotation.ProgressiveQuaternion); draft.SetRotation(rotation,ClipRotation.Quaternion);
            try { draft.SetRotation(rotation,ClipRotation.Euler); throw new Exception("Euler conversion silently lost turns"); }
            catch(InvalidOperationException error) { if(!error.Message.Contains("bake")) throw; }
            if(draft.Snapshot.Tracks.Single(t=>t.Id==rotation).Rotation!=ClipRotation.Quaternion)
              throw new Exception("Refused conversion changed the draft");
            var euler=draft.AddTrack(ClipBindingPath.Join("A/B"),"A/B",ClipProperty.Rotation,new float[]{0,0,0},ClipRotation.Euler);
            draft.PutPose(euler,1,new float[]{0,360,0});
            if(draft.Sample(euler,1).Length!=4) throw new Exception("Euler sampling did not return a runtime quaternion");
            var baked=draft.Bake(euler,new ClipBakeSettings(SampleRate:2,Tolerance:.01,Rotation:ClipRotation.Quaternion));
            if(baked.SampledFrames<=3 || baked.MaximumError>.01 || baked.VerifiedSamples==0 ||
              draft.Snapshot.Tracks.Single(t=>t.Id==euler).Rotation!=ClipRotation.Quaternion)
              throw new Exception("Real bake lost a full turn or ignored verification");
            draft.RemoveTrack(euler);
            draft.RemoveTrack(scale); draft.SetName("Mecanismo \"SDK\""); draft.SetDisplayRate(30);
            draft.Retime(2); draft.Crop(0,2); draft.Reverse(); draft.Reverse();
            var pose=draft.Sample(root,.6f); var curve=draft.SampleCurve(root,0,.6);
            if(Math.Abs(pose[0]-3)>1e-5 || Math.Abs(curve.Value-3)>1e-5) throw new Exception("Native evaluator mismatch");
            var snapshot=draft.Snapshot;
            if(snapshot.Name!="Mecanismo \"SDK\"" || snapshot.DisplayRate!=30 || snapshot.Tracks.Count!=3 || snapshot.Duration!=2)
              throw new Exception("Versioned snapshot lost authoring fields");
            var layer=draft.AddLayer("Correção");
            var offset=draft.AddLayerTrack(layer,root);
            draft.PutPose(offset,0,new float[]{2,0,0}); draft.PutPose(offset,2,new float[]{2,0,0});
            draft.ConfigureLayer(layer,"Correção",weight:.5f);
            if(draft.Sample(offset,.6f)[0]!=2 || Math.Abs(draft.SampleComposed(offset,.6f)[0]-4)>1e-5 || draft.Snapshot.Tracks.Single(t=>t.Id==offset).Layer!=layer)
              throw new Exception("Layer sample and composed pose were conflated");
            var formats=draft.AddLayer("Euler",ClipLayerBlend.Override);
            var eulerLayer=draft.AddLayerTrack(formats,rotation,rotation:ClipRotation.Euler);
            if(draft.Snapshot.Tracks.Single(t=>t.Id==eulerLayer).Rotation!=ClipRotation.Euler) throw new Exception("New layer rotation format ignored");
            draft.RemoveLayer(formats);
            var copy=draft.DuplicateLayer(layer); draft.MoveLayer(copy,1);
            var copiedTrack=draft.Snapshot.Tracks.Single(t=>t.Layer==copy).Id; draft.CopyBaseToTrack(copiedTrack);
            if(Math.Abs(draft.Sample(copiedTrack,.6f)[0]-3)>1e-5) throw new Exception("Base channel copy ignored");
            draft.RemoveLayer(copy);
            draft.ConfigureLayer(layer,"Correção",weight:.5f,muted:true);
            if(Math.Abs(draft.SampleComposed(root,.6f)[0]-3)>1e-5) throw new Exception("Mute ignored by composition");
            draft.ConfigureLayer(layer,"Correção",weight:.5f,solo:true);
            if(Math.Abs(draft.SampleComposed(root,.6f)[0]-1)>1e-5) throw new Exception("Solo did not exclude base motion");
            draft.ConfigureLayer(layer,"Correção",weight:.5f,referenceTime:0);
            if(Math.Abs(draft.SampleComposed(root,.6f)[0]-3)>1e-5) throw new Exception("Reference pose ignored");
            draft.ConfigureLayer(layer,"Correção",weight:.5f);
            draft.Commit();
            using var inspect=context.BeginClip(id);
            try { draft.Reverse(); throw new Exception("Committed draft remained live"); } catch(ObjectDisposedException) {}
          }
          [EditorCommand("Consolidar pela API")]
          public static void Consolidate(EditorContext context) {
            var id=context.Clips[0];var original=context.InspectClip(id);
            using var draft=context.BeginClip(id);
            var rotation=draft.Snapshot.Tracks.First(t=>t.Property==ClipProperty.Rotation).Id;
            try { draft.Bake(rotation,new ClipBakeSettings(Rotation:ClipRotation.Euler));throw new Exception("Missing branch accepted"); }
              catch(InvalidOperationException) {}
            var settings=new ClipBakeSettings(Rotation:ClipRotation.Euler,EulerReference:new ClipEulerReference(360,0,0));
            var report=draft.Bake(rotation,settings);
            if(report.VerifiedSamples==0 || draft.Snapshot.Tracks.Single(t=>t.Id==rotation).Rotation!=ClipRotation.Euler) throw new Exception("Euler conversion failed");
            var result=draft.CreateConsolidated("Consolidado SDK",settings);
            var output=context.InspectClip(result.Clip);
            if(result.Clip==id || output.Layers.Count!=1 || result.Report.VerifiedSamples==0 || context.Clips.Count!=2) throw new Exception("Consolidation not published");
            if(context.InspectClip(id).Revision!=original.Revision) throw new Exception("Source draft was committed implicitly");
          }
          [EditorCommand("Vida do comando")]
          public static void Lifetime(EditorContext context) {
            Task.Run(()=> { try { var owner=context.SelectedObject; throw new Exception("Foreign thread accepted"); }
              catch(InvalidOperationException) {} }).GetAwaiter().GetResult();
            var id=context.Clips[0]; var before=context.InspectClip(id).Revision;
            using(var cancelled=context.BeginClip(id)) cancelled.SetName("Cancelled");
            if(context.InspectClip(id).Revision!=before) throw new Exception("Dispose published a draft");
            context.Dispose();
            try { var owner=context.SelectedObject; throw new Exception("Closed context accepted"); } catch(ObjectDisposedException) {}
          }
        }
        """;
}
