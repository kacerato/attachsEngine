namespace Aether.Diagnostics;

/// <summary>
/// Item 1.2.5 do plano: marca um método (ou uma propriedade get/set) como caminho de frame que
/// não pode alocar no heap gerenciado (docs/CONVENCOES.md §3). O analisador Roslyn em
/// <c>Aether.Analyzers</c> (regra AETH001) verifica isso ESTATICAMENTE em tempo de build —
/// complementa, não substitui, <c>Assert.NoAlloc</c> (que verifica em RUNTIME, rodando o código
/// de verdade uma vez): o analisador pega o padrão de código que aloca antes mesmo de rodar um
/// teste; <c>Assert.NoAlloc</c> pega alocação que só acontece condicionalmente (um branch que o
/// analisador estático não consegue provar que nunca aloca em geral, mas que de fato não aloca
/// no caminho exercitado pelo teste).
/// <para>
/// Aplicar em métodos/propriedades chamados todo frame (sistemas de ECS, `TransformSystem.Propagate`,
/// `PhysicsSyncSystem.Step`, etc.) — não é para código de setup/import/editor, que pode alocar
/// livremente (mesma distinção que <c>ConfigurationStore</c>/`Signal&lt;T&gt;.Subscribe` já fazem
/// nos próprios comentários).
/// </para>
/// </summary>
[AttributeUsage(AttributeTargets.Method | AttributeTargets.Property, Inherited = false)]
public sealed class NoAllocAttribute : Attribute
{
}
