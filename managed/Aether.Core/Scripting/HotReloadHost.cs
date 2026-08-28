using System.Reflection;
using System.Runtime.Loader;

namespace Aether.Scripting;

/// <summary>
/// PoC-D do plano (item 0.2): "hot reload C# no dispositivo — editar → ver mudança em &lt; 2 s".
/// Prova a fundação técnica mínima: carregar, descarregar e recarregar um assembly gerenciado
/// dentro do processo já hospedado pelo CoreCLR (ver <c>native/platform/android/dotnet_host.h</c>,
/// item 0.1.4), sem reiniciar o app nem o host .NET.
/// <para>
/// <b>Por que não o protocolo completo de Hot Reload do .NET</b> (o que <c>dotnet watch</c>/IDEs
/// usam via <c>System.Reflection.Metadata.MetadataUpdater</c>): esse mecanismo depende de um
/// debugger externo attachado por named pipe aplicando deltas de IL/metadata a um assembly já
/// carregado — reimplementar esse protocolo standalone dentro do APK é um projeto de escopo muito
/// maior que uma prova de conceito. O que este tipo prova em vez disso é a alternativa mais simples
/// e igualmente viável para o caso de uso do editor: descarregar o assembly de script inteiro via
/// <see cref="AssemblyLoadContext"/> <b>collectible</b> e carregar a versão nova recém-compilada no
/// lugar. O host (<c>Aether.Core</c>, este próprio assembly) nunca é descarregado — só o assembly
/// de script é substituível, mesma separação que o plano já desenha entre "núcleo" e "camada de
/// script do usuário" (Parte 11 do plano).
/// </para>
/// <para>
/// O ciclo real de edição (Roslyn compilando o `.cs` editado para um novo `.dll` em memória) é
/// trabalho futuro do AetherFlow/pipeline de scripting (Fase 5, 9) — esta classe recebe os bytes de
/// um assembly já compilado (por quem quer que seja: um <c>dotnet build</c> local, ou futuramente
/// Roslyn embutido) e mede/prova só a parte "carregar isso no processo rodando e chamar um método",
/// que é a etapa cuja viabilidade em hardware Android real este PoC precisa demonstrar.
/// </para>
/// </summary>
public sealed class HotReloadHost
{
    private AssemblyLoadContext? _context;
    private WeakReference? _contextWeakRef;

    /// <summary>Verdadeiro entre <see cref="Load"/> bem-sucedido e <see cref="Unload"/>.</summary>
    public bool IsLoaded => _context is not null;

    /// <summary>
    /// Carrega <paramref name="assemblyBytes"/> num <see cref="AssemblyLoadContext"/> novo,
    /// collectible (<c>isCollectible: true</c> — obrigatório para <see cref="Unload"/> funcionar;
    /// o contexto padrão/não-collectible do runtime nunca descarrega um assembly). Lança se já
    /// houver um contexto carregado — chamador deve <see cref="Unload"/> antes de recarregar,
    /// nunca acumular contextos (cada um mantém seu assembly em memória até coleta de GC).
    /// </summary>
    public void Load(ReadOnlySpan<byte> assemblyBytes)
    {
        if (_context is not null)
            throw new InvalidOperationException("Já há um assembly de script carregado — chame Unload() antes de recarregar.");

        var context = new AssemblyLoadContext(name: "AetherScript", isCollectible: true);
        using var stream = new MemoryStream(assemblyBytes.ToArray());
        context.LoadFromStream(stream);

        _context = context;
        _contextWeakRef = new WeakReference(context, trackResurrection: true);
    }

    /// <summary>
    /// Localiza um método estático público em <paramref name="typeName"/> (nome qualificado
    /// simples, sem assembly — resolvido dentro do único assembly carregado por <see cref="Load"/>)
    /// e o invoca via reflection, devolvendo o valor de retorno como <see cref="object"/>. Reflection
    /// simples (não <c>[UnmanagedCallersOnly]</c>/ponteiro de função) porque o assembly de script é
    /// carregado e descartado em runtime — não há como o C++ resolver um ponteiro de função estável
    /// para um assembly que ainda nem existe no momento em que o app iniciou.
    /// </summary>
    public object? InvokeStaticMethod(string typeName, string methodName, params object?[] args)
    {
        if (_context is null) throw new InvalidOperationException("Nenhum assembly de script carregado — chame Load() primeiro.");

        Assembly scriptAssembly = _context.Assemblies.Single();
        Type type = scriptAssembly.GetType(typeName)
            ?? throw new MissingMemberException($"Tipo '{typeName}' não encontrado no assembly de script carregado.");
        MethodInfo method = type.GetMethod(methodName, BindingFlags.Public | BindingFlags.Static)
            ?? throw new MissingMethodException($"Método estático público '{methodName}' não encontrado em '{typeName}'.");

        return method.Invoke(null, args);
    }

    /// <summary>
    /// Descarrega o assembly de script atual. <see cref="AssemblyLoadContext.Unload"/> apenas
    /// inicia o descarregamento — a memória só é de fato liberada quando o GC coleta o contexto
    /// (nenhuma referência viva a tipos/instâncias dele pode sobreviver, senão o unload trava
    /// silenciosamente "pendurado"). Força coletas completas em até <see
    /// cref="MaxUnloadCollectionAttempts"/> tentativas — medido em hardware Android real (item 0.2,
    /// PoC-D): sob a carga concorrente do shell gráfico (Vulkan/renderer já rodando no mesmo
    /// processo), uma única dupla passagem de <see cref="GC.Collect()"/> nem sempre foi suficiente
    /// para coletar o contexto na primeira tentativa — o host (Windows/x64, sem outra carga)
    /// sempre coletou de primeira; o comportamento diverge por causa da pressão de heap real do
    /// processo, não por um bug deste tipo. Retentar é seguro e barato (o custo de uma coleta
    /// adicional é desprezível frente ao orçamento de 2s do PoC).
    /// </summary>
    public void Unload()
    {
        if (_context is null) return;

        _context.Unload();
        _context = null;

        for (int attempt = 0; attempt < MaxUnloadCollectionAttempts; attempt++)
        {
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
            if (LastContextFullyCollected) return;
        }
    }

    private const int MaxUnloadCollectionAttempts = 10;

    /// <summary>
    /// Verdadeiro se o último <see cref="AssemblyLoadContext"/> descarregado por <see
    /// cref="Unload"/> já foi de fato coletado pelo GC — a confirmação de que o ciclo completo
    /// (carregar → usar → descarregar) não vazou memória. Usado pelos testes/probe deste PoC para
    /// medir a garantia real, não só assumir que <see cref="Unload"/> funcionou.
    /// </summary>
    public bool LastContextFullyCollected => _contextWeakRef is { IsAlive: false };
}
