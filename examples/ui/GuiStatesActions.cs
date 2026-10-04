using Astra;

[ComponentId("example.gui.states-actions")]
public sealed class GuiStatesActions : Behavior
{
    private GuiElement status;
    private int clicks;
    public override void Start()
    {
        status=Gui.Find("status");var image=Gui.Find("open_image");
        var actions=image.Actions;
        // Exercise native list mutation without changing the authored scene or its final order.
        image.AddAction(GuiEventKind.Click,GuiClickAction.Notify);
        image.RemoveAction(image.Actions.Length-1);
        if(actions.Length>1){image.MoveAction(0,1);image.MoveAction(1,0);image.ReplaceAction(0,actions[0]);}
        image.Transitions=image.Transitions with {Enabled=true,Duration=.3f};
        status.Text=$"API: {image.Actions.Length} acoes adicionais | estados: {image.Transitions.Enabled}";
    }
    public override void Update(float elapsed)
    {
        while(Gui.Poll(out var message))
        {
            if(message.Kind==GuiEventKind.Click)status.Text=$"{++clicks} cliques | {message.Element.Name} | {message.Element.Kind}";
        }
        if(Gui.Diagnostic.Length!=0)status.Text=Gui.Diagnostic;
    }
}
