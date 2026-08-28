using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Aether.Tests;

/// <summary>
/// Item 0.4.2 do plano: valida que os arquivos <c>docs/idl/*.idl</c> descrevem fielmente a
/// fronteira P/Invoke real (<c>Native*.cs</c>) — nenhuma função, enum ou struct pode divergir entre
/// o que o IDL declara e o que o C# efetivamente expõe. Ver <c>docs/idl/FORMATO-IDL.md</c> para a
/// especificação completa do formato e o motivo de ser descritivo (não gerador) — o risco real que
/// isto fecha é <b>divergência silenciosa</b> entre os dois lados da fronteira C#↔C++ (ADR-01), não
/// ausência de binding em si.
/// </summary>
public static class IdlValidationTests
{
    // As classes Native* são deliberadamente internal (NativePhysics.cs, NativeSqlite.cs,
    // NativeTransformKernel.cs — não devem ser chamadas fora de Aether.Physics/Resources/raiz).
    // Por isso âncoras aqui usam um tipo PÚBLICO qualquer do mesmo assembly para obter o Assembly,
    // e ValidateModule resolve o tipo internal de verdade via Assembly.GetType(nomeCompleto) —
    // reflection em runtime não é bloqueada por internal, só o acesso em tempo de compilação era.
    [Test] public static void Physics_IdlDescreveFielmenteOBindingReal()
        => ValidateModule("physics.idl", typeof(Aether.Physics.PhysicsWorld).Assembly);

    [Test] public static void Sqlite_IdlDescreveFielmenteOBindingReal()
        => ValidateModule("sqlite.idl", typeof(Aether.Resources.SqliteConnection).Assembly);

    [Test] public static void Transform_IdlDescreveFielmenteOBindingReal()
        => ValidateModule("transform.idl", typeof(Aether.float2).Assembly);

    [Test] public static void ParseIdl_ArquivoMinimoValido_ExtraiCabecalhoESecoes()
    {
        string conteudo = "library: teste_lib\nmanaged_type: Teste.Tipo\nnative_header: teste.h\n\n[functions]\nMinhaFuncao | x: int | returns: void\n";
        var doc = IdlDocument.Parse(conteudo, "<inline>");

        Assert.Equal("teste_lib", doc.Library, what: "cabeçalho library deve ser extraído");
        Assert.Equal("Teste.Tipo", doc.ManagedType, what: "cabeçalho managed_type deve ser extraído");
        Assert.Equal(1, doc.Functions.Count, what: "uma função declarada na seção [functions]");
        Assert.Equal("MinhaFuncao", doc.Functions[0].Name, what: "nome da função deve bater");
        Assert.Equal(1, doc.Functions[0].Parameters.Count, what: "um parâmetro declarado");
        Assert.Equal("void", doc.Functions[0].ReturnType, what: "tipo de retorno deve bater");
    }

    [Test] public static void ParseIdl_FuncaoSemParametros_ListaVazia()
    {
        string conteudo = "library: t\nmanaged_type: T\nnative_header: t.h\n\n[functions]\nSemParametros |  | returns: int\n";
        var doc = IdlDocument.Parse(conteudo, "<inline>");
        Assert.Equal(0, doc.Functions[0].Parameters.Count, what: "seção de parâmetros vazia deve virar lista vazia, não uma entrada fantasma");
    }

    [Test] public static void ParseIdl_EnumComFlags_ExtraiCorretamente()
    {
        string conteudo = "library: t\nmanaged_type: T\nnative_header: t.h\n\n[enums]\nMeuEnum | uint32 | flags=true | A, B, C\n";
        var doc = IdlDocument.Parse(conteudo, "<inline>");
        Assert.Equal(1, doc.Enums.Count, what: "um enum declarado");
        Assert.True(doc.Enums[0].Flags, what: "flags=true deve virar bool true");
        Assert.Equal(3, doc.Enums[0].Values.Count, what: "três membros de enum listados");
    }

    [Test] public static void ParseIdl_LinhaDeComentario_EhIgnorada()
    {
        string conteudo = "library: t\nmanaged_type: T\nnative_header: t.h\n# isto é um comentário\n\n[functions]\n# outro comentário\nF |  | returns: void\n";
        var doc = IdlDocument.Parse(conteudo, "<inline>");
        Assert.Equal(1, doc.Functions.Count, what: "linhas de comentário não devem virar entradas");
    }

    private static void ValidateModule(string idlFileName, Assembly assembly)
    {
        string idlPath = Path.Combine(RepoRoot(), "docs", "idl", idlFileName);
        if (!File.Exists(idlPath)) throw new AssertException($"arquivo IDL não encontrado: {idlPath}");

        var doc = IdlDocument.Parse(File.ReadAllText(idlPath), idlPath);
        Type? nativeType = assembly.GetType(doc.ManagedType);
        if (nativeType is null)
            throw new AssertException($"{idlFileName}: managed_type '{doc.ManagedType}' não existe no assembly {assembly.GetName().Name}");

        ValidateEnums(doc, assembly, idlFileName);
        ValidateStructs(doc, assembly, idlFileName);
        ValidateFunctions(doc, nativeType, idlFileName);
    }

    private static void ValidateEnums(IdlDocument doc, Assembly assembly, string idlFileName)
    {
        foreach (var enumDecl in doc.Enums)
        {
            Type? type = FindType(assembly, enumDecl.Name);
            if (type is null || !type.IsEnum)
                throw new AssertException($"{idlFileName}: enum '{enumDecl.Name}' declarado no IDL não existe (ou não é enum) no assembly");

            bool hasFlags = type.GetCustomAttribute<FlagsAttribute>() is not null;
            if (hasFlags != enumDecl.Flags)
                throw new AssertException($"{idlFileName}: enum '{enumDecl.Name}' — IDL declara flags={enumDecl.Flags}, tipo real tem [Flags]={hasFlags}");

            string[] realNames = Enum.GetNames(type);
            if (!realNames.SequenceEqual(enumDecl.Values))
                throw new AssertException($"{idlFileName}: enum '{enumDecl.Name}' — IDL lista [{string.Join(", ", enumDecl.Values)}], tipo real tem [{string.Join(", ", realNames)}]");
        }
    }

    private static void ValidateStructs(IdlDocument doc, Assembly assembly, string idlFileName)
    {
        foreach (var structDecl in doc.Structs)
        {
            Type? type = FindType(assembly, structDecl.Name);
            if (type is null || !type.IsValueType || type.IsEnum)
                throw new AssertException($"{idlFileName}: struct '{structDecl.Name}' declarado no IDL não existe (ou não é struct) no assembly");

            // Campos de struct de fronteira nem sempre são public (ex.: NativeTransformPlanEntry usa
            // internal readonly — o layout sequencial importa para a ABI, a visibilidade C# não).
            string[] realFields = type.GetFields(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance)
                .Where(f => !f.IsStatic)
                .Select(f => f.Name).ToArray();
            if (!realFields.SequenceEqual(structDecl.Fields))
                throw new AssertException($"{idlFileName}: struct '{structDecl.Name}' — IDL lista campos [{string.Join(", ", structDecl.Fields)}], tipo real tem [{string.Join(", ", realFields)}]");
        }
    }

    private static void ValidateFunctions(IdlDocument doc, Type nativeType, string idlFileName)
    {
        // O IDL descreve o símbolo nativo exportado (o que a fronteira ABI de verdade resolve), não
        // o nome do método wrapper em C# — os dois só coincidem quando [LibraryImport] não usa
        // EntryPoint explícito (caso comum, mas não universal: NativeTransformKernel usa um nome de
        // método C# privado diferente do EntryPoint nativo real).
        var realMethods = nativeType.GetMethods(BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.Static)
            .Select(m => (Method: m, Attr: m.GetCustomAttributes().FirstOrDefault(a => a.GetType().Name == "LibraryImportAttribute")))
            .Where(x => x.Attr is not null)
            .ToDictionary(x => NativeEntryPointName(x.Method, x.Attr!), x => x.Method);

        var declaredNames = doc.Functions.Select(f => f.Name).ToHashSet();

        foreach (var missing in realMethods.Keys.Where(name => !declaredNames.Contains(name)))
            throw new AssertException($"{idlFileName}: função '{missing}' existe em {nativeType.Name} via [LibraryImport], mas não está declarada no IDL");

        foreach (var funcDecl in doc.Functions)
        {
            if (!realMethods.TryGetValue(funcDecl.Name, out var method))
                throw new AssertException($"{idlFileName}: função '{funcDecl.Name}' declarada no IDL não existe em {nativeType.Name} (ou não é [LibraryImport])");

            ParameterInfo[] realParams = method.GetParameters();
            if (realParams.Length != funcDecl.Parameters.Count)
                throw new AssertException($"{idlFileName}: função '{funcDecl.Name}' — IDL declara {funcDecl.Parameters.Count} parâmetro(s), assinatura real tem {realParams.Length}");

            for (int i = 0; i < realParams.Length; i++)
            {
                string expected = funcDecl.Parameters[i].Type;
                string actual = DescribeParameterType(realParams[i]);
                if (!TypesMatch(expected, actual))
                    throw new AssertException($"{idlFileName}: função '{funcDecl.Name}', parâmetro {i} ('{funcDecl.Parameters[i].Name}') — IDL declara '{expected}', assinatura real é '{actual}'");
            }

            string expectedReturn = funcDecl.ReturnType;
            string actualReturn = DescribeType(method.ReturnType);
            if (!TypesMatch(expectedReturn, actualReturn))
                throw new AssertException($"{idlFileName}: função '{funcDecl.Name}' — IDL declara retorno '{expectedReturn}', assinatura real retorna '{actualReturn}'");
        }
    }

    private static string NativeEntryPointName(MethodInfo method, Attribute libraryImportAttr)
    {
        // LibraryImportAttribute.EntryPoint é public string?, mas o tipo do atributo em si não é
        // referenciável aqui sem acoplar a um using específico de plataforma — reflection sobre a
        // própria instância do atributo evita isso e funciona igual em qualquer TFM que exponha
        // [LibraryImport] (net7.0+).
        var entryPointProperty = libraryImportAttr.GetType().GetProperty("EntryPoint");
        string? entryPoint = entryPointProperty?.GetValue(libraryImportAttr) as string;
        return entryPoint ?? method.Name;
    }

    private static string DescribeParameterType(ParameterInfo p)
    {
        string prefix = p.IsIn ? "in " : p.IsOut ? "out " : p.ParameterType.IsByRef ? "ref " : "";
        return prefix + DescribeType(p.ParameterType);
    }

    private static string DescribeType(Type t)
    {
        Type effective = t.IsByRef ? t.GetElementType()! : t;
        if (effective.IsPointer) return DescribeType(effective.GetElementType()!) + "*";
        return effective.Name switch
        {
            "Int32" => "int",
            "UInt32" => "uint",
            "Int64" => "long",
            "UInt64" => "ulong",
            "Single" => "float",
            "Double" => "double",
            "Byte" => "byte",
            "Void" => "void",
            "IntPtr" => "nint",
            _ => effective.Name,
        };
    }

    // "in"/"ref"/"out" no IDL são anotação de intenção; a comparação de tipo em si ignora esses
    // prefixos além de já terem sido comparados por igualdade de string acima — mantido simples de
    // propósito (ver docs/idl/FORMATO-IDL.md, "O que o validador NÃO faz").
    private static bool TypesMatch(string expected, string actual) => expected.Trim() == actual.Trim();

    private static Type? FindType(Assembly assembly, string simpleName)
        => assembly.GetTypes().FirstOrDefault(t => t.Name == simpleName);

    private static string RepoRoot([CallerFilePath] string here = "")
    {
        // tests/Aether.Tests/IdlValidationTests.cs -> sobe dois níveis até a raiz do repositório.
        // Usa o caminho do PRÓPRIO arquivo fonte (resolvido em tempo de compilação), não o diretório
        // de execução (que seria bin/Debug/net8.0/ do processo de teste) — robusto independente de
        // onde o executável de teste é lançado.
        string dir = Path.GetDirectoryName(here)!;
        return Path.GetFullPath(Path.Combine(dir, "..", ".."));
    }
}

// ---------------------------------------------------------------------------------------------
// Parser do formato .idl — ver docs/idl/FORMATO-IDL.md para a especificação completa.
// ---------------------------------------------------------------------------------------------

internal sealed class IdlDocument
{
    public required string Library { get; init; }
    public required string ManagedType { get; init; }
    public required string NativeHeader { get; init; }
    public List<IdlEnum> Enums { get; } = [];
    public List<IdlStruct> Structs { get; } = [];
    public List<IdlFunction> Functions { get; } = [];

    public static IdlDocument Parse(string content, string sourcePath)
    {
        string[] lines = content.Replace("\r\n", "\n").Split('\n');

        string? library = null, managedType = null, nativeHeader = null;
        var enums = new List<IdlEnum>();
        var structs = new List<IdlStruct>();
        var functions = new List<IdlFunction>();
        string? currentSection = null;

        foreach (string rawLine in lines)
        {
            string line = rawLine.Trim();
            if (line.Length == 0) continue;
            if (line.StartsWith('#')) continue;

            if (line.StartsWith('[') && line.EndsWith(']'))
            {
                currentSection = line[1..^1].Trim();
                continue;
            }

            if (currentSection is null)
            {
                int colonIndex = line.IndexOf(':');
                if (colonIndex < 0)
                    throw new FormatException($"{sourcePath}: linha de cabeçalho sem ':' — '{line}'");
                string key = line[..colonIndex].Trim();
                string value = line[(colonIndex + 1)..].Trim();
                switch (key)
                {
                    case "library": library = value; break;
                    case "managed_type": managedType = value; break;
                    case "native_header": nativeHeader = value; break;
                    default: throw new FormatException($"{sourcePath}: chave de cabeçalho desconhecida '{key}'");
                }
                continue;
            }

            switch (currentSection)
            {
                case "enums": enums.Add(ParseEnum(line, sourcePath)); break;
                case "structs": structs.Add(ParseStruct(line, sourcePath)); break;
                case "functions": functions.Add(ParseFunction(line, sourcePath)); break;
                default: throw new FormatException($"{sourcePath}: seção desconhecida '[{currentSection}]'");
            }
        }

        if (library is null || managedType is null || nativeHeader is null)
            throw new FormatException($"{sourcePath}: cabeçalho incompleto — library/managed_type/native_header são todos obrigatórios");

        return new IdlDocument { Library = library, ManagedType = managedType, NativeHeader = nativeHeader }
            .With(enums, structs, functions);
    }

    private IdlDocument With(List<IdlEnum> e, List<IdlStruct> s, List<IdlFunction> f)
    {
        Enums.AddRange(e);
        Structs.AddRange(s);
        Functions.AddRange(f);
        return this;
    }

    private static IdlEnum ParseEnum(string line, string sourcePath)
    {
        string[] parts = SplitPipe(line);
        if (parts.Length != 4) throw new FormatException($"{sourcePath}: entrada de enum malformada (esperado 4 campos separados por '|') — '{line}'");
        string name = parts[0];
        string underlying = parts[1];
        bool flags = ParseFlagsField(parts[2], sourcePath, line);
        var values = parts[3].Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries).ToList();
        return new IdlEnum(name, underlying, flags, values);
    }

    private static bool ParseFlagsField(string field, string sourcePath, string line)
    {
        int eqIndex = field.IndexOf('=');
        if (eqIndex < 0) throw new FormatException($"{sourcePath}: campo flags malformado (esperado 'flags=true|false') — '{line}'");
        string value = field[(eqIndex + 1)..].Trim();
        return value == "true";
    }

    private static IdlStruct ParseStruct(string line, string sourcePath)
    {
        string[] parts = SplitPipe(line);
        if (parts.Length != 2) throw new FormatException($"{sourcePath}: entrada de struct malformada (esperado 2 campos separados por '|') — '{line}'");
        var fields = parts[1].Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries).ToList();
        return new IdlStruct(parts[0], fields);
    }

    private static IdlFunction ParseFunction(string line, string sourcePath)
    {
        string[] parts = SplitPipe(line);
        if (parts.Length != 3) throw new FormatException($"{sourcePath}: entrada de função malformada (esperado 3 campos separados por '|') — '{line}'");
        string name = parts[0];
        var parameters = new List<IdlParameter>();
        if (parts[1].Length > 0)
        {
            foreach (string paramText in parts[1].Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries))
            {
                int colonIndex = paramText.IndexOf(':');
                if (colonIndex < 0) throw new FormatException($"{sourcePath}: parâmetro sem ':' — '{paramText}' em '{line}'");
                parameters.Add(new IdlParameter(paramText[..colonIndex].Trim(), paramText[(colonIndex + 1)..].Trim()));
            }
        }
        string returnsField = parts[2].Trim();
        const string returnsPrefix = "returns:";
        if (!returnsField.StartsWith(returnsPrefix))
            throw new FormatException($"{sourcePath}: campo de retorno malformado (esperado 'returns: tipo') — '{line}'");
        string returnType = returnsField[returnsPrefix.Length..].Trim();
        return new IdlFunction(name, parameters, returnType);
    }

    private static string[] SplitPipe(string line) => line.Split('|').Select(p => p.Trim()).ToArray();
}

internal sealed record IdlEnum(string Name, string Underlying, bool Flags, List<string> Values);
internal sealed record IdlStruct(string Name, List<string> Fields);
internal sealed record IdlParameter(string Name, string Type);
internal sealed record IdlFunction(string Name, List<IdlParameter> Parameters, string ReturnType);
