namespace Astra;
public enum AnimationLayerBlendMode { Override=0, Additive=1 }

/// <summary>Estado atual de uma camada do Animator (AnimatorStateInfo da Unity).</summary>
/// <param name="Name">Estado tocando (o de origem durante uma transição).</param>
/// <param name="NormalizedTime">Tempo do estado em voltas: 1,5 = uma volta e meia.</param>
/// <param name="IsInTransition">Verdadeiro enquanto mistura com <paramref name="NextName"/>.</param>
/// <param name="NextName">Estado de destino da transição; vazio fora dela.</param>
/// <param name="TransitionProgress">0 a 1 da transição.</param>
public readonly record struct AnimatorStateInfo(string Name, float NormalizedTime, bool IsInTransition, string NextName, float TransitionProgress);

public sealed partial class GameObject
{
    /// <summary>Controle do componente Animator (`astra.animation.animator`) deste objeto no Play.</summary>
    public AnimatorController Animator()
    {
        Require("acessar Animator");
        var component = GetComponent("astra.animation.animator") ?? throw new WorldException(WorldStatus.ComponentMissing, "acessar Animator");
        return new(component);
    }
}

/// <summary>
/// Parâmetros e estados de um Animator, por nome, como a API da Unity: SetFloat, SetInteger, SetBool,
/// SetTrigger e ResetTrigger mudam os valores que as transições leem; Play e CrossFade trocam o estado
/// de uma camada; GetCurrentState informa o estado tocando. Valores de execução: o Play sempre parte dos
/// padrões autorados. Nome ou tipo errado lança <see cref="WorldException"/>.
/// Parâmetros vinculados a movimento físico podem ser lidos; setters são recusados com
/// <see cref="WorldStatus.Rejected"/> para preservar uma única origem do valor.
/// </summary>
public readonly struct AnimatorController
{
    private readonly Component component;
    internal AnimatorController(Component value) => component = value;
    public Component Component => component;
    /// <summary>Runtime composition settings are independent per instance and do not edit controller assets.</summary>
    public float GetLayerWeight(uint layer) => (float)component.Invoke("get_layer_weight",ComponentValue.Integer(layer)).AsNumber();
    public void SetLayerWeight(uint layer,float weight) => component.Invoke("set_layer_weight",ComponentValue.Integer(layer),ComponentValue.Number(weight));
    public AnimationLayerBlendMode GetLayerBlendMode(uint layer) => (AnimationLayerBlendMode)component.Invoke("get_layer_blend",ComponentValue.Integer(layer)).AsInteger();
    public void SetLayerBlendMode(uint layer,AnimationLayerBlendMode mode) => component.Invoke("set_layer_blend",ComponentValue.Integer(layer),ComponentValue.Integer((long)mode));
    public float GetLayerReferenceTime(uint layer) => (float)component.Invoke("get_layer_reference_time",ComponentValue.Integer(layer)).AsNumber();
    public void SetLayerReferenceTime(uint layer,float seconds) => component.Invoke("set_layer_reference_time",ComponentValue.Integer(layer),ComponentValue.Number(seconds));
    public void SetLayerReferenceClip(uint layer,AssetGuid clip) => component.Invoke("set_layer_reference",ComponentValue.Integer(layer),ComponentValue.Integer(unchecked((long)clip.High)),ComponentValue.Integer(unchecked((long)clip.Low)));
    public AssetGuid GetLayerReferenceClip(uint layer) => new(unchecked((ulong)component.Invoke("get_layer_reference_high",ComponentValue.Integer(layer)).AsInteger()),unchecked((ulong)component.Invoke("get_layer_reference_low",ComponentValue.Integer(layer)).AsInteger()));
    public void ResetLayerOverrides(uint layer) => component.Invoke("reset_layer_overrides",ComponentValue.Integer(layer));

    private float Parameter(uint operation, string name, float value, string action)
    {
        ArgumentException.ThrowIfNullOrEmpty(name);
        var o = component.Object ?? throw new WorldException(WorldStatus.InvalidArgument, action);
        if (!component.Scene.AnimatorParameter(o.ObjectId, o.World, o.Generation, component.InstanceId, operation, name, value, out var result))
            throw new WorldException(component.Scene.LastStatus, action + " " + name);
        return result;
    }
    public float GetFloat(string name) => Parameter(0, name, 0, "ler parâmetro");
    public int GetInteger(string name) => (int)MathF.Round(Parameter(0, name, 0, "ler parâmetro"));
    public bool GetBool(string name) => Parameter(0, name, 0, "ler parâmetro") != 0;
    public void SetFloat(string name, float value) => Parameter(1, name, value, "definir Float");
    public void SetInteger(string name, int value) => Parameter(2, name, value, "definir Int");
    public void SetBool(string name, bool value) => Parameter(3, name, value ? 1 : 0, "definir Bool");
    /// <summary>Liga o gatilho; ele fica ligado até uma transição usá-lo ou <see cref="ResetTrigger"/>.</summary>
    public void SetTrigger(string name) => Parameter(4, name, 1, "disparar gatilho");
    public void ResetTrigger(string name) => Parameter(5, name, 0, "limpar gatilho");

    /// <summary>Troca a camada para o estado na hora, do início.</summary>
    public void Play(string state, uint layer = 0) => CrossFade(state, 0, layer);
    /// <summary>Transição para o estado em <paramref name="seconds"/> segundos.</summary>
    public void CrossFade(string state, float seconds, uint layer = 0)
    {
        ArgumentException.ThrowIfNullOrEmpty(state);
        var o = component.Object ?? throw new WorldException(WorldStatus.InvalidArgument, "trocar estado");
        if (!component.Scene.AnimatorPlay(o.ObjectId, o.World, o.Generation, component.InstanceId, layer, state, seconds))
            throw new WorldException(component.Scene.LastStatus, "trocar para o estado " + state);
    }
    public AnimatorStateInfo GetCurrentState(uint layer = 0)
    {
        var o = component.Object ?? throw new WorldException(WorldStatus.InvalidArgument, "ler estado");
        if (!component.Scene.AnimatorState(o.ObjectId, o.World, o.Generation, component.InstanceId, layer, out var state))
            throw new WorldException(component.Scene.LastStatus, "ler estado do Animator");
        return state;
    }
}
