using System.Globalization;
using System.Text.Json;

namespace Astra.Runtime;

// Cold inspection boundary. The native editor reads the same field syntax as
// its authoring serializer, without interpreting JSON on the frame path.
internal static class ScriptFieldSnapshot
{
    internal static string Quote(string value) => "\"" + value.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"";
    private static string Number(JsonElement value) => value.GetRawText();
    private static string Parts(JsonElement value, params string[] names) =>
        string.Join(" ", names.Select(name => Number(value.GetProperty(name))));
    internal static string Value(JsonElement value, string kind)
    {
        if (value.ValueKind == JsonValueKind.Null)
            throw new InvalidDataException("Elemento nulo; edição desta lista indisponível");
        if (kind.StartsWith("array:", StringComparison.Ordinal))
        {
            var items = value.EnumerateArray().Select(v => Quote(Value(v, kind[6..]))).ToArray();
            return items.Length.ToString(CultureInfo.InvariantCulture) + " " + string.Join(" ", items);
        }
        if (kind == "string") return value.GetString()!;
        if (kind == "asset") return value.GetProperty("AssetId").GetString() ?? "";
        if (kind == "object") return Number(value.GetProperty("ObjectId"));
        if (kind.StartsWith("component:", StringComparison.Ordinal))
            return Number(value.GetProperty("ObjectId")) + ":" + Number(value.GetProperty("InstanceId"));
        if (kind == "vector3") return Parts(value, "X", "Y", "Z");
        if (kind.StartsWith("color", StringComparison.Ordinal)) return Parts(value, "R", "G", "B", "A");
        if (kind == "curve")
        {
            var keys = value.GetProperty("Keys").EnumerateArray().ToArray();
            return Parts(value, "PreWrapMode", "PostWrapMode") + " " + keys.Length.ToString(CultureInfo.InvariantCulture) + " " +
                string.Join(" ", keys.Select(k => Parts(k, "Time", "Value", "InTangent", "OutTangent", "LeftMode", "RightMode") +
                    (k.GetProperty("Broken").GetBoolean() ? " 1" : " 0")));
        }
        if (kind.StartsWith("gradient", StringComparison.Ordinal))
        {
            var colors = value.GetProperty("ColorKeys").EnumerateArray().ToArray();
            var alphas = value.GetProperty("AlphaKeys").EnumerateArray().ToArray();
            return Number(value.GetProperty("Mode")) + " " + colors.Length.ToString(CultureInfo.InvariantCulture) + " " +
                string.Join(" ", colors.Select(k => Number(k.GetProperty("Time")) + " " + Parts(k.GetProperty("Color"), "R", "G", "B"))) + " " +
                alphas.Length.ToString(CultureInfo.InvariantCulture) + " " + string.Join(" ", alphas.Select(k => Parts(k, "Time", "Alpha")));
        }
        if (kind is "bool" or "float" or "int32" or "enum") return value.GetRawText();
        throw new InvalidDataException("Tipo de campo não inspecionável: " + kind);
    }
}
