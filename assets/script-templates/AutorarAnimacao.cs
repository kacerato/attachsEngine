using System;
using System.Linq;
using System.Numerics;
using Astra.Editor;

// Selecione um objeto, compile e abra IDE > Ferramentas do editor.
// O recurso fica editável no Animation Studio; sua criação e edição têm Undo.
// Uma ferramenta roda em Edit, sem anexar Behavior nem entrar em Play.
public static class AutorarAnimacao
{
    [EditorCommand("Criar giro no objeto")]
    public static void CriarGiro(EditorContext editor)
    {
        var owner = editor.SelectedObject;
        if (owner == 0) throw new InvalidOperationException("Selecione um objeto na cena.");
        var clip = editor.CreateClip(owner, 2);
        using var edit = editor.BeginClip(clip);
        var rotation = edit.Snapshot.Tracks.Single(t => t.Property == ClipProperty.Rotation).Id;
        var initial = edit.Sample(rotation, 0);
        var start = new Quaternion(initial[0], initial[1], initial[2], initial[3]);
        var turned = Quaternion.Normalize(start * Quaternion.CreateFromAxisAngle(Vector3.UnitY, MathF.PI / 2));
        edit.PutPose(rotation, 1, new[] { turned.X, turned.Y, turned.Z, turned.W });
        edit.PutPose(rotation, 2, initial);
        edit.SetName("Giro do objeto");
        edit.Commit();
    }
}
