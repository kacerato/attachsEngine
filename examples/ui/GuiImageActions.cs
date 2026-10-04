using Astra;

// Attach to a scene object, with behavior.aeui as UI/main.aeui.
// Images retain their own resources, style and layout; actions are authored in the UI file.
[ComponentId("example.gui.image-actions")]
public sealed class GuiImageActions : Behavior
{
    private GuiElement status,notify;
    private int clicks;
    public override void Start()
    {
        status=Gui.Find("status");notify=Gui.Find("notify_image");
        status.Text="Clique nas imagens: abrir / animar / evento C#";
    }
    public override void Update(float elapsed)
    {
        while(Gui.Poll(out var message))
        {
            if(message.Kind!=GuiEventKind.Click)continue;
            ++clicks;
            status.Text=$"{clicks} cliques | {message.Element.Name} | {message.Element.Kind}";
            if(message.Element.Id==notify.Id)
            {
                // An arbitrary script reaction can coexist with editor-authored actions.
                var image=Gui.Find("animate_image");
                image.Animation=image.Animation with {Duration=.6f};
                image.PlayAnimation();
            }
        }
        if(Gui.Diagnostic.Length!=0)status.Text=Gui.Diagnostic;
    }
}
