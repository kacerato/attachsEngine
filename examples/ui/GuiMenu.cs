using Astra;

// Add this behavior to an object, and copy main.aeui to the project's UI/main.aeui.
[ComponentId("example.gui.menu")]
public sealed class GuiMenu : Behavior
{
    private GuiElement volume,audio,start;
    public override void Start()
    {
        volume=Gui.Find("volume");audio=Gui.Find("audio");start=Gui.Find("start");
        volume.Text="Volume";
    }
    public override void Update(float elapsed)
    {
        while(Gui.Poll(out var message))
        {
            if(message.Kind==GuiEventKind.Click && message.Element.Id==start.Id) start.Text="Pronto";
            if(message.Kind==GuiEventKind.ValueChanged && message.Element.Id==volume.Id)
                volume.Text=$"Volume: {message.Value:P0}";
            if(message.Kind==GuiEventKind.ValueChanged && message.Element.Id==audio.Id)
                volume.Enabled=message.Value!=0;
        }
    }
}
