# Bloco C2 — cenas do projeto em Play (F004)

Branch `claude/api-componentes`. Família `astra.scenes` da ABI42 ([bloco A](API-METODOS-EVENTOS-ABI42-2026-10-06.md)). Referência: Unity 6000.0 [SceneManager](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/SceneManagement.SceneManager.html).

## O que passou a ser possível

| C# (`Behavior.Scenes`) | Efeito |
|---|---|
| `Scenes.Active` | Nome da cena que iniciou ou substituiu o mundo atual |
| `Scenes.Names` | Cenas `.aescene` do projeto, caminho relativo sem extensão, em ordem estável |
| `Scenes.Load(nome)` | Troca o mundo inteiro no fim do quadro; scripts atuais recebem Stop/Destroy e os da cena nova Awake/Start |
| `Scenes.LoadAdditive(nome, pai)` | Acrescenta a cena ao mundo sob um contêiner com o nome dela; scripts da cena publicados como numa instanciação de prefab |

O nome aceita o caminho relativo (`Fases/Nivel2`) ou só o arquivo (`Nivel2`) quando único; nome ambíguo ou ausente é recusado na mesma chamada.

## Cadeia

```text
Scenes.Load → astra.scenes.requestSingle → carregador da sessão (resolve, lê, tags,
  reconcilia recursos e materiais, confere extração) → pedido pendente
  → EditorSession, depois de advance: stop → configureGui → start(grafo novo)
Scenes.LoadAdditive → loadAdditive → GameWorld::instantiateScene (cena inteira sob
  contêiner, referências internas remapeadas) → InstantiationAttachments/FinishInstantiation
```

O documento autoral nunca é tocado: parar o Play volta à cena editada, como as cenas carregadas em Play Mode na Unity.

## Diferenças e limites

| Aspecto | Astra | Classificação |
|---|---|---|
| Carregamento assíncrono (`LoadSceneAsync`) | Leitura síncrona; troca no fim do quadro | Pendente |
| `DontDestroyOnLoad` | Não existe; a troca única substitui tudo | Pendente |
| Descarregar cena aditiva | Remover o contêiner com `Destroy` | Adaptação explícita |
| Build Settings / índice de cena | Catálogo é o conjunto de `.aescene` do projeto | Adaptação explícita |
| Jogo exportado | Exige o mesmo catálogo no exportador | Pendente |

## Validação executada (06/10/2026)

Host C++: `play_scenes_*` 3/3 — instanciação aditiva com contêiner e referência remapeada; família pela ABI com catálogo, aditiva publicada, recusas de cena ausente e de segundo pedido no mesmo quadro; sessão real com projeto em disco resolvendo nome único, recusando ambíguo e ausente, trocando o mundo no quadro seguinte e preservando o documento autoral.
