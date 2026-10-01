using System.Text;
using System.Text.Json;

namespace Astra;

internal interface ISaveHost { SaveStore Save { get; } }
public enum SaveRecovery { None, PreviousSnapshot }
public sealed class SaveVersionException(string message) : IOException(message) { }

/// <summary>One project-owned save namespace. Changes to keys commit with Flush;
/// standalone files commit immediately. Errors never select a temporary directory.</summary>
public sealed class SaveStore : IDisposable
{
    public const int MaxSnapshotBytes = 256 * 1024, MaxFileBytes = 1024 * 1024;
    public const long MaxFilesBytes = 16 * 1024 * 1024;
    private static readonly UTF8Encoding Utf8 = new(false, true);
    private sealed record Value(string Type, JsonElement Data);
    private sealed record Snapshot(int Version, Dictionary<string, Value> Values);
    private readonly string _root, _files;
    private readonly FileStream _lease;
    private readonly object _gate = new();
    private Dictionary<string, Value> _values = new(StringComparer.Ordinal);
    private bool _dirty, _disposed;
    public SaveRecovery Recovery { get; private set; }

    public SaveStore(string projectRoot)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(projectRoot);
        var project = Path.GetFullPath(projectRoot);
        if (!Directory.Exists(project)) throw new DirectoryNotFoundException("Save requires an existing project directory.");
        CheckLinks(project);
        _root = Path.Combine(project, ".astra", "save"); _files = Path.Combine(_root, "files");
        CheckLinks(_root); Directory.CreateDirectory(_root); CheckLinks(_root);
        _lease = new FileStream(SafeInternal("writer.lock"), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None);
        try { Load(); } catch { _lease.Dispose(); throw; }
    }
    public bool Contains(string key) { lock (_gate) { EnsureOpen(); ValidateKey(key); return _values.ContainsKey(key); } }
    public bool Delete(string key) { lock (_gate) { EnsureOpen(); ValidateKey(key); var removed = _values.Remove(key); _dirty |= removed; return removed; } }
    public void SetString(string key, string value) { ArgumentNullException.ThrowIfNull(value); if (Utf8.GetByteCount(value) > 16 * 1024) throw new ArgumentOutOfRangeException(nameof(value)); Set(key, "string", value); }
    public void SetBoolean(string key, bool value) => Set(key, "bool", value);
    public void SetInt64(string key, long value) => Set(key, "int64", value);
    public void SetDouble(string key, double value) { if (!double.IsFinite(value)) throw new ArgumentOutOfRangeException(nameof(value)); Set(key, "double", value); }
    public string GetString(string key, string fallback = "") => Get(key, "string", fallback);
    public bool GetBoolean(string key, bool fallback = false) => Get(key, "bool", fallback);
    public long GetInt64(string key, long fallback = 0) => Get(key, "int64", fallback);
    public double GetDouble(string key, double fallback = 0) => Get(key, "double", fallback);
    private T Get<T>(string key, string type, T fallback)
    {
        lock (_gate) { EnsureOpen(); ValidateKey(key); if (!_values.TryGetValue(key, out var value)) return fallback;
            if (value.Type != type) throw new InvalidDataException($"Save key '{key}' is {value.Type}, expected {type}.");
            return value.Data.Deserialize<T>()!; }
    }
    private void Set<T>(string key, string type, T data)
    {
        lock (_gate) { EnsureOpen(); ValidateKey(key);
            var candidate = new Dictionary<string, Value>(_values, StringComparer.Ordinal) { [key] = new(type, JsonSerializer.SerializeToElement(data)) };
            if (candidate.Count > 4096 || Serialize(candidate).Length > MaxSnapshotBytes) throw new IOException("Save key quota exceeded.");
            _values = candidate; _dirty = true; }
    }
    private static void ValidateKey(string key) { if (string.IsNullOrWhiteSpace(key) || Utf8.GetByteCount(key) > 128 || key.Contains('\0')) throw new ArgumentException("Save key must contain 1-128 UTF-8 bytes.", nameof(key)); }
    private static byte[] Serialize(Dictionary<string, Value> values) => JsonSerializer.SerializeToUtf8Bytes(new Snapshot(1, values));
    public void Flush()
    {
        lock (_gate) { EnsureOpen(); if (!_dirty) return;
            AtomicWrite(SafeInternal("values.json"), Serialize(_values), SafeInternal("values.previous.json")); _dirty = false; }
    }
    private void Load()
    {
        var current = SafeInternal("values.json"); var previous = SafeInternal("values.previous.json");
        if (!File.Exists(current) && !File.Exists(previous)) return;
        try { _values = Parse(ReadBounded(current, MaxSnapshotBytes)); }
        catch (SaveVersionException) { throw; }
        catch (Exception error) when (error is JsonException or InvalidDataException or FileNotFoundException)
        {
            if (!File.Exists(previous)) throw new InvalidDataException("Save snapshot is corrupt and has no compatible recovery snapshot.", error);
            _values = Parse(ReadBounded(previous, MaxSnapshotBytes)); Recovery = SaveRecovery.PreviousSnapshot;
            // Preserve the known-good backup when replacing a damaged current file.
            AtomicWrite(current, Serialize(_values), null);
        }
    }
    private static Dictionary<string, Value> Parse(byte[] bytes)
    {
        var snapshot = JsonSerializer.Deserialize<Snapshot>(bytes) ?? throw new InvalidDataException("Empty save snapshot.");
        if (snapshot.Version != 1) throw new SaveVersionException($"Unsupported save version {snapshot.Version}.");
        if (snapshot.Values is null || snapshot.Values.Count > 4096) throw new InvalidDataException("Invalid save key count.");
        foreach (var (key, value) in snapshot.Values)
        {
            try { ValidateKey(key); } catch (ArgumentException error) { throw new InvalidDataException("Invalid save key.", error); } if (value is null) throw new InvalidDataException("Null save value.");
            bool valid = value.Type switch {
                "string" => value.Data.ValueKind == JsonValueKind.String && Utf8.GetByteCount(value.Data.GetString()!) <= 16 * 1024,
                "bool" => value.Data.ValueKind is JsonValueKind.True or JsonValueKind.False,
                "int64" => value.Data.ValueKind == JsonValueKind.Number && value.Data.TryGetInt64(out _),
                "double" => value.Data.ValueKind == JsonValueKind.Number && value.Data.TryGetDouble(out var n) && double.IsFinite(n), _ => false };
            if (!valid) throw new InvalidDataException($"Invalid typed save key '{key}'.");
        }
        return new(snapshot.Values, StringComparer.Ordinal);
    }
    public void WriteText(string safeRelativePath, string text) { ArgumentNullException.ThrowIfNull(text); if (Utf8.GetByteCount(text) > MaxFileBytes) throw new IOException("Save file quota exceeded."); WriteBytes(safeRelativePath, Utf8.GetBytes(text)); }
    public string ReadText(string safeRelativePath) => Utf8.GetString(ReadBytes(safeRelativePath));
    public byte[] ReadBytes(string safeRelativePath) { lock (_gate) { EnsureOpen(); return ReadBounded(FilePath(safeRelativePath), MaxFileBytes); } }
    public void WriteBytes(string safeRelativePath, ReadOnlySpan<byte> bytes)
    {
        lock (_gate) { EnsureOpen(); if (bytes.Length > MaxFileBytes) throw new IOException("Save file quota exceeded.");
            var target = FilePath(safeRelativePath); long total = 0; int count = 0;
            if (Directory.Exists(_files)) foreach (var file in EnumerateFiles(_files)) { ++count; total += new FileInfo(file).Length; if (count > 1024 || total > MaxFilesBytes + MaxFileBytes) throw new IOException("Existing save namespace exceeds quota."); }
            long old = File.Exists(target) ? new FileInfo(target).Length : 0;
            if (total - old + bytes.Length > MaxFilesBytes || (count >= 1024 && !File.Exists(target))) throw new IOException("Project save file quota exceeded.");
            Directory.CreateDirectory(Path.GetDirectoryName(target)!); CheckLinks(target); AtomicWrite(target, bytes, null); }
    }
    public bool DeleteFile(string safeRelativePath) { lock (_gate) { EnsureOpen(); var path = FilePath(safeRelativePath); if (!File.Exists(path)) return false; File.Delete(path); return true; } }
    private IEnumerable<string> EnumerateFiles(string directory)
    {
        var pending = new Stack<(string Path, int Depth)>(); pending.Push((directory, 0)); int entries = 0;
        while (pending.Count != 0) {
            var item = pending.Pop(); CheckLinks(item.Path);
            if (item.Depth > 16) throw new IOException("Existing save directory depth exceeds quota.");
            foreach (var entry in Directory.EnumerateFileSystemEntries(item.Path)) {
                if (++entries > 2048) throw new IOException("Existing save directory entries exceed quota.");
                CheckLinks(entry); if (Directory.Exists(entry)) pending.Push((entry, item.Depth + 1)); else yield return entry;
            }
        }
    }
    private string FilePath(string relative)
    {
        if (string.IsNullOrWhiteSpace(relative) || relative.Length > 512 || relative.Contains('\\') || Path.IsPathRooted(relative)) throw new ArgumentException("Use a relative '/'-separated save path.", nameof(relative));
        var segments = relative.Split('/'); if (segments.Length > 16) throw new ArgumentException("Save path is too deep.");
        foreach (var segment in segments) {
            var stem = segment.Split('.')[0].ToUpperInvariant();
            if (segment is "" or "." or ".." || segment.EndsWith('.') || segment.EndsWith(' ') || segment.IndexOfAny([':', '\0', '*', '?', '"', '<', '>', '|']) >= 0 || segment.Any(char.IsControl) || stem is "CON" or "PRN" or "AUX" or "NUL" || (stem.Length == 4 && (stem.StartsWith("COM") || stem.StartsWith("LPT")) && stem[3] is >= '0' and <= '9')) throw new ArgumentException("Unsafe save path segment.");
        }
        var path = Path.GetFullPath(Path.Combine(_files, relative));
        if (!path.StartsWith(_files + Path.DirectorySeparatorChar, OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal)) throw new ArgumentException("Save path escapes its namespace.");
        CheckLinks(path); return path;
    }
    private string SafeInternal(string file) { var path = Path.Combine(_root, file); CheckLinks(path); return path; }
    private static void CheckLinks(string path)
    {
        for (var current = Path.GetFullPath(path); current is not null; current = Path.GetDirectoryName(current))
        {
            try { if ((File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0) throw new IOException("Save paths cannot traverse symbolic links or reparse points."); }
            catch (FileNotFoundException) { } catch (DirectoryNotFoundException) { }
        }
    }
    private static byte[] ReadBounded(string path, int limit)
    {
        CheckLinks(path); using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        if (stream.Length > limit) throw new InvalidDataException("Save read quota exceeded.");
        var bytes = new byte[checked((int)stream.Length)]; stream.ReadExactly(bytes);
        if (stream.ReadByte() != -1) throw new InvalidDataException("Save grew beyond bounded read."); return bytes;
    }
    private static void AtomicWrite(string path, ReadOnlySpan<byte> bytes, string? backup)
    {
        CheckLinks(path); if (backup is not null) CheckLinks(backup);
        var temp = Path.Combine(Path.GetDirectoryName(path)!, ".write-" + Guid.NewGuid().ToString("N"));
        try { using (var stream = new FileStream(temp, FileMode.CreateNew, FileAccess.Write, FileShare.None)) { stream.Write(bytes); stream.Flush(flushToDisk: true); }
            CheckLinks(path); if (backup is not null && File.Exists(path)) File.Replace(temp, path, backup); else File.Move(temp, path, overwrite: true); }
        finally { if (File.Exists(temp)) File.Delete(temp); }
    }
    private void EnsureOpen() { ObjectDisposedException.ThrowIf(_disposed, this); CheckLinks(_root); }
    public void Dispose() { lock (_gate) { if (_disposed) return; try { Flush(); } finally { _disposed = true; _lease.Dispose(); } } }
}
