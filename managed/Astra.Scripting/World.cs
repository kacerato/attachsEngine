using System.Numerics;

namespace Astra;

/// <summary>Built-in geometry and its collision shape. Cylinder uses a convex 32-sided mesh.</summary>
public enum PrimitiveType : uint { Cube, Sphere, Capsule, Cylinder, Plane, Quad }

/// <summary>
/// Por que uma chamada do mundo de execução foi recusada. Espelha
/// <c>ae::runtime::WorldStatus</c> (native/runtime/game_world.h) por posição:
/// acrescentar valor só no fim, e nunca reordenar.
/// </summary>
public enum WorldStatus : uint
{
    Ok,
    NotRunning,
    /// <summary>A referência veio de outra sessão de Play.</summary>
    ForeignWorld,
    /// <summary>O objeto foi destruído; a referência venceu.</summary>
    StaleHandle,
    UnknownObject,
    UnknownComponent,
    ComponentMissing,
    ComponentUnavailable,
    ComponentInUse,
    NotMutableInPlay,
    TransformOwnedByPhysics,
    InvalidArgument,
    LimitReached,
    Rejected,
    UnknownResource,
    ResourceTypeMismatch,
    /// <summary>O clipe existe mas não está na lista do componente Animation.</summary>
    ClipNotInComponent,
    UnknownElement,
    OperationExpired,
}

public sealed class WorldException(WorldStatus status, string operation)
    : InvalidOperationException($"{operation}: {Describe(status)}")
{
    public WorldStatus Status { get; } = status;

    private static string Describe(WorldStatus status) => status switch
    {
        WorldStatus.NotRunning => "não há mundo de execução ativo",
        WorldStatus.ForeignWorld => "a referência pertence a outra execução",
        WorldStatus.StaleHandle => "a referência aponta para um objeto já removido",
        WorldStatus.UnknownObject => "objeto inexistente",
        WorldStatus.UnknownComponent => "tipo de componente desconhecido",
        WorldStatus.ComponentMissing => "o objeto não possui esse componente",
        WorldStatus.ComponentUnavailable => "o componente não pode ser usado neste objeto",
        WorldStatus.ComponentInUse => "outro componente ainda depende deste",
        WorldStatus.NotMutableInPlay => "esta alteração não é permitida durante a execução",
        WorldStatus.TransformOwnedByPhysics => "a pose deste objeto é publicada pela física",
        WorldStatus.InvalidArgument => "argumento inválido",
        WorldStatus.LimitReached => "limite de capacidade atingido",
        WorldStatus.UnknownResource => "recurso inexistente",
        WorldStatus.ResourceTypeMismatch => "tipo de recurso incompatível",
        WorldStatus.UnknownElement => "elemento da coleção inexistente",
        WorldStatus.OperationExpired => "resultado da operação não está mais disponível",
        _ => "operação recusada",
    };
}

/// <summary>Os identificadores dos componentes nativos anexáveis.</summary>
public static class ComponentIds
{
    public const string PhysicsBody = "astra.physics.body";
    public const string Collider = "astra.physics.collider";
    public const string Character = "astra.physics.character";
    public const string Joint = "astra.physics.joint";
    public const string Camera = "astra.camera";
    public const string CameraLook = "astra.camera.look";
    /// <summary>Follow target and damping for a Camera; only position is driven in Play.</summary>
    public const string CameraFollow = "astra.camera.follow";
    public const string MeshRenderer = "astra.render.mesh";
    /// <summary>Luz: `kind`, `enabled`, `color`, `intensity`, `range`, `inner_angle`,
    /// `outer_angle`, `unit` (0 escala legada, 1 lux/candela, 2 lux/lúmen),
    /// `use_color_temperature` e `color_temperature` em kelvin.</summary>
    public const string Light = "astra.render.light";
    /// <summary>Ambiente autoral da cena: céu, atmosfera, neblina e pós.</summary>
    public const string Environment = "astra.render.environment";
    /// <summary>LOD Group: `level_count`, `transition_0..3` (% da altura da tela), `level_0..3` (objeto do nível),
    /// `size`, `fade_mode` (0 nenhum, 1 cross-fade), `animate_cross_fading`, `fade_width_0..3` e `force_level`
    /// (ForceLOD da Unity: 0 automático, n força o LOD n-1; estado de execução, não vai para o arquivo).</summary>
    public const string LodGroup = "astra.render.lod_group";
    /// <summary>Malha deformável: `quality` (0 automática, 1, 2 ou 4 influências),
    /// `skinned_motion_vectors` e `blend_shape_weight` por slot (0..100). Os ossos
    /// são ligados pela importação. Use <see cref="Component.DeformableMesh"/>.</summary>
    public const string SkinnedMesh = "astra.render.skinned_mesh";
    /// <summary>Animação: recursos `clip` (padrão) e `clips` (lista), `clip_count`,
    /// `play_automatically`, `wrap_mode` e `speed`. Tocar e misturar: <see cref="Component.Animation"/>.</summary>
    public const string Animation = "astra.animation";
    /// <summary>Timer do mundo de Play. O evento chega em <see cref="Behavior.TimerElapsed"/>.</summary>
    public const string Timer = "astra.time.timer";
    public const string ScriptBehavior = "astra.script.behavior";
}

/// <summary>
/// Uma referência resolvida a um objeto do mundo de execução.
///
/// Ela carrega mundo e geração além do id. Guardar um <see cref="GameObject"/>
/// entre quadros é seguro: assim que o objeto é destruído — ainda dentro do
/// mesmo callback — <see cref="IsAlive"/> passa a ser falso e qualquer operação
/// lança <see cref="WorldException"/> em vez de acertar outra coisa.
/// </summary>
public enum MessageRoute { Object, Descendants, Ancestors }
public enum MessageOptions { RequireReceiver, DontRequireReceiver }

public enum ReparentPosePolicy : uint { KeepLocal, KeepWorld }

public enum WorldOperationState : uint { Pending, Applied, Failed }

/// <summary>Resultado de uma edição estrutural no ponto seguro do Play.</summary>
public readonly struct WorldOperation
{
    private readonly ISceneAccess _scene;
    private readonly uint _world;
    public ulong Id { get; }
    internal WorldOperation(ISceneAccess scene, uint world, ulong id)
    {
        _scene = scene; _world = world; Id = id;
    }
    public (WorldOperationState State, WorldStatus Result) Read()
    {
        if (_scene is null || Id == 0) throw new WorldException(WorldStatus.InvalidArgument, "consultar operação");
        var status = _scene.QueryOperation(_world, Id, out var state, out var result);
        if (status != WorldStatus.Ok) throw new WorldException(status, "consultar operação");
        return (state, result);
    }
}

public sealed class GameObject : IEquatable<GameObject>
{
    private readonly ISceneAccess _scene;
    internal GameObject(ISceneAccess scene, ulong objectId, uint world, uint generation)
    {
        _scene = scene; ObjectId = objectId; World = world; Generation = generation;
    }

    public ulong ObjectId { get; }
    public uint World { get; }
    public uint Generation { get; }
    public bool IsValid => ObjectId != 0 && Generation != 0;

    private ISceneAccess Scene => _scene;

    /// <summary>Falso quando o objeto foi destruído ou pertence a outra execução.</summary>
    public bool IsAlive => IsValid && _scene.WorldId == World && _scene.GenerationOf(ObjectId) == Generation;

    private void Require(string operation)
    {
        if (!IsValid) throw new WorldException(WorldStatus.InvalidArgument, operation);
        if (_scene.WorldId != World) throw new WorldException(WorldStatus.ForeignWorld, operation);
        if (_scene.GenerationOf(ObjectId) != Generation) throw new WorldException(WorldStatus.StaleHandle, operation);
    }
    private void Check(bool ok, string operation)
    {
        if (!ok) throw new WorldException(Scene.LastStatus, operation);
    }

    public string Name
    {
        get { Require("ler nome"); return Scene.GetName(ObjectId); }
        set { Require("renomear"); Check(Scene.SetName(ObjectId, value), "renomear"); }
    }

    /// <summary>Estado local persistido; pode ser verdadeiro com um ancestral inativo.</summary>
    public bool ActiveSelf
    {
        get { Require("ler estado ativo local"); var value = Scene.GetActiveSelf(ObjectId);
            Check(value >= 0, "ler estado ativo local"); return value == 1; }
    }

    /// <summary>Ativo considerando também os ancestrais.</summary>
    public bool ActiveInHierarchy
    {
        get { Require("ler estado ativo"); var value = Scene.GetActive(ObjectId);
            Check(value >= 0, "ler estado ativo"); return value == 1; }
    }
    public void SetActive(bool active)
    {
        Require("alterar estado ativo");
        Check(Scene.SetActive(ObjectId, active), "alterar estado ativo");
    }

    /// <summary>Tag do catálogo do projeto; nomes desconhecidos são recusados.</summary>
    public string Tag
    {
        get { Require("ler tag"); return Scene.GetTag(ObjectId); }
        set { Require("alterar tag"); Check(Scene.SetTag(ObjectId, value), "alterar tag"); }
    }
    public bool CompareTag(string tag)
    {
        Require("comparar tag"); var value = Scene.CompareTag(ObjectId, tag);
        Check(value >= 0, "comparar tag"); return value == 1;
    }
    /// <summary>Busca global neste mundo, somente objetos ativos. Não inclui a raiz sintética.</summary>
    public GameObject? FindWithTag(string tag)
    {
        Require("buscar tag"); var ids = Scene.FindTagged(tag, true);
        return ids.Length == 0 ? null : Resolve(Scene, ids[0]);
    }
    /// <summary>Snapshot dos objetos ativos deste mundo com a tag; custo linear na cena.</summary>
    public GameObject[] FindGameObjectsWithTag(string tag)
    {
        Require("buscar tag"); var ids = Scene.FindTagged(tag, false);
        var result = new GameObject[ids.Length];
        for (var i = 0; i < ids.Length; ++i) result[i] = Resolve(Scene, ids[i]);
        return result;
    }

    public GameObject? Parent
    {
        get
        {
            Require("ler pai");
            var parent = Scene.ParentOf(ObjectId);
            return parent == 0 ? null : Resolve(Scene, parent);
        }
    }

    public int ChildCount { get { Require("contar filhos"); return Math.Max(0, Scene.ChildCount(ObjectId)); } }

    public GameObject ChildAt(int index)
    {
        Require("acessar filho");
        if (index < 0) throw new ArgumentOutOfRangeException(nameof(index));
        var child = Scene.ChildAt(ObjectId, (uint)index);
        if (child == 0) throw new WorldException(WorldStatus.UnknownObject, "acessar filho");
        return Resolve(Scene, child);
    }

    public IEnumerable<GameObject> Children()
    {
        var count = ChildCount;
        for (var index = 0; index < count; ++index) yield return ChildAt(index);
    }

    internal void PushAliveChildren(Stack<GameObject> pending)
    {
        Require("percorrer filhos");
        // Destroy invalida o handle antes de retirar o slot da hierarquia no
        // ponto seguro. Consultas devem ignorar esse slot intermediário.
        for (var i = ChildCount - 1; i >= 0; --i)
        {
            var id = Scene.ChildAt(ObjectId, (uint)i);
            if (id != 0 && Scene.Exists(id)) pending.Push(Resolve(Scene, id));
        }
    }

    /// <summary>
    /// Procura por nome na subárvore. Resolver uma vez e guardar o resultado é o
    /// uso pretendido — chamar isto todo quadro percorre a cena inteira.
    /// </summary>
    public GameObject? Find(string name, bool recursive = true)
    {
        Require("procurar objeto");
        var found = Scene.FindChild(ObjectId, name, recursive);
        return found == 0 ? null : Resolve(Scene, found);
    }

    /// <summary>Creates a primitive as a child, with renderer and static collision body.</summary>
    public GameObject CreatePrimitive(PrimitiveType type)
    {
        Require("criar primitiva");
        if (!Enum.IsDefined(type)) throw new ArgumentOutOfRangeException(nameof(type));
        var created = Scene.CreatePrimitive(ObjectId, type);
        if (created == 0) throw new WorldException(Scene.LastStatus, "criar primitiva");
        return Resolve(Scene, created);
    }

    public GameObject CreateChild(string name)
    {
        Require("criar objeto");
        var created = Scene.CreateObject(ObjectId, name);
        if (created == 0) throw new WorldException(Scene.LastStatus, "criar objeto");
        return Resolve(Scene, created);
    }

    /// <summary>
    /// Marca o objeto e a subárvore como vencidos imediatamente; o armazenamento
    /// sai no próximo ponto seguro do mundo.
    /// </summary>
    public void Destroy()
    {
        Require("destruir objeto");
        Check(Scene.DestroyObject(ObjectId), "destruir objeto");
    }

    /// <summary>Segundos simulados do Play; pedidos repetidos conservam o menor prazo.</summary>
    public void Destroy(double delaySeconds)
    {
        Require("agendar destruição");
        if (!double.IsFinite(delaySeconds) || delaySeconds < 0) throw new ArgumentOutOfRangeException(nameof(delaySeconds));
        Check(Scene.DestroyAfter(ObjectId, delaySeconds), "agendar destruição");
    }

    public WorldOperation DestroyTracked()
    {
        Require("destruir objeto");
        var id = Scene.QueueStructuralOperation(0, ObjectId, 0, 0, ReparentPosePolicy.KeepLocal);
        if (id == 0) throw new WorldException(Scene.LastStatus, "destruir objeto");
        return new(Scene, World, id);
    }

    /// <summary>Enfileira a troca de pai para o próximo ponto seguro.</summary>
    public void SetParent(GameObject parent, int index = 0,
                          ReparentPosePolicy posePolicy = ReparentPosePolicy.KeepLocal)
    {
        Require("reparentear");
        if (parent is not { IsValid: true }) throw new WorldException(WorldStatus.InvalidArgument, "reparentear");
        if (index < 0 || !Enum.IsDefined(posePolicy))
            throw new WorldException(WorldStatus.InvalidArgument, "reparentear");
        if (!ReferenceEquals(Scene, parent.Scene) || parent.World != World)
            throw new WorldException(WorldStatus.ForeignWorld, "reparentear");
        parent.Require("reparentear");
        Check(Scene.SetParentWithPolicy(ObjectId, parent.ObjectId, (uint)index, posePolicy), "reparentear");
    }

    public WorldOperation SetParentTracked(GameObject parent, int index = 0,
                                            ReparentPosePolicy posePolicy = ReparentPosePolicy.KeepLocal)
    {
        Require("reparentear");
        if (parent is not { IsValid: true } || index < 0 || !Enum.IsDefined(posePolicy))
            throw new WorldException(WorldStatus.InvalidArgument, "reparentear");
        if (!ReferenceEquals(Scene, parent.Scene) || parent.World != World)
            throw new WorldException(WorldStatus.ForeignWorld, "reparentear");
        parent.Require("reparentear");
        var id = Scene.QueueStructuralOperation(1, ObjectId, parent.ObjectId, (uint)index, posePolicy);
        if (id == 0) throw new WorldException(Scene.LastStatus, "reparentear");
        return new(Scene, World, id);
    }

    public TransformValue LocalTransform
    {
        get { Require("ler transform"); return Scene.GetTransform(ObjectId); }
        set { Require("escrever transform"); Check(Scene.SetTransform(ObjectId, value), "escrever transform"); }
    }

    public TransformValue WorldTransform
    {
        get { Require("ler transform de mundo"); return Scene.GetWorldTransform(ObjectId); }
        set { Require("escrever transform de mundo"); Check(Scene.SetWorldTransform(ObjectId, value), "escrever transform de mundo"); }
    }

    public Vector3 Position
    {
        get => LocalTransform.Position;
        set { var t = LocalTransform; LocalTransform = t with { Position = value }; }
    }

    /// <summary>Envia direção normalizada ao Character para os passos físicos deste quadro.
    /// O yaw usa radianos; no quadro seguinte prevalece o toque até novo comando.</summary>
    public void MoveCharacter(Vector2 input, float yawRadians)
    {
        Require("mover personagem");
        Check(Scene.CharacterMove(ObjectId, input, yawRadians), "mover personagem");
    }

    /// <summary>Tenta saltar. Retorna falso quando o motor recusa o salto no ar.</summary>
    public bool TryJumpCharacter()
    {
        Require("saltar personagem");
        if (Scene.CharacterJump(ObjectId)) return true;
        if (Scene.LastStatus == WorldStatus.Rejected) return false;
        throw new WorldException(Scene.LastStatus, "saltar personagem");
    }

    /// <summary>Aplica um delta de olhar normalizado pela viewport à CameraLook local.</summary>
    public void LookCamera(Vector2 normalizedDelta)
    {
        Require("olhar pela câmera");
        Check(Scene.CameraLook(ObjectId, normalizedDelta), "olhar pela câmera");
    }

    public int ComponentCount { get { Require("contar componentes"); return Math.Max(0, Scene.ComponentCount(ObjectId)); } }

    public Component ComponentAt(int index)
    {
        Require("acessar componente");
        var (instance, typeId) = Scene.ComponentAt(ObjectId, (uint)Math.Max(0, index));
        if (instance == 0) throw new WorldException(WorldStatus.ComponentMissing, "acessar componente");
        return new Component(Scene, this, instance, typeId);
    }

    public IEnumerable<Component> Components()
    {
        var count = ComponentCount;
        for (var index = 0; index < count; ++index) yield return ComponentAt(index);
    }

    /// <summary>A instância de ordem <paramref name="ordinal"/> desse tipo, ou null.</summary>
    public Component? GetComponent(string typeId, int ordinal = 0)
    {
        Require("procurar componente");
        var instance = Scene.FindComponent(ObjectId, typeId, (uint)Math.Max(0, ordinal));
        return instance == 0 ? null : new Component(Scene, this, instance, typeId);
    }

    public bool HasComponent(string typeId) => GetComponent(typeId) is not null;

    public Component AddComponent(string typeId)
    {
        Require("adicionar componente");
        var instance = Scene.AddComponent(ObjectId, typeId);
        if (instance == 0) throw new WorldException(Scene.LastStatus, "adicionar componente");
        return new Component(Scene, this, instance, typeId);
    }

    internal bool BelongsTo(ISceneAccess scene) => ReferenceEquals(_scene, scene) && IsAlive;
    private IBehaviorRegistry Behaviors
    {
        get { Require("acessar scripts"); return Scene.Behaviors ?? throw new WorldException(WorldStatus.NotRunning, "acessar scripts"); }
    }
    public T? GetBehavior<T>() where T : class => Behaviors.FindBehavior(ObjectId, typeof(T)) as T;
    public T[] GetBehaviors<T>() where T : class => Behaviors.FindBehaviors(ObjectId, typeof(T)).OfType<T>().ToArray();
    public T AddBehavior<T>() where T : Behavior => (T)Behaviors.AddBehavior(this, typeof(T));
    /// <summary>Copia a hierarquia sob parent, preservando pose local. Campos serializados são copiados do estado atual.</summary>
    public GameObject Instantiate(GameObject parent)
    {
        ArgumentNullException.ThrowIfNull(parent);
        return Behaviors.Instantiate(this, parent);
    }

    /// <summary>Busca por nome exato no mundo: pré-ordem, ativos, sem a raiz sintética. O(N).</summary>
    public GameObject? FindInWorld(string name)
    {
        Require("buscar objeto no mundo");
        if (string.IsNullOrEmpty(name) || name.Contains('\0')) throw new ArgumentException("Nome inválido", nameof(name));
        var root = this;
        while (root.Parent is { } parent) root = parent;
        var pending = new Stack<GameObject>();
        root.PushAliveChildren(pending);
        while (pending.TryPop(out var current))
        {
            if (!current.IsAlive || !current.ActiveInHierarchy) continue;
            if (current.Name == name) return current;
            current.PushAliveChildren(pending);
        }
        return null;
    }
    private static bool RequiresReceiver(MessageOptions options) => Enum.IsDefined(options)
        ? options == MessageOptions.RequireReceiver : throw new ArgumentOutOfRangeException(nameof(options));
    // A sobrecarga genérica preserva payload 0/bool: não o converte para opções.
    public int SendMessage<T>(string method, T payload, MessageOptions options = MessageOptions.RequireReceiver) => SendMessage(method, (object?)payload, options);
    public int BroadcastMessage<T>(string method, T payload, MessageOptions options = MessageOptions.RequireReceiver) => BroadcastMessage(method, (object?)payload, options);
    public int SendMessageUpwards<T>(string method, T payload, MessageOptions options = MessageOptions.RequireReceiver) => SendMessageUpwards(method, (object?)payload, options);
    public int SendMessage(string method, MessageOptions options = MessageOptions.RequireReceiver) => Behaviors.Message(this, method, null, false, MessageRoute.Object, RequiresReceiver(options));
    public int SendMessage(string method, object? payload, MessageOptions options = MessageOptions.RequireReceiver) => Behaviors.Message(this, method, payload, true, MessageRoute.Object, RequiresReceiver(options));
    public int BroadcastMessage(string method, MessageOptions options = MessageOptions.RequireReceiver) => Behaviors.Message(this, method, null, false, MessageRoute.Descendants, RequiresReceiver(options));
    public int BroadcastMessage(string method, object? payload, MessageOptions options = MessageOptions.RequireReceiver) => Behaviors.Message(this, method, payload, true, MessageRoute.Descendants, RequiresReceiver(options));
    public int SendMessageUpwards(string method, MessageOptions options = MessageOptions.RequireReceiver) => Behaviors.Message(this, method, null, false, MessageRoute.Ancestors, RequiresReceiver(options));
    public int SendMessageUpwards(string method, object? payload, MessageOptions options = MessageOptions.RequireReceiver) => Behaviors.Message(this, method, payload, true, MessageRoute.Ancestors, RequiresReceiver(options));

    public ObjectReference AsReference()
    {
        Require("obter referência");
        return ObjectReference.Capture(Scene, ObjectId);
    }

    /// <summary>
    /// Resolve um id cru no mundo atual, capturando mundo e geração. É o que o
    /// hospedeiro usa para entregar um objeto a um comportamento, e o que um
    /// script usa ao converter uma <see cref="ObjectReference"/> autorada.
    /// </summary>
    public static GameObject Resolve(ISceneAccess scene, ulong objectId)
    {
        ArgumentNullException.ThrowIfNull(scene);
        return new(scene, objectId, scene.WorldId, scene.GenerationOf(objectId));
    }

    internal static GameObject Wrap(ISceneAccess scene, ulong objectId) => Resolve(scene, objectId);

    public bool Equals(GameObject? other) => other is not null &&
        ObjectId == other.ObjectId && World == other.World && Generation == other.Generation;
    public override bool Equals(object? obj) => Equals(obj as GameObject);
    public override int GetHashCode() => HashCode.Combine(ObjectId, World, Generation);
    public override string ToString() => IsValid ? $"objeto #{ObjectId}/{Generation}" : "objeto inválido";
    public static bool operator ==(GameObject? a, GameObject? b) => a is null ? b is null : a.Equals(b);
    public static bool operator !=(GameObject? a, GameObject? b) => !(a == b);
}

/// <summary>
/// Uma instância de componente endereçada por objeto + instanceId. Componentes
/// repetíveis (colisor, junta, comportamento) têm cada instância endereçável.
/// </summary>
public readonly struct Component
{
    private readonly ISceneAccess _scene;
    private readonly GameObject _object;
    internal Component(ISceneAccess scene, GameObject owner, ulong instanceId, string typeId)
    {
        _scene = scene; _object = owner; InstanceId = instanceId; TypeId = typeId;
    }

    public ulong InstanceId { get; }
    public string TypeId { get; }
    public GameObject Object => _object;
    internal ISceneAccess Scene => _scene;
    public bool IsAlive
    {
        get
        {
            if (_object is null || !_object.IsAlive) return false;
            var count = _scene.ComponentCount(_object.ObjectId);
            for (var index = 0u; index < count; ++index)
                if (_scene.ComponentAt(_object.ObjectId, index).Instance == InstanceId) return true;
            return false;
        }
    }

    private void Check(bool ok, string operation)
    {
        if (!ok) throw new WorldException(_scene.LastStatus, operation);
    }

    /// <summary>
    /// Estado local dos tipos que declaram a propriedade enabled no schema.
    /// Tipos sem essa capacidade recusam a operação; não desativa o objeto.
    /// </summary>
    public bool Enabled
    {
        get => GetBool("enabled");
        set => SetBool("enabled", value);
    }

    public float GetFloat(string propertyId) => Read(propertyId, 0, out var bits)
        ? BitConverter.UInt32BitsToSingle((uint)bits)
        : throw new WorldException(_scene.LastStatus, "ler " + propertyId);
    public bool GetBool(string propertyId) => Read(propertyId, 1, out var bits)
        ? bits != 0
        : throw new WorldException(_scene.LastStatus, "ler " + propertyId);
    public uint GetEnum(string propertyId) => Read(propertyId, 2, out var bits)
        ? (uint)bits
        : throw new WorldException(_scene.LastStatus, "ler " + propertyId);
    public ObjectReference GetReference(string propertyId) => Read(propertyId, 3, out var bits)
        ? ObjectReference.Capture(_scene, bits)
        : throw new WorldException(_scene.LastStatus, "ler " + propertyId);

    public void SetFloat(string propertyId, float value) =>
        Check(_scene.SetProperty(_object.ObjectId, InstanceId, propertyId, 0, BitConverter.SingleToUInt32Bits(value)), "escrever " + propertyId);
    public void SetBool(string propertyId, bool value) =>
        Check(_scene.SetProperty(_object.ObjectId, InstanceId, propertyId, 1, value ? 1u : 0u), "escrever " + propertyId);
    public void SetEnum(string propertyId, uint value) =>
        Check(_scene.SetProperty(_object.ObjectId, InstanceId, propertyId, 2, value), "escrever " + propertyId);
    public void SetReference(string propertyId, ObjectReference value)
    {
        var status = value.StatusIn(_scene);
        if (status != WorldStatus.Ok) throw new WorldException(status, "escrever " + propertyId);
        Check(_scene.SetProperty(_object.ObjectId, InstanceId, propertyId, 3, value.ObjectId), "escrever " + propertyId);
    }

    public void SetReference(string propertyId, GameObject? value) =>
        SetReference(propertyId, value is null ? default(ObjectReference) : value.AsReference());
    public AssetGuid GetResource(string propertyId, uint slot = 0) =>
        _scene.TryGetResource(_object.ObjectId, InstanceId, propertyId, slot, out var value)
            ? value : throw new WorldException(_scene.LastStatus, "ler recurso " + propertyId);
    public void SetResource(string propertyId, AssetGuid value, uint slot = 0) =>
        Check(_scene.SetResource(_object.ObjectId, InstanceId, propertyId, slot, value), "escrever recurso " + propertyId);
    public ulong ResourceElementId(string propertyId, uint slot) =>
        _scene.ResourceElementId(_object.ObjectId, InstanceId, propertyId, slot, out var id)
            ? id : throw new WorldException(_scene.LastStatus, "ler elemento " + propertyId);
    public AssetGuid GetResourceByElementId(string propertyId, ulong elementId) =>
        _scene.TryGetResourceByElementId(_object.ObjectId, InstanceId, propertyId, elementId, out var value)
            ? value : throw new WorldException(_scene.LastStatus, "ler recurso " + propertyId);
    public void SetResourceByElementId(string propertyId, ulong elementId, AssetGuid value) =>
        Check(_scene.SetResourceByElementId(_object.ObjectId, InstanceId, propertyId, elementId, value),
              "escrever recurso " + propertyId);
    public float GetSlotFloat(string propertyId,uint slot=0) => ReadSlot(propertyId,slot,0,out var bits)
        ? BitConverter.UInt32BitsToSingle((uint)bits)
        : throw new WorldException(_scene.LastStatus,"ler "+propertyId);
    public uint GetSlotEnum(string propertyId,uint slot=0) => ReadSlot(propertyId,slot,2,out var bits)
        ? (uint)bits : throw new WorldException(_scene.LastStatus,"ler "+propertyId);
    public void SetSlotFloat(string propertyId,float value,uint slot=0) =>
        Check(_scene.SetSlotProperty(_object.ObjectId,InstanceId,propertyId,slot,0,BitConverter.SingleToUInt32Bits(value)),
              "escrever "+propertyId);
    public void SetSlotEnum(string propertyId,uint value,uint slot=0) =>
        Check(_scene.SetSlotProperty(_object.ObjectId,InstanceId,propertyId,slot,2,value),"escrever "+propertyId);
    public AnimationPlayer Animation()
    {
        if(TypeId!=ComponentIds.Animation) throw new WorldException(WorldStatus.InvalidArgument,"acessar animação");
        return new AnimationPlayer(this);
    }
    public DeformableMesh DeformableMesh()
    {
        if(TypeId!=ComponentIds.SkinnedMesh) throw new WorldException(WorldStatus.InvalidArgument,"acessar malha deformável");
        return new DeformableMesh(this);
    }
    public MaterialSlot Material(uint slot=0)
    {
        if(TypeId!=ComponentIds.MeshRenderer) throw new WorldException(WorldStatus.InvalidArgument,"acessar material");
        return new MaterialSlot(this,slot);
    }
    /// <summary>Fachada gerada do schema (`Astra.Components.GameTimer`).</summary>
    public Astra.Components.GameTimer Timer()
    {
        if (TypeId != ComponentIds.Timer) throw new WorldException(WorldStatus.InvalidArgument, "acessar timer");
        return new Astra.Components.GameTimer(this);
    }
    public CameraFollowRig CameraFollow()
    {
        if (TypeId != ComponentIds.CameraFollow) throw new WorldException(WorldStatus.InvalidArgument, "acessar acompanhamento de câmera");
        return new CameraFollowRig(this);
    }

    /// <summary>Remoção aplicada no próximo ponto seguro do mundo.</summary>
    public void Remove() => Check(_scene.RemoveComponent(_object.ObjectId, InstanceId), "remover componente");
    public WorldOperation RemoveTracked()
    {
        if (!_object.IsAlive) throw new WorldException(WorldStatus.StaleHandle, "remover componente");
        var id = _scene.QueueStructuralOperation(2, _object.ObjectId, InstanceId, 0, ReparentPosePolicy.KeepLocal);
        if (id == 0) throw new WorldException(_scene.LastStatus, "remover componente");
        return new(_scene, _object.World, id);
    }

    private bool Read(string propertyId, uint expected, out ulong bits)
    {
        if (!_scene.TryGetProperty(_object.ObjectId, InstanceId, propertyId, out var kind, out bits)) return false;
        // Um campo que mudou de tipo no schema não deve chegar reinterpretado:
        // devolver bits de float como enum produziria um valor plausível e errado.
        if (kind != expected) throw new WorldException(WorldStatus.InvalidArgument, "tipo de " + propertyId);
        return true;
    }
    private bool ReadSlot(string propertyId,uint slot,uint expected,out ulong bits)
    {
        if(!_scene.TryGetSlotProperty(_object.ObjectId,InstanceId,propertyId,slot,out var kind,out bits))return false;
        if(kind!=expected)throw new WorldException(WorldStatus.InvalidArgument,"tipo de "+propertyId);
        return true;
    }

    public override string ToString() => $"{TypeId}#{InstanceId}";
}
