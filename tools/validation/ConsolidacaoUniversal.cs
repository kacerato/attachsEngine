using System;
using System.Linq;
using System.Numerics;
using Astra;
using Astra.Editor;

// Isolated physical acceptance. The source resource and its layers remain untouched.
public static class ConsolidacaoUniversal
{
    const string Source = "Camadas SDK · mecanismo", Result = "Consolidado SDK · mecanismo";
    [EditorCommand("Consolidar e conferir 401 poses")]
    public static void Criar(EditorContext editor)
    {
        var source = editor.Clips.Single(g => editor.InspectClip(g).Name == Source);
        var original = editor.InspectClip(source);
        using var draft = editor.BeginClip(source);
        var rotation = original.Tracks.Single(t => t.Layer == 0 && t.Property == ClipProperty.Rotation).Id;
        var expected = Enumerable.Range(0, 401).Select(i => draft.SampleComposed(rotation, i / 200f)).ToArray();
        draft.Bake(rotation, new(Rotation: ClipRotation.Quaternion));
        bool refused = false;
        try { draft.Bake(rotation, new(Rotation: ClipRotation.Euler)); }
        catch (InvalidOperationException) { refused = true; }
        if (!refused) throw new InvalidOperationException("A conversão escolheu um ramo sem confirmação.");
        draft.Bake(rotation, new(Rotation: ClipRotation.Euler, EulerReference: new(0, 360, 0)));
        for (int i = 0; i < expected.Length; ++i) Same(expected[i], draft.SampleComposed(rotation, i / 200f));
        var published = draft.CreateConsolidated(Result);
        if (published.Report.VerifiedSamples == 0 || editor.InspectClip(source).Revision != original.Revision)
            throw new InvalidOperationException("Publicação sem verificação ou com mutação da origem.");
        Conferir(editor);
    }

    [EditorCommand("Conferir consolidação salva")]
    public static void Conferir(EditorContext editor)
    {
        var source = editor.Clips.Single(g => editor.InspectClip(g).Name == Source);
        var result = editor.Clips.Single(g => editor.InspectClip(g).Name == Result);
        using var authored = editor.BeginClip(source);
        using var baked = editor.BeginClip(result);
        if (baked.Snapshot.Layers.Count != 1 || baked.Snapshot.Tracks.Any(t => t.Layer != 0))
            throw new InvalidOperationException("O resultado ainda depende de camadas autorais.");
        foreach (var track in authored.Snapshot.Tracks.Where(t => t.Layer == 0))
        {
            var target = baked.Snapshot.Tracks.Single(t => t.Binding == track.Binding && t.Property == track.Property).Id;
            for (int i = 0; i <= 400; ++i)
            {
                var a = authored.SampleComposed(track.Id, i / 200f);
                var b = baked.SampleComposed(target, i / 200f);
                if (track.Property == ClipProperty.Rotation) Same(a, b);
                else for (int c = 0; c < a.Length; ++c)
                    if (Math.Abs(a[c] - b[c]) > .011f) throw new InvalidOperationException("Canal consolidado divergiu.");
            }
        }
    }
    static void Same(float[] a, float[] b)
    {
        var x = Quaternion.Normalize(new(a[0], a[1], a[2], a[3]));
        var y = Quaternion.Normalize(new(b[0], b[1], b[2], b[3]));
        if (!float.IsFinite(x.X) || !float.IsFinite(y.X) || Math.Abs(Quaternion.Dot(x, y)) < .999999f)
            throw new InvalidOperationException("Orientação convertida divergiu.");
    }
}
