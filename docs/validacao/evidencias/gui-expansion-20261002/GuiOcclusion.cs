using Astra;
using System.Numerics;
[ComponentId("example.gui.menu")]
public sealed class GuiMenu : Behavior
{
    private GuiElement start;
    private GameObject occluder = null!;
    public override void Start()
    {
        start=Gui.Find("start");
        Gui.Find("banner").ImageStyle=new(GuiImageFit.Contain,0xFFFFFFFF);
        occluder=Object.CreatePrimitive(PrimitiveType.Cube);
        occluder.LocalTransform=new(new Vector3(1,1,-1),Quaternion.Identity,new Vector3(1.1f,.8f,.35f));
        occluder.SetActive(false);
    }
    public override void Update(float elapsed)
    {
        while(Gui.Poll(out var e))
        {
            if(e.Kind!=GuiEventKind.Click)continue;
            var name=e.Element.Name;
            if(name=="start")start.Text="Pronto";
            if(name=="world"){Gui.Canvas=Gui.Canvas with {Mode=GuiCanvasMode.World};occluder.SetActive(true);}
            if(name=="screen"){Gui.Canvas=Gui.Canvas with {Mode=GuiCanvasMode.Screen};occluder.SetActive(false);}
            if(name=="reflow")occluder.SetActive(false);
        }
    }
}

