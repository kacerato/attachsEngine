using System.Numerics;

namespace Astra;

[AttributeUsage(AttributeTargets.Class, Inherited = false)]
public sealed class ComponentIdAttribute(string id) : Attribute
{
    public string Id { get; } = id;
}

[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true)]
public sealed class PropertyIdAttribute(string id) : Attribute
{
    public string Id { get; } = id;
}

/// <summary>
/// Cor em RGB linear com alfa (Unity: <c>Color</c>). Canais acima de 1 só em
/// campos HDR; o Inspector edita pela janela de cor.
/// </summary>
public struct Color : IEquatable<Color>
{
    public float R, G, B, A;
    public Color(float r, float g, float b, float a = 1) { R = r; G = g; B = b; A = a; }
    public static Color White => new(1, 1, 1, 1);
    public static Color Black => new(0, 0, 0, 1);
    public static Color Clear => new(0, 0, 0, 0);
    public static Color Lerp(Color a, Color b, float t)
    {
        t = Math.Clamp(t, 0, 1);
        return new(a.R + (b.R - a.R) * t, a.G + (b.G - a.G) * t, a.B + (b.B - a.B) * t, a.A + (b.A - a.A) * t);
    }
    public static Color operator *(Color c, float s) => new(c.R * s, c.G * s, c.B * s, c.A);
    public bool Equals(Color other) => R == other.R && G == other.G && B == other.B && A == other.A;
    public override bool Equals(object? obj) => obj is Color other && Equals(other);
    public override int GetHashCode() => HashCode.Combine(R, G, B, A);
    public static bool operator ==(Color a, Color b) => a.Equals(b);
    public static bool operator !=(Color a, Color b) => !a.Equals(b);
    public override string ToString() => $"({R}, {G}, {B}, {A})";
}

/// <summary>Parada de cor de um <see cref="Gradient"/> (tempo em [0,1]).</summary>
public struct GradientColorKey
{
    public Color Color; public float Time;
    public GradientColorKey(Color color, float time) { Color = color; Time = time; }
}
/// <summary>Parada de alfa de um <see cref="Gradient"/>.</summary>
public struct GradientAlphaKey
{
    public float Alpha; public float Time;
    public GradientAlphaKey(float alpha, float time) { Alpha = alpha; Time = time; }
}
/// <summary>Interpolação entre paradas (Unity: GradientMode).</summary>
public enum GradientMode { Blend = 0, Fixed = 1, PerceptualBlend = 2 }

/// <summary>
/// Gradiente de cor e alfa (Unity: <c>Gradient</c>), editado no Inspector pelo
/// editor de gradiente. <see cref="Evaluate"/> segue a mesma regra do editor.
/// </summary>
public sealed class Gradient
{
    public GradientMode Mode { get; set; } = GradientMode.Blend;
    public GradientColorKey[] ColorKeys { get; set; } = [new(Color.White, 0), new(Color.White, 1)];
    public GradientAlphaKey[] AlphaKeys { get; set; } = [new(1, 0), new(1, 1)];

    public Color Evaluate(float time)
    {
        var t = Math.Clamp(time, 0, 1);
        var colors = ColorKeys.OrderBy(k => k.Time).ToArray();
        var alphas = AlphaKeys.OrderBy(k => k.Time).ToArray();
        Color color = colors.Length == 0 ? Color.White : Interpolate(colors, t);
        color.A = alphas.Length == 0 ? 1 : InterpolateAlpha(alphas, t);
        return color;
    }
    private Color Interpolate(GradientColorKey[] keys, float t)
    {
        if (keys.Length == 1 || t <= keys[0].Time) return keys[0].Color;
        if (t >= keys[^1].Time) return keys[^1].Color;
        var i = 1; while (i < keys.Length && keys[i].Time < t) ++i;
        var a = keys[i - 1]; var b = keys[i];
        if (Mode == GradientMode.Fixed) return b.Color;
        var span = b.Time - a.Time; var f = span > 0 ? (t - a.Time) / span : 1;
        if (Mode != GradientMode.PerceptualBlend) return Color.Lerp(a.Color, b.Color, f);
        var la = ToOklab(a.Color); var lb = ToOklab(b.Color);
        return FromOklab(la + (lb - la) * f);
    }
    private float InterpolateAlpha(GradientAlphaKey[] keys, float t)
    {
        if (keys.Length == 1 || t <= keys[0].Time) return keys[0].Alpha;
        if (t >= keys[^1].Time) return keys[^1].Alpha;
        var i = 1; while (i < keys.Length && keys[i].Time < t) ++i;
        var a = keys[i - 1]; var b = keys[i];
        if (Mode == GradientMode.Fixed) return b.Alpha;
        var span = b.Time - a.Time; var f = span > 0 ? (t - a.Time) / span : 1;
        return a.Alpha + (b.Alpha - a.Alpha) * f;
    }
    private static Vector3 ToOklab(Color c)
    {
        var l = MathF.Cbrt(0.4122214708f * c.R + 0.5363325363f * c.G + 0.0514459929f * c.B);
        var m = MathF.Cbrt(0.2119034982f * c.R + 0.6806995451f * c.G + 0.1073969566f * c.B);
        var s = MathF.Cbrt(0.0883024619f * c.R + 0.2817188376f * c.G + 0.6299787005f * c.B);
        return new(0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s,
                   1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s,
                   0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s);
    }
    private static Color FromOklab(Vector3 lab)
    {
        var l = MathF.Pow(lab.X + 0.3963377774f * lab.Y + 0.2158037573f * lab.Z, 3);
        var m = MathF.Pow(lab.X - 0.1055613458f * lab.Y - 0.0638541728f * lab.Z, 3);
        var s = MathF.Pow(lab.X - 0.0894841775f * lab.Y - 1.2914855480f * lab.Z, 3);
        return new(MathF.Max(0, 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s),
                   MathF.Max(0, -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s),
                   MathF.Max(0, -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s), 1);
    }
}

/// <summary>Gradiente com cores HDR no Inspector (Unity: <c>[GradientUsage(hdr)]</c>).</summary>
[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true)]
public sealed class GradientUsageAttribute(bool hdr) : Attribute
{
    public bool Hdr { get; } = hdr;
}

/// <summary>
/// Como o Inspector mostra um campo <see cref="Color"/> (Unity: <c>[ColorUsage]</c>):
/// sem alfa e/ou com intensidade HDR.
/// </summary>
[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true)]
public sealed class ColorUsageAttribute(bool showAlpha, bool hdr = false) : Attribute
{
    public bool ShowAlpha { get; } = showAlpha;
    public bool Hdr { get; } = hdr;
}

/// <summary>
/// Campo guardado na cena que o Inspector não mostra (Unity: <c>[HideInInspector]</c>).
/// O valor autoral continua preservado e entregue no Play.
/// </summary>
[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true)]
public sealed class HideInInspectorAttribute : Attribute { }

/// <summary>
/// Expõe um campo não público, junto com <see cref="PropertyIdAttribute"/>
/// (Unity: <c>[SerializeField]</c>). Sem ele só campos públicos entram no Inspector.
/// </summary>
[AttributeUsage(AttributeTargets.Field, Inherited = true)]
public sealed class SerializeFieldAttribute : Attribute { }

/// <summary>
/// O ID autoral é persistido na cena. Referências obtidas em Play também guardam
/// a sessão e a geração, para não apontarem para outro objeto após Stop/Play.
/// </summary>
public readonly record struct ObjectReference(ulong ObjectId)
{
    internal ISceneAccess? Scene { get; init; }
    internal uint World { get; init; }
    internal uint Generation { get; init; }

    internal static ObjectReference Capture(ISceneAccess scene, ulong id) => id == 0
        ? default
        : new(id) { Scene = scene, World = scene.WorldId, Generation = scene.GenerationOf(id) };

    internal WorldStatus StatusIn(ISceneAccess scene)
    {
        if (ObjectId == 0) return WorldStatus.Ok;
        if (Scene is null) return WorldStatus.Ok; // ID autoral ainda não vinculado a uma sessão.
        if (!ReferenceEquals(Scene, scene) || World != scene.WorldId) return WorldStatus.ForeignWorld;
        return Generation != 0 && Generation == scene.GenerationOf(ObjectId)
            ? WorldStatus.Ok : WorldStatus.StaleHandle;
    }
}
public readonly record struct AssetReference(string AssetId);
public readonly record struct TransformValue(Vector3 Position, Quaternion Rotation, Vector3 Scale);

/// <summary>
/// A superfície de baixo nível do mundo de execução: uma tradução direta da ABI
/// nativa v12 (native/scene/script_runtime.h). O documento autoral nunca cruza
/// esta API.
///
/// Os comportamentos do projeto usam <see cref="GameObject"/> e
/// <see cref="Component"/>, que embrulham isto com identidade verificada; estes
/// membros existem para quem precisa do id cru e para os testes.
///
/// Membros com implementação padrão que lança existiam antes da v3 ou podem não
/// estar disponíveis num hospedeiro mais velho: um duplo de teste só precisa
/// implementar o que usa.
/// </summary>
public interface ISceneAccess
{
    bool Exists(ulong objectId);
    TransformValue GetTransform(ulong objectId);
    bool SetTransform(ulong objectId, TransformValue value);
    bool SetBodyVelocity(ulong objectId, Vector3 velocity);
    bool MoveKinematic(ulong objectId, Vector3 position, Quaternion rotation);
    bool AddForce(ulong objectId, Vector3 force) => throw new NotSupportedException();
    bool AddImpulse(ulong objectId, Vector3 impulse) => throw new NotSupportedException();
    bool AddTorque(ulong objectId, Vector3 torque) => throw new NotSupportedException();
    bool AddAngularImpulse(ulong objectId, Vector3 impulse) => throw new NotSupportedException();
    Vector3 GetBodyVelocity(ulong objectId) => throw new NotSupportedException();
    void Log(ulong objectId, string message);

    // --- v3: identidade -----------------------------------------------------
    /// <summary>Identidade desta sessão de Play; muda a cada Play.</summary>
    uint WorldId => throw new NotSupportedException();
    /// <summary>Zero quando o objeto já foi destruído.</summary>
    uint GenerationOf(ulong objectId) => throw new NotSupportedException();
    /// <summary>Motivo da última recusa nesta thread.</summary>
    WorldStatus LastStatus => WorldStatus.Rejected;

    // --- v3: hierarquia -----------------------------------------------------
    ulong ParentOf(ulong objectId) => throw new NotSupportedException();
    int ChildCount(ulong objectId) => throw new NotSupportedException();
    ulong ChildAt(ulong objectId, uint index) => throw new NotSupportedException();
    ulong FindChild(ulong objectId, string name, bool recursive) => throw new NotSupportedException();
    string GetName(ulong objectId) => throw new NotSupportedException();
    bool SetName(ulong objectId, string name) => throw new NotSupportedException();
    /// <summary>1 ativo, 0 inativo, -1 objeto indisponível.</summary>
    int GetActive(ulong objectId) => throw new NotSupportedException();
    bool SetActive(ulong objectId, bool active) => throw new NotSupportedException();

    // --- v3: ciclo de vida --------------------------------------------------
    ulong CreateObject(ulong parent, string name) => throw new NotSupportedException();
    bool DestroyObject(ulong objectId) => throw new NotSupportedException();
    bool SetParent(ulong objectId, ulong parent, uint childIndex) => throw new NotSupportedException();
    bool SetParentWithPolicy(ulong objectId, ulong parent, uint childIndex, ReparentPosePolicy policy)
        => throw new NotSupportedException();
    ulong QueueStructuralOperation(uint kind, ulong objectId, ulong other, uint childIndex,
                                    ReparentPosePolicy policy) => throw new NotSupportedException();
    WorldStatus QueryOperation(uint world, ulong operationId, out WorldOperationState state,
                               out WorldStatus result) => throw new NotSupportedException();

    // --- v3: componentes ----------------------------------------------------
    int ComponentCount(ulong objectId) => throw new NotSupportedException();
    (ulong Instance, string TypeId) ComponentAt(ulong objectId, uint index) => throw new NotSupportedException();
    ulong FindComponent(ulong objectId, string typeId, uint ordinal) => throw new NotSupportedException();
    ulong AddComponent(ulong objectId, string typeId) => throw new NotSupportedException();
    bool RemoveComponent(ulong objectId, ulong instanceId) => throw new NotSupportedException();
    bool TryGetProperty(ulong objectId, ulong instanceId, string propertyId, out uint kind, out ulong bits)
        => throw new NotSupportedException();
    bool SetProperty(ulong objectId, ulong instanceId, string propertyId, uint kind, ulong bits)
        => throw new NotSupportedException();

    // --- v3: transform de mundo --------------------------------------------
    TransformValue GetWorldTransform(ulong objectId) => throw new NotSupportedException();
    bool SetWorldTransform(ulong objectId, TransformValue value) => throw new NotSupportedException();

    // --- v4: consultas físicas ----------------------------------------------
    /// <summary>Contagem REAL de acertos, que pode exceder o buffer recebido.</summary>
    int RayCast(Vector3 origin, Vector3 direction, in QueryFilter filter, Span<RawQueryHit> results)
        => throw new NotSupportedException();
    int ShapeCast(in ShapeQuery shape, Vector3 origin, Vector3 direction, in QueryFilter filter, out RawQueryHit hit)
        => throw new NotSupportedException();
    int Overlap(in ShapeQuery shape, Vector3 origin, in QueryFilter filter, Span<RawQueryHit> results)
        => throw new NotSupportedException();
    int LayerByName(string name) => throw new NotSupportedException();
    string LayerName(uint layer) => throw new NotSupportedException();

    // --- v5: entrada por ações -----------------------------------------------
    bool InputAxis(string action, out Vector2 value) => throw new NotSupportedException();
    /// <summary>0 pressionado agora, 1 acabou de descer, 2 acabou de subir; -1 ação desconhecida.</summary>
    int InputButton(string action, uint query) => throw new NotSupportedException();
    bool InputContext(string context, int enabled) => throw new NotSupportedException();
    /// <summary>Nome da ação que cumpre o papel: 0 mover, 1 olhar, 2 saltar.</summary>
    string InputRole(uint role) => throw new NotSupportedException();

    // --- v6: gráficos globais e recursos tipados --------------------------
    GraphicsSnapshot GetGraphicsState(uint expectedWorld) => throw new NotSupportedException();
    bool SetGraphicsSettings(uint expectedWorld, GraphicsSettings settings, out ulong requestId)
        => throw new NotSupportedException();
    bool TryGetResource(ulong objectId, ulong instanceId, string propertyId, uint slot, out AssetGuid value)
        => throw new NotSupportedException();
    bool SetResource(ulong objectId, ulong instanceId, string propertyId, uint slot, AssetGuid value)
        => throw new NotSupportedException();
    // --- v7: propriedades tipadas endereçadas por slot --------------------
    bool TryGetSlotProperty(ulong objectId, ulong instanceId, string propertyId, uint slot,
        out uint kind, out ulong bits) => throw new NotSupportedException();
    bool SetSlotProperty(ulong objectId, ulong instanceId, string propertyId, uint slot, uint kind, ulong bits)
        => throw new NotSupportedException();

    // --- v8: comandos dos consumidores de personagem/câmera -------------
    bool CharacterMove(ulong objectId, Vector2 input, float yawRadians) => throw new NotSupportedException();
    bool CharacterJump(ulong objectId) => throw new NotSupportedException();
    bool CameraLook(ulong objectId, Vector2 normalizedDelta) => throw new NotSupportedException();

    // --- v9: animação ---------------------------------------------------------
    bool AnimationCommand(ulong objectId, ulong instanceId, AnimationCommandKind op, AssetGuid clip,
                          float seconds, float targetWeight, AnimationPlayMode mode) => throw new NotSupportedException();
    bool TryGetAnimationState(ulong objectId, ulong instanceId, AssetGuid clip, out AnimationStateValue value)
        => throw new NotSupportedException();
    bool SetAnimationState(ulong objectId, ulong instanceId, in AnimationStateValue value) => throw new NotSupportedException();
    /// <summary>Quantos clipes o componente lista (-1 sem componente); com índice válido, o clipe e o nome.</summary>
    int AnimationClipAt(ulong objectId, ulong instanceId, uint index, out AssetGuid clip, out string name)
        => throw new NotSupportedException();
    // v10: elemento persistente de uma coleção de recursos reordenável.
    bool ResourceElementId(ulong objectId, ulong instanceId, string propertyId, uint slot, out ulong elementId)
        => throw new NotSupportedException();
    bool TryGetResourceByElementId(ulong objectId, ulong instanceId, string propertyId, ulong elementId, out AssetGuid value)
        => throw new NotSupportedException();
    bool SetResourceByElementId(ulong objectId, ulong instanceId, string propertyId, ulong elementId, AssetGuid value)
        => throw new NotSupportedException();
    // v11: estrutura da lista de clipes do componente Animation no mundo Play.
    bool AppendAnimationClip(ulong objectId, ulong instanceId, AssetGuid clip, out ulong elementId)
        => throw new NotSupportedException();
    bool RemoveAnimationClip(ulong objectId, ulong instanceId, ulong elementId)
        => throw new NotSupportedException();
    bool MoveAnimationClip(ulong objectId, ulong instanceId, ulong elementId, uint targetIndex)
        => throw new NotSupportedException();
}

/// <summary>
/// Como um comportamento encontra OUTRO comportamento por capacidade.
///
/// É o que torna "interação" um contrato do projeto em vez de um tipo especial
/// do núcleo: a porta implementa uma interface, e quem interage procura por essa
/// interface, sem saber que existe uma porta.
/// </summary>
public interface IBehaviorRegistry
{
    /// <summary>Primeiro comportamento desse objeto atribuível ao tipo pedido.</summary>
    object? FindBehavior(ulong objectId, Type contract);
    /// <summary>Todos os comportamentos desse objeto atribuíveis ao tipo pedido.</summary>
    IEnumerable<object> FindBehaviors(ulong objectId, Type contract);
}

public abstract class Behavior
{
    private ISceneAccess? _scene;
    private IBehaviorRegistry? _registry;
    public ulong ObjectId { get; private set; }
    public ulong InstanceId { get; private set; }
    public bool Enabled { get; set; } = true;
    protected ISceneAccess Scene => _scene ?? throw new InvalidOperationException("Behavior is not attached to an execution world.");

    private GameObject? _object;

    /// <summary>
    /// O objeto a que este comportamento está anexado. Resolvido uma vez: um
    /// `Object` novo a cada quadro seria lixo alocado por comportamento e por
    /// quadro, e o handle não muda enquanto o comportamento existe.
    /// </summary>
    protected GameObject Object => _object ??= GameObject.Resolve(Scene, ObjectId);

    /// <summary>As consultas físicas do mundo de execução.</summary>
    protected PhysicsAccess Physics => new(Scene);

    /// <summary>As ações de entrada configuradas no projeto.</summary>
    protected InputAccess Input => new(Scene);

    /// <summary>Resolve uma referência autorada no inspetor para um objeto vivo.</summary>
    protected GameObject? Resolve(ObjectReference reference)
    {
        if (reference.StatusIn(Scene) != WorldStatus.Ok) return null;
        if (reference.ObjectId == 0 || !Scene.Exists(reference.ObjectId)) return null;
        return GameObject.Resolve(Scene, reference.ObjectId);
    }

    protected TransformValue Transform
    {
        get => Scene.GetTransform(ObjectId);
        set
        {
            if (!Scene.SetTransform(ObjectId, value))
                throw new InvalidOperationException("A execução recusou a transformação.");
        }
    }
    /// <summary>
    /// O comportamento de <paramref name="target"/> que cumpre o contrato
    /// <typeparamref name="T"/>, ou null. `T` pode ser uma interface: é assim
    /// que um objeto interage com outro sem conhecer o tipo concreto dele.
    /// </summary>
    protected T? FindBehavior<T>(GameObject? target) where T : class =>
        target is { IsAlive: true } ? _registry?.FindBehavior(target.ObjectId, typeof(T)) as T : null;

    protected IEnumerable<T> FindBehaviors<T>(GameObject? target) where T : class =>
        target is { IsAlive: true } && _registry is not null
            ? _registry.FindBehaviors(target.ObjectId, typeof(T)).OfType<T>()
            : [];

    internal void Attach(ISceneAccess scene, ulong objectId, ulong instanceId, IBehaviorRegistry? registry = null)
    {
        if (_scene is not null || objectId == 0 || instanceId == 0)
            throw new InvalidOperationException("Invalid or duplicate behavior attachment.");
        _scene = scene; ObjectId = objectId; InstanceId = instanceId; _registry = registry;
    }
    internal void Detach() { _scene = null; _registry = null; _object = null; ObjectId = 0; InstanceId = 0; }
    // Ordem de uma sessão de Play (comparável a Unity 6000.0, Manual/execution-order):
    //   Awake  → uma vez por instância, depois que TODAS foram criadas e receberam
    //            as propriedades autoradas; roda mesmo desativada.
    //   Enable → quando passa a ativa (logo após Awake, ou ao religar Enabled).
    //   Start  → antes do primeiro quadro em que está ativa.
    //   quadro → Update, depois animação e física (FixedUpdate e contatos),
    //            depois LateUpdate, depois o acompanhamento de câmera.
    //   Disable → ao desligar Enabled e no fim do Play; Stop por último.
    // Exceção em qualquer callback desativa só aquela instância, sem Disable.
    public virtual void Awake() { }
    public virtual void Enable() { }
    public virtual void Disable() { }
    public virtual void Start() { }
    public virtual void Update(float deltaTime) { }
    /// <summary>Depois de animação e física no mesmo quadro: pose final dos
    /// objetos para câmeras e ajustes que dependem dela.</summary>
    public virtual void LateUpdate(float deltaTime) { }
    public virtual void FixedUpdate(float deltaTime) { }
    /// <summary>O aplicativo foi para segundo plano (true) ou voltou (false).</summary>
    public virtual void ApplicationPause(bool paused) { }
    /// <summary>A janela do aplicativo ganhou (true) ou perdeu (false) o foco.</summary>
    public virtual void ApplicationFocus(bool focused) { }
    /// <summary>Called when a Timer on this object expires. Multiple expirations
    /// in one frame are delivered together. The ID distinguishes Timer instances.</summary>
    public virtual void TimerElapsed(ulong timerInstanceId, uint count) { }
    // Dispatched after each fixed step to behaviors on the sensor's owning body.
    // One event per body pair, regardless of how many compound parts overlap.
    public virtual void TriggerEnter(ObjectReference other) { }
    public virtual void TriggerStay(ObjectReference other) { }
    public virtual void TriggerExit(ObjectReference other) { }
    // Contato SÓLIDO, o par em que nenhum dos dois é sensor. Entregue por passo
    // físico, agregado por par de corpos. A normal acompanha Enter/Stay; o fim
    // de um contato não traz geometria, e por isso `Collision.Normal` é nulo lá.
    public virtual void CollisionEnter(Collision collision) { }
    public virtual void CollisionStay(Collision collision) { }
    public virtual void CollisionExit(Collision collision) { }
    public virtual void Stop() { }
}
