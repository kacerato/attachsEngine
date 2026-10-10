using System.Runtime.InteropServices;
using System.Text;

namespace Astra.Editor;

public enum ClipCueKind : uint { Marker, Event }
public sealed record ClipCue(ulong Id, float Time, ClipCueKind Kind, string Name, uint Tag = 0, double Value = 0, bool Forward = true, bool Reverse = true, bool Enabled = true);
public enum ClipProperty : uint { Translation, Rotation, Scale, MorphWeights }
public enum ClipRotation : uint { Quaternion, Euler, ProgressiveQuaternion }
public enum ClipTangent : uint { Free, Linear, Constant, Auto, ClampedAuto, Flat, NextConstant }
public enum ClipLayerBlend : uint { Override, Additive }
public enum ClipPaste : uint { Replace, InsertTracks, InsertAllTracks }
public readonly record struct ClipKey(ulong Id, float Time, float Value,
    float IncomingSlope = 0, float OutgoingSlope = 0,
    float IncomingWeight = 1f / 3, float OutgoingWeight = 1f / 3,
    ClipTangent Incoming = ClipTangent.ClampedAuto, ClipTangent Outgoing = ClipTangent.ClampedAuto,
    bool Broken = false, bool WeightedIncoming = false, bool WeightedOutgoing = false);
public readonly record struct ClipKeyAddress(ulong Track, uint Component, ulong Key);
public readonly record struct ClipCurveSample(double Value, double Derivative);
public readonly record struct ClipEulerReference(float X, float Y, float Z);
public sealed record ClipBakeSettings(uint SampleRate = 60, double Tolerance = .01,
    bool Reduce = true, ClipRotation? Rotation = null, uint VerificationSteps = 4, uint MaximumFrames = 16384,
    ClipEulerReference? EulerReference = null);
public readonly record struct ClipBakeReport(uint InputKeys, uint OutputKeys, uint SampledFrames,
    uint VerifiedSamples, double MaximumError);
public readonly record struct ClipConsolidation(AssetGuid Clip, ClipBakeReport Report);

/// <summary>One synchronous editor command. Retaining it after the command or using it on a worker is rejected.</summary>
public sealed unsafe class EditorContext : IDisposable
{
    private readonly AnimationAuthorAccess _access;
    private readonly int _thread = Environment.CurrentManagedThreadId;
    private bool _active = true;
    private static readonly UTF8Encoding Utf8 = new(false, true);
    internal EditorContext(AnimationAuthorAccess* access)
    {
        var prefixSize = (uint)Marshal.OffsetOf<AnimationAuthorAccess>(nameof(AnimationAuthorAccess.Bake)).ToInt32();
        if (access == null || (access->Version < 1 || access->Version > 6) ||
            access->Size != (access->Version == 1 ? prefixSize : access->Version == 2 ? (uint)Marshal.OffsetOf<AnimationAuthorAccess>(nameof(AnimationAuthorAccess.SampleComposed)).ToInt32() : access->Version == 3 ? (uint)Marshal.OffsetOf<AnimationAuthorAccess>(nameof(AnimationAuthorAccess.BakeAdvanced)).ToInt32() : access->Version == 4 ? (uint)Marshal.OffsetOf<AnimationAuthorAccess>(nameof(AnimationAuthorAccess.PreviewClip)).ToInt32() : (uint)sizeof(AnimationAuthorAccess)) || access->Context == null ||
            access->Selected == null || access->Count == null || access->At == null || access->Create == null ||
            access->Extract == null || access->Begin == null || access->Snapshot == null || access->Apply == null ||
            access->AddTrack == null || access->Commit == null || access->Cancel == null || access->Copy == null ||
            access->Paste == null || access->TransformSelection == null || access->EraseSelection == null ||
            access->Sample == null || access->SampleCurve == null || access->Diagnostic == null || access->ReleaseClipboard == null)
            throw new NotSupportedException("The editor does not provide a compatible animation authoring ABI.");
        if (access->Version >= 2 && access->Bake == null) throw new NotSupportedException("The editor has no native bake implementation.");
        if (access->Version >= 3 && access->SampleComposed == null) throw new NotSupportedException("The editor has no layer composition evaluator.");
        if (access->Version >= 4 && (access->BakeAdvanced == null || access->Consolidate == null)) throw new NotSupportedException("The editor has no clip conversion/consolidation implementation.");
        if (access->Version >= 5 && (access->PreviewClip == null || access->SeekPreview == null || access->StagePose == null || access->RecordPose == null || access->CancelPose == null || access->ClosePreview == null)) throw new NotSupportedException("The editor has no isolated pose authoring implementation.");
        AnimationAuthorAccess copied = default;
        Buffer.MemoryCopy(access, &copied, sizeof(AnimationAuthorAccess), access->Size);
        _access = copied;
    }
    internal ref readonly AnimationAuthorAccess Access { get { Check(); return ref _access; } }
    internal bool Active => _active;
    internal void Check()
    {
        ObjectDisposedException.ThrowIf(!_active, this);
        if (_thread != Environment.CurrentManagedThreadId) throw new InvalidOperationException("Editor commands run synchronously on the editor thread.");
    }
    internal void Require(bool accepted)
    {
        Check(); if (accepted) return;
        var length = _access.Diagnostic(_access.Context, null, 0);
        if (length is > 0 and <= 65536)
        {
            var bytes = new byte[length];
            fixed (byte* p = bytes)
                if (_access.Diagnostic(_access.Context, p, length) == length)
                    throw new InvalidOperationException(Utf8.GetString(bytes));
        }
        throw new InvalidOperationException("Animation authoring was refused by the editor.");
    }
    internal static byte[] Text(string text, int maximum)
    {
        ArgumentNullException.ThrowIfNull(text);
        var count = Utf8.GetByteCount(text);
        if (count > maximum || text.Contains('\0')) throw new ArgumentException("Text exceeds the authoring limit or contains NUL.", nameof(text));
        return Utf8.GetBytes(text);
    }
    public ulong SelectedObject => Access.Selected(_access.Context);
    public IReadOnlyList<AssetGuid> Clips => Catalog(false);
    public IReadOnlyList<AssetGuid> ImportedClips => Catalog(true);
    private IReadOnlyList<AssetGuid> Catalog(bool imported)
    {
        ref readonly var api = ref Access;
        var count = api.Count(api.Context, imported ? 1u : 0u); Require(count >= 0);
        if (count > 65536) throw new InvalidDataException("Animation catalog exceeds its limit.");
        var items = new AssetGuid[count];
        for (uint i = 0; i < count; ++i)
        {
            AuthorGuid guid = default; Require(api.At(api.Context, imported ? 1u : 0u, i, &guid) == 1);
            items[i] = guid.Value;
        }
        return Array.AsReadOnly(items);
    }
    /// <summary>Creates a resource and its own undo step; subsequent edits use BeginClip.</summary>
    public AssetGuid CreateClip(ulong owner, float duration = 1, ClipRotation rotation = ClipRotation.Quaternion)
    {
        ref readonly var api = ref Access; AuthorGuid guid = default;
        Require(api.Create(api.Context, owner, duration, (uint)rotation, &guid) == 1); return guid.Value;
    }
    /// <summary>Copies an imported animation into an editable resource without modifying the source.</summary>
    public AssetGuid ExtractClip(AssetGuid sourceClip, ulong owner)
    {
        ref readonly var api = ref Access; AuthorGuid guid = default;
        Require(api.Extract(api.Context, new(sourceClip), owner, &guid) == 1); return guid.Value;
    }
    public ClipEdit BeginClip(AssetGuid clip, uint expectedRevision = 0)
    {
        ref readonly var api = ref Access; var token = api.Begin(api.Context, new(clip), expectedRevision);
        Require(token != 0); return new(this, token);
    }
    public ClipSnapshot InspectClip(AssetGuid clip)
    {
        using var edit = BeginClip(clip); return edit.Snapshot;
    }
    private void RequirePosePreview()
    {
        if (Access.Version < 5) throw new NotSupportedException("This editor does not support isolated pose authoring.");
    }
    /// <summary>Opens an isolated clip preview. Source-scene transforms remain unchanged.</summary>
    public void PreviewClip(AssetGuid clip, ulong owner, float time = 0)
    {
        RequirePosePreview(); ref readonly var api = ref Access;
        Require(api.PreviewClip(api.Context, new(clip), owner, time) == 1);
    }
    /// <summary>Seeks the preview, discarding any unrecorded pose.</summary>
    public void SeekClipPreview(float time) { RequirePosePreview(); ref readonly var api = ref Access; Require(api.SeekPreview(api.Context, time) == 1); }
    /// <summary>Stages one property in native track units. No resource or history is published until RecordPose.</summary>
    public void StagePose(ulong track, ReadOnlySpan<float> values)
    {
        RequirePosePreview(); ref readonly var api = ref Access;
        if (values.Length is < 1 or > 64) throw new ArgumentException("A pose requires between 1 and 64 components.", nameof(values));
        fixed (float* p = values) Require(api.StagePose(api.Context, track, p, values.Length) == 1);
    }
    public void RecordPose() { RequirePosePreview(); ref readonly var api = ref Access; Require(api.RecordPose(api.Context) == 1); }
    public void CancelPose() { RequirePosePreview(); ref readonly var api = ref Access; Require(api.CancelPose(api.Context) == 1); }
    public void CloseClipPreview() { RequirePosePreview(); ref readonly var api = ref Access; Require(api.ClosePreview(api.Context) == 1); }
    public void Dispose() { if (!_active) return; Check(); _active = false; }
}

/// <summary>An owned native draft. Commit publishes one resource/history transaction; Dispose discards an uncommitted draft.</summary>
public sealed unsafe class ClipEdit : IDisposable
{
    private readonly EditorContext _context;
    private ulong _token;
    internal ClipEdit(EditorContext context, ulong token) { _context = context; _token = token; }
    private ref readonly AnimationAuthorAccess Api
    {
        get { _context.Check(); ObjectDisposedException.ThrowIf(_token == 0, this); return ref _context.Access; }
    }
    public ClipSnapshot Snapshot
    {
        get
        {
            ref readonly var api = ref Api;
            var length = api.Snapshot(api.Context, _token, null, 0); _context.Require(length > 0);
            if (length > 64 * 1024 * 1024) throw new InvalidDataException("Clip snapshot exceeds 64 MiB.");
            var bytes = new byte[length];
            fixed (byte* p = bytes) _context.Require(api.Snapshot(api.Context, _token, p, length) == length);
            return ClipSnapshot.Read(bytes);
        }
    }
    private ulong Apply(AuthorOperation operation, ulong track = 0, uint component = 0,
        AuthorKey key = default, double first = 0, double second = 0, uint mode = 0,
        ReadOnlySpan<float> values = default, string text = "")
    {
        ref readonly var api = ref Api;
        if (operation >= AuthorOperation.PutCue && api.Version < 6) throw new NotSupportedException("This editor does not support clip cues.");
        if (operation >= AuthorOperation.LayerAdd && api.Version < 3) throw new NotSupportedException("This editor build does not support authoring layers.");
        if (values.Length > 256) throw new ArgumentException("Too many pose components.", nameof(values));
        var bytes = EditorContext.Text(text, 256);
        var command = new AuthorCommand { Operation = (uint)operation, Track = track, Component = component,
            Key = key, First = first, Second = second, Mode = mode };
        ulong result = 0;
        fixed (float* v = values) fixed (byte* t = bytes)
            _context.Require(api.Apply(api.Context, _token, &command, v, values.Length, t, bytes.Length, &result) == 1);
        return result;
    }
    /// <summary>ID zero creates a cue; a nonzero ID updates an existing cue. Markers never execute gameplay.</summary>
    public ulong PutCue(ClipCue cue) => Apply(AuthorOperation.PutCue, cue.Id, cue.Tag,
        new AuthorKey { Flags = (cue.Forward ? 1u : 0) | (cue.Reverse ? 2u : 0) | (cue.Enabled ? 4u : 0) },
        cue.Time, cue.Value, (uint)cue.Kind, text: cue.Name);
    public void RemoveCue(ulong cue) => Apply(AuthorOperation.RemoveCue, cue);
    public ulong PutKey(ulong track, uint component, ClipKey key) => Apply(AuthorOperation.PutKey, track, component, new(key));
    public ulong SplitKey(ulong track, uint component, float time) => Apply(AuthorOperation.SplitKey, track, component, first: time);
    public void EraseKey(ClipKeyAddress address) => Apply(AuthorOperation.EraseKey, address.Track, address.Component, new AuthorKey { Id = address.Key });
    public void PutPose(ulong track, float time, ReadOnlySpan<float> values) => Apply(AuthorOperation.PutPose, track, first: time, values: values);
    public void Retime(double scale) => Apply(AuthorOperation.Retime, first: scale);
    public void Reverse() => Apply(AuthorOperation.Reverse);
    public void Crop(float start, float end) => Apply(AuthorOperation.Crop, first: start, second: end);
    public void SetRotation(ulong track, ClipRotation rotation) => Apply(AuthorOperation.RotationMode, track, mode: (uint)rotation);
    public void RemoveTrack(ulong track) => Apply(AuthorOperation.RemoveTrack, track);
    public void SetName(string name) => Apply(AuthorOperation.Name, text: name);
    public void SetDisplayRate(uint framesPerSecond) => Apply(AuthorOperation.DisplayRate, mode: framesPerSecond);
    public ulong AddLayer(string name, ClipLayerBlend blend = ClipLayerBlend.Additive) => Apply(AuthorOperation.LayerAdd, text: name, mode: (uint)blend);
    /// <summary>Reference time null uses neutral offsets. Mute/solo are stored and affect both preview and runtime.</summary>
    public void ConfigureLayer(ulong layer, string name, ClipLayerBlend blend = ClipLayerBlend.Additive,
        float weight = 1, float? referenceTime = null, bool muted = false, bool solo = false) =>
        Apply(AuthorOperation.LayerConfigure, layer, key: new AuthorKey { Flags = (muted ? 1u : 0) | (solo ? 2u : 0) },
            text: name, mode: (uint)blend, first: weight, second: referenceTime ?? -1);
    public void MoveLayer(ulong layer, uint position) => Apply(AuthorOperation.LayerMove, layer, mode: position);
    public ulong DuplicateLayer(ulong layer) => Apply(AuthorOperation.LayerDuplicate, layer);
    public void RemoveLayer(ulong layer) => Apply(AuthorOperation.LayerRemove, layer);
    /// <summary>Adds the source property to a sparse layer mask. Copies keys or initializes neutral/override values.</summary>
    public ulong AddLayerTrack(ulong layer, ulong sourceTrack, bool copyCurves = false, ClipRotation? rotation = null) =>
        Apply(AuthorOperation.LayerAddTrack, sourceTrack, component: rotation is { } r ? (uint)r + 1 : 0, key: new AuthorKey { Id = layer }, mode: copyCurves ? 1u : 0);
    public void CopyBaseToTrack(ulong track) => Apply(AuthorOperation.LayerCopyBase, track);
    public float[] SampleComposed(ulong track, float time)
    {
        ref readonly var api = ref Api;
        if (api.Version < 3 || api.SampleComposed == null) throw new NotSupportedException("This editor build does not support authoring layers.");
        var count = api.SampleComposed(api.Context, _token, track, time, null, 0); _context.Require(count > 0);
        if (count > 256) throw new InvalidDataException("Invalid pose component count.");
        var values = new float[count];
        fixed (float* p = values) _context.Require(api.SampleComposed(api.Context, _token, track, time, p, count) == count);
        return values;
    }
    /// <summary>Measured source/candidate error in component units or rotation degrees. Does not certify continuous-time error.</summary>
    public ClipBakeReport Bake(ulong track, ClipBakeSettings? settings = null)
    {
        ref readonly var api = ref Api;
        if (api.Version < 2 || api.Bake == null) throw new NotSupportedException("This editor build does not support track bake.");
        settings ??= new();
        var wire = new AuthorBakeSettings { SampleRate = settings.SampleRate, Tolerance = settings.Tolerance,
            Flags = settings.Reduce ? 1u : 0, Rotation = settings.Rotation is { } mode ? (uint)mode : 3,
            VerificationSteps = settings.VerificationSteps, MaximumFrames = settings.MaximumFrames };
        AuthorBakeReport report = default;
        if (settings.EulerReference is { } reference)
        {
            if (api.Version < 4 || api.BakeAdvanced == null) throw new NotSupportedException("This editor build does not support seeded Euler conversion.");
            var request = new AuthorBakeRequest { Settings = wire, EulerPolicy = 1, X = reference.X, Y = reference.Y, Z = reference.Z };
            _context.Require(api.BakeAdvanced(api.Context, _token, track, &request, &report) == 1);
        }
        else _context.Require(api.Bake(api.Context, _token, track, &wire, &report) == 1);
        return new(report.InputKeys, report.OutputKeys, report.SampledFrames, report.VerifiedSamples, report.MaximumError);
    }
    /// <summary>Creates a new single-base resource from this draft's composed pose. Does not modify or commit the source draft. Publication has its own undo step.</summary>
    public ClipConsolidation CreateConsolidated(string name, ClipBakeSettings? settings = null)
    {
        ref readonly var api = ref Api;
        if (api.Version < 4 || api.Consolidate == null) throw new NotSupportedException("This editor build does not support clip consolidation.");
        settings ??= new();var bytes = EditorContext.Text(name, 256);
        var reference = settings.EulerReference;
        var request = new AuthorBakeRequest { Settings = new AuthorBakeSettings {
            SampleRate = settings.SampleRate, Tolerance = settings.Tolerance, Flags = settings.Reduce ? 1u : 0,
            Rotation = settings.Rotation is { } mode ? (uint)mode : 3, VerificationSteps = settings.VerificationSteps, MaximumFrames = settings.MaximumFrames },
            EulerPolicy = reference.HasValue ? 1u : 0, X = reference?.X ?? 0, Y = reference?.Y ?? 0, Z = reference?.Z ?? 0 };
        AuthorGuid guid = default;AuthorBakeReport report = default;
        fixed (byte* text = bytes) _context.Require(api.Consolidate(api.Context, _token, text, bytes.Length, &request, &guid, &report) == 1);
        return new(guid.Value, new(report.InputKeys, report.OutputKeys, report.SampledFrames, report.VerifiedSamples, report.MaximumError));
    }
    /// <summary>Canonical relative binding path, or empty for the owner. Use ClipBindingPath.Join for object names.</summary>
    public ulong AddTrack(string path, string name, ClipProperty property, ReadOnlySpan<float> initialValues,
        ClipRotation rotation = ClipRotation.Quaternion, AssetGuid sourceNode = default)
    {
        ref readonly var api = ref Api;
        if (initialValues.Length > 256) throw new ArgumentException("Too many pose components.", nameof(initialValues));
        var p = EditorContext.Text(path, 1024); var n = EditorContext.Text(name, 256); ulong result = 0;
        fixed (byte* pp = p) fixed (byte* nn = n) fixed (float* v = initialValues)
            _context.Require(api.AddTrack(api.Context, _token, new(sourceNode), pp, p.Length, nn, n.Length,
                (uint)property, (uint)rotation, v, initialValues.Length, &result) == 1);
        return result;
    }
    private static AuthorAddress[] Addresses(ReadOnlySpan<ClipKeyAddress> selection)
    {
        if (selection.Length is <= 0 or > 262144) throw new ArgumentException("Select between 1 and 262144 keys.", nameof(selection));
        var addresses = new AuthorAddress[selection.Length];
        for (var i = 0; i < addresses.Length; ++i) addresses[i] = new(selection[i]);
        return addresses;
    }
    public ClipClipboard Copy(ReadOnlySpan<ClipKeyAddress> selection)
    {
        ref readonly var api = ref Api; var addresses = Addresses(selection); ulong token;
        fixed (AuthorAddress* p = addresses) token = api.Copy(api.Context, _token, p, addresses.Length);
        _context.Require(token != 0); return new(_context, token);
    }
    public void Paste(ClipClipboard clipboard, float time, ClipPaste mode = ClipPaste.Replace)
    {
        ArgumentNullException.ThrowIfNull(clipboard); ref readonly var api = ref Api;
        if (!ReferenceEquals(clipboard.Context, _context)) throw new ArgumentException("Clipboard belongs to another editor command.", nameof(clipboard));
        ObjectDisposedException.ThrowIf(clipboard.Token == 0, clipboard);
        _context.Require(api.Paste(api.Context, _token, clipboard.Token, time, (uint)mode) == 1);
    }
    public void TransformSelection(ReadOnlySpan<ClipKeyAddress> selection, double pivot = 0, double scale = 1, double offset = 0)
    {
        ref readonly var api = ref Api; var addresses = Addresses(selection);
        fixed (AuthorAddress* p = addresses)
            _context.Require(api.TransformSelection(api.Context, _token, p, addresses.Length, pivot, scale, offset) == 1);
    }
    public void EraseSelection(ReadOnlySpan<ClipKeyAddress> selection)
    {
        ref readonly var api = ref Api; var addresses = Addresses(selection);
        fixed (AuthorAddress* p = addresses) _context.Require(api.EraseSelection(api.Context, _token, p, addresses.Length) == 1);
    }
    /// <summary>Native runtime pose evaluator; rotations always return quaternion XYZW, including Euler/progressive authoring.</summary>
    public float[] Sample(ulong track, float time)
    {
        ref readonly var api = ref Api; var count = api.Sample(api.Context, _token, track, time, null, 0);
        _context.Require(count > 0);
        if (count > 256) throw new InvalidDataException("Invalid pose component count.");
        var values = new float[count];
        fixed (float* p = values) _context.Require(api.Sample(api.Context, _token, track, time, p, count) == count);
        return values;
    }
    public ClipCurveSample SampleCurve(ulong track, uint component, double time)
    {
        ref readonly var api = ref Api; double value = 0, derivative = 0;
        _context.Require(api.SampleCurve(api.Context, _token, track, component, time, &value, &derivative) == 1);
        return new(value, derivative);
    }
    public void Commit() { ref readonly var api = ref Api; _context.Require(api.Commit(api.Context, _token) == 1); _token = 0; }
    public void Dispose()
    {
        if (_token == 0) return;
        if (_context.Active) { ref readonly var api = ref Api; _context.Require(api.Cancel(api.Context, _token) == 1); }
        _token = 0;
    }
}
public sealed unsafe class ClipClipboard : IDisposable
{
    internal EditorContext Context { get; }
    internal ulong Token { get; private set; }
    internal ClipClipboard(EditorContext context, ulong token) { Context = context; Token = token; }
    public void Dispose()
    {
        if (Token == 0) return;
        if (Context.Active) { ref readonly var api = ref Context.Access; Context.Require(api.ReleaseClipboard(api.Context, Token) == 1); }
        Token = 0;
    }
}

/// <summary>Uses the native binding grammar; slash/percent/backslash and dot segments are escaped without altering Unicode names.</summary>
public static class ClipBindingPath
{
    public static string Join(params string[] names)
    {
        ArgumentNullException.ThrowIfNull(names);
        return string.Join('/', names.Select(EncodeSegment));
    }
    public static string EncodeSegment(string name)
    {
        ArgumentNullException.ThrowIfNull(name);
        if (name.Length == 0) throw new ArgumentException("An object name cannot be empty.", nameof(name));
        var bytes = new UTF8Encoding(false, true).GetBytes(name); var output = new List<byte>();
        const string hex = "0123456789ABCDEF";
        var dot = name is "." or "..";
        foreach (var value in bytes)
            if (value is (byte)'%' or (byte)'/' or (byte)'\\' || value < 32 || value == 127 || dot && value == '.')
            { output.Add((byte)'%'); output.Add((byte)hex[value >> 4]); output.Add((byte)hex[value & 15]); }
            else output.Add(value);
        return Encoding.UTF8.GetString(output.ToArray());
    }
}

internal enum AuthorOperation : uint { PutKey, SplitKey, EraseKey, PutPose, Retime, Reverse, Crop, RotationMode, RemoveTrack, Name, DisplayRate, LayerAdd, LayerConfigure, LayerMove, LayerDuplicate, LayerRemove, LayerAddTrack, LayerCopyBase, PutCue, RemoveCue }
[StructLayout(LayoutKind.Sequential)]
internal struct AuthorGuid
{
    public ulong High, Low;
    public AuthorGuid(AssetGuid guid) { High = guid.High; Low = guid.Low; }
    public readonly AssetGuid Value => new(High, Low);
}
[StructLayout(LayoutKind.Sequential)]
internal struct AuthorKey
{
    public ulong Id;
    public float Time, Value, InSlope, OutSlope, InWeight, OutWeight;
    public uint Incoming, Outgoing, Flags;
    public AuthorKey(ClipKey key)
    {
        Id = key.Id; Time = key.Time; Value = key.Value; InSlope = key.IncomingSlope; OutSlope = key.OutgoingSlope;
        InWeight = key.IncomingWeight; OutWeight = key.OutgoingWeight; Incoming = (uint)key.Incoming; Outgoing = (uint)key.Outgoing;
        Flags = (key.Broken ? 1u : 0u) | (key.WeightedIncoming ? 2u : 0u) | (key.WeightedOutgoing ? 4u : 0u);
    }
}
[StructLayout(LayoutKind.Sequential)]
internal struct AuthorCommand
{
    public uint Operation, Component;
    public ulong Track;
    public AuthorKey Key;
    public double First, Second;
    public uint Mode, Reserved;
}
[StructLayout(LayoutKind.Sequential)]
internal struct AuthorAddress
{
    public ulong Track;
    public uint Component, Reserved;
    public ulong Key;
    public AuthorAddress(ClipKeyAddress address) { Track = address.Track; Component = address.Component; Reserved = 0; Key = address.Key; }
}
[StructLayout(LayoutKind.Sequential)]
internal unsafe struct AnimationAuthorAccess
{
    public uint Version, Size;
    public void* Context;
    public delegate* unmanaged<void*, ulong> Selected;
    public delegate* unmanaged<void*, uint, int> Count;
    public delegate* unmanaged<void*, uint, uint, AuthorGuid*, int> At;
    public delegate* unmanaged<void*, ulong, float, uint, AuthorGuid*, int> Create;
    public delegate* unmanaged<void*, AuthorGuid, ulong, AuthorGuid*, int> Extract;
    public delegate* unmanaged<void*, AuthorGuid, uint, ulong> Begin;
    public delegate* unmanaged<void*, ulong, byte*, int, int> Snapshot;
    public delegate* unmanaged<void*, ulong, AuthorCommand*, float*, int, byte*, int, ulong*, int> Apply;
    public delegate* unmanaged<void*, ulong, AuthorGuid, byte*, int, byte*, int, uint, uint, float*, int, ulong*, int> AddTrack;
    public delegate* unmanaged<void*, ulong, int> Commit;
    public delegate* unmanaged<void*, ulong, int> Cancel;
    public delegate* unmanaged<void*, ulong, AuthorAddress*, int, ulong> Copy;
    public delegate* unmanaged<void*, ulong, ulong, float, uint, int> Paste;
    public delegate* unmanaged<void*, ulong, AuthorAddress*, int, double, double, double, int> TransformSelection;
    public delegate* unmanaged<void*, ulong, AuthorAddress*, int, int> EraseSelection;
    public delegate* unmanaged<void*, ulong, ulong, float, float*, int, int> Sample;
    public delegate* unmanaged<void*, ulong, ulong, uint, double, double*, double*, int> SampleCurve;
    public delegate* unmanaged<void*, byte*, int, int> Diagnostic;
    public delegate* unmanaged<void*, ulong, int> ReleaseClipboard;
    public delegate* unmanaged<void*, ulong, ulong, AuthorBakeSettings*, AuthorBakeReport*, int> Bake;
    public delegate* unmanaged<void*, ulong, ulong, float, float*, int, int> SampleComposed;
    public delegate* unmanaged<void*, ulong, ulong, AuthorBakeRequest*, AuthorBakeReport*, int> BakeAdvanced;
    public delegate* unmanaged<void*, ulong, byte*, int, AuthorBakeRequest*, AuthorGuid*, AuthorBakeReport*, int> Consolidate;
    public delegate* unmanaged<void*, AuthorGuid, ulong, float, int> PreviewClip;
    public delegate* unmanaged<void*, float, int> SeekPreview;
    public delegate* unmanaged<void*, ulong, float*, int, int> StagePose;
    public delegate* unmanaged<void*, int> RecordPose;
    public delegate* unmanaged<void*, int> CancelPose;
    public delegate* unmanaged<void*, int> ClosePreview;
}
[StructLayout(LayoutKind.Sequential)]
internal struct AuthorBakeSettings
{
    public uint SampleRate, VerificationSteps, MaximumFrames, Rotation, Flags, Reserved;
    public double Tolerance;
}
[StructLayout(LayoutKind.Sequential)]
internal struct AuthorBakeReport
{
    public uint InputKeys, OutputKeys, SampledFrames, VerifiedSamples;
    public double MaximumError;
}
[StructLayout(LayoutKind.Sequential)]
internal struct AuthorBakeRequest
{
    public AuthorBakeSettings Settings;
    public uint EulerPolicy, Reserved;
    public float X, Y, Z;
    public uint Reserved2;
}
