using System.Numerics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using Astra.Compilation;

namespace Astra.Runtime;

public static unsafe class NativeBehaviorRuntime
{
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeGuiState
    {
        public uint World,Node,Kind,Visible,Enabled;
        public float Value,Minimum,Maximum;
        public uint Event;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeGuiProperties
    {
        public float AnchorMinX,AnchorMinY,AnchorMaxX,AnchorMaxY;
        public float Left,Top,Right,Bottom;
        public uint Background,Foreground,Accent,ClipChildren;
        public float FontSize,Radius;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeGuiSizing {
        public float MinX,MinY,PrefX,PrefY,FlexX,FlexY,Left,Top,Right,Bottom,SpaceX,SpaceY;
        public uint Alignment,Columns,Ignore,ImageFit,ImageTint;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeGuiCanvas {
        public uint Mode;public float Width,Height,X,Y,Z,Rx,Ry,Rz,Units;public uint Occlusion;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeGuiBehavior {
        public uint Clickable,Action,Target;public float Value;
        public uint Enabled,AutoPlay,Loop,PingPong,Easing;
        public float Duration,Delay,FromX,FromY,FromScale,FromOpacity,ToX,ToY,ToScale,ToOpacity;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeGuiAction { public uint Event,Action,Target;public float Value; }
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeGuiTransitions {
        public uint Enabled,Easing;public float Duration;
        public float NormalX,NormalY,NormalScale,NormalOpacity,PressedX,PressedY,PressedScale,PressedOpacity,DisabledX,DisabledY,DisabledScale,DisabledOpacity;
        public uint NormalTint,PressedTint,DisabledTint;
    }
    /// <summary>Espelho de <c>ae::scene::ScriptQueryFilter</c>.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeQueryFilter
    {
        public uint Size, GameplayLayerMask, Flags, Reserved;
        public ulong Ignore;
    }

    /// <summary>Espelho de <c>ae::scene::ScriptShapeQuery</c>.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeShapeQuery
    {
        public uint Kind;
        public float HalfX, HalfY, HalfZ;
        public float Radius, HalfHeight;
        public float RotationX, RotationY, RotationZ, RotationW;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeAssetGuid { public ulong High, Low; }

    /// <summary>Espelho de <c>ae::scene::ScriptAnimationCommand</c> (ABI v9, 40 bytes).</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeAnimationCommand
    {
        public uint Size, Op;
        public NativeAssetGuid Clip;
        public float Seconds, TargetWeight;
        public uint PlayMode, Reserved;
    }
    /// <summary>Espelho de <c>ae::scene::ScriptAnimationState</c> (ABI v9, 48 bytes).</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeAnimationState
    {
        public uint Size, Enabled;
        public NativeAssetGuid Clip;
        public float Time, Speed, Weight, Length;
        public uint Layer, WrapMode;
    }

    /// <summary>
    /// Espelho exato de <c>ae::scene::ScriptSceneAccess</c> (ABI v42). A ordem dos
    /// campos É o contrato: acrescentar só no fim, e conferir <c>Size</c> antes de
    /// ler qualquer ponteiro — uma struct maior do que a acordada seria lida além
    /// do fim do que o nativo alocou.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct SceneAccess
    {
        public uint Version, Size;
        public void* Context;
        public delegate* unmanaged<void*, ulong, int> Exists;
        public delegate* unmanaged<void*, ulong, float*, int> GetTransform, SetTransform, SetVelocity, MoveKinematic;
        public delegate* unmanaged<void*, ulong, byte*, int, void> Log;
        public delegate* unmanaged<void*, ulong, float*, uint, int> BodyForce;
        public delegate* unmanaged<void*, ulong, float*, int> GetVelocity;
        // v3 — identidade
        public delegate* unmanaged<void*, uint> WorldId;
        public delegate* unmanaged<void*, ulong, uint> Generation;
        public delegate* unmanaged<void*, uint> LastStatus;
        // v3 — hierarquia
        public delegate* unmanaged<void*, ulong, ulong> ParentOf;
        public delegate* unmanaged<void*, ulong, int> ChildCount;
        public delegate* unmanaged<void*, ulong, uint, ulong> ChildAt;
        public delegate* unmanaged<void*, ulong, byte*, int, int, ulong> FindChild;
        public delegate* unmanaged<void*, ulong, byte*, int, int> GetName;
        public delegate* unmanaged<void*, ulong, byte*, int, int> SetName;
        public delegate* unmanaged<void*, ulong, int> GetActive;
        public delegate* unmanaged<void*, ulong, int, int> SetActive;
        // v3 — ciclo de vida
        public delegate* unmanaged<void*, ulong, byte*, int, ulong> CreateObject;
        public delegate* unmanaged<void*, ulong, int> DestroyObject;
        public delegate* unmanaged<void*, ulong, ulong, uint, int> SetParent;
        // v3 — componentes
        public delegate* unmanaged<void*, ulong, int> ComponentCount;
        public delegate* unmanaged<void*, ulong, uint, byte*, int, ulong> ComponentAt;
        public delegate* unmanaged<void*, ulong, byte*, int, uint, ulong> FindComponent;
        public delegate* unmanaged<void*, ulong, byte*, int, ulong> AddComponent;
        public delegate* unmanaged<void*, ulong, ulong, int> RemoveComponent;
        public delegate* unmanaged<void*, ulong, ulong, byte*, int, uint*, ulong*, int> GetProperty;
        public delegate* unmanaged<void*, ulong, ulong, byte*, int, uint, ulong, int> SetProperty;
        // v3 — transform de mundo
        public delegate* unmanaged<void*, ulong, float*, int> GetWorldTransform;
        public delegate* unmanaged<void*, ulong, float*, int> SetWorldTransform;
        // v4 — consultas físicas
        public delegate* unmanaged<void*, float*, float*, NativeQueryFilter*, RawQueryHit*, int, int> RayCast;
        public delegate* unmanaged<void*, NativeShapeQuery*, float*, float*, NativeQueryFilter*, RawQueryHit*, int> ShapeCast;
        public delegate* unmanaged<void*, NativeShapeQuery*, float*, NativeQueryFilter*, RawQueryHit*, int, int> Overlap;
        public delegate* unmanaged<void*, byte*, int, int> LayerByName;
        public delegate* unmanaged<void*, uint, byte*, int, int> LayerName;
        // v5 — entrada por ações
        public delegate* unmanaged<void*, byte*, int, float*, int> InputAxis;
        public delegate* unmanaged<void*, byte*, int, uint, int> InputButton;
        public delegate* unmanaged<void*, byte*, int, int, int> InputContext;
        public delegate* unmanaged<void*, uint, byte*, int, int> InputRole;
        // v6 — gráficos e recursos
        public delegate* unmanaged<void*, uint, NativeGraphicsState*, int> GetRenderingState;
        public delegate* unmanaged<void*, uint, GraphicsSettings*, ulong*, int> SetRenderingSettings;
        public delegate* unmanaged<void*, uint, byte*, int, int> CopyRenderingDiagnostics;
        public delegate* unmanaged<void*, ulong, ulong, byte*, int, uint, NativeAssetGuid*, int> GetComponentResource;
        public delegate* unmanaged<void*, ulong, ulong, byte*, int, uint, NativeAssetGuid, int> SetComponentResource;
        // v7 — propriedades por slot de material
        public delegate* unmanaged<void*, ulong, ulong, byte*, int, uint, uint*, ulong*, int> GetComponentSlotProperty;
        public delegate* unmanaged<void*, ulong, ulong, byte*, int, uint, uint, ulong, int> SetComponentSlotProperty;
        // v8 — comandos de gameplay, acrescentados ao fim da ABI
        public delegate* unmanaged<void*, ulong, float*, int> CharacterMove;
        public delegate* unmanaged<void*, ulong, int> CharacterJump;
        public delegate* unmanaged<void*, ulong, float*, int> CameraLook;
        // v9 — animação por componente
        public delegate* unmanaged<void*, ulong, ulong, NativeAnimationCommand*, int> AnimationCommand;
        public delegate* unmanaged<void*, ulong, ulong, NativeAssetGuid, NativeAnimationState*, int> GetAnimationState;
        public delegate* unmanaged<void*, ulong, ulong, NativeAnimationState*, int> SetAnimationState;
        public delegate* unmanaged<void*, ulong, ulong, uint, NativeAssetGuid*, byte*, int, int> AnimationClipAt;
        // v10: IDs persistentes de elementos de coleções de recursos.
        public delegate* unmanaged<void*, ulong, ulong, byte*, int, uint, ulong*, int> ResourceElementId;
        public delegate* unmanaged<void*, ulong, ulong, byte*, int, ulong, NativeAssetGuid*, int> GetResourceByElementId;
        public delegate* unmanaged<void*, ulong, ulong, byte*, int, ulong, NativeAssetGuid, int> SetResourceByElementId;
        // v11: alterações estruturais da lista de clipes em Play.
        public delegate* unmanaged<void*, ulong, ulong, NativeAssetGuid, ulong*, int> AppendAnimationClip;
        public delegate* unmanaged<void*, ulong, ulong, ulong, int> RemoveAnimationClip;
        public delegate* unmanaged<void*, ulong, ulong, ulong, uint, int> MoveAnimationClip;
        // v12: política de pose ao alterar a hierarquia.
        public delegate* unmanaged<void*, ulong, ulong, uint, uint, int> SetParentWithPolicy;
        // v13: resultado das operações estruturais no ponto seguro.
        public delegate* unmanaged<void*, uint, ulong, ulong, uint, uint, ulong*, int> QueueStructuralOperation;
        public delegate* unmanaged<void*, uint, ulong, uint*, uint*, int> QueryOperation;
        // v14: estado ativo local, separado do estado herdado.
        public delegate* unmanaged<void*, ulong, int> GetActiveSelf;

        // v15: tags e consultas globais restritas ao mundo desta sessão.
        public delegate* unmanaged<void*, ulong, byte*, int, int> GetTag, SetTag, CompareTag;
        public delegate* unmanaged<void*, byte*, int, ulong*, int, int, int> FindTagged;

        public delegate* unmanaged<void*, ulong, byte*, int, byte*, int, ulong> AddBehavior;
        public delegate* unmanaged<void*, ulong, double, int> DestroyAfter;
        public delegate* unmanaged<void*, ulong, ulong, ulong*, int, int> Instantiate;
        public delegate* unmanaged<void*, ulong, int, int> FinishInstantiation;
        public delegate* unmanaged<void*, ulong, uint, ulong> CreatePrimitive;
        public delegate* unmanaged<void*, ulong, NativeAssetGuid, ulong> InstantiatePrefab;
        public delegate* unmanaged<void*, ulong, byte*, int, int> InstantiationAttachments;
        // v20 — publicação atômica de uma tripla refletida.
        public delegate* unmanaged<void*, ulong, ulong, byte*, int, float*, int> SetTriple;
        public delegate* unmanaged<void*, ulong, uint, uint, uint, float*, float*, int> Body2DCommand;
        public delegate* unmanaged<void*, uint, uint, float*, float*, float, NativeQueryFilter*, RawQueryHit*, int, int> Query2D;
        public delegate* unmanaged<void*, ulong, uint, uint, ulong, uint, ulong, uint, float*, float*, ulong*, int> PathPointCommand;
        public delegate* unmanaged<void*, ulong, uint, uint, ulong, uint, double, uint, float*, double*, int> PathRuntimeCommand;

        public delegate* unmanaged<void*, ulong, uint, uint, ulong, NativeAudioSnapshot*, int> AudioSnapshot;
        public delegate* unmanaged<void*, uint, NativeTimeState*, int> TimeSnapshot;
        public delegate* unmanaged<void*, uint, float, int> SetTimeScale;
        public delegate* unmanaged<void*, ulong, byte*, int, int, int> GroupMembership;
        public delegate* unmanaged<void*, byte*, int, ulong*, int, int, int> FindGroup;
        public delegate* unmanaged<void*, ulong, uint, byte*, int, int> GroupAt;
        public delegate* unmanaged<void*, byte*, int, uint, uint, InputBindingValue*, int> InputBindingCommand;
        public delegate* unmanaged<void*, uint, byte*, int, byte*, int, int> InputProfile;
        public delegate* unmanaged<void*, uint, byte*, int, uint, uint, uint, uint, int> InputCaptureCommand;

        public delegate* unmanaged<void*, ulong, ulong, uint, float, NativeTimerState*, int> TimerCommand;
        public delegate* unmanaged<void*, ulong, ulong, uint, NativeTweenState*, int> TweenCommand;
        public delegate* unmanaged<void*, ulong, ulong, byte*, int, NativeNumberTweenParameters*, ulong*, int> NumberTweenCreate;
        public delegate* unmanaged<void*, ulong, uint, NativeNumberTweenState*, int> NumberTweenCommand;

        public delegate* unmanaged<void*, ulong, NativeCharacterState*, int> CharacterSnapshot;
        public delegate* unmanaged<void*, ulong, uint, uint, ulong, uint, float*, float*, NativeBodyState*, int> BodyCommand;
        public delegate* unmanaged<void*, ulong, uint, uint, ulong, uint, float*, uint, NativeFieldState*, int> FieldQuery;
        public delegate* unmanaged<void*, ulong, uint, uint, int, int> ObjectLayer;
        public delegate* unmanaged<void*, byte*, int, uint, InputActionState*, int> InputActionCommand;

        public delegate* unmanaged<void*, ulong, uint, uint, ulong, uint, double, int> AudioCommand;
        public delegate* unmanaged<void*, uint, uint, uint, byte*, int, float, NativeGuiState*, int> GuiCommand;
        public delegate* unmanaged<void*, uint, uint, uint, NativeGuiProperties*, int> GuiProperties;
        public delegate* unmanaged<void*, uint, uint, uint, byte*, int, int> GuiText;
        public delegate* unmanaged<void*,uint,uint,uint,NativeGuiSizing*,int> GuiSizing;
        public delegate* unmanaged<void*,uint,uint,NativeGuiCanvas*,int> GuiCanvas;
        public delegate* unmanaged<void*,uint,uint,uint,NativeGuiBehavior*,int> GuiBehavior;
        public delegate* unmanaged<void*,uint,uint,uint,uint,NativeGuiAction*,int> GuiAction;
        public delegate* unmanaged<void*,uint,uint,uint,NativeGuiTransitions*,int> GuiTransitions;
        // v42 — último campo do núcleo; o resto entra por famílias nomeadas.
        public delegate* unmanaged<void*, byte*, int, uint*, uint*, void*> Extension;

        public bool Complete => Exists != null && GetTransform != null && SetTransform != null && SetVelocity != null &&
            MoveKinematic != null && Log != null && BodyForce != null && GetVelocity != null && WorldId != null &&
            Generation != null && LastStatus != null && ParentOf != null && ChildCount != null && ChildAt != null &&
            FindChild != null && GetName != null && SetName != null && GetActive != null && SetActive != null &&
            CreateObject != null && DestroyObject != null && SetParent != null && ComponentCount != null &&
            ComponentAt != null && FindComponent != null && AddComponent != null && RemoveComponent != null &&
            GetProperty != null && SetProperty != null && GetWorldTransform != null && SetWorldTransform != null &&
            RayCast != null && ShapeCast != null && Overlap != null && LayerByName != null && LayerName != null &&
            InputAxis != null && InputButton != null && InputContext != null && InputRole != null &&
            GetRenderingState != null && SetRenderingSettings != null && CopyRenderingDiagnostics != null &&
            GetComponentResource != null && SetComponentResource != null &&
            GetComponentSlotProperty != null && SetComponentSlotProperty != null &&
            CharacterMove != null && CharacterJump != null && CameraLook != null &&
            AnimationCommand != null && GetAnimationState != null && SetAnimationState != null && AnimationClipAt != null &&
            ResourceElementId != null && GetResourceByElementId != null && SetResourceByElementId != null &&
            AppendAnimationClip != null && RemoveAnimationClip != null && MoveAnimationClip != null &&
            SetParentWithPolicy != null && QueueStructuralOperation != null && QueryOperation != null && GetActiveSelf != null &&
            GetTag != null && SetTag != null && CompareTag != null && FindTagged != null && AddBehavior != null && DestroyAfter != null && Instantiate != null && FinishInstantiation != null && CreatePrimitive != null && InstantiatePrefab != null && InstantiationAttachments != null && SetTriple != null && Body2DCommand != null && Query2D != null && PathPointCommand != null && PathRuntimeCommand != null && AudioSnapshot != null && TimeSnapshot != null && SetTimeScale != null && GroupMembership != null && FindGroup != null && GroupAt != null && InputBindingCommand != null && InputProfile != null && InputCaptureCommand != null && TimerCommand != null && TweenCommand != null && NumberTweenCreate != null && NumberTweenCommand != null && CharacterSnapshot != null && BodyCommand != null && FieldQuery != null && ObjectLayer != null && InputActionCommand != null && AudioCommand != null && GuiCommand != null && GuiProperties != null && GuiText != null && GuiSizing != null && GuiCanvas != null && GuiBehavior != null && GuiAction != null && GuiTransitions != null && Extension != null;
    }

    /// <summary>Família <c>astra.component.operations</c> v1 (native/scene/script_extensions.h).</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeComponentOperations
    {
        public uint Version, Size;
        public delegate* unmanaged<void*, ulong, uint, uint, ulong, byte*, int, ComponentValue*, int, ComponentValue*, int> Invoke;
        public delegate* unmanaged<void*, NativeComponentEvent*, int, int> PollEvents;
        public delegate* unmanaged<void*, uint, uint, byte*, int, int> EventName;
        public delegate* unmanaged<void*, byte*, int, int> DeclaresEvent;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeViewState
    {
        public uint Size, Flags;
        public float Width, Height, Dpi, SafeX, SafeY, SafeWidth, SafeHeight;
        public uint Platform, Reserved;
        public ulong Camera;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeViewOperations
    {
        public uint Version, Size;
        public delegate* unmanaged<void*, NativeViewState*, int> State;
        public delegate* unmanaged<void*, float, float, float*, float*, int> ScreenRay;
        public delegate* unmanaged<void*, float*, float*, int> WorldToScreen;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeDebugOperations
    {
        public uint Version, Size;
        public delegate* unmanaged<void*, float*, float*, uint, float, int> DrawLine;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeHierarchyChange { public ulong Object; public uint Generation, Kind; }
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeHierarchyOperations
    {
        public uint Version, Size;
        public delegate* unmanaged<void*, NativeHierarchyChange*, int, int> PollChanges;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeHapticsOperations
    {
        public uint Version, Size;
        public delegate* unmanaged<void*, uint, float, int> Vibrate;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeComponentEvent
    {
        public ulong Object, Instance;
        public uint World, Generation, Type, Event, Count, Lost;
        public ComponentValue Value0, Value1, Value2;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeFieldState {public uint Size,Flags;public float Weight;public Vector3 Acceleration,WindVelocity;public float WindDrag,LinearDrag,AngularDrag,OverrideWeight;public uint AffectedBodies;public float AffectedMass;public uint Reserved;}
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeBodyState {public uint Size,Flags;public Vector3 Linear,Angular,CenterOfMass;public uint Reserved;}
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeCharacterState {public uint Size,GroundState,Flags,Reserved;public Vector3 Position,Velocity,MotorVelocity,GroundVelocity,GroundNormal;public uint TailReserved;}
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeNumberTweenParameters {public uint Size,Easing;public float Destination,Duration;public uint Flags,Reserved;}
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeNumberTweenState {public uint Size,Status,Failure,Flags;public double Elapsed;public float Value,Duration;}
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeTweenState {public uint Size,Status;public double Elapsed;public uint Flags,Reserved;}
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeTimerState { public uint Size,Flags;public double Remaining; }

    [StructLayout(LayoutKind.Sequential)]
    public struct NativeAudioSnapshot
    {
        public uint Size,State,OutputRunning,Reserved;
        public double Cursor;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NativeTimeState
    {
        public uint Size,Reserved;
        public ulong FrameCount;
        public double SimulationTime,UnscaledTime;
        public float Delta,UnscaledDelta,TimeScale,FrameScale;
    }

    private sealed class SceneAdapter(SceneAccess access) : ISceneAccess, IAudioVoiceAccess, ITimeSceneAccess, IGuiAccess, IGuiAdvancedAccess, IGuiBehaviorAccess, IGuiStateAccess, IComponentOperationAccess,
        IGameViewAccess, IDebugDrawAccess, IHierarchyChangeAccess, IHapticsAccess
    {
        // Resolve uma família pelo nome uma vez por sessão; nulo quando o host não
        // a oferece ou entrega versão/tamanho menores que os deste SDK.
        private readonly Dictionary<string, nint> _families = [];
        private T* Family<T>(string name, uint minimumVersion = 1) where T : unmanaged
        {
            if (!Accessible) throw new WorldException(WorldStatus.NotRunning, name);
            if (!_families.TryGetValue(name, out var cached))
            {
                var bytes = Encoding.ASCII.GetBytes(name);
                uint version = 0, size = 0; void* table;
                fixed (byte* pointer = bytes) table = access.Extension(access.Context, pointer, bytes.Length, &version, &size);
                cached = table != null && version >= minimumVersion && size >= (uint)sizeof(T) ? (nint)table : 0;
                _families[name] = cached;
            }
            return (T*)cached;
        }
        private T* Require<T>(string name) where T : unmanaged =>
            Family<T>(name) is var table && table != null ? table : throw new NotSupportedException($"O host não oferece a família {name}.");
        public bool ReadGameView(out GameViewState state)
        {
            state = default;
            var view = Require<NativeViewOperations>("astra.view");
            NativeViewState raw = new() { Size = (uint)sizeof(NativeViewState) };
            if (view->State(access.Context, &raw) == 0 || (raw.Flags & 1) == 0) return false;
            state = new(raw.Width, raw.Height, raw.Dpi, (raw.Flags & 2) != 0, new(raw.SafeX, raw.SafeY, raw.SafeWidth, raw.SafeHeight),
                (GameViewPlatform)raw.Platform, raw.Camera);
            return true;
        }
        public bool ScreenRay(float x, float y, out Ray ray)
        {
            ray = default;
            var view = Require<NativeViewOperations>("astra.view");
            float* origin = stackalloc float[3]; float* direction = stackalloc float[3];
            if (view->ScreenRay(access.Context, x, y, origin, direction) == 0) return false;
            ray = new(new(origin[0], origin[1], origin[2]), new(direction[0], direction[1], direction[2])); return true;
        }
        public bool WorldToScreen(Vector3 world, out Vector3 screen)
        {
            screen = default;
            var view = Require<NativeViewOperations>("astra.view");
            float* input = stackalloc float[3] { world.X, world.Y, world.Z }; float* output = stackalloc float[3];
            if (view->WorldToScreen(access.Context, input, output) == 0) return false;
            screen = new(output[0], output[1], output[2]); return true;
        }
        public bool DrawLine(Vector3 from, Vector3 to, uint argb, float seconds)
        {
            var debug = Require<NativeDebugOperations>("astra.debug");
            float* a = stackalloc float[3] { from.X, from.Y, from.Z }; float* b = stackalloc float[3] { to.X, to.Y, to.Z };
            return debug->DrawLine(access.Context, a, b, argb, seconds) != 0;
        }
        public int PollHierarchyChanges(Span<HierarchyChange> destination)
        {
            var hierarchy = Require<NativeHierarchyOperations>("astra.hierarchy");
            var capacity = Math.Min(destination.Length, 64);
            if (capacity == 0) return 0;
            var raw = stackalloc NativeHierarchyChange[capacity];
            var count = hierarchy->PollChanges(access.Context, raw, capacity);
            if (count < 0) throw new WorldException(LastStatus, "ler mudanças de hierarquia");
            for (var i = 0; i < count; ++i) destination[i] = new(raw[i].Object, raw[i].Generation, raw[i].Kind == 0);
            return count;
        }
        public bool HapticsAvailable => Accessible && Family<NativeHapticsOperations>("astra.haptics") != null;
        public bool Vibrate(uint milliseconds, float amplitude) =>
            Require<NativeHapticsOperations>("astra.haptics")->Vibrate(access.Context, milliseconds, amplitude) != 0;

        // Famílias opcionais: resolvidas uma vez por sessão; nulo quando o host
        // não oferece ou entrega uma versão/tamanho que este SDK não conhece.
        private bool _operationsResolved;
        private NativeComponentOperations* _operations;
        private readonly Dictionary<ulong, (string Type, string Event)> _eventNames = [];
        private NativeComponentOperations* Operations
        {
            get
            {
                if (!Accessible) throw new WorldException(WorldStatus.NotRunning, "operações de componente");
                if (!_operationsResolved)
                {
                    _operationsResolved = true;
                    ReadOnlySpan<byte> name = "astra.component.operations"u8;
                    uint version = 0, size = 0; void* table;
                    fixed (byte* pointer = name) table = access.Extension(access.Context, pointer, name.Length, &version, &size);
                    if (table != null && version >= 1 && size >= (uint)sizeof(NativeComponentOperations))
                        _operations = (NativeComponentOperations*)table;
                }
                return _operations != null ? _operations
                    : throw new NotSupportedException("O host não oferece a família astra.component.operations.");
            }
        }
        public bool InvokeComponentMethod(ulong objectId, uint world, uint generation, ulong instanceId, string method,
            ReadOnlySpan<ComponentValue> arguments, out ComponentValue result)
        {
            result = default;
            var operations = Operations;
            var bytes = Utf8(method, "método");
            ComponentValue output;
            int ok;
            fixed (byte* name = bytes)
            fixed (ComponentValue* args = arguments)
                ok = operations->Invoke(access.Context, objectId, world, generation, instanceId, name, bytes.Length, args, arguments.Length, &output);
            if (ok == 0) return false;
            result = output; return true;
        }
        public int PollComponentEvents(Span<ComponentEventRecord> destination)
        {
            var operations = Operations;
            if (destination.Length == 0) return 0;
            var capacity = Math.Min(destination.Length, 64);
            var raw = stackalloc NativeComponentEvent[capacity];
            var count = operations->PollEvents(access.Context, raw, capacity);
            if (count < 0) throw new WorldException(LastStatus, "ler eventos de componente");
            for (var i = 0; i < count; ++i)
            {
                var e = raw[i];
                var (type, name) = EventName(operations, e.Type, e.Event);
                destination[i] = new(e.Object, e.World, e.Generation, e.Instance, type, name, e.Count, e.Lost, e.Value0, e.Value1, e.Value2);
            }
            return count;
        }
        public bool DeclaresComponentEvent(string typeId, string eventId)
        {
            var operations = Operations;
            var bytes = Utf8(typeId + "/" + eventId, "evento");
            fixed (byte* pointer = bytes) return operations->DeclaresEvent(access.Context, pointer, bytes.Length) != 0;
        }
        private (string, string) EventName(NativeComponentOperations* operations, uint type, uint eventIndex)
        {
            var key = ((ulong)type << 32) | eventIndex;
            if (_eventNames.TryGetValue(key, out var cached)) return cached;
            var buffer = stackalloc byte[512];
            var length = operations->EventName(access.Context, type, eventIndex, buffer, 512);
            if (length <= 0 || length > 512) throw new WorldException(WorldStatus.UnknownOperation, "nome de evento");
            var text = Encoding.UTF8.GetString(buffer, length);
            var slash = text.IndexOf('/');
            var names = (text[..slash], text[(slash + 1)..]);
            _eventNames[key] = names;
            return names;
        }
        // Nomes cabem em 63 bytes e ids de tipo em 256; os buffers são o teto do
        // contrato nativo, não uma estimativa.
        private const int NameCapacity = 64;
        private const int TypeIdCapacity = 257;
        // Consultado pelo lifecycle em cada despacho; evita alocar UTF-8 por callback.
        private static readonly byte[] EnabledProperty = "enabled"u8.ToArray();
        private bool _active = true;
        private readonly int _ownerThread = Environment.CurrentManagedThreadId;
        private bool Accessible => _active && Environment.CurrentManagedThreadId == _ownerThread;
        public void Invalidate() => _active = false;
        public SimulationTimeState ReadTime() {
            if(!Accessible) throw new WorldException(WorldStatus.NotRunning,"Time.Read");
            NativeTimeState state=new() { Size=(uint)sizeof(NativeTimeState) };
            if(access.TimeSnapshot(access.Context,WorldId,&state)==0) throw new WorldException(LastStatus,"Time.Read");
            return new(state.FrameCount,state.SimulationTime,state.UnscaledTime,state.Delta,state.UnscaledDelta,state.TimeScale,state.FrameScale);
        }
        public WorldStatus SetTimeScale(float value) {
            if(!Accessible) return WorldStatus.NotRunning;
            return access.SetTimeScale(access.Context,WorldId,value)!=0?WorldStatus.Ok:LastStatus;
        }

        private static byte[] Utf8(string text, string what)
        {
            ArgumentNullException.ThrowIfNull(text);
            if (text == "enabled") return EnabledProperty;
            var bytes = Encoding.UTF8.GetBytes(text);
            if (bytes.Length == 0 || bytes.Length > 1024)
                throw new ArgumentException($"{what} inválido: {text.Length} caracteres", nameof(text));
            return bytes;
        }

        public bool Exists(ulong objectId) => Accessible && access.Exists(access.Context, objectId) != 0;

        public uint WorldId => Accessible ? access.WorldId(access.Context) : 0;
        public uint GenerationOf(ulong objectId) => Accessible ? access.Generation(access.Context, objectId) : 0;
        public WorldStatus LastStatus => Accessible ? (WorldStatus)access.LastStatus(access.Context) : WorldStatus.NotRunning;

        public TransformValue GetTransform(ulong objectId)
        {
            float* value = stackalloc float[10];
            if (!Accessible || access.GetTransform(access.Context, objectId, value) == 0)
                throw new WorldException(LastStatus, "ler transform");
            return Decode(value);
        }
        public bool SetTransform(ulong objectId, TransformValue value)
        {
            float* data = stackalloc float[10];
            Encode(value, data);
            return Accessible && access.SetTransform(access.Context, objectId, data) != 0;
        }
        public TransformValue GetWorldTransform(ulong objectId)
        {
            float* value = stackalloc float[10];
            if (!Accessible || access.GetWorldTransform(access.Context, objectId, value) == 0)
                throw new WorldException(LastStatus, "ler transform de mundo");
            return Decode(value);
        }
        public bool SetWorldTransform(ulong objectId, TransformValue value)
        {
            float* data = stackalloc float[10];
            Encode(value, data);
            return Accessible && access.SetWorldTransform(access.Context, objectId, data) != 0;
        }
        private static TransformValue Decode(float* v) =>
            new(new(v[0], v[1], v[2]), new(v[3], v[4], v[5], v[6]), new(v[7], v[8], v[9]));
        private static void Encode(TransformValue value, float* data)
        {
            data[0] = value.Position.X; data[1] = value.Position.Y; data[2] = value.Position.Z;
            data[3] = value.Rotation.X; data[4] = value.Rotation.Y; data[5] = value.Rotation.Z; data[6] = value.Rotation.W;
            data[7] = value.Scale.X; data[8] = value.Scale.Y; data[9] = value.Scale.Z;
        }

        public bool SetBodyVelocity(ulong objectId, Vector3 value)
        {
            float* data = stackalloc float[3] { value.X, value.Y, value.Z };
            return Accessible && access.SetVelocity(access.Context, objectId, data) != 0;
        }
        public bool MoveKinematic(ulong objectId, Vector3 position, Quaternion rotation)
        {
            float* data = stackalloc float[7] { position.X, position.Y, position.Z, rotation.X, rotation.Y, rotation.Z, rotation.W };
            return Accessible && access.MoveKinematic(access.Context, objectId, data) != 0;
        }
        private bool ApplyForce(ulong objectId, Vector3 value, uint kind)
        {
            float* data = stackalloc float[3] { value.X, value.Y, value.Z };
            return Accessible && access.BodyForce(access.Context, objectId, data, kind) != 0;
        }
        public bool AddForce(ulong objectId, Vector3 force) => ApplyForce(objectId, force, 0);
        public bool AddImpulse(ulong objectId, Vector3 impulse) => ApplyForce(objectId, impulse, 1);
        public bool AddTorque(ulong objectId, Vector3 torque) => ApplyForce(objectId, torque, 2);
        public bool AddAngularImpulse(ulong objectId, Vector3 impulse) => ApplyForce(objectId, impulse, 3);
        public Vector3 GetBodyVelocity(ulong objectId)
        {
            float* data = stackalloc float[3];
            if (!Accessible || access.GetVelocity(access.Context, objectId, data) == 0)
                throw new InvalidOperationException("Object has no active physics body.");
            return new(data[0], data[1], data[2]);
        }
        public void Log(ulong objectId, string message)
        {
            if (!Accessible) return;
            var bytes = Encoding.UTF8.GetBytes(message.Length > 8192 ? message[..8192] : message);
            fixed (byte* pointer = bytes) access.Log(access.Context, objectId, pointer, bytes.Length);
        }

        // --- hierarquia -----------------------------------------------------
        public ulong ParentOf(ulong objectId) => Accessible ? access.ParentOf(access.Context, objectId) : 0;
        public int ChildCount(ulong objectId) => Accessible ? access.ChildCount(access.Context, objectId) : -1;
        public ulong ChildAt(ulong objectId, uint index) => Accessible ? access.ChildAt(access.Context, objectId, index) : 0;
        public ulong FindChild(ulong objectId, string name, bool recursive)
        {
            if (!Accessible) return 0;
            var bytes = Utf8(name, "nome");
            fixed (byte* pointer = bytes)
                return access.FindChild(access.Context, objectId, pointer, bytes.Length, recursive ? 1 : 0);
        }
        public string GetName(ulong objectId)
        {
            if (!Accessible) throw new WorldException(WorldStatus.NotRunning, "ler nome");
            byte* buffer = stackalloc byte[NameCapacity];
            var size = access.GetName(access.Context, objectId, buffer, NameCapacity);
            if (size <= 0 || size > NameCapacity) throw new WorldException(LastStatus, "ler nome");
            return Encoding.UTF8.GetString(buffer, size);
        }
        public bool SetName(ulong objectId, string name)
        {
            if (!Accessible) return false;
            var bytes = Utf8(name, "nome");
            fixed (byte* pointer = bytes) return access.SetName(access.Context, objectId, pointer, bytes.Length) != 0;
        }
        public int GetActive(ulong objectId) => Accessible ? access.GetActive(access.Context, objectId) : -1;
        public int GetActiveSelf(ulong objectId) => Accessible ? access.GetActiveSelf(access.Context, objectId) : -1;
        public string GetTag(ulong objectId)
        {
            if (!Accessible) throw new WorldException(WorldStatus.NotRunning, "ler tag");
            byte* buffer = stackalloc byte[64];
            var size = access.GetTag(access.Context, objectId, buffer, 64);
            if (size <= 0 || size > 63) throw new WorldException(LastStatus, "ler tag");
            return Encoding.UTF8.GetString(buffer, size);
        }
        public bool SetTag(ulong objectId, string tag)
        {
            if (!Accessible) return false;
            var bytes = Utf8(tag, "tag");
            fixed (byte* pointer = bytes) return access.SetTag(access.Context, objectId, pointer, bytes.Length) != 0;
        }
        public int CompareTag(ulong objectId, string tag)
        {
            if (!Accessible) return -1;
            var bytes = Utf8(tag, "tag");
            fixed (byte* pointer = bytes) return access.CompareTag(access.Context, objectId, pointer, bytes.Length);
        }
        public ulong[] FindTagged(string tag, bool firstOnly)
        {
            if (!Accessible) throw new WorldException(WorldStatus.NotRunning, "buscar tag");
            var bytes = Utf8(tag, "tag");
            fixed (byte* pointer = bytes)
            {
                var count = access.FindTagged(access.Context, pointer, bytes.Length, null, 0, firstOnly ? 1 : 0);
                if (count < 0 || count > 65536) throw new WorldException(LastStatus, "buscar tag");
                if (count == 0) return [];
                var result = new ulong[count];
                fixed (ulong* output = result)
                    if (access.FindTagged(access.Context, pointer, bytes.Length, output, count, firstOnly ? 1 : 0) != count)
                        throw new WorldException(LastStatus, "buscar tag");
                return result;
            }
        }
        public int GroupMembership(ulong objectId,string name,int operation)
        {
            if(!Accessible) return -1;
            var bytes=Utf8(name,"grupo");
            fixed(byte* pointer=bytes) return access.GroupMembership(access.Context,objectId,pointer,bytes.Length,operation);
        }
        public string[] GetGroups(ulong objectId)
        {
            if(!Accessible) throw new WorldException(WorldStatus.NotRunning,"ler grupos");
            var count=access.GroupAt(access.Context,objectId,uint.MaxValue,null,0);
            if(count<0 || count>32) throw new WorldException(LastStatus,"ler grupos");
            var result=new string[count];byte* buffer=stackalloc byte[64];
            for(uint i=0;i<count;++i) {
                var size=access.GroupAt(access.Context,objectId,i,buffer,64);
                if(size<1 || size>63) throw new WorldException(LastStatus,"ler grupos");
                result[i]=Encoding.UTF8.GetString(new ReadOnlySpan<byte>(buffer,size));
            }
            return result;
        }
        public ulong[] FindGroup(string name,bool includeInactive)
        {
            if(!Accessible) throw new WorldException(WorldStatus.NotRunning,"buscar grupo");
            var bytes=Utf8(name,"grupo");
            fixed(byte* pointer=bytes) {
                var count=access.FindGroup(access.Context,pointer,bytes.Length,null,0,includeInactive?1:0);
                if(count<0 || count>65536) throw new WorldException(LastStatus,"buscar grupo");
                if(count==0) return [];
                var result=new ulong[count];
                fixed(ulong* output=result)
                    if(access.FindGroup(access.Context,pointer,bytes.Length,output,count,includeInactive?1:0)!=count)
                        throw new WorldException(LastStatus,"buscar grupo");
                return result;
            }
        }
        public bool SetActive(ulong objectId, bool active) =>
            Accessible && access.SetActive(access.Context, objectId, active ? 1 : 0) != 0;

        // --- ciclo de vida --------------------------------------------------
        public ulong CreateObject(ulong parent, string name)
        {
            if (!Accessible) return 0;
            var bytes = Utf8(name, "nome");
            fixed (byte* pointer = bytes) return access.CreateObject(access.Context, parent, pointer, bytes.Length);
        }
        public bool DestroyObject(ulong objectId) => Accessible && access.DestroyObject(access.Context, objectId) != 0;
        public bool SetParent(ulong objectId, ulong parent, uint childIndex) =>
            Accessible && access.SetParent(access.Context, objectId, parent, childIndex) != 0;
        public bool SetParentWithPolicy(ulong objectId, ulong parent, uint childIndex, ReparentPosePolicy policy) =>
            Accessible && access.SetParentWithPolicy(access.Context, objectId, parent, childIndex, (uint)policy) != 0;
        public ulong QueueStructuralOperation(uint kind, ulong objectId, ulong other, uint childIndex,
                                              ReparentPosePolicy policy)
        {
            if (!Accessible) return 0;
            ulong ticket = 0;
            return access.QueueStructuralOperation(access.Context, kind, objectId, other, childIndex,
                                                   (uint)policy, &ticket) != 0 ? ticket : 0;
        }
        public WorldStatus QueryOperation(uint world, ulong operationId, out WorldOperationState state,
                                          out WorldStatus result)
        {
            state = default;
            result = default;
            if (!Accessible) return WorldStatus.NotRunning;
            uint rawState = 0, rawResult = 0;
            if (access.QueryOperation(access.Context, world, operationId, &rawState, &rawResult) == 0)
                return LastStatus;
            state = (WorldOperationState)rawState;
            result = (WorldStatus)rawResult;
            return WorldStatus.Ok;
        }

        // --- componentes ----------------------------------------------------
        public int ComponentCount(ulong objectId) => Accessible ? access.ComponentCount(access.Context, objectId) : -1;
        public (ulong Instance, string TypeId) ComponentAt(ulong objectId, uint index)
        {
            if (!Accessible) return (0, string.Empty);
            byte* buffer = stackalloc byte[TypeIdCapacity];
            var instance = access.ComponentAt(access.Context, objectId, index, buffer, TypeIdCapacity);
            if (instance == 0) return (0, string.Empty);
            var length = 0;
            while (length < TypeIdCapacity && buffer[length] != 0) ++length;
            return (instance, Encoding.UTF8.GetString(buffer, length));
        }
        public ulong FindComponent(ulong objectId, string typeId, uint ordinal)
        {
            if (!Accessible) return 0;
            var bytes = Utf8(typeId, "tipo de componente");
            fixed (byte* pointer = bytes)
                return access.FindComponent(access.Context, objectId, pointer, bytes.Length, ordinal);
        }
        public IBehaviorRegistry? Behaviors => Accessible ? _world : null;
        public ulong AddBehavior(ulong objectId, string typeId, string source)
        {
            if (!Accessible) return 0;
            var typeBytes = Utf8(typeId, "tipo"); var sourceBytes = Utf8(source, "fonte");
            fixed (byte* t = typeBytes) fixed (byte* f = sourceBytes)
                return access.AddBehavior(access.Context, objectId, t, typeBytes.Length, f, sourceBytes.Length);
        }
        public bool DestroyAfter(ulong objectId, double seconds) => Accessible && access.DestroyAfter(access.Context, objectId, seconds) != 0;
        public IReadOnlyDictionary<ulong, ulong> Instantiate(ulong source, ulong parent)
        {
            if (!Accessible) throw new WorldException(WorldStatus.NotRunning, "instanciar hierarquia");
            var count = access.Instantiate(access.Context, source, parent, null, 0);
            if (count <= 0 || count > 65536) throw new WorldException(LastStatus, "preparar instanciação");
            var pairs = new ulong[count * 2];
            fixed (ulong* output = pairs)
                if (access.Instantiate(access.Context, source, parent, output, count) != count)
                    throw new WorldException(LastStatus, "instanciar hierarquia");
            var result = new Dictionary<ulong, ulong>(count);
            for (var i = 0; i < count; ++i) result.Add(pairs[2*i], pairs[2*i+1]);
            return result;
        }
        public ulong CreatePrimitive(ulong parent, PrimitiveType type) => Accessible ? access.CreatePrimitive(access.Context, parent, (uint)type) : 0;
        public ulong InstantiatePrefab(ulong parent, AssetGuid asset) => Accessible ? access.InstantiatePrefab(access.Context, parent, new NativeAssetGuid { High=asset.High, Low=asset.Low }) : 0;
        public string InstantiationAttachments(ulong root)
        {
            if (!Accessible) throw new WorldException(WorldStatus.NotRunning, "ler scripts do prefab");
            var count = access.InstantiationAttachments(access.Context, root, null, 0);
            if (count < 2 || count > 32*1024*1024) throw new WorldException(LastStatus, "ler scripts do prefab");
            var bytes = new byte[count];
            fixed (byte* output = bytes)
                if (access.InstantiationAttachments(access.Context, root, output, count) != count)
                    throw new WorldException(LastStatus, "ler scripts do prefab");
            return Encoding.UTF8.GetString(bytes);
        }
        public bool FinishInstantiation(ulong root, bool commit) => Accessible && access.FinishInstantiation(access.Context, root, commit ? 1 : 0) != 0;

        public ulong AddComponent(ulong objectId, string typeId)
        {
            if (!Accessible) return 0;
            var bytes = Utf8(typeId, "tipo de componente");
            fixed (byte* pointer = bytes) return access.AddComponent(access.Context, objectId, pointer, bytes.Length);
        }
        public bool RemoveComponent(ulong objectId, ulong instanceId) =>
            Accessible && access.RemoveComponent(access.Context, objectId, instanceId) != 0;
        public bool TryGetProperty(ulong objectId, ulong instanceId, string propertyId, out uint kind, out ulong bits)
        {
            kind = 0; bits = 0;
            if (!Accessible) return false;
            var bytes = Utf8(propertyId, "propriedade");
            fixed (byte* pointer = bytes)
            fixed (uint* kindOut = &kind)
            fixed (ulong* bitsOut = &bits)
                return access.GetProperty(access.Context, objectId, instanceId, pointer, bytes.Length, kindOut, bitsOut) != 0;
        }
        public bool SetProperty(ulong objectId, ulong instanceId, string propertyId, uint kind, ulong bits)
        {
            if (!Accessible) return false;
            var bytes = Utf8(propertyId, "propriedade");
            fixed (byte* pointer = bytes)
                return access.SetProperty(access.Context, objectId, instanceId, pointer, bytes.Length, kind, bits) != 0;
        }
        public bool SetTriple(ulong objectId, ulong instanceId, string propertyId, Vector3 value)
        {
            if (!Accessible) return false;
            var bytes = Utf8(propertyId, "propriedade vetorial");
            float* values = stackalloc float[3] { value.X, value.Y, value.Z };
            fixed (byte* pointer = bytes)
                return access.SetTriple(access.Context, objectId, instanceId, pointer, bytes.Length, values) != 0;
        }

        // --- consultas físicas ----------------------------------------------
        private static NativeQueryFilter Encode(in QueryFilter filter) => new()
        {
            Size = (uint)sizeof(NativeQueryFilter),
            GameplayLayerMask = filter.LayerMask,
            Flags = (filter.IncludeStatic ? 1u : 0u) | (filter.IncludeDynamic ? 2u : 0u) | (filter.IncludeSensors ? 4u : 0u),
            Reserved = 0,
            Ignore = filter.Ignore,
        };
        private static NativeShapeQuery Encode(in ShapeQuery shape) => new()
        {
            Kind = (uint)shape.Kind,
            HalfX = shape.HalfExtent.X, HalfY = shape.HalfExtent.Y, HalfZ = shape.HalfExtent.Z,
            Radius = shape.Radius, HalfHeight = shape.HalfHeight,
            RotationX = shape.Rotation.X, RotationY = shape.Rotation.Y,
            RotationZ = shape.Rotation.Z, RotationW = shape.Rotation.W,
        };

        public int PathPointCommand(ulong id,uint world,uint generation,ulong instance,uint operation,ulong element,uint index,ReadOnlySpan<float> input,Span<float> output,out ulong identity)
        {
            identity=0;
            if(!Accessible) return -1;
            if(operation>10 || ((operation==3||operation==4)&&input.Length!=9) || ((operation==1||operation==2)&&output.Length!=9) || ((operation==9||operation==10)&&input.Length!=10) || ((operation==7||operation==8)&&output.Length!=10))
                throw new ArgumentException("Invalid path point command buffers.");
            ulong pointId=0;
            fixed(float* source=input) fixed(float* destination=output)
            {
                int result=access.PathPointCommand(access.Context,id,world,generation,instance,operation,element,index,source,destination,&pointId);
                identity=pointId; return result;
            }
        }
        public bool PathRuntimeCommand(ulong id,uint world,uint generation,ulong instance,uint operation,double distance,bool wrap,Span<float> output,out double scalar)
        {
            scalar=0;
            if(!Accessible) return false;
            if(operation>5 || (operation==0&&output.Length!=6) || (operation==5&&output.Length!=10)) throw new ArgumentException("Invalid path runtime command buffer.");
            double value=0;
            fixed(float* destination=output)
            {
                bool ok=access.PathRuntimeCommand(access.Context,id,world,generation,instance,operation,distance,wrap?1u:0u,destination,&value)!=0;
                scalar=value;return ok;
            }
        }
        public bool CommandAudioVoice(ulong id,uint world,uint generation,ulong instance,AudioVoiceCommand command,double seconds)
        {
            if(!Accessible) return false;
            return access.AudioCommand(access.Context,id,world,generation,instance,(uint)command,seconds)!=0;
        }
        public bool GuiCommand(uint world,uint node,uint operation,string text,float value,GuiKind createKind,
            out GuiSnapshot snapshot,out GuiEventKind eventKind)
        {
            snapshot=default;eventKind=default;
            if(!Accessible) return false;
            byte[] bytes=Encoding.UTF8.GetBytes(text);
            if(bytes.Length>4096 || text.Contains('\0')) throw new ArgumentException("UI text exceeds 4096 bytes or contains NUL.",nameof(text));
            NativeGuiState result=new() { Kind=(uint)createKind };
            fixed(byte* pointer=bytes) {
                if(access.GuiCommand(access.Context,world,node,operation,pointer,bytes.Length,value,&result)==0) return false;
            }
            snapshot=new(result.World,result.Node,(GuiKind)result.Kind,result.Visible!=0,result.Enabled!=0,result.Value,result.Minimum,result.Maximum);
            eventKind=(GuiEventKind)result.Event;return true;
        }
        public bool GuiProperties(uint world,uint node,bool write,ref GuiLayout layout,ref GuiStyle style)
        {
            if(!Accessible) return false;
            NativeGuiProperties result=new() {
                AnchorMinX=layout.AnchorMin.X,AnchorMinY=layout.AnchorMin.Y,AnchorMaxX=layout.AnchorMax.X,AnchorMaxY=layout.AnchorMax.Y,
                Left=layout.Offsets.X,Top=layout.Offsets.Y,Right=layout.Offsets.Z,Bottom=layout.Offsets.W,
                Background=style.Background,Foreground=style.Foreground,Accent=style.Accent,ClipChildren=style.ClipChildren?1u:0u,
                FontSize=style.FontSize,Radius=style.Radius
            };
            if(access.GuiProperties(access.Context,world,node,write?1u:0u,&result)==0) return false;
            layout=new(new(result.AnchorMinX,result.AnchorMinY),new(result.AnchorMaxX,result.AnchorMaxY),new(result.Left,result.Top,result.Right,result.Bottom));
            style=new(result.Background,result.Foreground,result.Accent,result.ClipChildren!=0,result.FontSize,result.Radius);return true;
        }
        public bool GuiSizing(uint world,uint node,bool write,ref GuiSizing sizing,ref GuiImageStyle image) {
            if(!Accessible)return false;
            NativeGuiSizing raw=new(){MinX=sizing.Minimum.X,MinY=sizing.Minimum.Y,PrefX=sizing.Preferred.X,PrefY=sizing.Preferred.Y,FlexX=sizing.Flexible.X,FlexY=sizing.Flexible.Y,Left=sizing.Padding.X,Top=sizing.Padding.Y,Right=sizing.Padding.Z,Bottom=sizing.Padding.W,SpaceX=sizing.Spacing.X,SpaceY=sizing.Spacing.Y,Alignment=(uint)sizing.Alignment,Columns=sizing.Columns,Ignore=sizing.Ignore?1u:0u,ImageFit=(uint)image.Fit,ImageTint=image.Tint};
            if(access.GuiSizing(access.Context,world,node,write?1u:0u,&raw)==0)return false;
            sizing=new(new(raw.MinX,raw.MinY),new(raw.PrefX,raw.PrefY),new(raw.FlexX,raw.FlexY),new(raw.Left,raw.Top,raw.Right,raw.Bottom),new(raw.SpaceX,raw.SpaceY),(GuiAlignment)raw.Alignment,raw.Columns,raw.Ignore!=0);image=new((GuiImageFit)raw.ImageFit,raw.ImageTint);return true;
        }
        public bool GuiCanvas(uint world,bool write,ref GuiCanvas canvas) {
            if(!Accessible)return false;
            NativeGuiCanvas raw=new(){Mode=(uint)canvas.Mode,Width=canvas.Resolution.X,Height=canvas.Resolution.Y,X=canvas.Position.X,Y=canvas.Position.Y,Z=canvas.Position.Z,Rx=canvas.Rotation.X,Ry=canvas.Rotation.Y,Rz=canvas.Rotation.Z,Units=canvas.UnitsPerPixel,Occlusion=canvas.Occlusion?1u:0u};
            if(access.GuiCanvas(access.Context,world,write?1u:0u,&raw)==0)return false;
            canvas=new((GuiCanvasMode)raw.Mode,new(raw.Width,raw.Height),new(raw.X,raw.Y,raw.Z),new(raw.Rx,raw.Ry,raw.Rz),raw.Units,raw.Occlusion!=0);return true;
        }
        public bool GuiBehavior(uint world,uint node,bool write,ref GuiInteraction interaction,ref GuiAnimation animation) {
            if(!Accessible)return false;
            NativeGuiBehavior raw=new(){Clickable=interaction.Clickable?1u:0u,Action=(uint)interaction.Action,Target=interaction.Target,Value=interaction.Value,
                Enabled=animation.Enabled?1u:0u,AutoPlay=animation.AutoPlay?1u:0u,Loop=animation.Loop?1u:0u,PingPong=animation.PingPong?1u:0u,Easing=(uint)animation.Easing,Duration=animation.Duration,Delay=animation.Delay,
                FromX=animation.From.Position.X,FromY=animation.From.Position.Y,FromScale=animation.From.Scale,FromOpacity=animation.From.Opacity,
                ToX=animation.To.Position.X,ToY=animation.To.Position.Y,ToScale=animation.To.Scale,ToOpacity=animation.To.Opacity};
            if(access.GuiBehavior(access.Context,world,node,write?1u:0u,&raw)==0)return false;
            interaction=new(raw.Clickable!=0,(GuiClickAction)raw.Action,raw.Target,raw.Value);
            animation=new(raw.Enabled!=0,raw.AutoPlay!=0,raw.Loop!=0,raw.PingPong!=0,(GuiEasing)raw.Easing,raw.Duration,raw.Delay,new(new(raw.FromX,raw.FromY),raw.FromScale,raw.FromOpacity),new(new(raw.ToX,raw.ToY),raw.ToScale,raw.ToOpacity));return true;
        }
        public string ReadGuiDiagnostic(uint world)=>ReadGuiString(world,0,3);
        public int GuiAction(uint world,uint node,uint operation,uint index,ref GuiActionBinding binding) {
            if(!Accessible)return -1;
            NativeGuiAction raw=new(){Event=(uint)binding.Event,Action=(uint)binding.Action,Target=binding.Target,Value=binding.Value};
            int result=access.GuiAction(access.Context,world,node,operation,index,&raw);
            if(result>=0)binding=new((GuiEventKind)raw.Event,(GuiClickAction)raw.Action,raw.Target,raw.Value);return result;
        }
        public bool GuiTransitions(uint world,uint node,bool write,ref GuiTransitions transitions) {
            if(!Accessible)return false;
            var t=transitions;
            NativeGuiTransitions raw=new(){Enabled=t.Enabled?1u:0u,Easing=(uint)t.Easing,Duration=t.Duration,
                NormalX=t.Normal.Pose.Position.X,NormalY=t.Normal.Pose.Position.Y,NormalScale=t.Normal.Pose.Scale,NormalOpacity=t.Normal.Pose.Opacity,NormalTint=t.Normal.Tint,
                PressedX=t.Pressed.Pose.Position.X,PressedY=t.Pressed.Pose.Position.Y,PressedScale=t.Pressed.Pose.Scale,PressedOpacity=t.Pressed.Pose.Opacity,PressedTint=t.Pressed.Tint,
                DisabledX=t.Disabled.Pose.Position.X,DisabledY=t.Disabled.Pose.Position.Y,DisabledScale=t.Disabled.Pose.Scale,DisabledOpacity=t.Disabled.Pose.Opacity,DisabledTint=t.Disabled.Tint};
            if(access.GuiTransitions(access.Context,world,node,write?1u:0u,&raw)==0)return false;
            transitions=new(raw.Enabled!=0,raw.Duration,(GuiEasing)raw.Easing,new(new(new(raw.NormalX,raw.NormalY),raw.NormalScale,raw.NormalOpacity),raw.NormalTint),new(new(new(raw.PressedX,raw.PressedY),raw.PressedScale,raw.PressedOpacity),raw.PressedTint),new(new(new(raw.DisabledX,raw.DisabledY),raw.DisabledScale,raw.DisabledOpacity),raw.DisabledTint));return true;
        }
        public string ReadGuiText(uint world,uint node,bool name)=>ReadGuiString(world,node,name?1u:0u);
        public string ReadGuiImage(uint world,uint node)=>ReadGuiString(world,node,2);
        private string ReadGuiString(uint world,uint node,uint field)
        {
            if(!Accessible) throw new WorldException(WorldStatus.NotRunning,"read UI text");
            int length=access.GuiText(access.Context,world,node,field,null,0);
            if(length<0) throw new WorldException(LastStatus,"read UI text");
            if(length>4096) throw new InvalidOperationException("Native UI string exceeds its contract.");
            byte[] bytes=new byte[length];
            fixed(byte* buffer=bytes) if(access.GuiText(access.Context,world,node,field,buffer,length)!=length)
                throw new WorldException(LastStatus,"read UI text");
            return Encoding.UTF8.GetString(bytes);
        }
        public bool QueryAudioVoice(ulong id,uint world,uint generation,ulong instance,out AudioVoiceSnapshot snapshot)
        {
            snapshot=default;
            if(!Accessible) return false;
            NativeAudioSnapshot value=new() { Size=(uint)sizeof(NativeAudioSnapshot) };
            if(access.AudioSnapshot(access.Context,id,world,generation,instance,&value)==0) return false;
            snapshot=new((AudioVoiceState)value.State,value.Cursor,value.OutputRunning!=0);
            return true;
        }
        public bool Body2DCommand(ulong id, uint world, uint generation, Body2DCommandKind command, Vector2 value, float angular, out Vector2 result, out float angularResult)
        {
            result = default; angularResult = 0; if (!Accessible) return false;
            float* input = stackalloc float[3] { value.X, value.Y, angular };
            float* output = stackalloc float[3];
            if (access.Body2DCommand(access.Context,id,world,generation,(uint)command,input,output)==0) return false;
            if(command == Body2DCommandKind.GetVelocity) { result=new(output[0],output[1]); angularResult=output[2]; }
            return true;
        }
        public int Query2D(uint world, bool overlap, Vector2 origin, Vector2 translation, float radius, in QueryFilter filter, Span<RawQueryHit> results)
        {
            if (!Accessible) return -1;
            var encoded = Encode(filter); float* from=stackalloc float[2]{origin.X,origin.Y}; float* along=stackalloc float[2]{translation.X,translation.Y};
            fixed(RawQueryHit* hits=results) return access.Query2D(access.Context,world,overlap?1u:0u,from,along,radius,&encoded,hits,results.Length);
        }
        public int RayCast(Vector3 origin, Vector3 direction, in QueryFilter filter, Span<RawQueryHit> results)
        {
            if (!Accessible) return 0;
            var nativeFilter = Encode(filter);
            float* from = stackalloc float[3] { origin.X, origin.Y, origin.Z };
            float* along = stackalloc float[3] { direction.X, direction.Y, direction.Z };
            fixed (RawQueryHit* hits = results)
                return access.RayCast(access.Context, from, along, &nativeFilter, hits, results.Length);
        }
        public int ShapeCast(in ShapeQuery shape, Vector3 origin, Vector3 direction, in QueryFilter filter, out RawQueryHit hit)
        {
            hit = default;
            if (!Accessible) return 0;
            var nativeFilter = Encode(filter);
            var nativeShape = Encode(shape);
            float* from = stackalloc float[3] { origin.X, origin.Y, origin.Z };
            float* along = stackalloc float[3] { direction.X, direction.Y, direction.Z };
            fixed (RawQueryHit* single = &hit)
                return access.ShapeCast(access.Context, &nativeShape, from, along, &nativeFilter, single);
        }
        public int Overlap(in ShapeQuery shape, Vector3 origin, in QueryFilter filter, Span<RawQueryHit> results)
        {
            if (!Accessible) return 0;
            var nativeFilter = Encode(filter);
            var nativeShape = Encode(shape);
            float* from = stackalloc float[3] { origin.X, origin.Y, origin.Z };
            fixed (RawQueryHit* hits = results)
                return access.Overlap(access.Context, &nativeShape, from, &nativeFilter, hits, results.Length);
        }
        public int LayerByName(string name)
        {
            if (!Accessible) return -1;
            var bytes = Utf8(name, "camada");
            fixed (byte* pointer = bytes) return access.LayerByName(access.Context, pointer, bytes.Length);
        }
        public string LayerName(uint layer)
        {
            if (!Accessible) return string.Empty;
            byte* buffer = stackalloc byte[NameCapacity];
            var size = access.LayerName(access.Context, layer, buffer, NameCapacity);
            return size <= 0 || size > NameCapacity ? string.Empty : Encoding.UTF8.GetString(buffer, size);
        }

        // --- entrada por ações ----------------------------------------------
        public bool NumberTweenCreate(ulong objectId,ulong instance,string property,float destination,float duration,uint easing,bool unscaled,out ulong id)
        {
            if(!Accessible)throw new WorldException(WorldStatus.NotRunning,"criar tween numérico");
            var bytes=Encoding.UTF8.GetBytes(property);NativeNumberTweenParameters parameters=new(){Size=(uint)sizeof(NativeNumberTweenParameters),Easing=easing,Destination=destination,Duration=duration,Flags=unscaled?1u:0u};
            ulong handle=0;int result;fixed(byte* name=bytes)result=access.NumberTweenCreate(access.Context,objectId,instance,name,bytes.Length,&parameters,&handle);id=handle;return result==1;
        }
        public bool NumberTweenCommand(ulong id,uint operation,out NumberTweenState state)
        {
            if(!Accessible)throw new WorldException(WorldStatus.NotRunning,"controlar tween numérico");
            NativeNumberTweenState native=new(){Size=(uint)sizeof(NativeNumberTweenState)};state=default;
            if(access.NumberTweenCommand(access.Context,id,operation,&native)!=1)return false;
            state=new((NumberTweenStatus)native.Status,(WorldStatus)native.Failure,native.Elapsed,native.Value,native.Duration,(native.Flags&1)!=0,(native.Flags&2)!=0,(native.Flags&4)!=0);return true;
        }
        public bool TweenCommand(ulong objectId,ulong instance,uint operation,out TweenRuntimeState state)
        {
            if(!Accessible)throw new WorldException(WorldStatus.NotRunning,"controlar tween");
            NativeTweenState native=new(){Size=(uint)sizeof(NativeTweenState)};state=default;
            if(access.TweenCommand(access.Context,objectId,instance,operation,&native)!=1)return false;
            state=new((TweenRuntimeStatus)native.Status,native.Elapsed,(native.Flags&1)!=0,(native.Flags&2)!=0,(native.Flags&4)!=0);return true;
        }
        public bool TimerCommand(ulong objectId,ulong instance,uint operation,float seconds,out TimerRuntimeState state)
        {
            if(!Accessible)throw new WorldException(WorldStatus.NotRunning,"controlar timer");
            NativeTimerState native=new(){Size=(uint)sizeof(NativeTimerState)};state=default;
            if(access.TimerCommand(access.Context,objectId,instance,operation,seconds,&native)!=1)return false;
            state=new(native.Remaining,(native.Flags&1)!=0,(native.Flags&2)!=0,(native.Flags&4)!=0,(native.Flags&8)!=0,(native.Flags&16)!=0);return true;
        }
        public int InputCaptureCommand(uint operation,string action,uint index,InputSource source,bool negative,uint cancelKey)
        {
            var bytes=Encoding.UTF8.GetBytes(action);
            fixed(byte* name=bytes)return access.InputCaptureCommand(access.Context,operation,name,bytes.Length,index,(uint)source,negative?1u:0u,cancelKey);
        }
        public bool InputActionCommand(string action,uint operation,ref InputActionState state)
        {
            var bytes=Encoding.UTF8.GetBytes(action);
            fixed(byte* name=bytes)fixed(InputActionState* value=&state)
                return access.InputActionCommand(access.Context,name,bytes.Length,operation,value)==1;
        }
        public bool InputBindingCommand(string action,uint index,uint operation,ref InputBindingValue binding)
        {
            var bytes=Encoding.UTF8.GetBytes(action);
            fixed(byte* name=bytes) fixed(InputBindingValue* value=&binding)
                return access.InputBindingCommand(access.Context,name,bytes.Length,index,operation,value)==1;
        }
        public string ExportInputProfile()
        {
            var size=access.InputProfile(access.Context,0,null,0,null,0);
            if(size<=0||size>262144)throw new InvalidOperationException("Input profile unavailable");
            var data=new byte[size];
            fixed(byte* output=data)if(access.InputProfile(access.Context,0,null,0,output,size)!=size)
                throw new InvalidOperationException("Input profile changed during export");
            return Encoding.UTF8.GetString(data);
        }
        public bool ImportInputProfile(string profile)
        {
            var data=Encoding.UTF8.GetBytes(profile);
            if(data.Length==0||data.Length>262144)return false;
            fixed(byte* input=data)return access.InputProfile(access.Context,1,input,data.Length,null,0)==1;
        }
        public bool InputAxis(string action, out Vector2 value)
        {
            value = default;
            if (!Accessible) return false;
            var bytes = Utf8(action, "ação");
            float* raw = stackalloc float[2];
            int ok;
            fixed (byte* pointer = bytes) ok = access.InputAxis(access.Context, pointer, bytes.Length, raw);
            if (ok == 0) return false;
            value = new Vector2(raw[0], raw[1]);
            return true;
        }
        public int InputButton(string action, uint query)
        {
            if (!Accessible) return -1;
            var bytes = Utf8(action, "ação");
            fixed (byte* pointer = bytes) return access.InputButton(access.Context, pointer, bytes.Length, query);
        }
        public bool InputContext(string context, int enabled)
        {
            if (!Accessible) return false;
            var bytes = Utf8(context, "contexto");
            fixed (byte* pointer = bytes) return access.InputContext(access.Context, pointer, bytes.Length, enabled) != 0;
        }
        public string InputRole(uint role)
        {
            if (!Accessible) return string.Empty;
            byte* buffer = stackalloc byte[NameCapacity];
            var size = access.InputRole(access.Context, role, buffer, NameCapacity);
            return size <= 0 || size > NameCapacity ? string.Empty : Encoding.UTF8.GetString(buffer, size);
        }

        public GraphicsSnapshot GetGraphicsState(uint expectedWorld)
        {
            if (!Accessible) throw new WorldException(WorldStatus.NotRunning, "ler gráficos");
            NativeGraphicsState state = default;
            state.Size = (uint)sizeof(NativeGraphicsState);
            state.Requested.Size = (uint)sizeof(GraphicsSettings);
            state.Effective.Size = (uint)sizeof(ResolvedGraphicsSettings);
            state.Capabilities.Size = (uint)sizeof(GraphicsCapabilities);
            if (access.GetRenderingState(access.Context, expectedWorld, &state) == 0)
                throw new WorldException(LastStatus, "ler gráficos");
            var length = access.CopyRenderingDiagnostics(access.Context, expectedWorld, null, 0);
            var diagnostics = string.Empty;
            if (length > 0)
            {
                var bytes = new byte[length];
                fixed (byte* pointer = bytes)
                    if (access.CopyRenderingDiagnostics(access.Context, expectedWorld, pointer, length) == length)
                        diagnostics = Encoding.UTF8.GetString(bytes);
            }
            return new(state.Requested, state.Effective, state.Capabilities, state.Pending != 0,
                state.PendingRequestId, state.LastRequestSucceeded != 0, state.EffectiveAvailable != 0, diagnostics,
                state.ExecutedUpscaler, state.ExecutedStatus, state.TextureStreaming, state.Frame);
        }
        public bool SetGraphicsSettings(uint expectedWorld, GraphicsSettings settings, out ulong requestId)
        {
            requestId = 0;
            if (!Accessible) return false;
            settings.Size = (uint)sizeof(GraphicsSettings);
            fixed (ulong* request = &requestId)
                return access.SetRenderingSettings(access.Context, expectedWorld, &settings, request) != 0;
        }
        public bool TryGetResource(ulong objectId, ulong instanceId, string propertyId, uint slot, out AssetGuid value)
        {
            value = default;
            if (!Accessible) return false;
            var bytes = Utf8(propertyId, "recurso"); NativeAssetGuid raw;
            fixed (byte* pointer = bytes)
                if (access.GetComponentResource(access.Context, objectId, instanceId, pointer, bytes.Length, slot, &raw) == 0) return false;
            value = new(raw.High, raw.Low); return true;
        }
        public bool SetResource(ulong objectId, ulong instanceId, string propertyId, uint slot, AssetGuid value)
        {
            if (!Accessible) return false;
            var bytes = Utf8(propertyId, "recurso"); var raw = new NativeAssetGuid { High=value.High, Low=value.Low };
            fixed (byte* pointer = bytes)
                return access.SetComponentResource(access.Context, objectId, instanceId, pointer, bytes.Length, slot, raw) != 0;
        }
        public bool TryGetSlotProperty(ulong objectId,ulong instanceId,string propertyId,uint slot,out uint kind,out ulong bits)
        {
            kind=0;bits=0;if(!Accessible)return false;var bytes=Utf8(propertyId,"propriedade por slot");
            fixed(byte* pointer=bytes)fixed(uint* kindOut=&kind)fixed(ulong* bitsOut=&bits)
                return access.GetComponentSlotProperty(access.Context,objectId,instanceId,pointer,bytes.Length,slot,kindOut,bitsOut)!=0;
        }
        public bool SetSlotProperty(ulong objectId,ulong instanceId,string propertyId,uint slot,uint kind,ulong bits)
        {
            if(!Accessible)return false;var bytes=Utf8(propertyId,"propriedade por slot");
            fixed(byte* pointer=bytes)
                return access.SetComponentSlotProperty(access.Context,objectId,instanceId,pointer,bytes.Length,slot,kind,bits)!=0;
        }
        public bool CharacterMove(ulong objectId, Vector2 input, float yawRadians)
        {
            if (!Accessible) return false;
            float* command = stackalloc float[3] { input.X, input.Y, yawRadians };
            return access.CharacterMove(access.Context, objectId, command) != 0;
        }
        public int ObjectLayer(ulong id,uint world,uint generation,int layer) => Accessible ? access.ObjectLayer(access.Context,id,world,generation,layer) : -1;
        public bool FieldQuery(ulong id,uint world,uint generation,ulong instance,uint operation,Vector3 point,uint layer,out PhysicsFieldSample sample) {
            sample=default;var native=new NativeFieldState {Size=(uint)sizeof(NativeFieldState)};
            if(access.FieldQuery(access.Context,id,world,generation,instance,operation,(float*)&point,layer,&native)==0)return false;
            sample=new PhysicsFieldSample((native.Flags&1)!=0,(native.Flags&2)!=0,native.Weight,native.Acceleration,native.WindVelocity,native.WindDrag,native.LinearDrag,native.AngularDrag,native.OverrideWeight,native.AffectedBodies,native.AffectedMass);return true;
        }
        public bool BodyCommand(ulong id,uint world,uint generation,ulong instance,uint operation,Vector3 value,Vector3 point,out PhysicsBodyState state) {
            state=default;if(!Accessible)return false;
            var native=new NativeBodyState {Size=(uint)sizeof(NativeBodyState)};
            if(access.BodyCommand(access.Context,id,world,generation,instance,operation,(float*)&value,(float*)&point,&native)==0)return false;
            state=new PhysicsBodyState(native.Linear,native.Angular,native.CenterOfMass,(native.Flags&1)!=0,(native.Flags&8)!=0);return true;
        }
        public bool TryGetCharacterState(ulong objectId,out CharacterRuntimeState state) {
            state=default;if(!Accessible)return false;var value=new NativeCharacterState {Size=(uint)sizeof(NativeCharacterState)};
            if(access.CharacterSnapshot(access.Context,objectId,&value)==0)return false;
            state=new CharacterRuntimeState((CharacterGroundState)value.GroundState,(value.Flags&1)!=0,value.Position,value.Velocity,value.MotorVelocity,value.GroundVelocity,value.GroundNormal);return true;
        }
        public bool CharacterJump(ulong objectId) => Accessible && access.CharacterJump(access.Context, objectId) != 0;
        public bool CameraLook(ulong objectId, Vector2 normalizedDelta)
        {
            if (!Accessible) return false;
            float* command = stackalloc float[2] { normalizedDelta.X, normalizedDelta.Y };
            return access.CameraLook(access.Context, objectId, command) != 0;
        }
        public bool AnimationCommand(ulong objectId, ulong instanceId, AnimationCommandKind op, AssetGuid clip,
                                     float seconds, float targetWeight, AnimationPlayMode mode)
        {
            if (!Accessible) return false;
            var command = new NativeAnimationCommand
            {
                Size = (uint)sizeof(NativeAnimationCommand), Op = (uint)op,
                Clip = new NativeAssetGuid { High = clip.High, Low = clip.Low },
                Seconds = seconds, TargetWeight = targetWeight, PlayMode = (uint)mode
            };
            return access.AnimationCommand(access.Context, objectId, instanceId, &command) != 0;
        }
        public bool TryGetAnimationState(ulong objectId, ulong instanceId, AssetGuid clip, out AnimationStateValue value)
        {
            value = default;
            if (!Accessible) return false;
            var state = new NativeAnimationState { Size = (uint)sizeof(NativeAnimationState) };
            if (access.GetAnimationState(access.Context, objectId, instanceId,
                    new NativeAssetGuid { High = clip.High, Low = clip.Low }, &state) == 0) return false;
            value = new AnimationStateValue(new AssetGuid(state.Clip.High, state.Clip.Low), state.Enabled != 0, state.Time,
                state.Speed, state.Weight, state.Length, state.Layer, (AnimationWrapMode)state.WrapMode);
            return true;
        }
        public bool SetAnimationState(ulong objectId, ulong instanceId, in AnimationStateValue value)
        {
            if (!Accessible) return false;
            var state = new NativeAnimationState
            {
                Size = (uint)sizeof(NativeAnimationState), Enabled = value.Enabled ? 1u : 0u,
                Clip = new NativeAssetGuid { High = value.Clip.High, Low = value.Clip.Low },
                Time = value.Time, Speed = value.Speed, Weight = value.Weight, Layer = value.Layer, WrapMode = (uint)value.WrapMode
            };
            return access.SetAnimationState(access.Context, objectId, instanceId, &state) != 0;
        }
        public int AnimationClipAt(ulong objectId, ulong instanceId, uint index, out AssetGuid clip, out string name)
        {
            clip = default; name = "";
            if (!Accessible) return -1;
            const int capacity = 256;
            byte* text = stackalloc byte[capacity];
            text[0] = 0;
            NativeAssetGuid guid = default;
            var count = access.AnimationClipAt(access.Context, objectId, instanceId, index, &guid, text, capacity);
            if (count < 0 || index >= (uint)count) return count;
            clip = new AssetGuid(guid.High, guid.Low);
            var length = 0;
            while (length < capacity && text[length] != 0) ++length;
            name = Encoding.UTF8.GetString(text, length);
            return count;
        }
        public bool ResourceElementId(ulong objectId, ulong instanceId, string propertyId, uint slot, out ulong elementId)
        {
            elementId = 0;
            if (!Accessible) return false;
            var bytes = Utf8(propertyId, "elemento de recurso");
            fixed (byte* pointer = bytes) fixed (ulong* result = &elementId)
                return access.ResourceElementId(access.Context, objectId, instanceId, pointer, bytes.Length, slot, result) != 0;
        }
        public bool TryGetResourceByElementId(ulong objectId, ulong instanceId, string propertyId, ulong elementId, out AssetGuid value)
        {
            value = default;
            if (!Accessible) return false;
            var bytes = Utf8(propertyId, "elemento de recurso"); NativeAssetGuid raw;
            fixed (byte* pointer = bytes)
                if (access.GetResourceByElementId(access.Context, objectId, instanceId, pointer, bytes.Length, elementId, &raw) == 0) return false;
            value = new(raw.High, raw.Low); return true;
        }
        public bool SetResourceByElementId(ulong objectId, ulong instanceId, string propertyId, ulong elementId, AssetGuid value)
        {
            if (!Accessible) return false;
            var bytes = Utf8(propertyId, "elemento de recurso"); var raw = new NativeAssetGuid { High=value.High, Low=value.Low };
            fixed (byte* pointer = bytes)
                return access.SetResourceByElementId(access.Context, objectId, instanceId, pointer, bytes.Length, elementId, raw) != 0;
        }
        public bool AppendAnimationClip(ulong objectId, ulong instanceId, AssetGuid clip, out ulong elementId)
        {
            elementId = 0;
            if (!Accessible) return false;
            var raw = new NativeAssetGuid { High=clip.High, Low=clip.Low };
            fixed (ulong* result = &elementId)
                return access.AppendAnimationClip(access.Context, objectId, instanceId, raw, result) != 0;
        }
        public bool RemoveAnimationClip(ulong objectId, ulong instanceId, ulong elementId) =>
            Accessible && access.RemoveAnimationClip(access.Context, objectId, instanceId, elementId) != 0;
        public bool MoveAnimationClip(ulong objectId, ulong instanceId, ulong elementId, uint targetIndex) =>
            Accessible && access.MoveAnimationClip(access.Context, objectId, instanceId, elementId, targetIndex) != 0;
    }

    private static BehaviorWorld? _world;
    private static SaveStore? _saveStore;
    private static SceneAdapter? _scene;
    private static byte[] _diagnostics = [];
    [UnmanagedCallersOnly]
    public static int Start(byte* root, int rootLength, byte* json, int jsonLength, SceneAccess* access)
    {
        try
        {
            if (_world is not null || root == null || json == null || rootLength <= 0 || rootLength > 32768 ||
                jsonLength <= 0 || jsonLength > 32 * 1024 * 1024 || access == null || access->Version != 42 ||
                access->Size != sizeof(SceneAccess) || !access->Complete) return 1;
            var directory = new UTF8Encoding(false, true).GetString(new ReadOnlySpan<byte>(root, rootLength));
            var project = NativeCompiler.LoadApplied(directory);
            var attachments = JsonSerializer.Deserialize<BehaviorAttachment[]>(new ReadOnlySpan<byte>(json, jsonLength))
                ?? throw new InvalidDataException("Behavior attachment data is empty.");
            _saveStore = new SaveStore(directory);
            _scene = new(*access); Graphics.Bind(_scene); _world = new(); _world.Start(project, _scene, attachments, bindComponentState: true, saveStore: _saveStore);
            RefreshDiagnostics(); return 0;
        }
        catch (Exception error) { StopWorld(); _diagnostics = Encoding.UTF8.GetBytes(error.ToString()); return 1; }
    }
    [UnmanagedCallersOnly]
    public static int Update(float deltaTime)
    {
        try { _world?.Update(deltaTime); RefreshDiagnostics(); return 0; }
        catch (Exception error) { _diagnostics = Encoding.UTF8.GetBytes(error.ToString()); return 1; }
    }
    [UnmanagedCallersOnly]
    public static int LateUpdate(float deltaTime)
    {
        try { _world?.LateUpdate(deltaTime); RefreshDiagnostics(); return 0; }
        catch (Exception error) { _diagnostics = Encoding.UTF8.GetBytes(error.ToString()); return 1; }
    }
    /// <summary>Evento do aplicativo (scene::ScriptLifecycleEvent): 0 pausa, 1 foco; `value` 0 ou 1.</summary>
    [UnmanagedCallersOnly]
    public static int Lifecycle(uint kind, uint value)
    {
        try
        {
            if (kind > 1 || value > 1) return 1;
            _world?.Application((BehaviorWorld.ApplicationEvent)kind, value != 0); RefreshDiagnostics(); return 0;
        }
        catch (Exception error) { _diagnostics = Encoding.UTF8.GetBytes(error.ToString()); return 1; }
    }
    private static byte[] _fieldSnapshot = [];
    private static ulong _fieldSnapshotObject;
    [UnmanagedCallersOnly]
    public static int InspectFields(ulong objectId, byte* destination, int capacity)
    {
        try
        {
            if (_world is null) return -1;
            if (destination == null)
            {
                _fieldSnapshot = Encoding.UTF8.GetBytes(_world.InspectFields(objectId));
                _fieldSnapshotObject = objectId;
                if (_fieldSnapshot.Length > 1024 * 1024) { _fieldSnapshot = []; return -1; }
            }
            else
            {
                if (_fieldSnapshotObject != objectId || capacity < _fieldSnapshot.Length) return -1;
                _fieldSnapshot.CopyTo(new Span<byte>(destination, capacity));
            }
            return _fieldSnapshot.Length;
        }
        catch (Exception error) { _diagnostics = Encoding.UTF8.GetBytes(error.ToString()); _fieldSnapshot = []; return -1; }
    }
    /// <summary>Inspector em Play: JSON {"Enabled":bool,"Properties":{id:valor}} de uma instância viva.</summary>
    [UnmanagedCallersOnly]
    public static int Edit(ulong objectId, ulong instanceId, byte* json, int jsonLength)
    {
        try
        {
            if (_world is null || json == null || jsonLength <= 0 || jsonLength > 1024 * 1024) return 1;
            var edit = JsonSerializer.Deserialize<BehaviorEdit>(new ReadOnlySpan<byte>(json, jsonLength))
                ?? throw new InvalidDataException("Behavior edit is empty.");
            if (!_world.Edit(objectId, instanceId, edit))
            {
                _diagnostics = Encoding.UTF8.GetBytes($"{objectId}/{instanceId} Edit: instância inexistente nesta sessão de Play");
                return 1;
            }
            RefreshDiagnostics(); return 0;
        }
        catch (Exception error) { _diagnostics = Encoding.UTF8.GetBytes(error.ToString()); return 1; }
    }
    [UnmanagedCallersOnly]
    public static int FixedUpdate(float deltaTime)
    {
        try { _world?.FixedUpdate(deltaTime); RefreshDiagnostics(); return 0; }
        catch (Exception error) { _diagnostics = Encoding.UTF8.GetBytes(error.ToString()); return 1; }
    }
    [UnmanagedCallersOnly]
    public static int Timer(ulong objectId, ulong instanceId, uint count)
    {
        try { if (instanceId == 0 || count == 0) return 1; _world?.Timer(objectId, instanceId, count); RefreshDiagnostics(); return 0; }
        catch (Exception error) { _diagnostics = Encoding.UTF8.GetBytes(error.ToString()); return 1; }
    }
    [UnmanagedCallersOnly]
    public static int Trigger(ulong sensor, ulong other, uint phase)
    {
        try { if (phase > 2) return 1; _world?.Trigger(sensor, other, phase); RefreshDiagnostics(); return 0; }
        catch (Exception error) { _diagnostics = Encoding.UTF8.GetBytes(error.ToString()); return 1; }
    }
    /// <summary>
    /// Contato sólido vindo do passo físico. `normal` é nulo quando o backend não
    /// tem geometria a informar — o fim de um contato não traz.
    /// </summary>
    [UnmanagedCallersOnly]
    public static int Contact(ulong self, ulong other, uint phase, float* normal)
    {
        try
        {
            if (phase > 2) return 1;
            Vector3? direction = normal == null ? null : new Vector3(normal[0], normal[1], normal[2]);
            _world?.Contact(self, other, phase, direction);
            RefreshDiagnostics();
            return 0;
        }
        catch (Exception error) { _diagnostics = Encoding.UTF8.GetBytes(error.ToString()); return 1; }
    }
    [UnmanagedCallersOnly]
    public static void Stop() { try { StopWorld(); } catch (Exception error) { _diagnostics = Encoding.UTF8.GetBytes(error.ToString()); } }
    private static void StopWorld()
    {
        try { _world?.Dispose(); }
        finally { _world = null; _fieldSnapshot = []; _fieldSnapshotObject = 0; Graphics.Unbind(); _scene?.Invalidate(); _scene = null;
            var save = _saveStore; _saveStore = null; save?.Dispose(); }
    }
    private static void RefreshDiagnostics()
    {
        _diagnostics = Encoding.UTF8.GetBytes(string.Join("\n", _world?.Failures.Select(f =>
            $"{f.ObjectId}/{f.InstanceId} {f.Phase}: {f.Message}") ?? []));
    }
    [UnmanagedCallersOnly]
    public static int CopyDiagnostics(byte* destination, int capacity)
    {
        if (destination == null || capacity < _diagnostics.Length) return _diagnostics.Length;
        _diagnostics.CopyTo(new Span<byte>(destination, capacity)); return _diagnostics.Length;
    }
}
