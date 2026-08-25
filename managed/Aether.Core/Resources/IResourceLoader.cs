namespace Aether.Resources;

/// <summary>
/// Carregador de um tipo de recurso. Implementações reais (textura KTX2/Basis, malha, ...) chegam
/// na Fase 6 (pipeline de assets) — isto aqui é só o contrato de execução que <see
/// cref="ResourceManager"/> usa para rodar carregamento em background, independente de qual formato
/// está por trás.
/// </summary>
/// <typeparam name="T">Classe de recurso gerenciado que este carregador produz.</typeparam>
public interface IResourceLoader<out T> where T : class
{
    /// <summary>
    /// Executa o carregamento de fato — I/O de disco, decodificação, upload para a RHI, o que for.
    /// Roda numa thread de job do <see cref="Aether.Jobs.JobSystem"/>, nunca na thread principal:
    /// pode gastar o tempo que precisar aqui dentro, esse é exatamente o ponto de ser assíncrono
    /// (CONVENCOES.md item 3: nada bloqueia a thread principal por mais de 100 ms). Uma exceção
    /// lançada aqui é capturada por <see cref="ResourceManager.LoadAsync{T}"/> e vira estado <see
    /// cref="ResourceState.Failed"/> em vez de derrubar a job ou o editor — conteúdo de usuário
    /// (um asset corrompido, por exemplo) nunca pode crashar o editor.
    /// </summary>
    T Load(ResourceId id);
}
