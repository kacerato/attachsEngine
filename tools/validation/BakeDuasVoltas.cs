using System;
using System.Linq;
using Astra.Editor;

// Explicit editor command for the isolated physical acceptance project.
// Uses a generic selected root; no player, Behavior or Play dependency.
public static class BakeDuasVoltas
{
    [EditorCommand("Bake de duas voltas")]
    public static void Executar(EditorContext editor)
    {
        var guid = editor.CreateClip(editor.SelectedObject, 2, ClipRotation.Euler);
        using var edit = editor.BeginClip(guid);
        var rotation = edit.Snapshot.Tracks.Single(t => t.Property == ClipProperty.Rotation);
        edit.PutPose(rotation.Id, 0, new float[] { 0, 0, 0 });
        edit.PutPose(rotation.Id, 2, new float[] { 0, 720, 0 });
        for (var frame = 1; frame < 60; ++frame)
            edit.PutPose(rotation.Id, frame / 30f, new float[] { 0, frame * 12, 0 });
        var report = edit.Bake(rotation.Id, new ClipBakeSettings(
            SampleRate: 2, Tolerance: .01, Rotation: ClipRotation.Quaternion));
        if (report.SampledFrames <= 5 || report.VerifiedSamples == 0 || report.MaximumError > .01 ||
            report.OutputKeys >= report.InputKeys)
            throw new InvalidOperationException("Bake perdeu voltas ou não verificou a pose.");
        edit.SetName("Duas voltas · Bake SDK");
        edit.Commit();
    }
}
