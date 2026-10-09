using System.Globalization;
using System.Text;

namespace Astra.Editor;

public sealed record ClipLayer(ulong Id, string Name, ClipLayerBlend Blend, float Weight, float? ReferenceTime, bool Muted, bool Solo);
public sealed record ClipBinding(ulong Id, AssetGuid SourceNode, string Path, string Name);
public sealed record ClipCurve(IReadOnlyList<ClipKey> Keys);
public sealed record ClipTrack(ulong Id, ulong Binding, ClipProperty Property, uint WeightCount,
    bool SourceOverride, ClipRotation Rotation, IReadOnlyList<ClipCurve> Curves) { public ulong Layer { get; init; } }

/// <summary>Immutable inspection of the current draft. IDs and source provenance are preserved; evaluation and edits remain native.</summary>
public sealed record ClipSnapshot(AssetGuid Guid, uint Revision, ulong NextId, uint DisplayRate, float Duration,
    string Name, AssetGuid Source, AssetGuid SourceClip, string SourceHash,
    IReadOnlyList<ClipBinding> Bindings, IReadOnlyList<ClipTrack> Tracks)
{
    public IReadOnlyList<ClipLayer> Layers { get; init; } = Array.AsReadOnly(new[] { new ClipLayer(0, "Base", ClipLayerBlend.Override, 1, null, false, false) });
    internal static ClipSnapshot Read(ReadOnlySpan<byte> utf8)
    {
        var reader = new Reader(new UTF8Encoding(false, true).GetString(utf8));
        if (reader.Token() != "AECLIP") throw new InvalidDataException("Unsupported animation snapshot format.");
        var version = reader.UInt();
        if (version is < 2 or > 3) throw new InvalidDataException("Unsupported animation snapshot format.");
        var guid = reader.Guid(); var revision = reader.UInt(); var next = reader.ULong();
        var rate = reader.UInt(); var duration = reader.Float(); var name = reader.String();
        var source = reader.Guid(); var sourceClip = reader.Guid(); var hash = reader.String();
        var bindings = new ClipBinding[reader.Count(4096)];
        for (var i = 0; i < bindings.Length; ++i) bindings[i] = new(reader.ULong(), reader.Guid(), reader.String(), reader.String());
        var layers = new[] { new ClipLayer(0, "Base", ClipLayerBlend.Override, 1, null, false, false) };
        if (version >= 3)
        {
            layers = new ClipLayer[reader.Count(32)];
            for (var i = 0; i < layers.Length; ++i)
            {
                var id = reader.ULong(); var layerName = reader.String(); var blend = (ClipLayerBlend)reader.Range(1);
                var weight = reader.Float(); var reference = reader.Float();
                layers[i] = new(id, layerName, blend, weight, reference < 0 ? null : reference, reader.Bool(), reader.Bool());
            }
        }
        var tracks = new ClipTrack[reader.Count(16384)]; var total = 0;
        for (var i = 0; i < tracks.Length; ++i)
        {
            var id = reader.ULong(); var binding = reader.ULong(); var property = (ClipProperty)reader.Range(3);
            var weights = reader.UInt(); var sourceOverride = reader.Bool(); var rotation = (ClipRotation)reader.Range(2);
            var layer = version >= 3 ? reader.ULong() : 0;
            var curves = new ClipCurve[reader.Count(256)];
            for (var c = 0; c < curves.Length; ++c)
            {
                var count = reader.Count(262144 - total); total += count; var keys = new ClipKey[count];
                for (var k = 0; k < count; ++k)
                    keys[k] = new(reader.ULong(), reader.Float(), reader.Float(), reader.Float(), reader.Float(),
                        reader.Float(), reader.Float(), (ClipTangent)reader.Range(6), (ClipTangent)reader.Range(6),
                        reader.Bool(), reader.Bool(), reader.Bool());
                curves[c] = new(Array.AsReadOnly(keys));
            }
            tracks[i] = new(id, binding, property, weights, sourceOverride, rotation, Array.AsReadOnly(curves)) { Layer = layer };
        }
        reader.End();
        return new(guid, revision, next, rate, duration, name, source, sourceClip, hash,
            Array.AsReadOnly(bindings), Array.AsReadOnly(tracks)) { Layers = Array.AsReadOnly(layers) };
    }
    private sealed class Reader(string text)
    {
        private int _position;
        private void Space() { while (_position < text.Length && char.IsWhiteSpace(text[_position])) ++_position; }
        public string Token()
        {
            Space(); var start = _position;
            while (_position < text.Length && !char.IsWhiteSpace(text[_position])) ++_position;
            if (start == _position) throw new InvalidDataException("Incomplete animation snapshot.");
            return text[start.._position];
        }
        public string String()
        {
            Space();
            if (_position >= text.Length || text[_position++] != '"') throw new InvalidDataException("Expected quoted animation text.");
            var result = new StringBuilder();
            while (_position < text.Length)
            {
                var c = text[_position++];
                if (c == '"') return result.ToString();
                if (c == '\\')
                {
                    if (_position >= text.Length) break;
                    c = text[_position++];
                }
                result.Append(c);
                if (result.Length > 65536) throw new InvalidDataException("Animation snapshot text exceeds its limit.");
            }
            throw new InvalidDataException("Unterminated animation snapshot text.");
        }
        public ulong ULong() => ulong.TryParse(Token(), NumberStyles.None, CultureInfo.InvariantCulture, out var value)
            ? value : throw new InvalidDataException("Invalid animation identity.");
        public uint UInt() { var value = ULong(); return value <= uint.MaxValue ? (uint)value : throw new InvalidDataException("Animation integer overflow."); }
        public uint Range(uint maximum) { var value = UInt(); return value <= maximum ? value : throw new InvalidDataException("Invalid animation enumeration."); }
        public int Count(int maximum) { var value = UInt(); return value <= maximum ? (int)value : throw new InvalidDataException("Animation snapshot exceeds its structural limit."); }
        public bool Bool() => Range(1) == 1;
        public float Float() => float.TryParse(Token(), NumberStyles.Float, CultureInfo.InvariantCulture, out var value) && float.IsFinite(value)
            ? value : throw new InvalidDataException("Invalid animation number.");
        public AssetGuid Guid()
        {
            var token = Token(); return token == "-" ? default : AssetGuid.TryParse(token, out var value)
                ? value : throw new InvalidDataException("Invalid animation asset GUID.");
        }
        public void End() { Space(); if (_position != text.Length) throw new InvalidDataException("Trailing animation snapshot content."); }
    }
}
