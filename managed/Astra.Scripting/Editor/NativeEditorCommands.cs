using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.Text;
using Astra.Compilation;

namespace Astra.Editor;

/// <summary>Marks a public static void method taking EditorContext. Runs explicitly in Edit mode, never automatically in Play.</summary>
[AttributeUsage(AttributeTargets.Method, AllowMultiple = false, Inherited = false)]
public sealed class EditorCommandAttribute(string label) : Attribute
{
    public string Label { get; } = label;
}
public sealed record EditorCommandInfo(string Id, string Label);

/// <summary>Loads only the applied code generation. A failed or unpublished build cannot change executable editor tools.</summary>
public static unsafe class NativeEditorCommands
{
    private static readonly object Gate = new();
    private static readonly UTF8Encoding Utf8 = new(false, true);
    private static byte[] _report = [];
    private sealed class CommandAssembly : IDisposable
    {
        private sealed class LoadContext() : AssemblyLoadContext("Astra.EditorCommands", isCollectible: true)
        {
            protected override Assembly? Load(AssemblyName name) => name.Name == typeof(EditorContext).Assembly.GetName().Name
                ? typeof(EditorContext).Assembly : null;
        }
        private readonly LoadContext _load = new();
        public List<(EditorCommandInfo Info, MethodInfo Method)> Commands { get; } = [];
        public CommandAssembly(string root)
        {
            try
            {
                var project = NativeCompiler.LoadApplied(root);
                using var dll = new MemoryStream(project.Assembly, writable: false);
                using var symbols = new MemoryStream(project.Symbols, writable: false);
                var assembly = _load.LoadFromStream(dll, symbols);
                foreach (var type in assembly.GetTypes())
                    foreach (var method in type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static | BindingFlags.Instance | BindingFlags.DeclaredOnly))
                    {
                        var attribute = method.GetCustomAttribute<EditorCommandAttribute>();
                        if (attribute is null) continue;
                        var id = type.FullName + "::" + method.Name;
                        var parameters = method.GetParameters();
                        if (!method.IsPublic || !method.IsStatic || method.ContainsGenericParameters || type.ContainsGenericParameters ||
                            method.ReturnType != typeof(void) || parameters.Length != 1 || parameters[0].ParameterType != typeof(EditorContext) ||
                            method.GetCustomAttribute<AsyncStateMachineAttribute>() is not null)
                            throw new InvalidDataException("Invalid editor command signature: " + id + ". Use public static void Method(EditorContext context), without async.");
                        var label = attribute.Label;
                        if (string.IsNullOrWhiteSpace(label) || Utf8.GetByteCount(label) > 256 || label.Any(char.IsControl) || Utf8.GetByteCount(id) > 4096)
                            throw new InvalidDataException("Invalid editor command label/identity: " + id);
                        if (Commands.Any(c => c.Info.Id == id)) throw new InvalidDataException("Ambiguous editor command: " + id);
                        if (Commands.Count == 256) throw new InvalidDataException("A project may expose at most 256 editor commands.");
                        Commands.Add((new(id, label), method));
                    }
                Commands.Sort((a, b) => StringComparer.Ordinal.Compare(a.Info.Id, b.Info.Id));
            }
            catch { Dispose(); throw; }
        }
        public void Dispose() { Commands.Clear(); _load.Unload(); }
    }
    public static IReadOnlyList<EditorCommandInfo> Discover(string root)
    {
        using var assembly = new CommandAssembly(root);
        return Array.AsReadOnly(assembly.Commands.Select(c => c.Info).ToArray());
    }
    private static string Argument(byte* pointer, int length, int maximum)
    {
        if (pointer == null || length <= 0 || length > maximum) throw new ArgumentException("Invalid editor command argument.");
        var value = Utf8.GetString(new ReadOnlySpan<byte>(pointer, length));
        if (value.Contains('\0')) throw new ArgumentException("Editor command argument contains NUL.");
        return value;
    }
    private static string Quote(string text) => "\"" + text.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"";
    [UnmanagedCallersOnly]
    public static int Catalog(byte* root, int length)
    {
        lock (Gate)
        {
            try
            {
                var commands = Discover(Argument(root, length, 32768));
                var text = new StringBuilder("ASTRA_EDITOR_COMMANDS 1 ").Append(commands.Count).Append(' ');
                foreach (var command in commands) text.Append(Quote(command.Id)).Append(' ').Append(Quote(command.Label)).Append(' ');
                _report = Utf8.GetBytes(text.ToString()); return 0;
            }
            catch (Exception error) { Failure(error); return 1; }
        }
    }
    [UnmanagedCallersOnly]
    public static int Run(byte* root, int rootLength, byte* id, int idLength, void* access)
    {
        lock (Gate)
        {
            try
            {
                using var assembly = new CommandAssembly(Argument(root, rootLength, 32768));
                var identity = Argument(id, idLength, 4096);
                var command = assembly.Commands.Find(c => c.Info.Id == identity);
                if (command.Method is null) throw new InvalidOperationException("Editor command is absent from the applied generation: " + identity);
                using var context = new EditorContext((AnimationAuthorAccess*)access);
                command.Method.Invoke(null, [context]);
                _report = Utf8.GetBytes("Ferramenta concluída: " + command.Info.Label); return 0;
            }
            catch (Exception error)
            {
                if (error is TargetInvocationException { InnerException: { } cause }) error = cause;
                Failure(error); return 1;
            }
        }
    }
    private static void Failure(Exception error)
    {
        var message = error.Message;
        _report = Encoding.UTF8.GetBytes(message.Length > 16384 ? message[..16384] : message);
    }
    [UnmanagedCallersOnly]
    public static int CopyReport(byte* destination, int capacity)
    {
        lock (Gate)
        {
            if (destination != null && capacity >= _report.Length) _report.CopyTo(new Span<byte>(destination, capacity));
            return _report.Length;
        }
    }
}
