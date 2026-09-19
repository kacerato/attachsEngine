using System.Numerics;

namespace Astra;

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
    public const string MeshRenderer = "astra.render.mesh";
    public const string Light = "astra.render.light";
    /// <summary>LOD Group: `level_count`, `transition_0..3` (% da altura da tela), `level_0..3` (objeto do nível) e `size`.</summary>
    public const string LodGroup = "astra.render.lod_group";
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

    /// <summary>Ativo considerando também os ancestrais.</summary>
    public bool ActiveInHierarchy
    {
        get { Require("ler estado ativo"); return Scene.GetActive(ObjectId) == 1; }
    }
    public void SetActive(bool active)
    {
        Require("alterar estado ativo");
        Check(Scene.SetActive(ObjectId, active), "alterar estado ativo");
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

    /// <summary>Reparent aplicado no próximo ponto seguro.</summary>
    public void SetParent(GameObject parent, int index = 0)
    {
        Require("reparentear");
        if (parent is not { IsValid: true }) throw new WorldException(WorldStatus.InvalidArgument, "reparentear");
        Check(Scene.SetParent(ObjectId, parent.ObjectId, (uint)Math.Max(0, index)), "reparentear");
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

    public ObjectReference AsReference() => new(ObjectId);

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
    public bool IsAlive
    {
        get
        {
            if (!_object.IsAlive) return false;
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
        ? new ObjectReference(bits)
        : throw new WorldException(_scene.LastStatus, "ler " + propertyId);

    public void SetFloat(string propertyId, float value) =>
        Check(_scene.SetProperty(_object.ObjectId, InstanceId, propertyId, 0, BitConverter.SingleToUInt32Bits(value)), "escrever " + propertyId);
    public void SetBool(string propertyId, bool value) =>
        Check(_scene.SetProperty(_object.ObjectId, InstanceId, propertyId, 1, value ? 1u : 0u), "escrever " + propertyId);
    public void SetEnum(string propertyId, uint value) =>
        Check(_scene.SetProperty(_object.ObjectId, InstanceId, propertyId, 2, value), "escrever " + propertyId);
    public void SetReference(string propertyId, ObjectReference value) =>
        Check(_scene.SetProperty(_object.ObjectId, InstanceId, propertyId, 3, value.ObjectId), "escrever " + propertyId);

    /// <summary>Remoção aplicada no próximo ponto seguro do mundo.</summary>
    public void Remove() => Check(_scene.RemoveComponent(_object.ObjectId, InstanceId), "remover componente");

    private bool Read(string propertyId, uint expected, out ulong bits)
    {
        if (!_scene.TryGetProperty(_object.ObjectId, InstanceId, propertyId, out var kind, out bits)) return false;
        // Um campo que mudou de tipo no schema não deve chegar reinterpretado:
        // devolver bits de float como enum produziria um valor plausível e errado.
        if (kind != expected) throw new WorldException(WorldStatus.InvalidArgument, "tipo de " + propertyId);
        return true;
    }

    public override string ToString() => $"{TypeId}#{InstanceId}";
}
