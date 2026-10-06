using Astra;
using System.Numerics;

[ComponentId("example.gui.canvas-instances")]
public sealed class GuiCanvasInstances : Behavior
{
    private GuiAccess hud = null!, worldPanel = null!;
    private int hudClicks, worldClicks;
    public override void Start()
    {
        hud=Gui.ForCanvas(Object.FindInWorld("HUD")!);
        worldPanel=Gui.ForCanvas(Object.FindInWorld("WorldPanel")!);
        hud.Find("status").Text="HUD: 0 cliques";
        var label=worldPanel.Find("status");
        label.Text="Corpo: 0 cliques";
        label.Style=label.Style with { FontSize=56 };
        label.Layout=label.Layout with { Offsets=new Vector4(40,180,780,280) };
    }
    public override void Update(float seconds)
    {
        Read(hud,"HUD",ref hudClicks);Read(worldPanel,"Corpo",ref worldClicks);
    }
    private static void Read(GuiAccess canvas,string name,ref int clicks)
    {
        while(canvas.Poll(out var message))
            if(message.Kind==GuiEventKind.Click)
                canvas.Find("status").Text=$"{name}: {++clicks} cliques";
    }
}
