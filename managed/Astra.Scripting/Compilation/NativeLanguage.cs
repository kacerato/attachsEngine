using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Text;

namespace Astra.Compilation;

public sealed record LanguageDocument(string Path, string Text);
public sealed record LanguageRequest(string Root, string File, int Position, int Operation,
    string Query, LanguageDocument[] Documents);
public sealed record LanguageItem(string Label, string Detail, string Insert, int Start, int Length,
    string File, int Line, int Column);
public sealed record LanguageResult(string Message, LanguageItem[] Items, bool Truncated = false);

/// <summary>C# semantic queries use the compiler's references and open-buffer overlays.
/// Never emits an assembly, instantiates a Behavior, saves a file or publishes a catalog.</summary>
public static class ProjectLanguage
{
    private static readonly Lazy<MetadataReference[]> References = new(() => ProjectCompiler.ReferencePaths()
        .Select(p => MetadataReference.CreateFromFile(p)).ToArray());
    private sealed record Analysis(string Root, Dictionary<string, string> Sources,
        Dictionary<string, SyntaxTree> Trees, CSharpCompilation Compilation);
    private static readonly object CacheGate = new();
    private static Analysis? _analysis;
    private static CSharpCompilation Analyze(string projectRoot, Dictionary<string,string> sources,
        CancellationToken cancellation)
    {
        Analysis? previous;
        lock(CacheGate) previous=_analysis;
        if(previous?.Root!=projectRoot) previous=null;
        var compilation=previous?.Compilation ?? CSharpCompilation.Create("Astra.Language",
            references:References.Value, options:new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary,
                allowUnsafe:false,nullableContextOptions:NullableContextOptions.Enable));
        var trees=previous==null?new Dictionary<string,SyntaxTree>(StringComparer.Ordinal)
            :new Dictionary<string,SyntaxTree>(previous.Trees,StringComparer.Ordinal);
        foreach(var removed in trees.Keys.Where(path=>!sources.ContainsKey(path)).ToArray())
        { cancellation.ThrowIfCancellationRequested();compilation=compilation.RemoveSyntaxTrees(trees[removed]);trees.Remove(removed); }
        foreach(var (path,text) in sources)
        {
            cancellation.ThrowIfCancellationRequested();
            if(previous!=null && previous.Sources.TryGetValue(path,out var oldText) && oldText==text) continue;
            if(trees.TryGetValue(path,out var oldTree))
            {
                var next=oldTree.WithChangedText(SourceText.From(text,Encoding.UTF8));
                compilation=compilation.ReplaceSyntaxTree(oldTree,next);trees[path]=next;
            }
            else
            {
                var next=CSharpSyntaxTree.ParseText(text,new CSharpParseOptions(LanguageVersion.CSharp12),path,Encoding.UTF8,cancellation);
                compilation=compilation.AddSyntaxTrees(next);trees[path]=next;
            }
        }
        cancellation.ThrowIfCancellationRequested();
        // A bounded single-project immutable snapshot. Concurrent requests can
        // finish in either order without sharing mutable trees or semantic models.
        lock(CacheGate) _analysis=new(projectRoot,sources,trees,compilation);
        return compilation;
    }
    public static LanguageResult Query(LanguageRequest request, CancellationToken cancellation = default)
    {
        var inputs = ProjectCompiler.ReadSources(Path.GetFullPath(request.Root), cancellation)
            .ToDictionary(s => s.Path, s => s.Text, StringComparer.Ordinal);
        if (request.Documents.Length > 16) throw new IOException("Muitos buffers abertos.");
        foreach (var document in request.Documents)
        {
            cancellation.ThrowIfCancellationRequested();
            if (Path.IsPathRooted(document.Path) || document.Path.Split('/').Any(p => p is ".." or ".") ||
                !document.Path.EndsWith(".cs", StringComparison.OrdinalIgnoreCase)) continue;
            if (Encoding.UTF8.GetByteCount(document.Text) > ProjectCompiler.MaximumSourceBytes)
                throw new IOException("Arquivo maior que 512 KiB.");
            inputs[document.Path] = document.Text;
        }
        if (inputs.Count > ProjectCompiler.MaximumSources || inputs.Values.Sum(s => (long)Encoding.UTF8.GetByteCount(s)) > ProjectCompiler.MaximumProjectBytes)
            throw new IOException("Limite de fontes do projeto excedido.");
        if (request.Operation == 2) return Search(inputs, request.Query, cancellation);
        if (!inputs.TryGetValue(request.File, out var text)) return new("Abra um arquivo C# do projeto.", []);
        int position = Math.Clamp(request.Position, 0, text.Length);
        var compilation=Analyze(Path.GetFullPath(request.Root),inputs,cancellation);
        var tree = compilation.SyntaxTrees.First(t => t.FilePath == request.File);
        var root = tree.GetRoot(cancellation);
        var model = compilation.GetSemanticModel(tree);
        if (request.Operation == 1)
        {
            var token = root.FindToken(Math.Min(position, Math.Max(0, text.Length - 1)));
            if (token.SpanStart == position && position > 0 && !token.IsKind(SyntaxKind.IdentifierToken))
                token = root.FindToken(position - 1);
            ISymbol? symbol = null;
            foreach (var node in token.Parent?.AncestorsAndSelf() ?? [])
            {
                cancellation.ThrowIfCancellationRequested();
                if (node is ExpressionSyntax expression) symbol = model.GetSymbolInfo(expression, cancellation).Symbol;
                symbol ??= model.GetDeclaredSymbol(node, cancellation);
                if (symbol != null) break;
            }
            if (symbol is IAliasSymbol alias) symbol = alias.Target;
            if (symbol is IMethodSymbol method) symbol = method.ReducedFrom ?? method;
            symbol = symbol?.OriginalDefinition;
            var items = symbol?.Locations.Where(l => l.IsInSource).Select(l => {
                var span = l.GetLineSpan();
                return new LanguageItem(symbol.Name, symbol.ToDisplayString(), "", 0, 0,
                    span.Path, span.StartLinePosition.Line + 1, span.StartLinePosition.Character + 1);
            }).Take(64).ToArray() ?? [];
            return new(items.Length > 0 ? "Definições" : symbol != null
                ? "Símbolo da API compilada: " + symbol.ToDisplayString() : "Definição não resolvida neste ponto.", items);
        }
        int start = position;
        while (start > 0 && (SyntaxFacts.IsIdentifierPartCharacter(text[start - 1]) || text[start - 1] == '@')) --start;
        int end = position;
        while (end < text.Length && SyntaxFacts.IsIdentifierPartCharacter(text[end])) ++end;
        var prefix = text[start..position].TrimStart('@');
        var near = root.FindToken(Math.Max(0, position - 1));
        if (near.IsKind(SyntaxKind.StringLiteralToken) || near.IsKind(SyntaxKind.CharacterLiteralToken) ||
            near.Parent?.AncestorsAndSelf().Any(n => n is InterpolatedStringTextSyntax) == true)
            return new("", []);
        INamespaceOrTypeSymbol? container = null;
        bool member = start > 0 && text[start - 1] == '.';
        bool staticReceiver = false;
        if (member)
        {
            var dot = root.FindToken(start - 1);
            if (dot.Parent is MemberAccessExpressionSyntax access)
            {
                var receiver = model.GetSymbolInfo(access.Expression, cancellation).Symbol;
                staticReceiver = receiver is INamespaceOrTypeSymbol;
                container = receiver as INamespaceOrTypeSymbol ?? model.GetTypeInfo(access.Expression, cancellation).Type;
            }
            if (container == null) return new("Tipo do receptor ainda não resolvido.", []);
        }
        cancellation.ThrowIfCancellationRequested();
        var candidates = model.LookupSymbols(position, container, includeReducedExtensionMethods: true)
            .Where(s => !s.IsImplicitlyDeclared && s.CanBeReferencedByName &&
                s.Name.StartsWith(prefix, StringComparison.OrdinalIgnoreCase) &&
                (!member || (staticReceiver ? s.IsStatic || s is INamespaceOrTypeSymbol : !s.IsStatic)))
            .Where(s => s is not IMethodSymbol m || m.MethodKind is MethodKind.Ordinary or MethodKind.ReducedExtension)
            .OrderBy(s => s.Name, StringComparer.Ordinal).ThenBy(s => s.ToDisplayString(), StringComparer.Ordinal)
            .GroupBy(s => s.Name).ToArray();
        var completions = candidates.Take(80).Select(group => {
            var symbol = group.First(); var name = symbol.Name;
            var insert = SyntaxFacts.GetKeywordKind(name) != SyntaxKind.None ? "@" + name : name;
            return new LanguageItem(name, symbol.ToDisplayString(SymbolDisplayFormat.MinimallyQualifiedFormat) +
                (group.Count() > 1 ? " · " + group.Count() + " assinaturas" : ""), insert,
                start, end - start, "", 0, 0);
        }).ToArray();
        return new("C# · símbolos disponíveis", completions, candidates.Length > 80);
    }
    private static LanguageResult Search(Dictionary<string, string> sources, string query, CancellationToken cancellation)
    {
        if (string.IsNullOrEmpty(query)) return new("Digite o texto para buscar nas fontes C#.", []);
        var matches = new List<LanguageItem>();
        foreach (var (file, text) in sources.OrderBy(s => s.Key, StringComparer.Ordinal))
        {
            int position = 0, scanned = 0, line = 1, lineStart = 0;
            while ((position = text.IndexOf(query, position, StringComparison.OrdinalIgnoreCase)) >= 0)
            {
                cancellation.ThrowIfCancellationRequested();
                while (scanned < position) if (text[scanned++] == '\n') { ++line; lineStart = scanned; }
                int end = text.IndexOf('\n', position); if (end < 0) end = text.Length;
                matches.Add(new(file + ":" + line, text[lineStart..Math.Min(end, lineStart + 180)].Trim(),
                    "", 0, 0, file, line, position - lineStart + 1));
                if (matches.Count == 200) return new("Busca nas fontes C# · refine para ver mais", matches.ToArray(), true);
                position += query.Length;
            }
        }
        return new(matches.Count + " ocorrências nas fontes C#", matches.ToArray());
    }
}

/// <summary>Single native worker, bounded reply and cooperative cancellation. The gate
/// protects only ownership; semantic work never holds it or the build publication gate.</summary>
public static class NativeLanguage
{
    private static readonly object Gate = new();
    private static CancellationTokenSource? _cancellation;
    private static byte[] _reply = [];
    [UnmanagedCallersOnly]
    public static void Cancel() { lock (Gate) _cancellation?.Cancel(); }
    [UnmanagedCallersOnly]
    public static unsafe int Query(byte* data, int length)
    {
        using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(8));
        lock (Gate) _cancellation = cancellation;
        LanguageResult result;
        try
        {
            if (data == null || length < 1 || length > 9 * 1024 * 1024) throw new IOException("Pedido de linguagem inválido.");
            var request = JsonSerializer.Deserialize<LanguageRequest>(new ReadOnlySpan<byte>(data, length))
                ?? throw new IOException("Pedido vazio.");
            result = ProjectLanguage.Query(request, cancellation.Token);
        }
        catch (OperationCanceledException) { result = new("Consulta cancelada; tente novamente.", []); }
        catch (Exception error) { result = new("Análise indisponível: " + error.Message, []); }
        lock (Gate) { _reply = JsonSerializer.SerializeToUtf8Bytes(result); _cancellation = null; }
        return 0;
    }
    [UnmanagedCallersOnly]
    public static unsafe int CopyReply(byte* destination, int capacity)
    {
        lock (Gate)
        {
            if (destination == null || capacity < _reply.Length) return _reply.Length;
            _reply.CopyTo(new Span<byte>(destination, capacity)); return _reply.Length;
        }
    }
}
