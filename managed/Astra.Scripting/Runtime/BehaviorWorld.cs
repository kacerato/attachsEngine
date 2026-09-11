using System.Numerics;
using System.Reflection;
using System.Runtime.Loader;
using System.Text.Json;
using Astra.Compilation;

namespace Astra.Runtime;

public sealed record BehaviorAttachment(ulong ObjectId, ulong InstanceId, string TypeId,
    bool Enabled, IReadOnlyDictionary<string, JsonElement> Properties, IReadOnlyDictionary<string, string>? PropertyTypes = null);
public sealed record BehaviorFailure(ulong ObjectId, ulong InstanceId, string Phase, string Message);

/// <summary>Owns one isolated script assembly and its instances for a Play session.</summary>
public sealed class BehaviorWorld : IDisposable, IBehaviorRegistry
{
    private sealed class ProjectLoadContext() : AssemblyLoadContext("Astra.Project", isCollectible: true)
    {
        protected override Assembly? Load(AssemblyName name) =>
            name.Name == typeof(Behavior).Assembly.GetName().Name ? typeof(Behavior).Assembly : null;
    }
    private sealed class Entry(Behavior instance, ScriptTypeSchema schema)
    {
        public Behavior Instance { get; } = instance;
        public ScriptTypeSchema Schema { get; } = schema;
        public bool Started { get; set; }
    }
    private ProjectLoadContext? _context;
    private readonly List<Entry> _entries = [];
    private readonly List<BehaviorFailure> _failures = [];
    private bool _started;
    public IReadOnlyList<BehaviorFailure> Failures => _failures;
    public bool Running => _context is not null && _started;

    public void Start(CompiledProject project, ISceneAccess scene, IEnumerable<BehaviorAttachment> attachments)
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
                instance.Attach(scene, attachment.ObjectId, attachment.InstanceId, this);
                instance.Enabled = attachment.Enabled;
                prepared.Add(new(instance, schema));
                if (attachment.PropertyTypes is { } authoredTypes)
                    foreach (var field in schema.Properties)
                        if (authoredTypes.TryGetValue(field.Id, out var kind) && kind != field.ValueType)
                            throw new InvalidDataException("Property type changed; migrate its authored value: " + schema.Name + "." + field.Id);
                ApplyProperties(instance, schema, attachment.Properties);
            }
            _context = context; _entries.AddRange(prepared); _failures.Clear(); _started = true;
            foreach (var entry in _entries) EnsureStarted(entry);
        }
        catch
        {
            foreach (var entry in prepared) entry.Instance.Detach();
            _entries.Clear(); _context = null; _started = false; context.Unload(); throw;
        }
    }
    public void Update(float deltaTime)
    {
        if (!Running || !float.IsFinite(deltaTime) || deltaTime < 0) return;
        foreach (var entry in _entries) if (EnsureStarted(entry))
            Invoke(entry, "Update", behavior => behavior.Update(deltaTime));
    }
    public void FixedUpdate(float fixedDeltaTime)
    {
        if (!Running || !float.IsFinite(fixedDeltaTime) || fixedDeltaTime <= 0) return;
        foreach (var entry in _entries) if (EnsureStarted(entry))
            Invoke(entry, "FixedUpdate", behavior => behavior.FixedUpdate(fixedDeltaTime));
    }
    public void Trigger(ulong sensor, ulong other, uint phase)
    {
        if (!Running || phase > 2) return;
        foreach (var entry in _entries)
            if (entry.Instance.ObjectId == sensor && EnsureStarted(entry))
                Invoke(entry, phase == 0 ? "TriggerEnter" : phase == 1 ? "TriggerStay" : "TriggerExit", behavior =>
                {
                    var reference = new ObjectReference(other);
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
        var collision = new Collision(new ObjectReference(other), normal);
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

    private bool EnsureStarted(Entry entry)
    {
        if (!entry.Instance.Enabled) return false;
        if (!entry.Started) { entry.Started = true; Invoke(entry, "Start", static b => b.Start()); }
        return entry.Instance.Enabled;
    }
    private void Invoke(Entry entry, string phase, Action<Behavior> action)
    {
        try { action(entry.Instance); }
        catch (Exception error)
        {
            entry.Instance.Enabled = false;
            if (_failures.Count < 1024) _failures.Add(new(entry.Instance.ObjectId, entry.Instance.InstanceId,
                phase, error.ToString()));
        }
    }
    private static void ApplyProperties(Behavior behavior, ScriptTypeSchema schema,
        IReadOnlyDictionary<string, JsonElement> values)
    {
        var type = behavior.GetType();
        var options = new JsonSerializerOptions { IncludeFields = true };
        foreach (var property in schema.Properties)
        {
            if (!values.TryGetValue(property.Id, out var value)) continue;
            var field = type.GetField(property.Name, BindingFlags.Instance | BindingFlags.Public);
            var member = type.GetProperty(property.Name, BindingFlags.Instance | BindingFlags.Public);
            var memberType = field?.FieldType ?? member?.PropertyType
                ?? throw new InvalidOperationException("Compiled schema no longer matches its member.");
            var converted = JsonSerializer.Deserialize(value.GetRawText(), memberType, options);
            if (field is not null) field.SetValue(behavior, converted);
            else member!.SetValue(behavior, converted);
        }
        // Unknown properties remain in authoring storage; they are not silently
        // written into unrelated fields after a script renames its schema.
    }
    public void Dispose()
    {
        if (_context is null) return;
        for (var i = _entries.Count - 1; i >= 0; --i)
        {
            if (_entries[i].Started) Invoke(_entries[i], "Stop", static behavior => behavior.Stop());
            _entries[i].Instance.Detach();
        }
        _entries.Clear(); _started = false;
        var context = _context; _context = null; context.Unload();
    }
}
