using System;
using System.Linq;
using Astra.Editor;

public static class ClipCuesAuthoring
{
    [EditorCommand("Criar cues SDK Android")]
    public static void Create(EditorContext editor)
    {
        var guid = editor.Clips.Single(g => editor.InspectClip(g).Name == "Ciclo de eventos");
        using var draft = editor.BeginClip(guid);
        foreach (var cue in draft.Snapshot.Cues.Where(c => c.Name.StartsWith("SDK Android", StringComparison.Ordinal)).ToArray())
            draft.RemoveCue(cue.Id);
        var id = draft.PutCue(new(0, .6f, ClipCueKind.Event, "SDK Android evento", 16777215, 1e100,
            Forward: false, Reverse: true, Enabled: false));
        var marker = draft.PutCue(new(0, 1.4f, ClipCueKind.Marker, "SDK Android marco"));
        var point = draft.Snapshot.Cues.Single(c => c.Id == id);
        if (point.Value != 1e100 || point.Tag != 16777215 || point.Forward || !point.Reverse || point.Enabled || marker == id)
            throw new InvalidOperationException("Draft perdeu tipo, precisão ou identidade.");
        draft.Commit();
        Verify(editor);
    }

    [EditorCommand("Conferir cues SDK salvos")]
    public static void Verify(EditorContext editor)
    {
        var clip = editor.InspectClip(editor.Clips.Single(g => editor.InspectClip(g).Name == "Ciclo de eventos"));
        var point = clip.Cues.Single(c => c.Name == "SDK Android evento");
        var marker = clip.Cues.Single(c => c.Name == "SDK Android marco");
        var touch = clip.Cues.Single(c => c.Name == "ToqueAndroid");
        var original = clip.Cues.Single(c => c.Name == "Trava");
        if (point.Kind != ClipCueKind.Event || point.Value != 1e100 || point.Tag != 16777215 ||
            point.Time != .6f || point.Forward || !point.Reverse || point.Enabled ||
            marker.Kind != ClipCueKind.Marker || marker.Time != 1.4f)
            throw new InvalidOperationException("Cues salvos divergiram do contrato SDK.");
        if (touch.Tag != 99 || touch.Value != 3.75 || touch.Time != .25f ||
            touch.Enabled || touch.Forward || !touch.Reverse || original.Time != .25f ||
            clip.Cues.Any(c => c.Name == "Marcador") || clip.Cues.Count != 6)
            throw new InvalidOperationException("Autoria por toque ou undo perdeu dados.");
    }
}
