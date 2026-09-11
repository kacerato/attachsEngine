# Validação — mundo de execução, física consultável e ações de entrada

Data: 10/09/2026. Branch `codex/gameplay-runtime`, a partir de
`a34d0325d6c4302942c14442a9d900a199d4cdd0`. Escopo: entregas A, D e E de
[Próximo pacote — criação de gameplay](../PROXIMO-PACOTE-GAMEPLAY.md).

**Este relatório cobre build e testes automatizados no host, mais a compilação
do APK.** Ele NÃO relata execução no aparelho: nenhuma sessão ADB foi aberta
nesta rodada, nenhuma captura de tela foi tomada e nenhuma cena foi montada pela
interface no aparelho. As entregas B, C, F e G não foram implementadas.

## Comandos e resultados

| Comando | Resultado |
|---|---|
| `cmake --build build/editor-host --target aether_tests --parallel 6` | compila sem warning (`-Werror` ligado) |
| `./build/editor-host/aether_tests.exe` | **792/792** aprovados |
| `dotnet run --project tests/Aether.Tests/Aether.Tests.csproj -c Release -p:AetherNativeBuildDir=…/build/editor-host` | **520/520** aprovados |
| `android/gradlew.bat :app:assembleDebug --console=plain` | BUILD SUCCESSFUL — `app-debug.apk`, 50.505.046 bytes |
| `android/gradlew.bat :app:assembleRelease :app:testDebugUnitTest --console=plain` | BUILD SUCCESSFUL — `app-release.apk`, 30.705.870 bytes; 16 testes Java (10 `ProjectSceneSourceTest` + 6 `ProjectStoreTest`), 0 falhas |

Linha de base antes das alterações: 767 nativos e 513 gerenciados. O acréscimo
são 25 testes nativos e 7 gerenciados novos, listados adiante.

## Testes acrescentados

### Mundo de execução (`tests/native/test_runtime_world.cpp`, 11)

- carga da cena e **documento autoral intacto** (revisão inalterada) durante
  trinta passos de simulação e depois do Stop;
- handle de outra sessão de Play recusado com `ForeignWorld`;
- destruição: referência vence **imediatamente**, armazenamento sai no ponto
  seguro, pai e filho relatados ao consumidor;
- criação imediata sem corromper iteração; busca recursiva e direta;
- API de componentes obedecendo ao schema: exigência (Olhar precisa de Câmera),
  incompatibilidade, singular não duplica, remoção bloqueada por dependente,
  `NotMutableInPlay` para corpo físico;
- propriedades por instância, tipo incompatível recusado, valor fora dos limites
  do descritor recusado, referência a objeto inexistente recusada;
- autoridade de pose protegendo ancestrais de corpos simulados;
- transform de mundo composto e decomposto pela hierarquia (ida e volta);
- Play/Stop preservando autoria e abrindo mundo novo;
- corpo do Jolt solto quando o objeto é destruído durante a execução;
- catálogo do inspetor e schema continuam sendo a mesma lista.

### Física acessível (`tests/native/test_runtime_physics_access.cpp`, 7)

- raycast devolvendo objeto, instância do colisor, ponto, **normal de superfície
  real** e distância; raio de comprimento zero recusado; objeto ignorado ausente;
- `rayCastAll` ordenado e devolvendo a contagem **real** mesmo truncado;
- máscara de camada e política de sensores (fora por padrão, marcados quando
  incluídos);
- **matriz de camadas valendo no solver**: a mesma bola repousa sobre o piso com
  as camadas interagindo e atravessa quando não interagem;
- contatos sólidos com um Enter por par de corpos e normal presente no Enter;
- shape cast e overlap para desvio de obstáculo de câmera, com overlap **sem**
  normal (não há direção ao longo de quê);
- camadas nomeadas: reciprocidade, nome duplicado recusado, ida e volta,
  matriz assimétrica recusada na leitura.

### Entrada (`tests/native/test_runtime_input.cpp`, 5)

- mapa padrão reproduzindo o controle de toque anterior;
- ações como dado do projeto: renomear leva o papel junto, papel para ação
  inexistente recusado, remover limpa o papel;
- zona morta, sensibilidade e inversão;
- **foco e contexto**: perder o foco solta o botão na hora, a soltura é
  observável, contexto desligado zera sem apagar a configuração;
- ida e volta do mapa no arquivo de cena.

### Modelos de script (`tests/native/test_editor_script_templates.cpp`, 2)

- metadados e código dos sete modelos;
- criação renomeando a classe e o `ComponentId`, trazendo os contratos na
  primeira vez e **preservando-os** na segunda; arquivo vazio sem modelo.

### Gerenciados (`tests/Aether.Tests`, 7)

- `AstraWorldTests` (5): hierarquia, referência vencida na destruição, referência
  que não sobrevive ao Stop, componentes por instância com tipo verificado,
  transform local e de mundo;
- `AstraTemplateTests` (2): **os sete modelos compilam** com o mesmo
  `ProjectCompiler` do projeto do usuário, e o schema extraído tem os tipos que o
  Inspector sabe editar (nenhum campo `unsupported`).

## Problemas encontrados e corrigidos nesta rodada

1. **`GameObject` como struct não permitia escrever transform.**
   `Object.WorldTransform = …` falha com CS1612. Descoberto pelo primeiro modelo
   escrito contra a API — motivo pelo qual os modelos passaram a ser compilados
   no teste. `GameObject` virou classe.
2. **Seção de camadas escrita só na leitura.** Uma substituição não aplicada
   deixou o desserializador esperando `LAYERS` num arquivo que não a tinha: 23
   testes falharam de uma vez. Corrigido antes do commit.
3. **`operator==` declarado depois do uso.** O libstdc++ aceitava; o libc++ do
   NDK não. O build Android falhou e revelou a divergência; as declarações
   subiram para antes de `InputActionMap`.

## Android

Debug e Release compilam com as alterações (ABI v5, runtime novo, camadas no
solver). Os 16 testes Java do shell continuam passando; eles cobrem projeto e
fonte de cena, não o runtime novo — as fontes Java não mudaram nesta rodada e o
Gradle os reportou como `UP-TO-DATE`.

A primeira tentativa de build Android **falhou**: o libc++ do NDK não encontrava
um `operator==` declarado depois do uso, onde o libstdc++ do host encontrava.
Só o build Android revelou isso; os números de teste acima são posteriores à
correção.

**O que NÃO foi feito:** instalar no aparelho, abrir o editor, montar cena pela
interface, exercitar Play/Stop/pausa/passo, sair e reabrir no mesmo processo, ou
comparar capturas. A prova de aceitação do pacote (duas composições montadas pela
interface) **não ocorreu** e continua pendente.

## Lacunas conhecidas

- **Entrega B** (recursos com GUID, importação GLB pelo Android, renomear/mover/
  reimportar, cenas reutilizáveis): não iniciada.
- **Entrega C** (MaterialAsset compartilhado, slots por submesh, overrides, Luz
  anexável com consumidor gráfico, pré-visualização isolada): não iniciada.
- **Entrega F** (console com histórico e navegação por diagnóstico, estados de
  compilação, busca/substituição, campos tipados no inspetor, ocultar `.astra`):
  não iniciada. O seletor de modelo de script é a única parte de interface deste
  pacote.
- **Entrega G**: a rodada integrada no aparelho não ocorreu.
- Sensor **por colisor** e `CharacterVirtual` na broadphase continuam fora, como
  o próprio pacote antecipava; sensores e consultas não acertam o personagem.
- Teclado e gamepad: fontes implementadas e testadas no host, **sem alimentação
  pelo shell Android**.
- Camada de gameplay é por corpo; o inspetor ainda não tem interface para
  nomear camadas nem editar a matriz — hoje isso é API e arquivo.
- Nenhuma medição de FPS, orçamento de quadro ou sessão prolongada.
