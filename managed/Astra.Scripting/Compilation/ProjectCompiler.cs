using System.Globalization;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.Emit;

namespace Astra.Compilation;

public sealed record ScriptDiagnostic(string File, int Line, int Column, string Code, string Message, bool Error,
    string SourceExcerpt = "", int ExcerptLine = 0);
public sealed record ScriptPropertySchema(string Id, string Name, string ValueType, bool Hidden = false);
public sealed record ScriptTypeSchema(string Id, string Name, string File, ScriptPropertySchema[] Properties);
public sealed record CompiledProject(string Id, byte[] Assembly, byte[] Symbols, ScriptTypeSchema[] Types);
public sealed record ScriptBuildResult(CompiledProject? Project, ScriptDiagnostic[] Diagnostics)
{
    public bool Success => Project is not null;
}

/// <summary>In-process C# compiler. It parses and emits; building never instantiates user classes.</summary>
public sealed class ProjectCompiler
{
    public const int MaximumSources = 1024;
    public const int MaximumSourceBytes = 512 * 1024;
    public const int MaximumProjectBytes = 32 * 1024 * 1024;
    internal static string CompilerIdentity => typeof(ProjectCompiler).Assembly.ManifestModule.ModuleVersionId.ToString("N");
    public ScriptBuildResult Build(string projectDirectory, CancellationToken cancellation = default)
    {
        try
        {
            var root = Path.GetFullPath(projectDirectory);
            if (!Directory.Exists(root)) return Failure("ASTRA001", "Project directory is unavailable.");
            var inputs = ReadSources(root, cancellation);
            // An empty project is valid. Emitting an empty assembly also clears
            // the published catalog when the last source is intentionally removed.
            var trees = inputs.Select(input => CSharpSyntaxTree.ParseText(input.Text,
                new CSharpParseOptions(LanguageVersion.CSharp12), input.Path, Encoding.UTF8, cancellation)).ToArray();
            var referencePaths = ReferencePaths();
            var references = referencePaths.Select(path => MetadataReference.CreateFromFile(path)).ToArray();
            using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
            foreach (var input in inputs)
            {
                hash.AppendData(Encoding.UTF8.GetBytes(input.Path + "\0" + input.Text + "\0"));
            }
            hash.AppendData(Encoding.UTF8.GetBytes(CompilerIdentity));
            var id = Convert.ToHexString(hash.GetHashAndReset()).ToLowerInvariant();
            var compilation = CSharpCompilation.Create("Astra.Project." + id, trees, references,
                new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary,
                    optimizationLevel: OptimizationLevel.Debug, allowUnsafe: false, deterministic: true,
                    nullableContextOptions: NullableContextOptions.Enable));
            var schemaErrors = new List<ScriptDiagnostic>();
            var types = ExtractSchemas(compilation, root, schemaErrors, cancellation);
            using var assembly = new MemoryStream(); using var symbols = new MemoryStream();
            var emitted = compilation.Emit(assembly, symbols,
                options: new EmitOptions(debugInformationFormat: DebugInformationFormat.PortablePdb),
                cancellationToken: cancellation);
            var diagnostics = emitted.Diagnostics.Where(d => d.Severity is DiagnosticSeverity.Error or DiagnosticSeverity.Warning)
                .Select(d => ConvertDiagnostic(d, root)).Concat(schemaErrors).Take(4096).Select(d => {
                    var source = inputs.FirstOrDefault(s => s.Path == d.File).Text;
                    if (source is null) return d;
                    int first = Math.Max(1, d.Line - 1), at = 0, line = 1;
                    while (line < first && at < source.Length) { if (source[at++] == '\n') ++line; }
                    var excerpt = new StringBuilder();
                    for (int shown = 0; shown < 3 && at < source.Length; ++shown)
                    {
                        int end = source.IndexOf('\n', at); if (end < 0) end = source.Length;
                        int length = Math.Min(300, end - at);
                        if (length > 0 && char.IsHighSurrogate(source[at + length - 1])) --length;
                        if (shown > 0) excerpt.Append('\n');
                        excerpt.Append(source, at, length); at = end + 1;
                    }
                    return d with { SourceExcerpt = excerpt.ToString(), ExcerptLine = first };
                }).ToArray();
            if (!emitted.Success || schemaErrors.Any(d => d.Error)) return new(null, diagnostics);
            return new(new(id, assembly.ToArray(), symbols.ToArray(), types), diagnostics);
        }
        catch (OperationCanceledException) { return Failure("ASTRA003", "Compilation cancelled."); }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or ArgumentException or BadImageFormatException)
        {
            return Failure("ASTRA004", error.Message);
        }
    }
    private static ScriptBuildResult Failure(string code, string message) =>
        new(null, [new("", 1, 1, code, message, true)]);

    internal static List<(string Path, string Text)> ReadSources(string root, CancellationToken cancellation)
    {
        var sources = new List<(string Path, string Text)>(); var total = 0;
        var pending = new Stack<string>(); pending.Push(root);
        while (pending.TryPop(out var directory))
        {
            cancellation.ThrowIfCancellationRequested();
            foreach (var path in Directory.EnumerateFileSystemEntries(directory).Order(StringComparer.Ordinal))
            {
                var attributes = File.GetAttributes(path);
                if ((attributes & FileAttributes.ReparsePoint) != 0) continue;
                if ((attributes & FileAttributes.Directory) != 0)
                {
                    var name = Path.GetFileName(path);
                    if (name is not (".astra" or ".git" or "bin" or "obj" or "Library" or "Packages")) pending.Push(path);
                    continue;
                }
                if (!path.EndsWith(".cs", StringComparison.OrdinalIgnoreCase)) continue;
                var length = new FileInfo(path).Length;
                if (length > MaximumSourceBytes || sources.Count >= MaximumSources || total + length > MaximumProjectBytes)
                    throw new IOException("Project source limits exceeded.");
                var text = File.ReadAllText(path, new UTF8Encoding(false, true));
                total += Encoding.UTF8.GetByteCount(text);
                sources.Add((Path.GetRelativePath(root, path).Replace('\\', '/'), text));
            }
        }
        sources.Sort((a, b) => StringComparer.Ordinal.Compare(a.Path, b.Path)); return sources;
    }
    internal static string[] ReferencePaths()
    {
        var paths = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        void Add(string path)
        {
            if (!File.Exists(path)) return;
            // Native libraries can share the host's runtime directory/probing
            // list. They are not C# references and must not abort compilation.
            string? name;
            try { name = System.Reflection.AssemblyName.GetAssemblyName(path).Name; }
            catch (BadImageFormatException) { return; }
            if (name is not null) paths.TryAdd(name, path);
        }
        // Prefer the host's resolved assembly identity. Adding all runtime DLLs
        // unconditionally would mix framework v8 and package v9 identities.
        if (AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES") is string trusted)
            foreach (var path in trusted.Split(Path.PathSeparator)) Add(path);
        var runtimeDirectory = Path.GetDirectoryName(typeof(object).Assembly.Location)!;
        foreach (var path in Directory.EnumerateFiles(runtimeDirectory, "*.dll")) Add(path);
        Add(typeof(Behavior).Assembly.Location);
        return paths.Values.Order(StringComparer.Ordinal).ToArray();
    }
    private static ScriptDiagnostic ConvertDiagnostic(Diagnostic diagnostic, string root)
    {
        var span = diagnostic.Location.GetLineSpan(); var file = span.Path;
        if (Path.IsPathRooted(file)) file = Path.GetRelativePath(root, file);
        return new(file.Replace('\\', '/'), span.StartLinePosition.Line + 1, span.StartLinePosition.Character + 1,
            diagnostic.Id, diagnostic.GetMessage(CultureInfo.InvariantCulture), diagnostic.Severity == DiagnosticSeverity.Error);
    }
    private static ScriptTypeSchema[] ExtractSchemas(CSharpCompilation compilation, string root,
        List<ScriptDiagnostic> errors, CancellationToken cancellation)
    {
        var types = new List<ScriptTypeSchema>(); var identities = new HashSet<string>(StringComparer.Ordinal);
        var behavior = compilation.GetTypeByMetadataName("Astra.Behavior");
        foreach (var tree in compilation.SyntaxTrees)
        {
            var model = compilation.GetSemanticModel(tree);
            foreach (var syntax in tree.GetRoot(cancellation).DescendantNodes().OfType<Microsoft.CodeAnalysis.CSharp.Syntax.ClassDeclarationSyntax>())
            {
                if (model.GetDeclaredSymbol(syntax, cancellation) is not INamedTypeSymbol type || type.IsAbstract) continue;
                bool derived = false;
                for (var parent = type.BaseType; parent is not null; parent = parent.BaseType)
                    if (SymbolEqualityComparer.Default.Equals(parent, behavior)) derived = true;
                if (!derived) continue;
                // Partial declarations share one type and are emitted once.
                if (!SymbolEqualityComparer.Default.Equals(type, model.GetDeclaredSymbol(syntax, cancellation)) ||
                    type.DeclaringSyntaxReferences[0].Span != syntax.Span || type.DeclaringSyntaxReferences[0].SyntaxTree != tree) continue;
                var location = syntax.GetLocation().GetLineSpan();
                void Error(string message) => errors.Add(new(tree.FilePath, location.StartLinePosition.Line + 1,
                    location.StartLinePosition.Character + 1, "ASTRA_SCHEMA", message, true));
                if (type.DeclaredAccessibility != Accessibility.Public || type.TypeParameters.Length != 0 || type.ContainingType is not null)
                { Error("Behavior types must be public, non-generic top-level classes."); continue; }
                if (!type.InstanceConstructors.Any(c => c.DeclaredAccessibility == Accessibility.Public && c.Parameters.Length == 0))
                { Error("Behavior requires a public parameterless constructor."); continue; }
                var id = AttributeId(type, "Astra.ComponentIdAttribute");
                if (!ValidId(id) || !identities.Add(id!)) { Error("Behavior requires a unique [ComponentId] with a stable ID."); continue; }
                var properties = new List<ScriptPropertySchema>(); var propertyIds = new HashSet<string>(StringComparer.Ordinal);
                var propertyNames = new HashSet<string>(StringComparer.Ordinal);
                for (var current = type; current is not null && !SymbolEqualityComparer.Default.Equals(current, behavior); current = current.BaseType)
                foreach (var member in current.GetMembers())
                {
                    var propertyId = AttributeId(member, "Astra.PropertyIdAttribute"); if (propertyId is null) continue;
                    var valueType = member switch
                    {
                        IFieldSymbol field when !field.IsReadOnly && !field.IsStatic &&
                            (field.DeclaredAccessibility == Accessibility.Public || AttributeFlag(field, "Astra.SerializeFieldAttribute")) => field.Type,
                        IPropertySymbol property when !property.IsStatic && !property.IsIndexer && property.GetMethod?.DeclaredAccessibility == Accessibility.Public && property.SetMethod?.DeclaredAccessibility == Accessibility.Public => property.Type,
                        _ => null
                    };
                    var name = valueType is null ? null : ColorFlags(PropertyKind(valueType), member);
                    if (!ValidId(propertyId) || !propertyIds.Add(propertyId) || !propertyNames.Add(member.Name) || valueType is null || !Supported(valueType))
                    { Error("Unsupported, duplicate or inaccessible [PropertyId]: " + member.Name); continue; }
                    properties.Add(new(propertyId, member.Name, name!, AttributeFlag(member, "Astra.HideInInspectorAttribute")));
                }
                types.Add(new(id!, type.ToDisplayString(), tree.FilePath, properties.ToArray()));
            }
        }
        _ = root;
        return types.OrderBy(t => t.Id, StringComparer.Ordinal).ToArray();
    }
    // Campo de componente (Unity: `public Rigidbody body;`): a fachada gerada em
    // Astra.Components, direta ou anulável. O id do tipo vem da própria fachada,
    // que mora neste assembly, então não há segunda tabela de nomes.
    private static string? FacadeTypeId(ITypeSymbol type)
    {
        if (type is INamedTypeSymbol { OriginalDefinition.SpecialType: SpecialType.System_Nullable_T } nullable)
            type = nullable.TypeArguments[0];
        if (type.TypeKind != TypeKind.Struct || type.ContainingNamespace?.ToDisplayString() != "Astra.Components") return null;
        var facade = typeof(Behavior).Assembly.GetType(type.ToDisplayString());
        if (facade is null || !facade.GetInterfaces().Any(i => i.IsGenericType && i.GetGenericTypeDefinition() == typeof(Astra.Components.IComponentFacade<>)))
            return null;
        return facade.GetProperty("TypeId", BindingFlags.Public | BindingFlags.Static)?.GetValue(null) as string;
    }
    // Lista (Unity Manual/InspectorArray): `T[]` ou `List<T>` de um tipo de
    // campo aceito; lista de lista não existe no Inspector.
    private static ITypeSymbol? ArrayElement(ITypeSymbol type) => type switch
    {
        IArrayTypeSymbol { Rank: 1 } array => array.ElementType,
        INamedTypeSymbol { IsGenericType: true } list when list.OriginalDefinition.ToDisplayString() == "System.Collections.Generic.List<T>" => list.TypeArguments[0],
        _ => null
    };
    // [ColorUsage(showAlpha, hdr)] vira marcas no tipo "color" (também nos
    // elementos de uma lista de cores).
    private static string ColorFlags(string kind, ISymbol member)
    {
        if (kind is "gradient" or "array:gradient")
        {
            var gradient = member.GetAttributes().FirstOrDefault(a => a.AttributeClass?.ToDisplayString() == "Astra.GradientUsageAttribute");
            return gradient?.ConstructorArguments.FirstOrDefault().Value is true ? kind + ":hdr" : kind;
        }
        if (kind != "color" && kind != "array:color") return kind;
        var usage = member.GetAttributes().FirstOrDefault(a => a.AttributeClass?.ToDisplayString() == "Astra.ColorUsageAttribute");
        if (usage is null) return kind;
        var arguments = usage.ConstructorArguments;
        var alpha = arguments.Length > 0 && arguments[0].Value is bool a ? a : true;
        var hdr = arguments.Length > 1 && arguments[1].Value is bool h && h;
        return kind + (hdr ? ":hdr" : "") + (alpha ? "" : ":noalpha");
    }
    private static bool AttributeFlag(ISymbol symbol, string attribute) =>
        symbol.GetAttributes().Any(a => a.AttributeClass?.ToDisplayString() == attribute);
    private static string PropertyKind(ITypeSymbol type) => ArrayElement(type) is { } element ? "array:" + ScalarKind(element) : ScalarKind(type);
    private static string ScalarKind(ITypeSymbol type) => FacadeTypeId(type) is { } component ? "component:" + component :
        type.TypeKind == TypeKind.Enum ? "enum" :
        type.SpecialType switch
        {
            SpecialType.System_Boolean => "bool", SpecialType.System_Int32 => "int32",
            SpecialType.System_Single => "float", SpecialType.System_String => "string",
            _ => type.ToDisplayString() switch
            {
                "System.Numerics.Vector3" => "vector3", "Astra.ObjectReference" => "object", "Astra.Color" => "color",
                "Astra.Gradient" => "gradient", "Astra.AnimationCurve" => "curve",
                "Astra.AssetReference" => "asset", _ => "unsupported"
            }
        };
    private static bool Supported(ITypeSymbol type) => ArrayElement(type) is { } element
        ? ArrayElement(element) is null && ScalarSupported(element)
        : ScalarSupported(type);
    private static bool ScalarSupported(ITypeSymbol type) => (type is INamedTypeSymbol { TypeKind: TypeKind.Enum, EnumUnderlyingType.SpecialType: SpecialType.System_Int32 }) ||
        type.SpecialType is SpecialType.System_Boolean or SpecialType.System_Int32 or SpecialType.System_Single or SpecialType.System_String ||
        type.ToDisplayString() is "System.Numerics.Vector3" or "Astra.ObjectReference" or "Astra.AssetReference" or "Astra.Color" or "Astra.Gradient" or "Astra.AnimationCurve" ||
        FacadeTypeId(type) is not null;
    private static string? AttributeId(ISymbol symbol, string attribute) => symbol.GetAttributes()
        .FirstOrDefault(a => a.AttributeClass?.ToDisplayString() == attribute)?.ConstructorArguments.FirstOrDefault().Value as string;
    private static bool ValidId(string? id) => !string.IsNullOrWhiteSpace(id) && id.Length <= 256 &&
        id.All(c => char.IsAsciiLetterOrDigit(c) || c is '.' or '_' or '-' or '/');
}
