using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;

namespace Astra.Compilation;

/// <summary>Blittable UTF-8 bridge; exceptions never cross into the native editor.</summary>
public static class NativeCompiler
{
    private static readonly object Gate = new();
    private static byte[] _report = [];
    private static CompiledProject? _applied, _candidate;
    private static string _candidateRoot = "", _appliedRoot = "";
    public static CompiledProject? Applied { get { lock (Gate) return _applied; } }

    [UnmanagedCallersOnly]
    public static unsafe int Build(byte* root, int length)
    {
        lock (Gate)
        {
            try
            {
                if (root == null || length <= 0 || length > 32768) return -1;
                var directory = new UTF8Encoding(false, true).GetString(new ReadOnlySpan<byte>(root, length));
                _candidate = null;
                var result = new ProjectCompiler().Build(directory);
                if (result.Project is { } project)
                {
                    _candidate = project; _candidateRoot = directory;
                }
                _report = Encoding.UTF8.GetBytes(SerializeReport(result));
                return result.Success ? 0 : 1;
            }
            catch (Exception error)
            {
                _report = Encoding.UTF8.GetBytes(SerializeReport(new(null,
                    [new("", 1, 1, "ASTRA_HOST", error.Message, true)])));
                return 1;
            }
        }
    }
    [UnmanagedCallersOnly]
    public static int Commit()
    {
        lock (Gate)
        {
            if (_candidate is null) return 1;
            try { PublishArtifacts(_candidateRoot, _candidate); _appliedRoot = Path.GetFullPath(_candidateRoot); _applied = _candidate; _candidate = null; return 0; }
            catch (Exception) { return 1; }
        }
    }
    [UnmanagedCallersOnly]
    public static unsafe int CopyReport(byte* destination, int capacity)
    {
        lock (Gate)
        {
            if (destination == null || capacity < _report.Length) return _report.Length;
            _report.CopyTo(new Span<byte>(destination, capacity)); return _report.Length;
        }
    }
    private static string Quote(string text) => "\"" + text.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"";
    private static string SerializeReport(ScriptBuildResult result)
    {
        var output = new StringBuilder("ASTRA_CODE 1 ");
        output.Append(result.Success ? "1 " : "0 ").Append(result.Diagnostics.Length).Append(' ');
        foreach (var diagnostic in result.Diagnostics)
            output.Append(Quote(diagnostic.File)).Append(' ').Append(diagnostic.Line).Append(' ')
                .Append(diagnostic.Column).Append(' ').Append(diagnostic.Error ? "1 " : "0 ")
                .Append(Quote(diagnostic.Code)).Append(' ').Append(Quote(diagnostic.Message)).Append(' ');
        var types = result.Project?.Types ?? [];
        output.Append(types.Length).Append(' ');
        foreach (var type in types)
        {
            output.Append(Quote(type.Id)).Append(' ').Append(Quote(type.Name)).Append(' ')
                .Append(Quote(type.File)).Append(' ').Append(type.Properties.Length).Append(' ');
            foreach (var property in type.Properties)
                output.Append(Quote(property.Id)).Append(' ').Append(Quote(property.Name)).Append(' ')
                    .Append(Quote(property.ValueType)).Append(' ');
        }
        return output.ToString();
    }
    public static CompiledProject LoadApplied(string root)
    {
        lock (Gate)
        {
            root = Path.GetFullPath(root);
            if (_applied is not null && _appliedRoot == root) return _applied;
            var code = ArtifactDirectory(root, ".astra", "code");
            var pointer = new FileInfo(Path.Combine(code, "current"));
            if (!pointer.Exists || pointer.Length > 256 || (pointer.Attributes & FileAttributes.ReparsePoint) != 0)
                throw new InvalidDataException("Applied generation pointer unavailable.");
            var id = File.ReadAllText(pointer.FullName).Trim();
            if (id.Length != 64 || !id.All(Uri.IsHexDigit)) throw new InvalidDataException("Invalid applied code identity.");
            var directory = ArtifactDirectory(code, id);
            byte[] Read(string name, long limit)
            {
                var info = new FileInfo(Path.Combine(directory, name));
                if (!info.Exists || info.Length > limit || (info.Attributes & FileAttributes.ReparsePoint) != 0)
                    throw new InvalidDataException("Applied artifact unavailable: " + name);
                return File.ReadAllBytes(info.FullName);
            }
            var types = JsonSerializer.Deserialize<ScriptTypeSchema[]>(Read("schema.json", 4 * 1024 * 1024))
                ?? throw new InvalidDataException("Applied schema unavailable.");
            _applied = new(id, Read("Project.dll", 32 * 1024 * 1024), Read("Project.pdb", 32 * 1024 * 1024), types);
            _appliedRoot = root; return _applied;
        }
    }
    private static string ArtifactDirectory(string root, params string[] children)
    {
        var directory = Path.GetFullPath(root);
        foreach (var child in children)
        {
            directory = Path.Combine(directory, child);
            if (Directory.Exists(directory) && (File.GetAttributes(directory) & FileAttributes.ReparsePoint) != 0)
                throw new IOException("Artifact directory cannot be a symbolic link.");
            Directory.CreateDirectory(directory);
        }
        return directory;
    }
    private static void PublishArtifacts(string root, CompiledProject project)
    {
        var directory = ArtifactDirectory(root, ".astra", "code", project.Id);
        WriteAtomic(Path.Combine(directory, "Project.dll"), project.Assembly);
        WriteAtomic(Path.Combine(directory, "Project.pdb"), project.Symbols);
        WriteAtomic(Path.Combine(directory, "schema.json"), JsonSerializer.SerializeToUtf8Bytes(project.Types));
        // The pointer is published last. A failed build keeps the prior generation intact.
        WriteAtomic(Path.Combine(root, ".astra", "code", "current"), Encoding.UTF8.GetBytes(project.Id));
    }
    private static void WriteAtomic(string destination, byte[] content)
    {
        var temporary = destination + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            using (var file = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            { file.Write(content); file.Flush(flushToDisk: true); }
            File.Move(temporary, destination, overwrite: true);
        }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
    }
}
