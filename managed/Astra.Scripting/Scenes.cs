namespace Astra;

/// <summary>Família <c>astra.scenes</c>: cenas do projeto vistas pelo Play.</summary>
public interface IProjectScenesAccess
{
    string ActiveScene();
    IReadOnlyList<string> ProjectScenes();
    /// <summary>Cria a cena sob <paramref name="parent"/>; zero quando recusada.</summary>
    ulong LoadSceneAdditive(ulong parent, string name);
    /// <summary>Aceita a troca de mundo para o fim do quadro; falso quando recusada.</summary>
    bool RequestSingleScene(string name);
}

internal interface IScenesHost
{
    GameObject LoadSceneAdditive(GameObject parent, string name);
}

/// <summary>
/// Cenas do projeto em Play (Unity 6000.0 <c>SceneManager</c>). Nome é o
/// caminho relativo sem extensão ou só o nome do arquivo, quando único.
/// </summary>
public readonly struct ScenesAccess
{
    private readonly ISceneAccess _scene;
    private readonly IScenesHost? _host;
    private readonly GameObject? _root;
    internal ScenesAccess(ISceneAccess scene, IScenesHost? host, GameObject? root) { _scene = scene; _host = host; _root = root; }
    private IProjectScenesAccess Access => _scene as IProjectScenesAccess
        ?? throw new NotSupportedException("O host não oferece cenas do projeto (astra.scenes).");
    /// <summary>A cena que iniciou ou substituiu o mundo atual.</summary>
    public string Active => Access.ActiveScene();
    /// <summary>Cenas do projeto, em ordem estável.</summary>
    public IReadOnlyList<string> Names => Access.ProjectScenes();
    /// <summary>
    /// Troca o mundo inteiro pela cena, no fim do quadro corrente. Os
    /// comportamentos atuais recebem Stop/Destroy; os da cena nova, Awake/Start.
    /// O documento editado não muda e volta ao parar o Play.
    /// </summary>
    public void Load(string name)
    {
        ArgumentException.ThrowIfNullOrEmpty(name);
        if (!Access.RequestSingleScene(name)) throw new WorldException(_scene.LastStatus, "carregar cena " + name);
    }
    /// <summary>
    /// Acrescenta a cena ao mundo atual sob um objeto contêiner com o nome dela,
    /// filho de <paramref name="parent"/> (raiz quando nulo). Os comportamentos
    /// da cena recebem Awake/Start como em uma instanciação.
    /// </summary>
    public GameObject LoadAdditive(string name, GameObject? parent = null)
    {
        ArgumentException.ThrowIfNullOrEmpty(name);
        var host = _host ?? throw new InvalidOperationException("Cena aditiva exige um comportamento em Play.");
        var destination = parent ?? _root ?? throw new InvalidOperationException("Sem raiz para a cena aditiva.");
        return host.LoadSceneAdditive(destination, name);
    }
}
