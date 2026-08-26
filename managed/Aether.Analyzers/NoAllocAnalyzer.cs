using System.Collections.Immutable;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Diagnostics;

namespace Aether.Analyzers;

/// <summary>
/// Item 1.2.5 do plano: sinaliza estaticamente padrões de alocação de heap gerenciado dentro de
/// um método/propriedade marcado <c>[Aether.Diagnostics.NoAlloc]</c> (docs/CONVENCOES.md §3).
/// <para>
/// <b>Escopo deliberado — o que detecta:</b> os padrões mais diretos e de baixo falso-positivo,
/// os mesmos citados explicitamente em CONVENCOES.md §3: <c>new</c> de tipo referência
/// (AETH001), lambda/método de grupo capturante (AETH002), LINQ (AETH003 — quase todo método de
/// <c>System.Linq</c> aloca um iterador ou coleção intermediária), concatenação/interpolação de
/// string além de um literal (AETH004), <c>foreach</c> sobre uma expressão cujo tipo estático é
/// uma interface, não um tipo concreto (AETH005 — o enumerador pode ser boxed se o tipo real for
/// struct, mas a análise estática não sabe o tipo em runtime; sinaliza o caso pessimista).
/// </para>
/// <para>
/// <b>O que NÃO detecta (falsos negativos aceitos, documentados em vez de fingidos):</b> boxing
/// implícito em atribuição a <c>object</c>/interface fora de <c>foreach</c>, alocação dentro de
/// um método CHAMADO pelo método marcado (a análise é local ao corpo, não faz interprocedural —
/// isso exigiria conhecer se o método chamado também é <c>[NoAlloc]</c> em toda a árvore de
/// chamada, análise bem mais cara e fora do escopo desta fatia), `params object[]` implícito.
/// Um método marcado <c>[NoAlloc]</c> que chama outro método não marcado que aloca não é pego
/// por este analisador — é responsabilidade de quem escreve <c>[NoAlloc]</c> também marcar (ou
/// verificar) as dependências, mesma disciplina que `Assert.NoAlloc` em teste já teria que
/// cobrir chamando o código de verdade.
/// </para>
/// </summary>
[DiagnosticAnalyzer(LanguageNames.CSharp)]
public sealed class NoAllocAnalyzer : DiagnosticAnalyzer
{
    private const string Category = "Performance";
    private const string NoAllocAttributeFullName = "Aether.Diagnostics.NoAllocAttribute";

    public static readonly DiagnosticDescriptor NewObjectRule = new(
        id: "AETH001",
        title: "Alocação de objeto em método [NoAlloc]",
        messageFormat: "'{0}' aloca um objeto no heap gerenciado dentro de um método marcado [NoAlloc]",
        category: Category,
        defaultSeverity: DiagnosticSeverity.Error,
        isEnabledByDefault: true,
        description: "new de tipo referência (class, delegate, array) sempre aloca no heap gerenciado.");

    public static readonly DiagnosticDescriptor CapturingLambdaRule = new(
        id: "AETH002",
        title: "Lambda/método de grupo capturante em método [NoAlloc]",
        messageFormat: "Esta expressão lambda/conversão de método captura variável externa, alocando uma closure, dentro de um método marcado [NoAlloc]",
        category: Category,
        defaultSeverity: DiagnosticSeverity.Error,
        isEnabledByDefault: true,
        description: "Uma lambda que referencia uma variável local, parâmetro ou 'this' do escopo externo aloca um objeto de closure no heap.");

    public static readonly DiagnosticDescriptor LinqRule = new(
        id: "AETH003",
        title: "Uso de LINQ em método [NoAlloc]",
        messageFormat: "'{0}' é um método de LINQ (System.Linq) e tipicamente aloca um iterador ou coleção intermediária, dentro de um método marcado [NoAlloc]",
        category: Category,
        defaultSeverity: DiagnosticSeverity.Error,
        isEnabledByDefault: true,
        description: "Métodos de extensão de System.Linq quase sempre alocam (iterador de yield, ou a coleção de destino de ToList/ToArray/etc.).");

    public static readonly DiagnosticDescriptor StringConcatenationRule = new(
        id: "AETH004",
        title: "Concatenação/interpolação de string em método [NoAlloc]",
        messageFormat: "Concatenação ou interpolação de string aloca uma nova string no heap, dentro de um método marcado [NoAlloc]",
        category: Category,
        defaultSeverity: DiagnosticSeverity.Error,
        isEnabledByDefault: true,
        description: "string é imutável — concatenar ou interpolar sempre produz uma nova alocação, mesmo quando o resultado nunca é usado.");

    public static readonly DiagnosticDescriptor ForeachOverInterfaceRule = new(
        id: "AETH005",
        title: "foreach sobre interface em método [NoAlloc]",
        messageFormat: "'foreach' sobre uma expressão de tipo de interface pode fazer boxing do enumerador se o tipo em runtime for um struct, dentro de um método marcado [NoAlloc]",
        category: Category,
        defaultSeverity: DiagnosticSeverity.Warning,
        isEnabledByDefault: true,
        description: "Quando o tipo ESTÁTICO da expressão iterada é uma interface (IEnumerable<T>, etc.), o compilador não pode usar o padrão de enumerador por valor mesmo que o tipo concreto em runtime seja uma struct — o enumerador é boxed. Prefira Span<T>/um tipo concreto/CompiledQuery.");

    public override ImmutableArray<DiagnosticDescriptor> SupportedDiagnostics { get; } =
        ImmutableArray.Create(NewObjectRule, CapturingLambdaRule, LinqRule, StringConcatenationRule, ForeachOverInterfaceRule);

    public override void Initialize(AnalysisContext context)
    {
        context.ConfigureGeneratedCodeAnalysis(GeneratedCodeAnalysisFlags.None);
        context.EnableConcurrentExecution();

        context.RegisterSyntaxNodeAction(AnalyzeMethod, SyntaxKind.MethodDeclaration);
        context.RegisterSyntaxNodeAction(AnalyzeAccessor, SyntaxKind.GetAccessorDeclaration, SyntaxKind.SetAccessorDeclaration);
    }

    private static void AnalyzeMethod(SyntaxNodeAnalysisContext context)
    {
        var method = (MethodDeclarationSyntax)context.Node;
        if (method.Body is null && method.ExpressionBody is null) return; // interface/abstract/partial sem corpo
        AnalyzeIfMarked(context, method, context.SemanticModel.GetDeclaredSymbol(method));
    }

    private static void AnalyzeAccessor(SyntaxNodeAnalysisContext context)
    {
        var accessor = (AccessorDeclarationSyntax)context.Node;
        if (accessor.Body is null && accessor.ExpressionBody is null) return;
        // O atributo [NoAlloc] é declarado na PROPRIEDADE, não no accessor individual — sobe um
        // nível para achar o símbolo certo a checar.
        if (accessor.Parent?.Parent is not PropertyDeclarationSyntax property) return;
        AnalyzeIfMarked(context, accessor, context.SemanticModel.GetDeclaredSymbol(property));
    }

    private static void AnalyzeIfMarked(SyntaxNodeAnalysisContext context, SyntaxNode bodyOwner, ISymbol? symbol)
    {
        if (symbol is null || !HasNoAllocAttribute(symbol)) return;

        foreach (var node in bodyOwner.DescendantNodes())
        {
            switch (node)
            {
                case ObjectCreationExpressionSyntax objectCreation:
                    ReportIfReferenceType(context, objectCreation, context.SemanticModel.GetTypeInfo(objectCreation).Type, NewObjectRule);
                    break;

                case ImplicitObjectCreationExpressionSyntax implicitCreation: // `new(...)` target-typed
                    ReportIfReferenceType(context, implicitCreation, context.SemanticModel.GetTypeInfo(implicitCreation).Type, NewObjectRule);
                    break;

                case ArrayCreationExpressionSyntax arrayCreation:
                    context.ReportDiagnostic(Diagnostic.Create(NewObjectRule, arrayCreation.GetLocation(), arrayCreation.ToString()));
                    break;

                case ImplicitArrayCreationExpressionSyntax implicitArrayCreation:
                    context.ReportDiagnostic(Diagnostic.Create(NewObjectRule, implicitArrayCreation.GetLocation(), implicitArrayCreation.ToString()));
                    break;

                case LambdaExpressionSyntax lambda:
                    if (CapturesOuterVariable(context, lambda))
                        context.ReportDiagnostic(Diagnostic.Create(CapturingLambdaRule, lambda.GetLocation()));
                    break;

                case InvocationExpressionSyntax invocation:
                    ReportIfLinq(context, invocation);
                    break;

                case BinaryExpressionSyntax binary when binary.IsKind(SyntaxKind.AddExpression):
                    ReportIfStringConcat(context, binary);
                    break;

                case InterpolatedStringExpressionSyntax interpolated:
                    // Interpolação sem NENHUM {expressão} é efetivamente um literal constante
                    // (o compilador às vezes já otimiza para string.Empty/literal) — só
                    // sinaliza quando há de fato conteúdo interpolado, que força concatenação.
                    if (interpolated.Contents.Any(c => c is InterpolationSyntax))
                        context.ReportDiagnostic(Diagnostic.Create(StringConcatenationRule, interpolated.GetLocation()));
                    break;

                case ForEachStatementSyntax forEach:
                    ReportIfForeachOverInterface(context, forEach);
                    break;
            }
        }
    }

    private static bool HasNoAllocAttribute(ISymbol symbol) =>
        symbol.GetAttributes().Any(a =>
            a.AttributeClass?.ToDisplayString() == NoAllocAttributeFullName);

    private static void ReportIfReferenceType(SyntaxNodeAnalysisContext context, SyntaxNode node, ITypeSymbol? type, DiagnosticDescriptor rule)
    {
        // Structs (incluindo os já usados no projeto: float3, PhysicsBodyHandle, ConfigValue...)
        // não alocam no heap com `new` — só tipos referência (class, delegate, record class).
        if (type is null || !type.IsReferenceType) return;
        context.ReportDiagnostic(Diagnostic.Create(rule, node.GetLocation(), node.ToString()));
    }

    private static bool CapturesOuterVariable(SyntaxNodeAnalysisContext context, LambdaExpressionSyntax lambda)
    {
        var dataFlow = context.SemanticModel.AnalyzeDataFlow(lambda);
        if (dataFlow is null || !dataFlow.Succeeded) return false;
        // CapturedInside inclui qualquer identificador de fora do lambda referenciado dentro
        // dele (variável local, parâmetro do método externo, ou implicitamente 'this') — é
        // exatamente a condição que força o compilador a gerar uma classe de closure alocada.
        return !dataFlow.CapturedInside.IsEmpty;
    }

    private static readonly string[] LinqNamespacePrefixes = { "System.Linq" };

    private static void ReportIfLinq(SyntaxNodeAnalysisContext context, InvocationExpressionSyntax invocation)
    {
        var symbolInfo = context.SemanticModel.GetSymbolInfo(invocation);
        var method = symbolInfo.Symbol as IMethodSymbol;
        if (method is null) return;

        string? containingNamespace = method.ContainingType?.ContainingNamespace?.ToDisplayString();
        if (containingNamespace is null) return;
        if (LinqNamespacePrefixes.Any(prefix => containingNamespace == prefix || containingNamespace.StartsWith(prefix + ".", StringComparison.Ordinal)))
        {
            context.ReportDiagnostic(Diagnostic.Create(LinqRule, invocation.GetLocation(), method.Name));
        }
    }

    private static void ReportIfStringConcat(SyntaxNodeAnalysisContext context, BinaryExpressionSyntax binary)
    {
        var typeInfo = context.SemanticModel.GetTypeInfo(binary);
        if (typeInfo.Type?.SpecialType == SpecialType.System_String)
        {
            context.ReportDiagnostic(Diagnostic.Create(StringConcatenationRule, binary.GetLocation()));
        }
    }

    private static void ReportIfForeachOverInterface(SyntaxNodeAnalysisContext context, ForEachStatementSyntax forEach)
    {
        var typeInfo = context.SemanticModel.GetTypeInfo(forEach.Expression);
        if (typeInfo.Type?.TypeKind == TypeKind.Interface)
        {
            context.ReportDiagnostic(Diagnostic.Create(ForeachOverInterfaceRule, forEach.Expression.GetLocation()));
        }
    }
}
