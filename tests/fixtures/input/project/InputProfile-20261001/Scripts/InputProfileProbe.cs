using System;
using System.IO;
using Astra;

[ComponentId("acceptance.input.profile")]
public sealed class InputProfileProbe : Behavior
{
    public override void Start()
    {
        // Dedicated acceptance project only. Games select their own user storage.
        const string path="/storage/emulated/0/Android/data/dev.aether.editor/files/Projetos/InputProfile-20261001/UserData/input.profile";
        var authored=Input.GetBinding("Saltar",0,true);
        if(!Input.ApplyBindingOverride("Saltar",0,new InputBindingValue(InputSource.Key,62)))
            throw new InvalidOperationException("Cannot apply player override");
        Input.SaveProfile(path);
        Input.RemoveAllBindingOverrides();
        if(Input.GetBinding("Saltar").Source!=authored.Source)
            throw new InvalidOperationException("Restore did not recover authored source");
        if(!Input.LoadProfile(path) || Input.GetBinding("Saltar").Source!=InputSource.Key || Input.GetBinding("Saltar").Code!=62)
            throw new InvalidOperationException("Saved profile did not restore key override");
        if(Input.ImportProfile(File.ReadAllText(path)+"invalid") || Input.GetBinding("Saltar").Code!=62)
            throw new InvalidOperationException("Corrupt profile changed effective binding");
        Scene.Log(ObjectId,"PROFILE PASS: saved file, defaults restored, profile reloaded, corruption rejected");
    }
    public override void Update(float delta)
    {
        if(Input.JustPressed("Saltar"))Scene.Log(ObjectId,"PROFILE INPUT: persisted key override reached C#");
    }
}
