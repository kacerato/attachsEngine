using Aether.Jobs;

namespace Aether.Resources;

/// <summary>
/// Ponto de entrada para carregar recursos: agenda o trabalho pesado (I/O, decodificação) como job
/// de background no <see cref="JobSystem"/> existente e devolve o controle para a chamadora na
/// hora — nunca bloqueia a thread principal esperando o carregamento (CONVENCOES.md item 3).
///
/// Carregamento vai para a classe de núcleo <see cref="CoreAffinity.Efficiency"/>, a mesma que o
/// <see cref="JobSystem"/> já documenta como destino de "import/compressão": é trabalho de I/O e
/// decodificação, não latência crítica de frame, então cabe melhor nos núcleos pequenos —
/// liberando os núcleos grandes para o trabalho de frame que está de fato no caminho crítico.
/// </summary>
public sealed class ResourceManager
{
    private readonly ResourceHandleTable _table;
    private readonly JobSystem _jobs;

    public ResourceManager(ResourceHandleTable table, JobSystem jobs)
    {
        ArgumentNullException.ThrowIfNull(table);
        ArgumentNullException.ThrowIfNull(jobs);
        _table = table;
        _jobs = jobs;
    }

    public ResourceHandleTable Table => _table;

    /// <summary>
    /// Pede o carregamento de <paramref name="id"/> via <paramref name="loader"/>. Devolve
    /// imediatamente uma referência FORTE (a contagem de uso já foi incrementada — a chamadora
    /// já é dona de uma posse, mesmo que o objeto ainda não esteja pronto) e a <see
    /// cref="JobHandle"/> da job de carregamento, para quem precisar esperar explicitamente com
    /// <see cref="JobSystem.Complete"/>.
    ///
    /// Se o recurso já foi pedido antes (por qualquer chamadora, em qualquer estado — carregando,
    /// carregado ou até falhado), NENHUMA job nova é agendada: esta chamada só concede mais uma
    /// posse forte sobre o que já existe, e devolve <c>default(JobHandle)</c> (que <see
    /// cref="JobSystem.Complete"/> trata como no-op) em vez de uma alça de verdade — evita
    /// carregar o mesmo asset em duplicidade quando duas partes do editor pedem o mesmo recurso
    /// ao mesmo tempo.
    /// </summary>
    public (ResourceRef<T> Reference, JobHandle Handle) LoadAsync<T>(ResourceId id, IResourceLoader<T> loader) where T : class
    {
        ArgumentNullException.ThrowIfNull(loader);

        bool precisaCarregar = _table.TryBeginLoad(id);

        // A posse forte é concedida já aqui, antes mesmo da job rodar: quem chamou LoadAsync
        // pretende usar o recurso assim que ele existir, então já é dona de uma referência —
        // não faria sentido exigir uma segunda chamada só para "confirmar" a posse depois.
        var reference = _table.AcquireStrong<T>(id);

        if (!precisaCarregar) return (reference, default);

        var handle = _jobs.Schedule(() =>
        {
            try
            {
                T value = loader.Load(id);
                _table.SetLoaded(id, value);
            }
            catch (Exception ex)
            {
                // Conteúdo do usuário (asset corrompido, formato inesperado, arquivo sumiu) nunca
                // pode derrubar o editor — CONVENCOES.md item 8, regra 3. O erro fica guardado
                // para diagnóstico/UI em vez de estourar da job.
                _table.SetFailed(id, ex);
            }
        }, CoreAffinity.Efficiency);

        return (reference, handle);
    }
}
