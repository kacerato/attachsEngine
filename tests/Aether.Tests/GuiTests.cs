using System.Numerics;
using System.Runtime.InteropServices;
using Astra;
using Astra.Runtime;
namespace Aether.Tests;

public static class GuiTests
{
    // SDK contract/routing. Actual Play, input, layout and lifecycle are tested
    // together in native test_gui.cpp, without this adapter.
    private sealed class Host : ISceneAccess,IGuiAccess,IGuiAdvancedAccess,IGuiBehaviorAccess,IGuiStateAccess
    {
        public uint WorldId=>3;
        public WorldStatus LastStatus {get;private set;}
        public uint GenerationOf(ulong id)=>1;
        public bool Exists(ulong id)=>true;
        public TransformValue GetTransform(ulong id)=>default;
        public bool SetTransform(ulong id,TransformValue v)=>false;
        public bool SetBodyVelocity(ulong id,Vector3 v)=>false;
        public bool MoveKinematic(ulong id,Vector3 v,Quaternion q)=>false;
        public void Log(ulong id,string text){}
        public bool Active=true,Removed;
        public string Text="Volume",Name="volume";
        public float Value=.5f;
        public GuiLayout Layout=new(Vector2.Zero,Vector2.One,new(16,16,-16,-16));
        public GuiStyle Style=new(0xFF242830,0xFFEEF1F5,0xFF70ACF5,true,18,4);
        public bool GuiCommand(uint world,uint node,uint operation,string text,float value,GuiKind kind,out GuiSnapshot snapshot,out GuiEventKind eventKind)
        {
            snapshot=default;eventKind=default;LastOperation=operation;
            LastStatus=!Active?WorldStatus.NotRunning:operation!=0&&world!=WorldId?WorldStatus.ForeignWorld:Removed?WorldStatus.UnknownElement:WorldStatus.Ok;
            if(LastStatus!=WorldStatus.Ok)return false;
            if(operation==0&&text!=Name){LastStatus=WorldStatus.UnknownElement;return false;}
            if(operation==2)Text=text;
            if(operation==3)Value=value;
            if(operation==8)Removed=true;
            if(operation==9)Name=text;
            if(operation==6)return false;
            snapshot=new(WorldId,7,GuiKind.Slider,true,true,Value,0,1);return true;
        }
        public bool GuiProperties(uint world,uint node,bool write,ref GuiLayout layout,ref GuiStyle style)
        {
            if(write){Layout=layout;Style=style;}else{layout=Layout;style=Style;}
            return true;
        }
        public string ReadGuiText(uint world,uint node,bool name)=>name?Name:Text;
        public GuiSizing Sizing=new(Vector2.Zero,new(200,48),Vector2.One,new(8,8,8,8),new(8,8),GuiAlignment.Stretch,2,false);
        public GuiImageStyle ImageStyle=new(GuiImageFit.Contain,0xFFFFFFFF);
        public GuiCanvas Canvas=new(GuiCanvasMode.Screen,new(800,600),Vector3.Zero,Vector3.Zero,.005f,true);
        public bool GuiSizing(uint world,uint node,bool write,ref GuiSizing sizing,ref GuiImageStyle image) {if(write){Sizing=sizing;ImageStyle=image;}sizing=Sizing;image=ImageStyle;return true;}
        public GuiInteraction Interaction;public GuiAnimation Animation=GuiAnimation.Default;public uint LastOperation;
        public string ReadGuiDiagnostic(uint world)=>"";
        public bool GuiBehavior(uint world,uint node,bool write,ref GuiInteraction interaction,ref GuiAnimation animation) {if(write){Interaction=interaction;Animation=animation;}interaction=Interaction;animation=Animation;return true;}
        public bool GuiCanvas(uint world,bool write,ref GuiCanvas canvas) {if(write)Canvas=canvas;canvas=Canvas;return true;}
        public string ReadGuiImage(uint world,uint node)=>"UI/banner.png";
        public List<GuiActionBinding> Actions=new();public GuiTransitions Transitions=Astra.GuiTransitions.Default;
        public int GuiAction(uint world,uint node,uint op,uint index,ref GuiActionBinding binding) {
            int i=(int)index;
            switch(op){case 0:return Actions.Count;case 1:binding=Actions[i];break;case 2:Actions.Add(binding);break;case 3:Actions[i]=binding;break;case 4:Actions.RemoveAt(i);break;case 5:var item=Actions[i];Actions.RemoveAt(i);Actions.Insert((int)binding.Target,item);break;default:return -1;}return 1;
        }
        public bool GuiTransitions(uint world,uint node,bool write,ref GuiTransitions transitions){if(write)Transitions=transitions;transitions=Transitions;return true;}
    }
    [Test] public static void Gui_TypedLayoutStyleTextAndLifetimeRouting()
    {
        var host=new Host();var gui=new GuiAccess(host);var element=gui.Find("volume");
        element.Value=.75f;element.Text="Som";element.Name="sound";
        Assert.Equal(.75f,element.Value);Assert.Equal("Som",element.Text);Assert.Equal("sound",element.Name);
        var oldStyle=element.Style;var layout=new GuiLayout(new(.5f,.5f),new(.5f,.5f),new(-100,-24,100,24));
        element.Layout=layout;Assert.Equal(layout,element.Layout);Assert.Equal(oldStyle,element.Style);
        var style=oldStyle with {Background=0xFF123456,FontSize=24};element.Style=style;
        Assert.Equal(style,element.Style);Assert.Equal(layout,element.Layout);
        Assert.False(gui.Poll(out _));
        Assert.Throws<ArgumentOutOfRangeException>(()=>element.Value=float.NaN);
        Assert.Throws<InvalidOperationException>(()=>default(GuiElement).Remove());
        element.Remove();Assert.Throws<WorldException>(()=>_ =element.Snapshot);
        host.Active=false;Assert.Throws<WorldException>(()=>gui.Poll(out _));
    }
    [Test] public static void Gui_AppendedNativeAbiAndTypedStructSizes()
    {
        Assert.Equal(36,Marshal.SizeOf<NativeBehaviorRuntime.NativeGuiState>());
        Assert.Equal(56,Marshal.SizeOf<NativeBehaviorRuntime.NativeGuiProperties>());
        Assert.Equal(68,Marshal.SizeOf<NativeBehaviorRuntime.NativeGuiSizing>());
        Assert.Equal(44,Marshal.SizeOf<NativeBehaviorRuntime.NativeGuiCanvas>());
        long audio=Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("AudioCommand").ToInt64();
        long command=Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("GuiCommand").ToInt64();
        long properties=Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("GuiProperties").ToInt64();
        long text=Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("GuiText").ToInt64();
        Assert.Equal(audio+IntPtr.Size,command);Assert.Equal(command+IntPtr.Size,properties);Assert.Equal(properties+IntPtr.Size,text);
        Assert.Equal(76,Marshal.SizeOf<NativeBehaviorRuntime.NativeGuiBehavior>());
        long behavior=Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("GuiBehavior").ToInt64();
        Assert.Equal(text+3*IntPtr.Size,behavior);
        Assert.Equal(16,Marshal.SizeOf<NativeBehaviorRuntime.NativeGuiAction>());Assert.Equal(72,Marshal.SizeOf<NativeBehaviorRuntime.NativeGuiTransitions>());
        long action=Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("GuiAction").ToInt64(), transitions=Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("GuiTransitions").ToInt64();
        Assert.Equal(behavior+IntPtr.Size,action);Assert.Equal(action+IntPtr.Size,transitions);Assert.Equal(transitions+IntPtr.Size,(long)Marshal.SizeOf<NativeBehaviorRuntime.SceneAccess>());
    }
    [Test] public static void Gui_ContainerImageAndCanvasTypedApiPreservesIndependentFields()
    {
        var host=new Host();var gui=new GuiAccess(host);var element=gui.Find("volume");
        var sizing=element.Sizing with {Padding=new(2,4,6,8),Preferred=new(300,120),Columns=3};element.Sizing=sizing;
        var image=element.ImageStyle with {Fit=GuiImageFit.Cover,Tint=0xFF123456};element.ImageStyle=image;
        Assert.Equal(sizing,element.Sizing);Assert.Equal(image,element.ImageStyle);Assert.Equal("UI/banner.png",element.Image);
        var canvas=gui.Canvas with {Mode=GuiCanvasMode.World,Position=new(0,1,5),Rotation=new(0,30,0)};gui.Canvas=canvas;
        Assert.Equal(canvas,gui.Canvas);Assert.Equal(sizing,element.Sizing);
    }
    [Test] public static void Gui_ImageBehaviorApiPreservesCompositionAndRejectsForeignTarget()
    {
        var host=new Host();var element=new GuiAccess(host).Find("volume");
        element.Animation=GuiAnimation.Default with {Enabled=true,Duration=.5f,To=new(new(40,0),1.2f,.5f)};
        var motion=element.Animation;element.OnClick(GuiClickAction.PlayAnimation);
        Assert.True(element.Interaction.Clickable);Assert.Equal(motion,element.Animation);
        element.PlayAnimation();Assert.Equal(13u,host.LastOperation);element.StopAnimation();Assert.Equal(14u,host.LastOperation);
        var foreign=new GuiAccess(new Host()).Find("volume");
        Assert.Throws<WorldException>(()=>element.OnClick(GuiClickAction.ToggleVisible,foreign));
    }
    [Test] public static void Gui_OrderedActionsAndTransitionsAreIndependentlyEditable()
    {
        var host=new Host();var element=new GuiAccess(host).Find("volume");element.OnClick(GuiClickAction.Notify);
        element.AddAction(GuiEventKind.Click,GuiClickAction.SetValue,element,.8f);element.AddAction(GuiEventKind.ValueChanged,GuiClickAction.PlayAnimation);
        element.MoveAction(1,0);Assert.Equal(GuiEventKind.ValueChanged,element.Actions[0].Event);
        element.ReplaceAction(1,new(GuiEventKind.Click,GuiClickAction.ToggleEnabled));element.RemoveAction(0);Assert.Equal(1,element.Actions.Length);
        var state=GuiTransitions.Default with {Enabled=true,Pressed=new(new(new(3,4),.8f,.7f),0xFF887766)};element.Transitions=state;
        Assert.Equal(state,element.Transitions);Assert.Equal(GuiClickAction.Notify,element.Interaction.Action);
        var foreign=new GuiAccess(new Host()).Find("volume");Assert.Throws<WorldException>(()=>element.AddAction(GuiEventKind.Click,GuiClickAction.ToggleVisible,foreign));
    }

}
