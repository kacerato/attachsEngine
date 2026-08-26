namespace Aether.Platform;

/// <summary>
/// Item 1.1.1 do plano: os escopos de armazenamento que toda plataforma alvo (Android, iOS, desktop
/// de desenvolvimento) precisa oferecer, mesmo que o caminho físico por trás de cada um seja
/// completamente diferente entre elas. Nomear os escopos aqui, em vez de aceitar caminho absoluto
/// livre em toda a engine, é o que permite ao Core nunca saber que "Android tem scoped storage" ou
/// "iOS tem sandbox de app" — só pede o escopo certo pela intenção.
/// </summary>
public enum FileSystemScope
{
    /// <summary>
    /// Conteúdo importado do projeto (malhas, texturas, cenas, scripts), somente leitura em tempo de
    /// execução do runtime publicado. No Android mapeia para os assets do APK; no editor rodando em
    /// desktop mapeia para a pasta do projeto no disco. Nunca é o destino de uma escrita do editor —
    /// isso é <see cref="PersistentData"/>.
    /// </summary>
    Assets,

    /// <summary>
    /// Dados que o usuário precisa não perder: o projeto em edição, saves, o WAL de recuperação
    /// (<c>Aether.Editing.WriteAheadLog</c>). No Android mapeia para o armazenamento de app privado
    /// (equivalente a <c>internalDataPath</c> do <c>ANativeActivity</c>); no iOS, para o diretório
    /// Documents (sincronizável por iCloud). Sobrevive à desinstalação-e-reinstalação em nenhuma das
    /// duas plataformas — não confundir com armazenamento externo compartilhado.
    /// </summary>
    PersistentData,

    /// <summary>
    /// Artefatos derivados e descartáveis: cache de importação, shaders compilados, pipeline cache.
    /// A plataforma pode apagar este escopo sob pressão de espaço em disco sem avisar o app — nunca
    /// gravar aqui algo que não pode ser reconstruído a partir de <see cref="Assets"/> ou <see
    /// cref="PersistentData"/>.
    /// </summary>
    Cache,
}
