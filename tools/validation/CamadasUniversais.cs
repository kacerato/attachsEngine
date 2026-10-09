using System;
using System.Linq;
using Astra.Editor;

// Physical acceptance tool for the isolated mechanism project. No player preset.
public static class CamadasUniversais
{
    [EditorCommand("Criar camadas universais")]
    public static void Criar(EditorContext editor)
    {
        var guid = editor.CreateClip(editor.SelectedObject, 2, ClipRotation.Euler);
        using var edit = editor.BeginClip(guid);
        var position = edit.Snapshot.Tracks.Single(t => t.Property == ClipProperty.Translation).Id;
        var rotation = edit.Snapshot.Tracks.Single(t => t.Property == ClipProperty.Rotation).Id;
        var scale = edit.Snapshot.Tracks.Single(t => t.Property == ClipProperty.Scale).Id;
        edit.PutPose(position, 0, new float[] { 0, 0, 0 });
        edit.PutPose(position, 2, new float[] { 2, 0, 0 });
        edit.PutPose(rotation, 0, new float[] { 0, 0, 0 });
        edit.PutPose(rotation, 2, new float[] { 0, 720, 0 });
        edit.PutPose(scale, 0, new float[] { 1, 1, 1 });
        edit.PutPose(scale, 2, new float[] { 1, 1, 1 });
        var correction = edit.AddLayer("Correção SDK");
        var offset = edit.AddLayerTrack(correction, position);
        edit.PutPose(offset, 0, new float[] { 0, 40, 0 });
        edit.PutPose(offset, 2, new float[] { 0, 40, 0 });
        var relativeScale = edit.AddLayerTrack(correction, scale);
        edit.PutPose(relativeScale, 0, new float[] { 2, 2, 2 });
        edit.PutPose(relativeScale, 2, new float[] { 2, 2, 2 });
        edit.ConfigureLayer(correction, "Correção SDK", weight: .5f);
        Near(edit.Sample(offset, 1)[1], 40, "raw channel");
        Near(edit.SampleComposed(position, 1)[1], 20, "additive position");
        Near(edit.SampleComposed(scale, 1)[0], 1.5f, "additive scale");
        edit.ConfigureLayer(correction, "Correção SDK", weight: .5f, muted: true);
        Near(edit.SampleComposed(position, 1)[1], 0, "mute");
        edit.ConfigureLayer(correction, "Correção SDK", weight: .5f, solo: true);
        Near(edit.SampleComposed(position, 1)[0], 0, "solo freezes base");
        edit.ConfigureLayer(correction, "Correção SDK", weight: .5f, referenceTime: 0);
        Near(edit.SampleComposed(position, 1)[1], 0, "temporal reference");
        var duplicate = edit.DuplicateLayer(correction);
        edit.MoveLayer(duplicate, 1);
        var duplicateTrack = edit.Snapshot.Tracks.Single(t => t.Layer == duplicate && t.Property == ClipProperty.Translation).Id;
        edit.CopyBaseToTrack(duplicateTrack);
        Near(edit.Sample(duplicateTrack, 2)[0], 2, "copy base");
        edit.RemoveLayer(duplicate);
        edit.ConfigureLayer(correction, "Correção SDK", weight: .5f);
        edit.SetName("Camadas SDK · mecanismo");
        edit.Commit();
        ConferirClipe(editor, guid);
    }

    [EditorCommand("Conferir camadas salvas")]
    public static void Conferir(EditorContext editor)
    {
        var found = 0;
        foreach (var guid in editor.Clips)
            if (editor.InspectClip(guid).Name == "Camadas SDK · mecanismo")
            {
                ConferirClipe(editor, guid);
                ++found;
            }
        if (found != 1) throw new InvalidOperationException("Esperado um clipe de aceite salvo.");
    }

    private static void ConferirClipe(EditorContext editor, Astra.AssetGuid guid)
    {
        using var edit = editor.BeginClip(guid);
        var snapshot = edit.Snapshot;
        var correction = snapshot.Layers.Single(l => l.Name == "Correção SDK");
        if (snapshot.Layers.Count != 2 || correction.Weight != .5f || correction.Muted || correction.Solo || correction.ReferenceTime != null)
            throw new InvalidOperationException("Metadados de camada não foram preservados.");
        var position = snapshot.Tracks.Single(t => t.Layer == 0 && t.Property == ClipProperty.Translation).Id;
        var scale = snapshot.Tracks.Single(t => t.Layer == 0 && t.Property == ClipProperty.Scale).Id;
        Near(edit.SampleComposed(position, 1)[0], 1, "base movement");
        Near(edit.SampleComposed(position, 1)[1], 20, "persisted additive position");
        Near(edit.SampleComposed(scale, 1)[0], 1.5f, "persisted additive scale");
    }

    private static void Near(float actual, float expected, string operation)
    {
        if (!float.IsFinite(actual) || Math.Abs(actual - expected) > .0001f)
            throw new InvalidOperationException(operation + ": " + actual + " != " + expected);
    }
}
