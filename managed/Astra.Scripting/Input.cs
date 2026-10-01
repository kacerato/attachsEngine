using System;
using System.IO;
using System.Text;
using System.Runtime.InteropServices;
using System.Numerics;

namespace Astra;

public enum InputCaptureStatus : uint { Idle, Waiting, Completed, Cancelled }
public enum InputInteraction : uint { Press, Hold, Tap }
public enum InputPhase : uint { Waiting, Started, Performed, Canceled, Disabled }
[Flags]
public enum InputDeviceGroups : uint { None=0, Touch=1, KeyboardMouse=2, Gamepad=4, All=7 }
[StructLayout(LayoutKind.Sequential)]
public struct InputActionState
{
    internal uint Size,Flags;
    public InputInteraction Interaction;
    public InputDeviceGroups DeviceGroups;
    public InputPhase Phase;
    public float Duration,Progress,Elapsed;
    public readonly bool Enabled => (Flags&1)!=0;
    public readonly bool AuthoredEnabled => (Flags&2)!=0;
}

public enum InputSource : uint { None, TouchMove, TouchLook, TouchButton, Key, GamepadAxis, GamepadButton, MouseButton, MouseAxis }

/// <summary>Valor tipado de um vínculo. Códigos de tecla/gamepad são códigos Android.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct InputBindingValue
{
    public InputSource Source;
    public uint Code, NegativeCode, Axis;
    public float Scale;
    private uint invert;
    public bool Invert { readonly get => invert!=0; set => invert=value?1u:0u; }
    public InputBindingValue(InputSource source,uint code=0,uint negativeCode=0,uint axis=0,float scale=1,bool invert=false)
    { Source=source;Code=code;NegativeCode=negativeCode;Axis=axis;Scale=scale;this.invert=invert?1u:0u; }
}

/// <summary>Os papéis que o projeto atribui às próprias ações.</summary>
public enum InputRole : uint
{
    Move = 0,
    Look = 1,
    Jump = 2,
}

/// <summary>
/// As ações de entrada vistas pelo projeto.
///
/// O núcleo não conhece "Mover" nem "Saltar": quem nomeia as ações, escolhe o
/// que as aciona e diz quais papéis elas cumprem é o projeto, no recurso de
/// ações. Um comportamento que queira "a ação de andar deste jogo, seja qual
/// for o nome" pergunta por <see cref="RoleAction"/>.
///
/// Quando a interface do editor está com o toque (IDE aberta, teclado, modal,
/// gizmo arrastado) o gameplay perde o foco: todos os eixos leem zero e nenhum
/// botão fica preso.
/// </summary>
public readonly struct InputAccess(ISceneAccess scene)
{
    /// <summary>State of the real interaction. Timing uses unscaled sampled input time.</summary>
    public InputActionState ActionState(string action)
    {
        var value=new InputActionState {Size=32};
        if(!scene.InputActionCommand(action,0,ref value))throw new ArgumentException("Action unavailable",nameof(action));
        return value;
    }
    /// <summary>Runtime policy only; the scene and rebind profile remain authored data.</summary>
    public bool SetActionEnabled(string action,bool enabled)
    {var value=new InputActionState {Size=32,Flags=enabled?1u:0u};return scene.InputActionCommand(action,1,ref value);}
    public bool RestoreActionEnabled(string action)
    {var value=new InputActionState {Size=32};return scene.InputActionCommand(action,2,ref value);}
    public InputDeviceGroups DeviceGroups {
        get {var value=new InputActionState {Size=32};if(!scene.InputActionCommand("",3,ref value))throw new InvalidOperationException("Input unavailable");return value.DeviceGroups;}
        set {
            if((value&~InputDeviceGroups.All)!=0)throw new ArgumentOutOfRangeException(nameof(value));
            var state=new InputActionState {Size=32,DeviceGroups=value};
            if(!scene.InputActionCommand("",4,ref state))throw new InvalidOperationException("Input unavailable");
        }
    }
    public InputCaptureStatus BindingCaptureStatus {
        get {
            var status=scene.InputCaptureCommand(0,"",0,InputSource.None,false,0);
            if(status<0||status>3)throw new InvalidOperationException("Input capture unavailable");
            return (InputCaptureStatus)status;
        }
    }
    public bool BeginBindingCapture(string action,uint index,InputSource source,bool negative=false,uint cancelKey=111) =>
        scene.InputCaptureCommand(1,action,index,source,negative,cancelKey)==1;
    public void CancelBindingCapture() {
        if(scene.InputCaptureCommand(2,"",0,InputSource.None,false,0)<0)throw new InvalidOperationException("Input capture unavailable");
    }

    public InputBindingValue GetBinding(string action,uint index=0,bool authored=false)
    {
        var binding=new InputBindingValue();
        if(!scene.InputBindingCommand(action,index,authored?4u:0u,ref binding))throw new ArgumentException("Action or binding unavailable",nameof(action));
        return binding;
    }
    public bool ApplyBindingOverride(string action,uint index,InputBindingValue binding) => scene.InputBindingCommand(action,index,1,ref binding);
    public bool RemoveBindingOverride(string action,uint index=0)
    { var binding=new InputBindingValue();return scene.InputBindingCommand(action,index,2,ref binding); }
    public void RemoveAllBindingOverrides()
    { var binding=new InputBindingValue();if(!scene.InputBindingCommand("",0,3,ref binding))throw new InvalidOperationException("Input unavailable"); }
    public string ExportProfile() => scene.ExportInputProfile();
    /// <summary>Troca todos os overrides atomicamente; mapa incompatível é recusado sem alteração.</summary>
    public bool ImportProfile(string profile) => scene.ImportInputProfile(profile);
    /// <summary>Grava o perfil separado da cena. O jogo escolhe o caminho de armazenamento do jogador.</summary>
    public void SaveProfile(string path)
    {
        var target=Path.GetFullPath(path);var folder=Path.GetDirectoryName(target)!;
        Directory.CreateDirectory(folder);var pending=Path.Combine(folder,".astra-input-"+Guid.NewGuid().ToString("N")+".tmp");
        try {File.WriteAllText(pending,ExportProfile(),new UTF8Encoding(false));File.Move(pending,target,true);}
        finally {if(File.Exists(pending))File.Delete(pending);}
    }
    public bool LoadProfile(string path)
    {
        if(!File.Exists(path))return false;
        using var file=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read);
        if(file.Length<=0||file.Length>262144)return false;
        var bytes=new byte[(int)file.Length];file.ReadExactly(bytes);
        if(file.ReadByte()!=-1)return false;
        return ImportProfile(Encoding.UTF8.GetString(bytes));
    }

    /// <summary>Eixo 2D da ação. Ação desconhecida lê zero.</summary>
    public Vector2 Axis2(string action) => scene.InputAxis(action, out var value) ? value : Vector2.Zero;

    /// <summary>Componente X do eixo, para ações Axis1D.</summary>
    public float Axis(string action) => Axis2(action).X;

    public bool Pressed(string action) => scene.InputButton(action, 0) == 1;
    public bool JustPressed(string action) => scene.InputButton(action, 1) == 1;
    public bool JustReleased(string action) => scene.InputButton(action, 2) == 1;
    /// <summary>Falso quando a ação nem existe no mapa do projeto.</summary>
    public bool Exists(string action) => scene.InputButton(action, 0) >= 0;

    /// <summary>Nome da ação que o projeto atribuiu a este papel, ou vazio.</summary>
    public string RoleAction(InputRole role) => scene.InputRole((uint)role);

    public Vector2 Move => Axis2(RoleAction(InputRole.Move));
    public Vector2 Look => Axis2(RoleAction(InputRole.Look));
    public bool JumpPressed => JustPressed(RoleAction(InputRole.Jump));

    /// <summary>
    /// Liga ou desliga um contexto. Uma ação em contexto desligado lê zero sem
    /// perder o que o usuário configurou — é assim que um menu do jogo silencia
    /// o controle do personagem sem apagar o mapa.
    /// </summary>
    public void SetContextEnabled(string context, bool enabled) => scene.InputContext(context, enabled ? 1 : 0);
    public bool ContextEnabled(string context) => scene.InputContext(context, -1);
}
