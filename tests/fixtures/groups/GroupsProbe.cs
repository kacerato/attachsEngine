using System;
using System.Linq;
using Astra;

[ComponentId("acceptance.groups")]
public sealed class GroupsProbe : Behavior
{
    public override void Start()
    {
        if(!Object.IsInGroup("guardas") || !Object.Groups.Contains("recebe-dano"))
            throw new InvalidOperationException("Authored groups missing");
        var original=Object.FindGameObjectsInGroup("guardas");
        if(original.Length!=2 || original[0].ObjectId!=ObjectId || original[1].Parent?.ObjectId!=ObjectId)
            throw new InvalidOperationException("Group query must retain tree order and child identity");
        Object.AddToGroup("runtime");Object.AddToGroup("runtime");
        if(Object.Groups.Count(n=>n=="runtime")!=1) throw new InvalidOperationException("Duplicate runtime membership");
        Object.RemoveFromGroup("guardas");
        if(Object.FindGameObjectsInGroup("guardas").Length!=1 || original.Length!=2)
            throw new InvalidOperationException("Query snapshot or removal contract");
        Object.AddToGroup("guardas");Object.SetActive(false);
        if(Object.FindGameObjectsInGroup("guardas").Length!=2 || Object.FindGameObjectsInGroup("guardas",false).Length!=0)
            throw new InvalidOperationException("Inactive hierarchy filter");
        Object.SetActive(true);Object.RemoveFromGroup("runtime");
        Scene.Log(ObjectId,"GROUPS PASS: authored names, tree order, snapshots, idempotent add/remove and inactive hierarchy reached native ABI25");
    }
}
