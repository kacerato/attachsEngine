using System.Numerics;
using System.Reflection;
using System.Runtime.Loader;
using System.Text.Json;
using Astra.Compilation;

namespace Astra.Runtime;

public sealed record BehaviorAttachment(ulong ObjectId, ulong InstanceId, string TypeId,
    bool Enabled, IReadOnlyDictionary<string, JsonElement> Properties, IReadOnlyDictionary<string, string>? PropertyTypes = null);
public sealed record BehaviorFailure(ulong ObjectId, ulong InstanceId, string Phase, string Message);
/// <summary>Edição do Inspector com o Play rodando: estado e campos alterados de uma instância.</summary>
public sealed record BehaviorEdit(bool Enabled, IReadOnlyDictionary<string, JsonElement>? Properties);

/// <summary>Owns one isolated script assembly and its instances for a Play session.</summary>
public sealed class BehaviorWorld : IDisposable, IBehaviorRegistry, ICoroutineHost, ISaveHost, ITimeHost, IComponentEventHost
{
    // Assinaturas de eventos de componente. Limite explícito: uma assinatura por
    // quadro em um loop seria vazamento, não uso legítimo.
    private const int SubscriptionLimit = 4096;
    private readonly List<ComponentSubscription> _subscriptions = [];
    private readonly ComponentEventRecord[] _eventBuffer = new ComponentEventRecord[64];
    ComponentSubscription IComponentEventHost.Connect(Behavior owner, Component source, string eventId, Action<ComponentEventArgs> handler)
    {
        ArgumentNullException.ThrowIfNull(eventId); ArgumentNullException.ThrowIfNull(handler);
        if (!Running || _stopping || _scene is null) throw new WorldException(WorldStatus.NotRunning, "assinar evento");
        if (!owner.IsAlive) throw new WorldException(WorldStatus.StaleHandle, "assinar evento");
        if (source.Object is null || !source.Object.BelongsTo(_scene)) throw new WorldException(WorldStatus.ForeignWorld, "assinar evento");
        if (!source.IsAlive) throw new WorldException(WorldStatus.ComponentMissing, "assinar evento");
        if (_scene is not IComponentOperationAccess access) throw new NotSupportedException("O host não oferece eventos de componente.");
        if (!access.DeclaresComponentEvent(source.TypeId, eventId))
            throw new WorldException(WorldStatus.UnknownOperation, $"assinar {source.TypeId}/{eventId}");
        _subscriptions.RemoveAll(s => !s.Active);
        if (_subscriptions.Count >= SubscriptionLimit) throw new WorldException(WorldStatus.LimitReached, "assinar evento");
        var subscription = new ComponentSubscription(owner, source, eventId, handler);
        _subscriptions.Add(subscription);
        return subscription;
    }
    // Drena a fila nativa mesmo sem assinaturas: um consumidor parado faria a
    // fila estourar e descartar eventos que conexões autoradas ainda leriam.
    private readonly HierarchyChange[] _hierarchyBuffer = new HierarchyChange[64];
    // Mudanças aplicadas no ponto seguro chegam no despacho seguinte, na ordem
    // em que o mundo as aplicou; geração antiga significa objeto já trocado.
    private void DispatchHierarchyChanges()
    {
        if (!Running || _scene is not IHierarchyChangeAccess access) return;
        for (var round = 0; round < 64; ++round)
        {
            var count = access.PollHierarchyChanges(_hierarchyBuffer);
            for (var i = 0; i < count; ++i)
            {
                var change = _hierarchyBuffer[i];
                if (_scene.GenerationOf(change.ObjectId) != change.Generation) continue;
                if (change.ParentChanged) Dispatch("ParentChanged", static b => b.ParentChanged(), change.ObjectId);
                else Dispatch("ChildrenChanged", static b => b.ChildrenChanged(), change.ObjectId);
            }
            if (count < _hierarchyBuffer.Length) break;
        }
    }
    private void DispatchComponentEvents()
    {
        DispatchHierarchyChanges();
        if (!Running || _scene is not IComponentOperationAccess access) return;
        for (var round = 0; round < 64; ++round)
        {
            var count = access.PollComponentEvents(_eventBuffer);
            for (var i = 0; i < count; ++i) DeliverComponentEvent(_eventBuffer[i]);
            if (count < _eventBuffer.Length) break;
        }
    }
    private void DeliverComponentEvent(ComponentEventRecord record)
    {
        if (_subscriptions.Count == 0 || _scene is null) return;
        for (int i = 0, count = _subscriptions.Count; i < count; ++i)
        {
            var subscription = _subscriptions[i];
            var source = subscription.Source;
            if (!subscription.Active || subscription.EventId != record.EventId || source.TypeId != record.TypeId) continue;
            var emitter = source.Object;
            if (emitter.ObjectId != record.ObjectId || emitter.World != record.World || emitter.Generation != record.Generation) continue;
            // Instância zero: o backend informou só o objeto (contatos 3D).
            if (record.InstanceId != 0 && record.InstanceId != source.InstanceId) continue;
            var entry = _entries.FirstOrDefault(e => ReferenceEquals(e.Instance, subscription.Owner));
            if (entry is null || entry.Retired || entry.Failed) { subscription.Active = false; continue; }
            var args = new ComponentEventArgs(_scene, source, record);
            Invoke(entry, "Event " + record.TypeId + "/" + record.EventId, _ => subscription.Handler(args));
        }
    }

    private sealed class ProjectLoadContext() : AssemblyLoadContext("Astra.Project", isCollectible: true)
    {
        protected override Assembly? Load(AssemblyName name) =>
            name.Name == typeof(Behavior).Assembly.GetName().Name ? typeof(Behavior).Assembly : null;
    }
    private sealed class Entry(Behavior instance, ScriptTypeSchema schema, GameObject owner)
    {
        public Behavior Instance { get; } = instance;
        public ScriptTypeSchema Schema { get; } = schema;
        public GameObject Owner { get; } = owner;
        public bool Awoken { get; set; }
        public bool Started { get; set; }
        // Último estado entregue por Enable/Disable; a transição é detectada
        // quando o próprio script (ou outro) muda Enabled.
        public bool Active { get; set; }
        // Desativada por exceção: não recebe Disable, que poderia falhar de novo.
        public bool Failed { get; set; }
        public bool Retired { get; set; }
    }
    /// <summary>Eventos do aplicativo repassados aos comportamentos.</summary>
    public enum ApplicationEvent : uint { Pause = 0, Focus = 1 }
    private ProjectLoadContext? _context;
    private ISceneAccess? _scene;
    private SaveStore? _saveStore;
    private TimeAccess? _time;
    public TimeAccess Time => _time?.Validate() ?? throw new InvalidOperationException("No active Play clock.");
    public SaveStore Save => _saveStore ?? throw new InvalidOperationException("This Play host has no configured project save store.");
    private readonly List<Entry> _entries = [];
    private readonly List<BehaviorFailure> _failures = [];
    private bool _started, _stopping, _bindComponentState;
    private int _dispatchDepth;
    private readonly Dictionary<Type, ScriptTypeSchema> _types = [];
    private CoroutineScheduler? _coroutines;
    private bool CoroutineMayRun(Behavior owner) => Running && !_stopping && owner.CoroutineEligible && owner.IsAlive &&
        _scene is not null && owner.CoroutineOwner.ActiveInHierarchy;
    private void CoroutineFailure(Coroutine handle, string phase, Exception error)
    {
        if (_failures.Count < 1024) _failures.Add(new(handle.ObjectId, handle.InstanceId, phase, error.ToString()));
    }
    Coroutine ICoroutineHost.StartCoroutine(Behavior owner, System.Collections.IEnumerator routine) =>
        (_coroutines ?? throw new InvalidOperationException("No active Play scheduler.")).Start(owner, routine);
    void ICoroutineHost.StopCoroutine(Behavior owner, Coroutine handle) =>
        (_coroutines ?? throw new InvalidOperationException("No active Play scheduler.")).Stop(owner, handle);
    void ICoroutineHost.StopCoroutine(Behavior owner, System.Collections.IEnumerator routine) =>
        (_coroutines ?? throw new InvalidOperationException("No active Play scheduler.")).Stop(owner, routine);
    void ICoroutineHost.StopAllCoroutines(Behavior owner) => _coroutines?.CancelOwner(owner);

    public IReadOnlyList<BehaviorFailure> Failures => _failures;
    public bool Running => _context is not null && _started;

    public void Start(CompiledProject project, ISceneAccess scene, IEnumerable<BehaviorAttachment> attachments,
        bool bindComponentState = false, SaveStore? saveStore = null)
    {
        if (_context is not null) throw new InvalidOperationException("A script world is already active.");
        var context = new ProjectLoadContext();
        var prepared = new List<Entry>();
        try
        {
            using var dll = new MemoryStream(project.Assembly, writable: false);
            using var pdb = new MemoryStream(project.Symbols, writable: false);
            var assembly = context.LoadFromStream(dll, pdb);
            _types.Clear();
            foreach (var schema in project.Types) _types.Add(assembly.GetType(schema.Name, throwOnError: true)!, schema);
            _bindComponentState = bindComponentState; _saveStore = saveStore;
            _time = new TimeAccess(scene as ITimeSceneAccess); _time.Activate();
            PrepareAttachments(scene, attachments, prepared);
            _context = context; _scene = scene; _entries.AddRange(prepared); _failures.Clear(); _started = true;
            _coroutines = new(CoroutineMayRun, CoroutineFailure);
            // Todas as instâncias existem antes dos callbacks. Objetos inativos
            // aguardam sua primeira ativação; Enabled=false não adia o Awake.
            Dispatch("Awake", static b => { }, awakenOnly: true);
            Dispatch("Start", static b => { });
        }
        catch
        {
            _coroutines?.Shutdown(); _coroutines = null;
            _time?.End(); _time = null;
            foreach (var entry in prepared) entry.Instance.Detach();
            _entries.Clear(); _types.Clear(); _messages.Clear(); _context = null; _scene = null; _saveStore = null; _started = false; context.Unload(); throw;
        }
    }
    // A contagem fica fixa por despacho. Adições podem realocar a lista sem
    // invalidar iteração; remoções só compactam fora de callbacks/reentradas.
    private void Dispatch(string phase, Action<Behavior> action, ulong? objectId = null, bool awakenOnly = false)
    {
        if (!Running) return;
        if (_dispatchDepth == 0) SweepRemoved();
        ++_dispatchDepth;
        try
        {
            for (int i = 0, count = _entries.Count; i < count; ++i)
            {
                var entry = _entries[i];
                if (objectId is { } id && entry.Instance.ObjectId != id) continue;
                if (awakenOnly) EnsureAwake(entry);
                else if (EnsureStarted(entry)) Invoke(entry, phase, action);
            }
        }
        finally { if (--_dispatchDepth == 0) SweepRemoved(); }
    }
    public void Update(float deltaTime)
    {
        if (!Running || !float.IsFinite(deltaTime) || deltaTime < 0) return;
        Time.BeginFrame(deltaTime);
        _coroutines?.BeginFrame(deltaTime);
        DispatchComponentEvents();
        Dispatch("Update", b => b.Update(deltaTime));
        _coroutines?.Tick();
    }
    public void LateUpdate(float deltaTime)
    {
        if (!Running || !float.IsFinite(deltaTime) || deltaTime < 0) return;
        Time.FramePhase();
        DispatchComponentEvents();
        Dispatch("LateUpdate", b => b.LateUpdate(deltaTime));
    }
    public void FixedUpdate(float deltaTime)
    {
        if (!Running || !float.IsFinite(deltaTime) || deltaTime <= 0) return;
        Time.BeginFixed(deltaTime);
        try { _coroutines?.BeginFixedStep(); DispatchComponentEvents(); Dispatch("FixedUpdate", b => b.FixedUpdate(deltaTime)); _coroutines?.TickFixed(); }
        finally { _time?.EndFixed(); }
    }
    public void Application(ApplicationEvent kind, bool value) => Dispatch(kind == ApplicationEvent.Pause ? "ApplicationPause" : "ApplicationFocus", b =>
    {
        if (kind == ApplicationEvent.Pause) b.ApplicationPause(value); else b.ApplicationFocus(value);
    });
    public void Timer(ulong objectId, ulong instanceId, uint count)
    {
        if (instanceId != 0 && count != 0) Dispatch("TimerElapsed", b => b.TimerElapsed(instanceId, count), objectId);
    }
    public void Trigger(ulong sensor, ulong other, uint phase)
    {
        if (phase > 2 || _scene is null) return;
        var reference = ObjectReference.Capture(_scene, other);
        Dispatch(phase == 0 ? "TriggerEnter" : phase == 1 ? "TriggerStay" : "TriggerExit", b =>
        {
            if (phase == 0) b.TriggerEnter(reference); else if (phase == 1) b.TriggerStay(reference); else b.TriggerExit(reference);
        }, sensor);
    }
    public void Contact(ulong self, ulong other, uint phase, Vector3? normal)
    {
        if (phase > 2 || _scene is null) return;
        var collision = new Collision(ObjectReference.Capture(_scene, other), normal);
        Dispatch(phase == 0 ? "CollisionEnter" : phase == 1 ? "CollisionStay" : "CollisionExit", b =>
        {
            if (phase == 0) b.CollisionEnter(collision); else if (phase == 1) b.CollisionStay(collision); else b.CollisionExit(collision);
        }, self);
    }
    private static bool Alive(Entry entry) => !entry.Retired && entry.Instance.IsAlive;
    private void SweepRemoved()
    {
        // Retire antes do callback: Destroy pode enviar mensagens ou remover outro receptor.
        ++_dispatchDepth;
        try
        {
            for (int i = 0, count = _entries.Count; i < count; ++i)
                if (!Alive(_entries[i]) && !_entries[i].Retired) Retire(_entries[i]);
            _entries.RemoveAll(e => e.Retired);
        }
        finally { --_dispatchDepth; }
    }
    private void Retire(Entry entry)
    {
        if (entry.Retired) return;
        entry.Retired = true;
        entry.Instance.BlockCoroutines();
        _coroutines?.CancelOwner(entry.Instance);
        foreach (var subscription in _subscriptions) if (ReferenceEquals(subscription.Owner, entry.Instance)) subscription.Active = false;
        Deactivate(entry);
        if (entry.Started) Invoke(entry, "Stop", static b => b.Stop());
        if (entry.Awoken) Invoke(entry, "Destroy", static b => b.Destroy());
        entry.Instance.Detach();
    }
    public Behavior AddBehavior(GameObject owner, Type type)
    {
        if (!Running || _stopping || _scene is null) throw new WorldException(WorldStatus.NotRunning, "adicionar script");
        if (!owner.BelongsTo(_scene)) throw new WorldException(WorldStatus.ForeignWorld, "adicionar script");
        if (!_types.TryGetValue(type, out var schema)) throw new WorldException(WorldStatus.UnknownComponent, "tipo de script não publicado");
        if (_entries.Count >= 4096 || _dispatchDepth >= 32) throw new WorldException(WorldStatus.LimitReached, "adicionar script");
        // Construtor primeiro: uma exceção não deixa componente nativo órfão.
        var instance = (Behavior)Activator.CreateInstance(type)!;
        var id = _scene.AddBehavior(owner.ObjectId, schema.Id, schema.File);
        if (id == 0) throw new WorldException(_scene.LastStatus, "adicionar script");
        var initialEnabled = instance.Enabled;
        instance.Attach(_scene, owner.ObjectId, id, this, _bindComponentState);
        instance.Enabled = initialEnabled;
        var entry = new Entry(instance, schema, owner);
        _entries.Add(entry);
        ++_dispatchDepth;
        try { EnsureActivated(entry); } // Start somente antes do primeiro despacho de execução.
        finally { --_dispatchDepth; }
        return instance;
    }
    public void RemoveBehavior(Behavior behavior)
    {
        var entry = _entries.Find(e => ReferenceEquals(e.Instance, behavior) && Alive(e));
        if (entry is null || _scene is null) throw new WorldException(WorldStatus.ComponentMissing, "remover script");
        if (!_scene.RemoveComponent(behavior.ObjectId, behavior.InstanceId)) throw new WorldException(_scene.LastStatus, "remover script");
        behavior.MarkRemoved();
        if (_dispatchDepth == 0) SweepRemoved();
    }

    private static readonly JsonSerializerOptions FieldJson = new() { IncludeFields = true };
    private JsonElement CaptureValue(object? value, string kind)
    {
        if (value is null) return JsonSerializer.SerializeToElement<object?>(null);
        if (kind.StartsWith("array:", StringComparison.Ordinal))
        {
            if (value is System.Collections.ICollection { Count: > 1024 })
                throw new InvalidDataException("Lista excede os 1024 elementos do formato serializado");
            return JsonSerializer.SerializeToElement(((System.Collections.IEnumerable)value).Cast<object?>()
                .Select(v => CaptureValue(v, kind[6..])).ToArray());
        }
        if (value is string { Length: > 4096 }) throw new InvalidDataException("Texto excede o limite do campo serializado");
        if (kind == "object")
        {
            var reference = (ObjectReference)value;
            if (reference.StatusIn(_scene!) == WorldStatus.ForeignWorld) throw new WorldException(WorldStatus.ForeignWorld, "copiar referência");
            return JsonSerializer.SerializeToElement(new ObjectReference(_scene!.Exists(reference.ObjectId) ? reference.ObjectId : 0));
        }
        if (kind.StartsWith("component:", StringComparison.Ordinal))
        {
            var component = (Component)value.GetType().GetProperty("Component")!.GetValue(value)!;
            if (component.IsAlive && !ReferenceEquals(component.Scene, _scene)) throw new WorldException(WorldStatus.ForeignWorld, "copiar componente");
            return JsonSerializer.SerializeToElement(new { ObjectId = component.IsAlive ? component.Object.ObjectId : 0, InstanceId = component.IsAlive ? component.InstanceId : 0 });
        }
        return JsonSerializer.SerializeToElement(value, value.GetType(), FieldJson);
    }
    private JsonElement CaptureProperty(Entry entry, ScriptPropertySchema property)
    {
        var member = ResolveMember(entry.Instance.GetType(), property);
        var value = member is FieldInfo field ? field.GetValue(entry.Instance) : ((PropertyInfo)member).GetValue(entry.Instance);
        return CaptureValue(value, property.ValueType);
    }
    public string InspectFields(ulong objectId)
    {
        if (!Running || _scene is null || !_scene.Exists(objectId)) throw new WorldException(WorldStatus.StaleHandle, "inspecionar campos");
        var entries = _entries.Where(e => Alive(e) && e.Instance.ObjectId == objectId).ToArray();
        var output = new System.Text.StringBuilder("ASTRA_FIELDS 1 ").Append(entries.Length).Append(' ');
        foreach (var entry in entries)
        {
            output.Append(entry.Instance.InstanceId).Append(' ').Append(entry.Schema.Properties.Length).Append(' ');
            foreach (var property in entry.Schema.Properties)
            {
                uint state = 0; string text;
                try
                {
                    var value = CaptureProperty(entry, property);
                    if (value.ValueKind == JsonValueKind.Null) { state = 1; text = "Nulo"; }
                    else text = ScriptFieldSnapshot.Value(value, property.ValueType);
                }
                catch (Exception error) { state = 2; text = "Falha de leitura: " + error.GetBaseException().Message; }
                if (text.Length > 256 * 1024 || text.Contains('\0')) { state = 2; text = "Valor não representável no Inspector"; }
                output.Append(ScriptFieldSnapshot.Quote(property.Id)).Append(' ').Append(ScriptFieldSnapshot.Quote(property.ValueType))
                    .Append(' ').Append(state).Append(' ').Append(ScriptFieldSnapshot.Quote(text)).Append(' ');
            }
        }
        return output.ToString();
    }
    private Dictionary<string, JsonElement> CaptureProperties(Entry entry)
    {
        var result = new Dictionary<string, JsonElement>();
        foreach (var property in entry.Schema.Properties)
        {
            result.Add(property.Id, CaptureProperty(entry, property));
        }
        return result;
    }
    private static JsonElement RemapValue(JsonElement value, string kind, IReadOnlyDictionary<ulong, ulong> mapping)
    {
        if (value.ValueKind == JsonValueKind.Null) return value;
        if (kind.StartsWith("array:", StringComparison.Ordinal))
            return JsonSerializer.SerializeToElement(value.EnumerateArray().Select(v => RemapValue(v, kind[6..], mapping)).ToArray());
        if (kind == "object" || kind.StartsWith("component:", StringComparison.Ordinal))
        {
            var source = value.GetProperty("ObjectId").GetUInt64();
            var target = mapping.TryGetValue(source, out var copy) ? copy : source;
            return kind == "object" ? JsonSerializer.SerializeToElement(new ObjectReference(target)) :
                JsonSerializer.SerializeToElement(new { ObjectId = target, InstanceId = value.GetProperty("InstanceId").GetUInt64() });
        }
        return value;
    }
    public GameObject Instantiate(GameObject source, GameObject parent)
    {
        if (!Running || _stopping || _scene is null) throw new WorldException(WorldStatus.NotRunning, "instanciar");
        if (!source.BelongsTo(_scene) || !parent.BelongsTo(_scene)) throw new WorldException(WorldStatus.ForeignWorld, "instanciar");
        if (source.Parent is null) throw new WorldException(WorldStatus.InvalidArgument, "instanciar raiz da cena");
        if (_dispatchDepth >= 32) throw new WorldException(WorldStatus.LimitReached, "instanciar");
        var ids = new HashSet<ulong>(); var pending = new Stack<GameObject>(); pending.Push(source);
        while (pending.TryPop(out var owner)) { ids.Add(owner.ObjectId); owner.PushAliveChildren(pending); }
        var originals = _entries.Where(e => Alive(e) && ids.Contains(e.Instance.ObjectId)).ToArray();
        if (_entries.Count + originals.Length > 4096) throw new WorldException(WorldStatus.LimitReached, "instanciar scripts");
        // Captura serializada e construtores antes de criar qualquer objeto. Não
        // copia caches privados, nem compartilha listas/curvas mutáveis da fonte.
        var snapshots = originals.Select(e => (Source: e, Values: CaptureProperties(e),
            Copy: (Behavior)Activator.CreateInstance(e.Instance.GetType())!)).ToArray();
        var mapping = _scene.Instantiate(source.ObjectId, parent.ObjectId);
        var root = mapping[source.ObjectId];
        var prepared = new List<Entry>();
        try
        {
            foreach (var snapshot in snapshots)
            {
                var original = snapshot.Source;
                var owner = GameObject.Resolve(_scene, mapping[original.Instance.ObjectId]);
                var instance = snapshot.Copy;
                instance.Enabled = original.Instance.Enabled;
                instance.Attach(_scene, owner.ObjectId, original.Instance.InstanceId, this, _bindComponentState);
                var entry = new Entry(instance, original.Schema, owner); prepared.Add(entry);
                var values = original.Schema.Properties.ToDictionary(p => p.Id,
                    p => RemapValue(snapshot.Values[p.Id], p.ValueType, mapping));
                ApplyProperties(instance, original.Schema, values, _scene);
            }
            if (!_scene.FinishInstantiation(root, true)) throw new WorldException(_scene.LastStatus, "publicar cópia");
        }
        catch
        {
            foreach (var entry in prepared) entry.Instance.Detach();
            if (!_scene.FinishInstantiation(root, false)) throw new InvalidOperationException("Falha ao reverter instanciação incompleta.");
            throw;
        }
        _entries.AddRange(prepared);
        var result = GameObject.Resolve(_scene, root);
        ++_dispatchDepth;
        try { foreach (var entry in prepared) EnsureActivated(entry); }
        finally { if (--_dispatchDepth == 0) SweepRemoved(); }
        return result;
    }

    public GameObject InstantiatePrefab(GameObject parent, AssetGuid asset)
    {
        if (!Running || _stopping || _scene is null) throw new WorldException(WorldStatus.NotRunning, "instanciar prefab");
        if (!parent.BelongsTo(_scene)) throw new WorldException(WorldStatus.ForeignWorld, "instanciar prefab");
        if (_dispatchDepth >= 32) throw new WorldException(WorldStatus.LimitReached, "instanciar prefab");
        var root = _scene.InstantiatePrefab(parent.ObjectId, asset);
        if (root == 0) throw new WorldException(_scene.LastStatus, "instanciar prefab");
        var prepared = new List<Entry>();
        try
        {
            var attachments = JsonSerializer.Deserialize<BehaviorAttachment[]>(_scene.InstantiationAttachments(root))
                ?? throw new InvalidDataException("Descrição de scripts do prefab ausente.");
            if (_entries.Count + attachments.Length > 4096) throw new WorldException(WorldStatus.LimitReached, "instanciar scripts");
            PrepareAttachments(_scene, attachments, prepared);
            if (!_scene.FinishInstantiation(root, true)) throw new WorldException(_scene.LastStatus, "publicar prefab");
        }
        catch
        {
            foreach (var entry in prepared) entry.Instance.Detach();
            if (!_scene.FinishInstantiation(root, false)) throw new InvalidOperationException("Falha ao reverter instanciação de prefab.");
            throw;
        }
        _entries.AddRange(prepared);
        var result = GameObject.Resolve(_scene, root);
        ++_dispatchDepth;
        try { foreach (var entry in prepared) EnsureActivated(entry); }
        finally { if (--_dispatchDepth == 0) SweepRemoved(); }
        return result;
    }

    private void PrepareAttachments(ISceneAccess scene, IEnumerable<BehaviorAttachment> attachments, List<Entry> prepared)
    {
        var identities = new HashSet<(ulong, ulong)>();
        foreach (var attachment in attachments)
        {
            if (attachment.InstanceId == 0 || !scene.Exists(attachment.ObjectId) ||
                !identities.Add((attachment.ObjectId, attachment.InstanceId)))
                throw new InvalidOperationException("Invalid or duplicate behavior instance identity.");
            var pair = _types.SingleOrDefault(t => t.Value.Id == attachment.TypeId);
            if (pair.Key is null) throw new InvalidOperationException("Unavailable behavior type: " + attachment.TypeId);
            var schema = pair.Value;
            var instance = (Behavior?)Activator.CreateInstance(pair.Key)
                ?? throw new InvalidOperationException("Behavior construction failed: " + schema.Name);
            instance.Enabled = attachment.Enabled;
            instance.Attach(scene, attachment.ObjectId, attachment.InstanceId, this, _bindComponentState);
            prepared.Add(new(instance, schema, GameObject.Resolve(scene, attachment.ObjectId)));
            if (attachment.PropertyTypes is { } authoredTypes)
                foreach (var field in schema.Properties)
                    if (authoredTypes.TryGetValue(field.Id, out var kind) && kind != field.ValueType)
                        throw new InvalidDataException("Property type changed; migrate its authored value: " + schema.Name + "." + field.Id);
            ApplyProperties(instance, schema, attachment.Properties, scene);
        }
    }

    public object? FindBehavior(ulong objectId, Type contract)
    {
        ArgumentNullException.ThrowIfNull(contract);
        foreach (var entry in _entries)
            if (Alive(entry) && entry.Instance.ObjectId == objectId && contract.IsInstanceOfType(entry.Instance))
                return entry.Instance;
        return null;
    }

    public IEnumerable<object> FindBehaviors(ulong objectId, Type contract)
    {
        ArgumentNullException.ThrowIfNull(contract);
        return _entries.Where(entry => Alive(entry) && entry.Instance.ObjectId == objectId && contract.IsInstanceOfType(entry.Instance))
            .Select(entry => (object)entry.Instance).ToArray();
    }

    private readonly Dictionary<(Type, string, bool, Type?), MethodInfo?> _messages = [];
    private MethodInfo? MessageMethod(Type type, string method, object? payload, bool hasPayload)
    {
        var key = (type, method, hasPayload, payload?.GetType());
        if (_messages.TryGetValue(key, out var cached)) return cached;
        if (_messages.Count >= 4096) _messages.Clear();
        for (var current = type; current is not null && current != typeof(Behavior); current = current.BaseType)
        {
            var candidates = current.GetMethods(BindingFlags.DeclaredOnly | BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic)
                .Where(m => m.Name == method && !m.IsGenericMethod && m.ReturnType == typeof(void))
                .Where(m =>
                {
                    var args = m.GetParameters();
                    if (!hasPayload) return args.Length == 0;
                    return args.Length == 1 && !args[0].ParameterType.IsByRef &&
                        (payload is null ? !args[0].ParameterType.IsValueType || Nullable.GetUnderlyingType(args[0].ParameterType) is not null
                                         : args[0].ParameterType.IsInstanceOfType(payload));
                }).ToArray();
            if (candidates.Length > 1) throw new AmbiguousMatchException("Mensagem ambígua: " + type.FullName + "." + method);
            if (candidates.Length == 1) return _messages[key] = candidates[0];
        }
        return _messages[key] = null;
    }
    public int Message(GameObject target, string method, object? payload, bool hasPayload, MessageRoute route, bool requireReceiver)
    {
        if (!Running || _scene is null || _stopping) throw new WorldException(WorldStatus.NotRunning, "enviar mensagem");
        if (!target.BelongsTo(_scene)) throw new WorldException(WorldStatus.ForeignWorld, "enviar mensagem");
        if (string.IsNullOrWhiteSpace(method) || method.Length > 256 || !Enum.IsDefined(route)) throw new ArgumentException("Mensagem inválida");
        if (_dispatchDepth >= 32) throw new WorldException(WorldStatus.LimitReached, "recursão de mensagens");
        var objects = new List<GameObject>();
        var pending = new Stack<GameObject>(); pending.Push(target);
        while (pending.TryPop(out var current))
        {
            if (!current.IsAlive) continue;
            if (current.ActiveInHierarchy) objects.Add(current);
            if (route == MessageRoute.Ancestors && current.Parent is { } parent) pending.Push(parent);
            else if (route == MessageRoute.Descendants && current.ActiveInHierarchy)
                current.PushAliveChildren(pending);
        }
        // Captura antes da primeira chamada: criar outro receptor não muda a mensagem corrente.
        var receivers = new List<(Entry Entry, MethodInfo Method)>();
        var byOwner = _entries.Where(Alive).ToLookup(e => e.Instance.ObjectId);
        foreach (var owner in objects)
            foreach (var entry in byOwner[owner.ObjectId])
                if (Alive(entry) && !entry.Failed && entry.Instance.ObjectId == owner.ObjectId &&
                    MessageMethod(entry.Instance.GetType(), method, payload, hasPayload) is { } handler)
                    receivers.Add((entry, handler));
        var delivered = 0;
        ++_dispatchDepth;
        try
        {
            foreach (var receiver in receivers)
            {
                var entry = receiver.Entry;
                EnsureAwake(entry);
                if (entry.Failed || !ObjectActive(entry)) continue;
                ++delivered;
                Invoke(entry, "Message:" + method, b =>
                {
                    try { receiver.Method.Invoke(b, hasPayload ? [payload] : null); }
                    catch (TargetInvocationException error) when (error.InnerException is not null)
                    { System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(error.InnerException).Throw(); }
                });
            }
        }
        finally { if (--_dispatchDepth == 0) SweepRemoved(); }
        if (delivered == 0 && requireReceiver) throw new MissingMethodException("Nenhum receptor ativo e compatível para: " + method);
        return delivered;
    }

    private static bool ObjectActive(Entry entry) => Alive(entry) && entry.Owner.ActiveInHierarchy;

    private void EnsureAwake(Entry entry)
    {
        if (entry.Awoken || entry.Failed || !ObjectActive(entry)) return;
        entry.Awoken = true;
        Invoke(entry, "Awake", static b => b.Awake());
    }

    private bool MayRun(Entry entry) => !entry.Failed && ObjectActive(entry) && entry.Instance.Enabled;

    private void Deactivate(Entry entry)
    {
        var wasActive = entry.Active;
        entry.Active = false;
        if (wasActive && !entry.Failed) Invoke(entry, "Disable", static b => b.Disable());
    }

    private bool EnsureActivated(Entry entry)
    {
        EnsureAwake(entry);
        if (!MayRun(entry))
        {
            if (!ObjectActive(entry)) _coroutines?.CancelOwner(entry.Instance);
            Deactivate(entry);
            return false;
        }
        if (!entry.Active)
        {
            entry.Active = true;
            Invoke(entry, "Enable", static b => b.Enable());
            if (!MayRun(entry)) { Deactivate(entry); return false; }
        }
        return true;
    }
    private bool EnsureStarted(Entry entry)
    {
        if (!EnsureActivated(entry)) return false;
        if (!entry.Started) { entry.Started = true; Invoke(entry, "Start", static b => b.Start()); }
        if (!MayRun(entry)) { Deactivate(entry); return false; }
        return true;
    }
    private void Invoke(Entry entry, string phase, Action<Behavior> action)
    {
        try { action(entry.Instance); }
        catch (Exception error)
        {
            entry.Failed = true;
            entry.Instance.BlockCoroutines();
            _coroutines?.CancelOwner(entry.Instance);
            // A falha pode ter ocorrido depois de Destroy. Não deixe uma
            // segunda exceção, ao desligar um handle vencido, escapar do isolamento.
            try { if (entry.Owner.IsAlive && entry.Instance.AttachedComponentAlive) entry.Instance.Enabled = false; }
            catch (WorldException) { }
            if (_failures.Count < 1024) _failures.Add(new(entry.Instance.ObjectId, entry.Instance.InstanceId,
                phase, error.ToString()));
        }
    }
    /// <summary>
    /// Inspector em Play (Unity: mudar um campo do script com o jogo rodando).
    /// Os campos são convertidos como no Start; Enabled segue pela mesma
    /// transição que o próprio script usaria, então Enable/Disable acontecem no
    /// próximo quadro. Falso quando a instância não existe nesta sessão.
    /// </summary>
    public bool Edit(ulong objectId, ulong instanceId, BehaviorEdit edit)
    {
        if (!Running) return false;
        var entry = _entries.FirstOrDefault(e => e.Instance.ObjectId == objectId && e.Instance.InstanceId == instanceId);
        if (entry is null || !Alive(entry)) return false;
        if (edit.Properties is { Count: > 0 } properties) ApplyProperties(entry.Instance, entry.Schema, properties, _scene!);
        // Uma instância que falhou continua desligada: religá-la pelo Inspector
        // repetiria a exceção a cada quadro sem o usuário ter mudado o código.
        if (!entry.Failed) entry.Instance.Enabled = edit.Enabled;
        return true;
    }
    private static MemberInfo ResolveMember(Type type, ScriptPropertySchema property)
    {
        // The compiler walks declared members through the hierarchy. Reflection
        // must do the same: GetField on a derived type omits private base fields.
        // Stable IDs also avoid selecting an unrelated member that hides a name.
        for (var current = type; current is not null && current != typeof(Behavior); current = current.BaseType)
            foreach (var member in current.GetMember(property.Name, BindingFlags.DeclaredOnly |
                BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic))
                if (member is FieldInfo or PropertyInfo && member.GetCustomAttribute<PropertyIdAttribute>()?.Id == property.Id)
                    return member;
        throw new InvalidOperationException("Compiled schema no longer matches its member: " + property.Id);
    }
    private static void ApplyProperties(Behavior behavior, ScriptTypeSchema schema,
        IReadOnlyDictionary<string, JsonElement> values, ISceneAccess scene)
    {
        var type = behavior.GetType();
        var options = new JsonSerializerOptions { IncludeFields = true };
        foreach (var property in schema.Properties)
        {
            if (!values.TryGetValue(property.Id, out var value)) continue;
            // Não públicos só com [SerializeField]; o compilador já recusou o resto.
            var resolved = ResolveMember(type, property);
            var field = resolved as FieldInfo;
            var member = resolved as PropertyInfo;
            var memberType = field?.FieldType ?? member?.PropertyType
                ?? throw new InvalidOperationException("Compiled schema no longer matches its member.");
            var converted = value.ValueKind == JsonValueKind.Null ?
                (!memberType.IsValueType || Nullable.GetUnderlyingType(memberType) is not null ? null : throw new InvalidDataException("Null em campo de valor: " + property.Name)) :
                property.ValueType.StartsWith("component:", StringComparison.Ordinal)
                ? ComponentValue(scene, value, memberType, property.ValueType["component:".Length..])
                : property.ValueType.StartsWith("array:component:", StringComparison.Ordinal)
                    ? ComponentList(scene, value, memberType, property.ValueType["array:component:".Length..])
                    : JsonSerializer.Deserialize(value.GetRawText(), memberType, options);
            if (field is not null) field.SetValue(behavior, converted);
            else member!.SetValue(behavior, converted);
        }
        // Unknown properties remain in authoring storage; they are not silently
        // written into unrelated fields after a script renames its schema.
    }
    /// <summary>
    /// Campo de componente: {"ObjectId","InstanceId"} vira a fachada tipada. A
    /// instância removida depois da autoria deixa o campo vazio (o "Missing" da
    /// Unity) em vez de apontar para outro componente do mesmo objeto.
    /// </summary>
    /// <summary>Lista de componentes: cada elemento resolvido como o campo único.</summary>
    private static object ComponentList(ISceneAccess scene, JsonElement value, Type memberType, string typeId)
    {
        var element = memberType.IsArray ? memberType.GetElementType()! : memberType.GetGenericArguments()[0];
        var items = value.EnumerateArray().Select(item => ComponentValue(scene, item, element, typeId)).ToArray();
        var array = Array.CreateInstance(element, items.Length);
        for (var i = 0; i < items.Length; ++i) array.SetValue(items[i], i);
        return memberType.IsArray ? array : Activator.CreateInstance(memberType, array)!;
    }
    private static object? ComponentValue(ISceneAccess scene, JsonElement value, Type memberType, string typeId)
    {
        var facade = Nullable.GetUnderlyingType(memberType) ?? memberType;
        object? empty = memberType.IsValueType ? Activator.CreateInstance(memberType) : null;
        var objectId = value.GetProperty("ObjectId").GetUInt64();
        var instanceId = value.GetProperty("InstanceId").GetUInt64();
        if (objectId == 0 || instanceId == 0 || !scene.Exists(objectId)) return empty;
        Component? found = null;
        foreach (var component in GameObject.Resolve(scene, objectId).Components())
            if (component.InstanceId == instanceId && component.TypeId == typeId) { found = component; break; }
        if (found is not { } resolved) return empty;
        return facade.GetMethod("Wrap", BindingFlags.Public | BindingFlags.Static)?.Invoke(null, [resolved])
            ?? throw new InvalidOperationException("Component facade has no Wrap: " + facade.FullName);
    }
    public void Dispose()
    {
        if (_context is null || _stopping) return;
        _stopping = true;
        _coroutines?.Shutdown();
        ++_dispatchDepth;
        try { for (var i = _entries.Count - 1; i >= 0; --i) Retire(_entries[i]); _saveStore?.Flush(); }
        finally
        {
            foreach (var subscription in _subscriptions) subscription.Active = false;
            _subscriptions.Clear();
            --_dispatchDepth; _time?.End(); _time = null; _entries.Clear(); _types.Clear(); _messages.Clear(); _scene = null; _saveStore = null; _started = false;
            var context = _context; _context = null; _coroutines = null; context.Unload(); _stopping = false;
        }
    }
}
