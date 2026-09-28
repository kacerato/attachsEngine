using System.Numerics;
using System.Text.Json;
using Astra;
using Astra.Components;

namespace Aether.Tests;

/// <summary>
/// A API de objetos/componentes vista pelo comportamento do projeto.
///
/// O duplo abaixo reimplementa o contrato do mundo de execução em memória — as
/// mesmas regras que `ae::runtime::GameWorld` aplica no nativo: geração que
/// vence na destruição, mundo próprio por sessão, remoção no ponto seguro e
/// propriedades tipadas. O que se prova aqui é o lado C#: que `GameObject`
/// recusa referência vencida em vez de acertar outro objeto, e que cada
/// instância de componente é endereçável.
/// </summary>
public static class AstraWorldTests
{
    private sealed class FakeWorld : ISceneAccess
    {
        private sealed class Entry
        {
            public ulong Parent;
            public string Name = "";
            public uint Generation = 1;
            public bool Active = true;
            public bool Destroyed;
            public TransformValue Transform = new(Vector3.Zero, Quaternion.Identity, Vector3.One);
            public readonly List<(ulong Instance, string TypeId, Dictionary<string, (uint Kind, ulong Bits)> Values)> Components = [];
        }

        private readonly Dictionary<ulong, Entry> _objects = [];
        private ulong _nextObject = 1;
        private ulong _nextInstance = 1;
        private readonly List<(ulong Object, ulong Instance)> _pendingRemovals = [];
        private readonly List<ulong> _pendingDestroys = [];
        private readonly List<(ulong Child, ulong Parent)> _pendingReparents = [];
        private bool _stopped;

        public FakeWorld(uint worldId)
        {
            _worldId = worldId;
            Root = Create(0, "Cena");
        }

        private readonly uint _worldId;
        /// <summary>Zero depois do Stop: handles guardados deixam de resolver.</summary>
        public uint WorldId => _stopped ? 0u : _worldId;
        public void Stop() => _stopped = true;
        public ulong Root { get; }
        public WorldStatus LastStatus { get; private set; } = WorldStatus.Ok;

        private ulong Create(ulong parent, string name)
        {
            var id = _nextObject++;
            _objects[id] = new Entry { Parent = parent, Name = name };
            return id;
        }
        private Entry? Live(ulong id) => _objects.TryGetValue(id, out var entry) && !entry.Destroyed ? entry : null;
        private bool Fail(WorldStatus status) { LastStatus = status; return false; }

        public bool Exists(ulong objectId) => Live(objectId) is not null;
        public uint GenerationOf(ulong objectId) => Live(objectId)?.Generation ?? 0;

        public TransformValue GetTransform(ulong objectId) =>
            Live(objectId)?.Transform ?? throw new WorldException(WorldStatus.StaleHandle, "ler transform");
        public bool SetTransform(ulong objectId, TransformValue value)
        {
            var entry = Live(objectId);
            if (entry is null) return Fail(WorldStatus.StaleHandle);
            entry.Transform = value; LastStatus = WorldStatus.Ok; return true;
        }
        public TransformValue GetWorldTransform(ulong objectId) => GetTransform(objectId);
        public bool SetWorldTransform(ulong objectId, TransformValue value) => SetTransform(objectId, value);
        public bool SetBodyVelocity(ulong objectId, Vector3 velocity) => Exists(objectId);
        public bool MoveKinematic(ulong objectId, Vector3 position, Quaternion rotation) => Exists(objectId);
        public void Log(ulong objectId, string message) { }

        public ulong ParentOf(ulong objectId) => Live(objectId)?.Parent ?? 0;
        public int ChildCount(ulong objectId) =>
            Live(objectId) is null ? -1 : _objects.Count(p => !p.Value.Destroyed && p.Value.Parent == objectId);
        public ulong ChildAt(ulong objectId, uint index)
        {
            var children = _objects.Where(p => !p.Value.Destroyed && p.Value.Parent == objectId).Select(p => p.Key).ToList();
            return index < children.Count ? children[(int)index] : 0;
        }
        public ulong FindChild(ulong objectId, string name, bool recursive)
        {
            foreach (var child in _objects.Where(p => !p.Value.Destroyed && p.Value.Parent == objectId))
            {
                if (child.Value.Name == name) return child.Key;
                if (!recursive) continue;
                var nested = FindChild(child.Key, name, true);
                if (nested != 0) return nested;
            }
            return 0;
        }
        public string GetName(ulong objectId) =>
            Live(objectId)?.Name ?? throw new WorldException(WorldStatus.StaleHandle, "ler nome");
        public bool SetName(ulong objectId, string name)
        {
            var entry = Live(objectId);
            if (entry is null) return Fail(WorldStatus.StaleHandle);
            if (string.IsNullOrEmpty(name)) return Fail(WorldStatus.InvalidArgument);
            entry.Name = name; LastStatus = WorldStatus.Ok; return true;
        }
        public int GetActiveSelf(ulong objectId) => Live(objectId) is { } entry ? (entry.Active ? 1 : 0) : -1;
        public int GetActive(ulong objectId)
        {
            var entry = Live(objectId);
            if (entry is null) return -1;
            for (var walk = objectId; walk != 0;)
            {
                var current = Live(walk);
                if (current is null || !current.Active) return 0;
                walk = current.Parent;
            }
            return 1;
        }
        public bool SetActive(ulong objectId, bool active)
        {
            var entry = Live(objectId);
            if (entry is null) return Fail(WorldStatus.StaleHandle);
            entry.Active = active; LastStatus = WorldStatus.Ok; return true;
        }

        public ulong CreateObject(ulong parent, string name)
        {
            if (Live(parent) is null) { LastStatus = WorldStatus.StaleHandle; return 0; }
            LastStatus = WorldStatus.Ok;
            return Create(parent, name);
        }
        public bool DestroyObject(ulong objectId)
        {
            var entry = Live(objectId);
            if (entry is null) return Fail(WorldStatus.StaleHandle);
            if (objectId == Root) return Fail(WorldStatus.Rejected);
            // A geração vence na hora; o armazenamento sai no ponto seguro.
            foreach (var member in Subtree(objectId)) { _objects[member].Generation = 0; _pendingDestroys.Add(member); }
            LastStatus = WorldStatus.Ok;
            return true;
        }
        private IEnumerable<ulong> Subtree(ulong id)
        {
            yield return id;
            foreach (var child in _objects.Where(p => !p.Value.Destroyed && p.Value.Parent == id).Select(p => p.Key).ToList())
                foreach (var nested in Subtree(child)) yield return nested;
        }
        public bool SetParent(ulong objectId, ulong parent, uint childIndex)
        {
            if (Live(objectId) is null || Live(parent) is null) return Fail(WorldStatus.StaleHandle);
            _pendingReparents.Add((objectId, parent));
            LastStatus = WorldStatus.Ok;
            return true;
        }
        public bool SetParentWithPolicy(ulong objectId, ulong parent, uint childIndex, ReparentPosePolicy policy) =>
            SetParent(objectId, parent, childIndex);

        public int ComponentCount(ulong objectId) => Live(objectId)?.Components.Count ?? -1;
        public (ulong Instance, string TypeId) ComponentAt(ulong objectId, uint index)
        {
            var entry = Live(objectId);
            if (entry is null || index >= entry.Components.Count) return (0, string.Empty);
            var component = entry.Components[(int)index];
            return (component.Instance, component.TypeId);
        }
        public ulong FindComponent(ulong objectId, string typeId, uint ordinal)
        {
            var entry = Live(objectId);
            if (entry is null) return 0;
            var matches = entry.Components.Where(c => c.TypeId == typeId).ToList();
            return ordinal < matches.Count ? matches[(int)ordinal].Instance : 0;
        }
        public ulong AddComponent(ulong objectId, string typeId)
        {
            var entry = Live(objectId);
            if (entry is null) { LastStatus = WorldStatus.StaleHandle; return 0; }
            if (typeId == ComponentIds.PhysicsBody) { LastStatus = WorldStatus.NotMutableInPlay; return 0; }
            if (typeId != ComponentIds.Collider && entry.Components.Any(c => c.TypeId == typeId))
            {
                LastStatus = WorldStatus.ComponentUnavailable;
                return 0;
            }
            var instance = _nextInstance++;
            entry.Components.Add((instance, typeId, []));
            LastStatus = WorldStatus.Ok;
            return instance;
        }
        public bool RemoveComponent(ulong objectId, ulong instanceId)
        {
            var entry = Live(objectId);
            if (entry is null) return Fail(WorldStatus.StaleHandle);
            if (entry.Components.All(c => c.Instance != instanceId)) return Fail(WorldStatus.ComponentMissing);
            _pendingRemovals.Add((objectId, instanceId));
            LastStatus = WorldStatus.Ok;
            return true;
        }
        public bool TryGetProperty(ulong objectId, ulong instanceId, string propertyId, out uint kind, out ulong bits)
        {
            kind = 0; bits = 0;
            var entry = Live(objectId);
            if (entry is null) return Fail(WorldStatus.StaleHandle);
            var component = entry.Components.FirstOrDefault(c => c.Instance == instanceId);
            if (component.Instance == 0) return Fail(WorldStatus.ComponentMissing);
            if (!component.Values.TryGetValue(propertyId, out var stored)) return Fail(WorldStatus.InvalidArgument);
            kind = stored.Kind; bits = stored.Bits; LastStatus = WorldStatus.Ok; return true;
        }
        public bool SetProperty(ulong objectId, ulong instanceId, string propertyId, uint kind, ulong bits)
        {
            var entry = Live(objectId);
            if (entry is null) return Fail(WorldStatus.StaleHandle);
            var component = entry.Components.FirstOrDefault(c => c.Instance == instanceId);
            if (component.Instance == 0) return Fail(WorldStatus.ComponentMissing);
            component.Values[propertyId] = (kind, bits);
            LastStatus = WorldStatus.Ok;
            return true;
        }

        /// <summary>O ponto seguro do mundo: só aqui o armazenamento muda.</summary>
        // v9: animação. Clipes por componente e o último comando recebido, para
        // o teste conferir o que o reprodutor C# manda pela ABI.
        public readonly Dictionary<ulong, List<(AssetGuid Clip, string Name)>> Clips = [];
        public readonly Dictionary<(ulong, AssetGuid), AnimationStateValue> States = [];
        public (AnimationCommandKind Op, AssetGuid Clip, float Seconds, float Weight, AnimationPlayMode Mode)? LastCommand;
        public bool AnimationCommand(ulong objectId, ulong instanceId, AnimationCommandKind op, AssetGuid clip,
                                     float seconds, float targetWeight, AnimationPlayMode mode)
        {
            if (!Clips.ContainsKey(instanceId)) return Fail(WorldStatus.ComponentMissing);
            LastCommand = (op, clip, seconds, targetWeight, mode);
            var target = clip.IsValid ? clip : Clips[instanceId][0].Clip;
            var state = States.GetValueOrDefault((instanceId, target), new AnimationStateValue(target, false, 0, 1, 0, 2, 0, AnimationWrapMode.Loop));
            States[(instanceId, target)] = op switch
            {
                AnimationCommandKind.Play or AnimationCommandKind.CrossFade => state with { Enabled = true, Weight = 1 },
                AnimationCommandKind.Stop => state with { Enabled = false, Weight = 0, Time = 0 },
                AnimationCommandKind.Rewind => state with { Time = 0 },
                _ => state with { Enabled = true, Weight = targetWeight },
            };
            LastStatus = WorldStatus.Ok;
            return true;
        }
        public bool TryGetAnimationState(ulong objectId, ulong instanceId, AssetGuid clip, out AnimationStateValue value)
        {
            value = States.GetValueOrDefault((instanceId, clip), new AnimationStateValue(clip, false, 0, 1, 0, 2, 0, AnimationWrapMode.Loop));
            LastStatus = WorldStatus.Ok;
            return Clips.ContainsKey(instanceId) || Fail(WorldStatus.ComponentMissing);
        }
        public bool SetAnimationState(ulong objectId, ulong instanceId, in AnimationStateValue value)
        {
            States[(instanceId, value.Clip)] = value;
            LastStatus = WorldStatus.Ok;
            return true;
        }
        public int AnimationClipAt(ulong objectId, ulong instanceId, uint index, out AssetGuid clip, out string name)
        {
            clip = default; name = "";
            if (!Clips.TryGetValue(instanceId, out var list)) { LastStatus = WorldStatus.ComponentMissing; return -1; }
            if (index < list.Count) (clip, name) = list[(int)index];
            LastStatus = WorldStatus.Ok;
            return list.Count;
        }

        public void Flush()
        {
            foreach (var (owner, instance) in _pendingRemovals)
                _objects[owner].Components.RemoveAll(c => c.Instance == instance);
            _pendingRemovals.Clear();
            foreach (var (child, parent) in _pendingReparents) _objects[child].Parent = parent;
            _pendingReparents.Clear();
            foreach (var id in _pendingDestroys) if (_objects.TryGetValue(id, out var entry)) entry.Destroyed = true;
            _pendingDestroys.Clear();
        }

        public GameObject RootObject => GameObject.Resolve(this, Root);
    }

    [Test]
    public static void HierarquiaCriaEncontraEReparenteiaObjetos()
    {
        var world = new FakeWorld(7);
        var root = world.RootObject;
        Assert.Equal("Cena", root.Name);
        Assert.Equal(0, root.ChildCount);

        var parent = root.CreateChild("Pátio");
        var child = parent.CreateChild("Plataforma");
        Assert.Equal(1, root.ChildCount);
        Assert.Equal(parent, child.Parent!);
        Assert.Equal(child, root.Find("Plataforma")!);
        Assert.True(root.Find("Plataforma", recursive: false) is null, "busca direta não atravessa níveis");

        child.SetParent(root);
        world.Flush();
        Assert.Equal(root, child.Parent!);
        Assert.Equal(2, root.ChildCount);

        child.SetParent(parent, posePolicy: ReparentPosePolicy.KeepWorld);
        world.Flush();
        Assert.Equal(parent, child.Parent!);

        child.Name = "Elevador";
        Assert.Equal("Elevador", child.Name);
        child.SetActive(false);
        Assert.False(child.ActiveInHierarchy, "objeto desativado");
        Assert.False(child.ActiveSelf);
        parent.SetActive(false);
        child.SetActive(true);
        Assert.True(child.ActiveSelf, "estado local preservado sob pai inativo");
        Assert.False(child.ActiveInHierarchy);
        parent.SetActive(true);
        Assert.True(child.ActiveSelf && child.ActiveInHierarchy, "reativação não altera estado local do filho");
    }

    [Test]
    public static void ReparentRecusaPaiDeOutroMundoMesmoComIdsIguais()
    {
        var world = new FakeWorld(7);
        var other = new FakeWorld(7);
        var child = world.RootObject.CreateChild("Filho");
        Assert.Throws<WorldException>(() => child.SetParent(other.RootObject),
            "a referência deve pertencer ao mesmo acesso de cena");
        Assert.Throws<WorldException>(() => child.SetParent(world.RootObject, -1),
            "índice negativo não deve virar zero");
        var stale = world.RootObject.CreateChild("Pai");
        stale.Destroy();
        Assert.Throws<WorldException>(() => child.SetParent(stale),
            "pai removido não deve ser aceito");
    }

    [Test]
    public static void ReferenciaVenceNaDestruicaoEArmazenamentoSaiNoPontoSeguro()
    {
        var world = new FakeWorld(3);
        var parent = world.RootObject.CreateChild("Pai");
        var child = parent.CreateChild("Filho");
        Assert.True(parent.IsAlive && child.IsAlive, "ambos vivos");

        parent.Destroy();
        // Ainda dentro do mesmo callback: a referência já não vale.
        Assert.False(parent.IsAlive, "referência do removido vence imediatamente");
        Assert.False(child.IsAlive, "a subárvore inteira vence junto");
        Assert.Throws<WorldException>(() => _ = parent.Name, "leitura por referência vencida é recusada");
        Assert.Throws<WorldException>(() => child.SetActive(true), "escrita por referência vencida é recusada");
        Assert.Throws<WorldException>(() => _ = child.ActiveSelf, "estado local de referência vencida é recusado");
        // O armazenamento só some no ponto seguro.
        Assert.True(world.Exists(parent.ObjectId), "objeto ainda presente antes do ponto seguro");
        world.Flush();
        Assert.False(world.Exists(parent.ObjectId), "liberado no ponto seguro");
    }

    [Test]
    public static void ReferenciaGuardadaNaoSobreviveAoStop()
    {
        var world = new FakeWorld(1);
        var kept = world.RootObject.CreateChild("Objeto");
        Assert.True(kept.IsAlive, "vivo durante a execução");
        // Depois do Stop o mundo tem outra identidade; um comportamento que
        // guardou a referência recebe recusa, não um objeto de outra sessão.
        world.Stop();
        Assert.False(kept.IsAlive, "handle de outra execução não resolve");
        Assert.Throws<WorldException>(() => _ = kept.Name, "e recusa explicitamente");
    }

    [Test]
    public static void ComponentesSaoEnderecadosPorInstanciaEObedecemAoContrato()
    {
        var world = new FakeWorld(5);
        var target = world.RootObject.CreateChild("Porta");

        var first = target.AddComponent(ComponentIds.Collider);
        var second = target.AddComponent(ComponentIds.Collider);
        Assert.NotEqual(first.InstanceId, second.InstanceId, "instâncias distintas");
        Assert.Equal(2, target.ComponentCount);
        Assert.Equal(first.InstanceId, target.GetComponent(ComponentIds.Collider)!.Value.InstanceId);
        Assert.Equal(second.InstanceId, target.GetComponent(ComponentIds.Collider, 1)!.Value.InstanceId);

        target.AddComponent(ComponentIds.Camera);
        Assert.Throws<WorldException>(() => target.AddComponent(ComponentIds.Camera), "componente singular não duplica");
        Assert.Throws<WorldException>(() => target.AddComponent(ComponentIds.PhysicsBody), "não mutável em Play");

        second.SetFloat("half_x", 2.5f);
        first.SetFloat("half_x", 0.5f);
        Assert.Close(2.5f, second.GetFloat("half_x"), 1e-6f, "cada instância guarda o próprio valor");
        Assert.Close(0.5f, first.GetFloat("half_x"), 1e-6f);
        second.SetEnum("shape", 1);
        Assert.Equal(1u, second.GetEnum("shape"));
        second.SetBool("enabled", false);
        Assert.False(second.GetBool("enabled"));
        second.SetReference("owner", target.AsReference());
        Assert.Equal(target.ObjectId, second.GetReference("owner").ObjectId);
        // Ler um campo com o tipo errado não pode devolver um número plausível.
        Assert.Throws<WorldException>(() => second.GetFloat("shape"), "tipo divergente é recusado");

        second.Remove();
        Assert.Equal(3, target.ComponentCount, "remoção só vale no ponto seguro");
        world.Flush();
        Assert.Equal(2, target.ComponentCount, "componente removido");
        Assert.False(second.IsAlive, "instância removida não resolve");
        Assert.True(first.IsAlive, "a outra instância continua válida");
    }

    [Test]
    public static void ReferenciasDeComponenteRespeitamMundoEGeracao()
    {
        var world = new FakeWorld(12);
        var other = new FakeWorld(12); // IDs e número de mundo iguais não bastam.
        var owner = world.RootObject.CreateChild("Origem");
        var component = owner.AddComponent(ComponentIds.Collider);
        var target = world.RootObject.CreateChild("Destino");
        var foreign = other.RootObject.CreateChild("Outro destino");
        var reference = target.AsReference();
        Assert.Equal("{\"ObjectId\":" + target.ObjectId + "}", JsonSerializer.Serialize(reference),
            "sessão e geração não entram no dado autoral");

        component.SetReference("owner", target);
        var read = component.GetReference("owner");
        Assert.Equal(target.ObjectId, read.ObjectId);
        Assert.Throws<WorldException>(() => component.SetReference("owner", foreign.AsReference()),
            "referência vinculada a outro acesso de cena é recusada");
        Assert.Throws<WorldException>(() => component.SetReference("owner", foreign),
            "sobrecarga com GameObject também valida o mundo");

        target.Destroy();
        Assert.Throws<WorldException>(() => component.SetReference("owner", reference),
            "referência guardada vence com o objeto");
        Assert.Throws<WorldException>(() => component.SetReference("owner", read),
            "referência lida do componente também vence");
        Assert.Throws<WorldException>(() => target.AsReference(),
            "objeto removido não emite referência nova");
        component.SetReference("owner", (GameObject?)null);
        Assert.Equal(0ul, component.GetReference("owner").ObjectId);
    }

    [Test]
    public static void AnimacaoComandaClipesPorNomePelaAbiDeAnimacao()
    {
        var world = new FakeWorld(11);
        var fox = world.RootObject.CreateChild("Fox");
        var component = fox.AddComponent(ComponentIds.Animation);
        var walk = new AssetGuid(1, 2);
        var run = new AssetGuid(3, 4);
        world.Clips[component.InstanceId] = [(walk, "Walk"), (run, "Run")];
        var player = component.Animation();
        Assert.Equal(2, player.ClipNames.Count, "a lista vem do componente");
        Assert.Equal("Run", player.ClipNames[1]);

        player.CrossFade("Run", 0.25f);
        Assert.Equal(AnimationCommandKind.CrossFade, world.LastCommand!.Value.Op, "CrossFade atravessa a ABI");
        Assert.Equal(run, world.LastCommand!.Value.Clip, "o nome vira a identidade do clipe");
        Assert.Close(0.25f, world.LastCommand!.Value.Seconds, 1e-6f, "com a duração do fade");
        Assert.True(player.IsPlaying && player.IsClipPlaying("Run"), "o estado do clipe responde");

        player.Blend("Walk", 0.4f, 0.5f);
        Assert.Equal(AnimationCommandKind.Blend, world.LastCommand!.Value.Op);
        Assert.Close(0.4f, world.LastCommand!.Value.Weight, 1e-6f, "peso alvo do Blend");

        var state = player["Walk"];
        state.Speed = -1;
        state.Layer = 2;
        state.NormalizedTime = 0.5f;
        Assert.Close(-1f, state.Speed, 1e-6f, "velocidade por clipe");
        Assert.Equal(2u, state.Layer, "camada por clipe");
        Assert.Close(1f, state.Time, 1e-6f, "normalizedTime = tempo / duração (2 s)");

        player.Stop();
        Assert.Equal(AnimationCommandKind.Stop, world.LastCommand!.Value.Op);
        Assert.False(world.LastCommand!.Value.Clip.IsValid, "Stop() sem nome vale para todos");
        Assert.Throws<WorldException>(() => player.Play("Idle"), "clipe fora da lista é recusado");

        component.SetBool("play_automatically", false);
        Assert.False(player.PlayAutomatically, "configuração autoral pelas propriedades persistentes");
        var collider = fox.AddComponent(ComponentIds.Collider);
        Assert.Throws<WorldException>(() => collider.Animation(), "só o componente de animação vira reprodutor");
    }

    [Test]
    public static void TransformLocalEDeMundoAtravessamAApi()
    {
        var world = new FakeWorld(9);
        var mover = world.RootObject.CreateChild("Plataforma");
        mover.Position = new Vector3(1, 2, 3);
        Assert.Close(1, mover.Position.X, 1e-6f);
        Assert.Close(2, mover.Position.Y, 1e-6f);
        Assert.Close(3, mover.Position.Z, 1e-6f);
        mover.WorldTransform = new TransformValue(new Vector3(4, 0, 0), Quaternion.Identity, Vector3.One);
        Assert.Close(4, mover.WorldTransform.Position.X, 1e-6f);
    }
    [Test]
    public static void FachadaGeradaUsaOsIdsETiposDoSchema()
    {
        var world = new FakeWorld(9);
        var target = world.RootObject.CreateChild("Câmera");
        // A fachada é gerada do registro nativo: o id e o tipo de cada campo vêm
        // de lá, e o acesso tipado passa pelo mesmo Get/Set genérico da ABI.
        var camera = target.AddComponent<Astra.Components.Camera>();
        Assert.Equal("astra.camera", Astra.Components.Camera.TypeId);
        Assert.True(target.HasComponent<Astra.Components.Camera>(), "tipo anexado reconhecido");
        var collider = target.AddComponent<Collider>();
        collider.Shape = Collider.ShapeOption.Esfera;
        Assert.Equal(Collider.ShapeOption.Esfera, collider.Shape);
        Assert.Equal(1u, collider.Component.GetEnum("shape"), "enumeração grava o valor do schema");
        collider.Enabled = false;
        Assert.False(target.GetComponent<Collider>()!.Value.Enabled, "leitura pela fachada recuperada do objeto");
        var follow = target.AddComponent<CameraFollow>();
        follow.Offset = new Vector3(1, 2, 3);
        Assert.Close(2, follow.Component.GetFloat("offset_y"), 1e-6f, "tripla vira Vector3 sobre os canais");
        Assert.Throws<WorldException>(() => _ = new GameTimer(camera.Component), "fachada recusa tipo diferente");
    }
}
