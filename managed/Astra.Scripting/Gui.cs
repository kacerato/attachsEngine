using System.Numerics;
namespace Astra;

public enum GuiKind : uint { Panel, Text, Button, Toggle, Slider, Progress, Image, HBox, VBox, Grid }
public enum GuiEventKind : uint { Click = 1, ValueChanged = 2 }
public readonly record struct GuiSnapshot(uint World,uint Id,GuiKind Kind,bool Visible,bool Enabled,
    float Value,float Minimum,float Maximum);
public readonly record struct GuiEvent(GuiElement Element,GuiEventKind Kind,float Value);
public readonly record struct GuiLayout(Vector2 AnchorMin,Vector2 AnchorMax,Vector4 Offsets);
/// <summary>Colors use 0xAARRGGBB, matching the native editor and .aeui file.</summary>
public readonly record struct GuiStyle(uint Background,uint Foreground,uint Accent,bool ClipChildren,float FontSize,float Radius);

public enum GuiAlignment : uint { Start, Center, End, Stretch }
public enum GuiImageFit : uint { Stretch, Contain, Cover }
public enum GuiCanvasMode : uint { Screen, World }
public enum GuiClickAction : uint { Notify, ToggleVisible, ToggleEnabled, SetValue, PlayAnimation, StopAnimation }
public enum GuiEasing : uint { Linear, Smooth, EaseIn, EaseOut }
/// <summary>Target is a stable ID in the same UI document; zero means this element.</summary>
public readonly record struct GuiInteraction(bool Clickable,GuiClickAction Action=GuiClickAction.Notify,uint Target=0,float Value=0);
public readonly record struct GuiPose(Vector2 Position,float Scale,float Opacity) {
    public static GuiPose Identity => new(Vector2.Zero,1,1);
}
/// <summary>One authored pose transition. Stop restores layout; completion holds the final pose.
/// PingPong runs both directions; Loop repeats after the one-time Delay. Children inherit pose.</summary>
public readonly record struct GuiAnimation(bool Enabled,bool AutoPlay,bool Loop,bool PingPong,GuiEasing Easing,
    float Duration,float Delay,GuiPose From,GuiPose To) {
    public static GuiAnimation Default => new(false,false,false,false,GuiEasing.Smooth,.3f,0,GuiPose.Identity,GuiPose.Identity);
}
public interface IGuiBehaviorAccess {
    bool GuiBehavior(uint world,uint node,bool write,ref GuiInteraction interaction,ref GuiAnimation animation);
    string ReadGuiDiagnostic(uint world);
}
/// <summary>Additional listeners execute in order after the primary OnClick action.</summary>
public readonly record struct GuiActionBinding(GuiEventKind Event,GuiClickAction Action,uint Target=0,float Value=0);
public readonly record struct GuiStateStyle(GuiPose Pose,uint Tint=0xFFFFFFFF) {
    public static GuiStateStyle Identity => new(GuiPose.Identity);
}
public readonly record struct GuiTransitions(bool Enabled,float Duration,GuiEasing Easing,GuiStateStyle Normal,GuiStateStyle Pressed,GuiStateStyle Disabled) {
    public static GuiTransitions Default => new(false,.12f,GuiEasing.Smooth,GuiStateStyle.Identity,new(new(Vector2.Zero,.94f,1)),new(new(Vector2.Zero,1,.45f)));
}
public interface IGuiStateAccess {
    int GuiAction(uint world,uint node,uint operation,uint index,ref GuiActionBinding binding);
    bool GuiTransitions(uint world,uint node,bool write,ref GuiTransitions transitions);
}
public readonly record struct GuiSizing(Vector2 Minimum,Vector2 Preferred,Vector2 Flexible,Vector4 Padding,Vector2 Spacing,GuiAlignment Alignment,uint Columns,bool Ignore);
public readonly record struct GuiImageStyle(GuiImageFit Fit,uint Tint);
public readonly record struct GuiCanvas(GuiCanvasMode Mode,Vector2 Resolution,Vector3 Position,Vector3 Rotation,float UnitsPerPixel,bool Occlusion);
public interface IGuiAdvancedAccess {
    bool GuiSizing(uint world,uint node,bool write,ref GuiSizing sizing,ref GuiImageStyle image);
    bool GuiCanvas(uint world,bool write,ref GuiCanvas canvas);
    string ReadGuiImage(uint world,uint node);
}

public interface IGuiAccess
{
    bool GuiCommand(uint world,uint node,uint operation,string text,float value,
        GuiKind createKind,out GuiSnapshot snapshot,out GuiEventKind eventKind);
    bool GuiProperties(uint world,uint node,bool write,ref GuiLayout layout,ref GuiStyle style);
    string ReadGuiText(uint world,uint node,bool name);
}

/// <summary>UI in this Play session. Preview/Play owns a copy of the authored .aeui.</summary>
public sealed class GuiAccess(ISceneAccess scene)
{
    private IGuiAccess Backend => scene as IGuiAccess ?? throw new NotSupportedException("Host does not provide game UI.");
    public string Diagnostic => scene is IGuiBehaviorAccess backend?backend.ReadGuiDiagnostic(scene.WorldId):throw new NotSupportedException("Host does not provide UI diagnostics.");
    public GuiCanvas Canvas {
        get {var c=default(GuiCanvas);if(scene is not IGuiAdvancedAccess advanced || !advanced.GuiCanvas(scene.WorldId,false,ref c))throw new WorldException(scene.LastStatus,"read canvas");return c;}
        set {if(scene is not IGuiAdvancedAccess advanced || !advanced.GuiCanvas(scene.WorldId,true,ref value))throw new WorldException(scene.LastStatus,"write canvas");}
    }
    public bool TryFind(string name,out GuiElement element)
    {
        ArgumentException.ThrowIfNullOrEmpty(name);
        if(!Backend.GuiCommand(0,0,0,name,0,default,out var snapshot,out _)) { element=default;return false; }
        element=new(scene,snapshot.World,snapshot.Id);return true;
    }
    public GuiElement Find(string name) => TryFind(name,out var element) ? element : throw new WorldException(scene.LastStatus,"find UI");
    public GuiElement Create(GuiKind kind,GuiElement parent=default,string name="")
    {
        if(!Enum.IsDefined(kind)) throw new ArgumentOutOfRangeException(nameof(kind));
        if(parent.Id!=0 && (!ReferenceEquals(parent.Scene,scene) || parent.World!=scene.WorldId))
            throw new WorldException(WorldStatus.ForeignWorld,"create UI");
        if(!Backend.GuiCommand(scene.WorldId,parent.Id,7,name,0,kind,out var snapshot,out _))
            throw new WorldException(scene.LastStatus,"create UI");
        return new(scene,snapshot.World,snapshot.Id);
    }
    public bool Poll(out GuiEvent message)
    {
        if(!Backend.GuiCommand(scene.WorldId,0,6,"",0,default,out var snapshot,out var kind)) {
            message=default;
            if(scene.LastStatus!=WorldStatus.Ok) throw new WorldException(scene.LastStatus,"poll UI");
            return false;
        }
        message=new(new(scene,snapshot.World,snapshot.Id),kind,snapshot.Value);return true;
    }
}

/// <summary>Stable ID and world identity; removed nodes and previous Play handles are rejected.</summary>
public readonly struct GuiElement
{
    internal ISceneAccess? Scene { get; }
    internal uint World { get; }
    public uint Id { get; }
    internal GuiElement(ISceneAccess scene,uint world,uint id) { Scene=scene;World=world;Id=id; }
    private GuiSnapshot Command(uint operation,string text="",float value=0)
    {
        if(Scene is not IGuiAccess backend || Id==0) throw new InvalidOperationException("Uninitialized UI handle.");
        if(!backend.GuiCommand(World,Id,operation,text,value,default,out var snapshot,out _))
            throw new WorldException(Scene.LastStatus,"UI command");
        return snapshot;
    }
    public GuiSnapshot Snapshot => Command(1);
    public GuiKind Kind => Snapshot.Kind;
    public float Value { get => Snapshot.Value; set { if(!float.IsFinite(value)) throw new ArgumentOutOfRangeException(nameof(value));Command(3,value:value); } }
    public bool Visible { get => Snapshot.Visible; set => Command(4,value:value?1:0); }
    public bool Enabled { get => Snapshot.Enabled; set => Command(5,value:value?1:0); }
    public void SetText(string text) { ArgumentNullException.ThrowIfNull(text);Command(2,text); }
    public string Text { get => Backend.ReadGuiText(World,Id,false);set => SetText(value); }
    public string Name { get => Backend.ReadGuiText(World,Id,true);set { ArgumentException.ThrowIfNullOrEmpty(value);Command(9,value); } }
    private IGuiAccess Backend => Scene as IGuiAccess ?? throw new InvalidOperationException("Uninitialized UI handle.");
    private (GuiLayout Layout,GuiStyle Style) Properties(bool write,GuiLayout layout=default,GuiStyle style=default)
    {
        if(Id==0) throw new InvalidOperationException("Uninitialized UI handle.");
        if(!Backend.GuiProperties(World,Id,write,ref layout,ref style)) throw new WorldException(Scene!.LastStatus,"UI properties");
        return (layout,style);
    }
    public GuiLayout Layout { get => Properties(false).Layout;set { var props=Properties(false);Properties(true,value,props.Style); } }
    public GuiStyle Style { get => Properties(false).Style;set { var props=Properties(false);Properties(true,props.Layout,value); } }
    private (GuiSizing Sizing,GuiImageStyle Image) Advanced(bool write,GuiSizing sizing=default,GuiImageStyle image=default) {
        if(Id==0 || Scene is not IGuiAdvancedAccess backend)throw new InvalidOperationException("UI layout extension unavailable.");
        if(!backend.GuiSizing(World,Id,write,ref sizing,ref image))throw new WorldException(Scene.LastStatus,"UI sizing");return (sizing,image);
    }
    public GuiSizing Sizing {get=>Advanced(false).Sizing;set {var p=Advanced(false);Advanced(true,value,p.Image);}}
    public GuiImageStyle ImageStyle {get=>Advanced(false).Image;set {var p=Advanced(false);Advanced(true,p.Sizing,value);}}
    public string Image {get=>Scene is IGuiAdvancedAccess backend?backend.ReadGuiImage(World,Id):throw new InvalidOperationException("UI image extension unavailable.");set {ArgumentNullException.ThrowIfNull(value);Command(10,value);}}
    public void MoveEarlier()=>Command(11);
    public void MoveLater()=>Command(12);
    private (GuiInteraction Interaction,GuiAnimation Animation) Behavior(bool write,GuiInteraction interaction=default,GuiAnimation animation=default) {
        if(Id==0 || Scene is not IGuiBehaviorAccess backend)throw new InvalidOperationException("UI behavior extension unavailable.");
        if(!backend.GuiBehavior(World,Id,write,ref interaction,ref animation))throw new WorldException(Scene.LastStatus,"UI behavior");return (interaction,animation);
    }
    public GuiInteraction Interaction {get=>Behavior(false).Interaction;set {var p=Behavior(false);Behavior(true,value,p.Animation);}}
    public GuiAnimation Animation {get=>Behavior(false).Animation;set {var p=Behavior(false);Behavior(true,p.Interaction,value);}}
    public void OnClick(GuiClickAction action,GuiElement target=default,float value=0) {
        if(!Enum.IsDefined(action) || !float.IsFinite(value))throw new ArgumentOutOfRangeException(nameof(action));
        if(target.Id!=0 && (!ReferenceEquals(target.Scene,Scene) || target.World!=World))throw new WorldException(WorldStatus.ForeignWorld,"UI click target");
        Interaction=new(true,action,target.Id,value);
    }
    public void PlayAnimation()=>Command(13);
    public void StopAnimation()=>Command(14);
    private IGuiStateAccess StateBackend => Scene as IGuiStateAccess ?? throw new InvalidOperationException("UI state extension unavailable.");
    private int ActionCommand(uint operation,int index,ref GuiActionBinding binding) {
        if(Id==0 || index<0)throw new ArgumentOutOfRangeException(nameof(index));
        var result=StateBackend.GuiAction(World,Id,operation,(uint)index,ref binding);
        if(result<0)throw new WorldException(Scene!.LastStatus,"UI action binding");return result;
    }
    public GuiActionBinding[] Actions {
        get {GuiActionBinding item=default;int count=ActionCommand(0,0,ref item);var list=new GuiActionBinding[count];for(int i=0;i<count;i++){ActionCommand(1,i,ref item);list[i]=item;}return list;}
    }
    public void AddAction(GuiEventKind @event,GuiClickAction action,GuiElement target=default,float value=0) {
        if(!Enum.IsDefined(@event) || !Enum.IsDefined(action) || !float.IsFinite(value))throw new ArgumentOutOfRangeException(nameof(action));
        if(target.Id!=0 && (!ReferenceEquals(target.Scene,Scene) || target.World!=World))throw new WorldException(WorldStatus.ForeignWorld,"UI action target");
        var binding=new GuiActionBinding(@event,action,target.Id,value);ActionCommand(2,0,ref binding);
    }
    public void ReplaceAction(int index,GuiActionBinding binding)=>ActionCommand(3,index,ref binding);
    public void RemoveAction(int index) {GuiActionBinding item=default;ActionCommand(4,index,ref item);}
    public void MoveAction(int index,int destination) {if(destination<0)throw new ArgumentOutOfRangeException(nameof(destination));var item=new GuiActionBinding(GuiEventKind.Click,GuiClickAction.Notify,(uint)destination);ActionCommand(5,index,ref item);}
    public GuiTransitions Transitions {
        get {GuiTransitions value=default;if(!StateBackend.GuiTransitions(World,Id,false,ref value))throw new WorldException(Scene!.LastStatus,"UI transitions");return value;}
        set {if(!StateBackend.GuiTransitions(World,Id,true,ref value))throw new WorldException(Scene!.LastStatus,"UI transitions");}
    }
    public void Remove() => Command(8);
}
