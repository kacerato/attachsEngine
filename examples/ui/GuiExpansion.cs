using Astra;
// Use with layouts.aeui; replaces GuiMenu.cs in the example project.
[ComponentId("example.gui.menu")]
public sealed class GuiMenu : Behavior
{
    private GuiElement start,volume,audio,banner;
    public override void Start()
    {
        start=Gui.Find("start");volume=Gui.Find("volume");audio=Gui.Find("audio");banner=Gui.Find("banner");
        banner.ImageStyle=banner.ImageStyle with {Fit=GuiImageFit.Contain,Tint=0xFFFFFFFF};
    }
    public override void Update(float elapsed)
    {
        while(Gui.Poll(out var message))
        {
            if(message.Kind==GuiEventKind.Click)
            {
                var name=message.Element.Name;
                if(name=="start")start.Text="Pronto";
                if(name=="world")Gui.Canvas=Gui.Canvas with {Mode=GuiCanvasMode.World};
                if(name=="screen")Gui.Canvas=Gui.Canvas with {Mode=GuiCanvasMode.Screen};
                if(name=="reflow") {var row=Gui.Find("actions");row.Sizing=row.Sizing with {Columns=row.Sizing.Columns==3?2u:3u};}
            }
            if(message.Kind==GuiEventKind.ValueChanged && message.Element.Id==volume.Id)volume.Text=$"Volume: {message.Value:P0}";
            if(message.Kind==GuiEventKind.ValueChanged && message.Element.Id==audio.Id)volume.Enabled=message.Value!=0;
        }
    }
}
