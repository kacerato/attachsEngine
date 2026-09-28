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
public sealed class BehaviorWorld : IDisposable, IBehaviorRegistry
{
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
    }
    /// <summary>Eventos do aplicativo repassados aos comportamentos.</summary>
    public enum ApplicationEvent : uint { Pause = 0, Focus = 1 }
    private ProjectLoadContext? _context;
    private ISceneAccess? _scene;
    private readonly List<Entry> _entries = [];
    private readonly List<BehaviorFailure> _failures = [];
    private bool _started;
    public IReadOnlyList<BehaviorFailure> Failures => _failures;
    public bool Running => _context is not null && _started;

    public void Start(CompiledProject project, ISceneAccess scene, IEnumerable<BehaviorAttachment> attachments,
        bool bindComponentState = false)
    {
        if (_context is not null) throw new InvalidOperationException("A script world is already active.");
        var context = new ProjectLoadContext();
        var prepared = new List<Entry>();
        try
        {
            using var dll = new MemoryStream(project.Assembly, writable: false);
            using var pdb = new MemoryStream(project.Symbols, writable: false);
            var assembly = context.LoadFromStream(dll, pdb);
            var identities = new HashSet<(ulong, ulong)>();
            foreach (var attachment in attachments)
            {
                if (attachment.InstanceId == 0 || !scene.Exists(attachment.ObjectId) ||
                    !identities.Add((attachment.ObjectId, attachment.InstanceId)))
                    throw new InvalidOperationException("Invalid or duplicate behavior instance identity.");
                var schema = project.Types.SingleOrDefault(t => t.Id == attachment.TypeId)
                    ?? throw new InvalidOperationException("Unavailable behavior type: " + attachment.TypeId);
                var type = assembly.GetType(schema.Name, throwOnError: true)!;
                var instance = (Behavior?)Activator.CreateInstance(type)
                    ?? throw new InvalidOperationException("Behavior construction failed: " + schema.Name);
                instance.Enabled = attachment.Enabled;
                instance.Attach(scene, attachment.ObjectId, attachment.InstanceId, this, bindComponentState);
                prepared.Add(new(instance, schema, GameObject.Resolve(scene, attachment.ObjectId)));
                if (attachment.PropertyTypes is { } authoredTypes)
                    foreach (var field in schema.Properties)
                        if (authoredTypes.TryGetValue(field.Id, out var kind) && kind != field.ValueType)
                            throw new InvalidDataException("Property type changed; migrate its authored value: " + schema.Name + "." + field.Id);
                ApplyProperties(instance, schema, attachment.Properties, scene);
            }
            _context = context; _scene = scene; _entries.AddRange(prepared); _failures.Clear(); _started = true;
            // Todas as instâncias existem antes dos callbacks. Objetos inativos
            // aguardam sua primeira ativação; Enabled=false não adia o Awake.
            foreach (var entry in _entries) EnsureAwake(entry);
            foreach (var entry in _entries) EnsureStarted(entry);
        }
        catch
        {
            foreach (var entry in prepared) entry.Instance.Detach();
            _entries.Clear(); _context = null; _scene = null; _started = false; context.Unload(); throw;
        }
    }
    public void Update(float deltaTime)
    {
        if (!Running || !float.IsFinite(deltaTime) || deltaTime < 0) return;
        foreach (var entry in _entries) if (EnsureStarted(entry))
            Invoke(entry, "Update", behavior => behavior.Update(deltaTime));
    }
    public void LateUpdate(float deltaTime)
    {
        if (!Running || !float.IsFinite(deltaTime) || deltaTime < 0) return;
        foreach (var entry in _entries) if (EnsureStarted(entry))
            Invoke(entry, "LateUpdate", behavior => behavior.LateUpdate(deltaTime));
    }
    /// <summary>Pausa ou foco do aplicativo, entregue a toda instância ativa.</summary>
    public void Application(ApplicationEvent kind, bool value)
    {
        if (!Running) return;
        foreach (var entry in _entries) if (EnsureStarted(entry))
            Invoke(entry, kind == ApplicationEvent.Pause ? "ApplicationPause" : "ApplicationFocus", behavior =>
            {
                if (kind == ApplicationEvent.Pause) behavior.ApplicationPause(value);
                else behavior.ApplicationFocus(value);
            });
    }
    public void FixedUpdate(float fixedDeltaTime)
    {
        if (!Running || !float.IsFinite(fixedDeltaTime) || fixedDeltaTime <= 0) return;
        foreach (var entry in _entries) if (EnsureStarted(entry))
            Invoke(entry, "FixedUpdate", behavior => behavior.FixedUpdate(fixedDeltaTime));
    }
    public void Timer(ulong objectId, ulong instanceId, uint count)
    {
        if (!Running || instanceId == 0 || count == 0 || _scene is null || !_scene.Exists(objectId)) return;
        foreach (var entry in _entries)
            if (entry.Instance.ObjectId == objectId && EnsureStarted(entry))
                Invoke(entry, "TimerElapsed", behavior => behavior.TimerElapsed(instanceId, count));
    }
    public void Trigger(ulong sensor, ulong other, uint phase)
    {
        if (!Running || phase > 2) return;
        foreach (var entry in _entries)
            if (entry.Instance.ObjectId == sensor && EnsureStarted(entry))
                Invoke(entry, phase == 0 ? "TriggerEnter" : phase == 1 ? "TriggerStay" : "TriggerExit", behavior =>
                {
                    var reference = ObjectReference.Capture(_scene!, other);
                    if (phase == 0) behavior.TriggerEnter(reference);
                    else if (phase == 1) behavior.TriggerStay(reference);
                    else behavior.TriggerExit(reference);
                });
    }
    /// <summary>
    /// Entrega um contato sólido aos comportamentos de <paramref name="self"/>.
    /// O par chega duas vezes, uma por objeto, para que nenhum dos dois precise
    /// saber qual corpo o backend listou primeiro.
    /// </summary>
    public void Contact(ulong self, ulong other, uint phase, Vector3? normal)
    {
        if (!Running || phase > 2) return;
        var collision = new Collision(ObjectReference.Capture(_scene!, other), normal);
        foreach (var entry in _entries)
            if (entry.Instance.ObjectId == self && EnsureStarted(entry))
                Invoke(entry, phase == 0 ? "CollisionEnter" : phase == 1 ? "CollisionStay" : "CollisionExit", behavior =>
                {
                    if (phase == 0) behavior.CollisionEnter(collision);
                    else if (phase == 1) behavior.CollisionStay(collision);
                    else behavior.CollisionExit(collision);
                });
    }
    public object? FindBehavior(ulong objectId, Type contract)
    {
        ArgumentNullException.ThrowIfNull(contract);
        foreach (var entry in _entries)
            if (entry.Instance.ObjectId == objectId && contract.IsInstanceOfType(entry.Instance))
                return entry.Instance;
        return null;
    }

    public IEnumerable<object> FindBehaviors(ulong objectId, Type contract)
    {
        ArgumentNullException.ThrowIfNull(contract);
        foreach (var entry in _entries)
            if (entry.Instance.ObjectId == objectId && contract.IsInstanceOfType(entry.Instance))
                yield return entry.Instance;
    }

    private static bool ObjectActive(Entry entry) => entry.Owner.IsAlive && entry.Instance.AttachedComponentAlive && entry.Owner.ActiveInHierarchy;

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

    private bool EnsureStarted(Entry entry)
    {
        EnsureAwake(entry);
        if (!MayRun(entry))
        {
            Deactivate(entry);
            return false;
        }
        if (!entry.Active)
        {
            entry.Active = true;
            Invoke(entry, "Enable", static b => b.Enable());
            if (!MayRun(entry)) { Deactivate(entry); return false; }
        }
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
        if (entry is null) return false;
        if (edit.Properties is { Count: > 0 } properties) ApplyProperties(entry.Instance, entry.Schema, properties, _scene!);
        // Uma instância que falhou continua desligada: religá-la pelo Inspector
        // repetiria a exceção a cada quadro sem o usuário ter mudado o código.
        if (!entry.Failed) entry.Instance.Enabled = edit.Enabled;
        return true;
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
            var field = type.GetField(property.Name, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
            var member = type.GetProperty(property.Name, BindingFlags.Instance | BindingFlags.Public);
            var memberType = field?.FieldType ?? member?.PropertyType
                ?? throw new InvalidOperationException("Compiled schema no longer matches its member.");
            var converted = property.ValueType.StartsWith("component:", StringComparison.Ordinal)
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
        if (_context is null) return;
        for (var i = _entries.Count - 1; i >= 0; --i)
        {
            if (_entries[i].Active && !_entries[i].Failed) Invoke(_entries[i], "Disable", static behavior => behavior.Disable());
            if (_entries[i].Started) Invoke(_entries[i], "Stop", static behavior => behavior.Stop());
            _entries[i].Instance.Detach();
        }
        _entries.Clear(); _scene = null; _started = false;
        var context = _context; _context = null; context.Unload();
    }
}
