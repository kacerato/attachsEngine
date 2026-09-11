# Validação — mundo de execução, física consultável e ações de entrada

Rodada de host em 10/09/2026 e rodada de aparelho em 11/09/2026, branch
`codex/gameplay-runtime`, a partir de `a34d0325d6c4302942c14442a9d900a199d4cdd0`.
Escopo: entregas A, D e E de
[Próximo pacote — criação de gameplay](../PROXIMO-PACOTE-GAMEPLAY.md).

O usuário autorizou ADB. **A rodada no aparelho exercitou o caminho autoral
completo** — criar projeto, compor objetos, criar script a partir de modelo,
Aplicar, anexar, Play, pausa, passo, Stop e reabertura — e encontrou dois
defeitos que nenhum teste de host pegava. As entregas B, C, F e a prova de
aceitação com duas composições continuam **fora** desta rodada.

## Comandos e resultados

| Comando | Resultado |
|---|---|
| `cmake --build build/editor-host --target aether_tests --parallel 6` | compila sem warning (`-Werror` ligado) |
| `./build/editor-host/aether_tests.exe` | **794/794** aprovados |
| `dotnet run --project tests/Aether.Tests/Aether.Tests.csproj -c Release -p:AetherNativeBuildDir=…/build/editor-host` | **521/521** aprovados |
| `android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain` | BUILD SUCCESSFUL; 16 testes Java (10 `ProjectSceneSourceTest` + 6 `ProjectStoreTest`), 0 falhas |
| `android/gradlew.bat :app:assembleRelease --console=plain` | BUILD SUCCESSFUL — `app-release.apk`, 30.705.870 bytes |

Linha de base antes das alterações: 767 nativos e 513 gerenciados.

APK Debug instalado no aparelho (SHA256):
`FC583315716C52043319F589F1CA55C678E2F5B73B8167286C160FBBF1EF22D3`.
Os hashes das builds anteriores desta rodada estão em
`build/validation-runtime-20260910/apk-sha256.txt`.

## Aparelho

Xiaomi 25053PC47G (`onyx_global`), serial `53e1eb7`, Android 16, arm64, ADB por
Wi-Fi. **Os transport ids mudaram no meio da rodada** (5 → 1/2, com duas
entradas mDNS para o mesmo aparelho): a sessão passou a endereçar por serial,
e `-t N` de rodada anterior não deve ser reutilizado.

Instalação com `install -r`, sem limpar dados; os projetos anteriores foram
preservados. Projeto exclusivo criado pela interface: `RuntimeGameplay0910`.
`screen_off_timeout` foi elevado para 30 min durante a rodada e **restaurado**
para o valor original (600000) ao final.

O overlay GameTurbo ("Wild Boost ativado") apareceu sobre a primeira captura e
saiu sozinho; a captura seguinte é a que vale. Nada nesta rodada mede FPS.

Evidências em `build/validation-runtime-20260910/` (57 arquivos).

### Defeitos encontrados no aparelho

1. **O shell Java recusava qualquer cena da build nova.**
   `ProjectSceneSource.LAST_SUPPORTED_ARCHIVE_VERSION` estava em 10 e o escritor
   nativo passou a emitir v12: abrir o projeto falhava com
   `java.io.IOException: Formato de cena não reconhecido`, e o editor nem abria.
   Nenhum dos 792 testes nativos nem dos 520 gerenciados via isso — são duas
   linguagens que nenhum compilador liga.
   Corrigido, e agora **guardado por teste**: `ArquivoDeCenaTests` lê a versão
   emitida por `editor_archive.cpp` e a constante do Java e falha se
   divergirem. A guarda foi verificada ao contrário (voltando a constante para
   10, o teste falha com "esperado <12>, obtido <10>").
2. **O caminho Play→script não tinha teste de host.** Os testes de contato
   exercitavam `ScenePhysics` direto; `EditorPlayScene` → `ScriptBridge` → ABI
   não tinha nenhum. Acrescentados dois testes com um duplo de runtime
   (`test_editor_play_scripts.cpp`), um deles verificando que uma ABI incompleta
   (por exemplo um hospedeiro que não resolveu `Contact`) **recusa o Play** em
   vez de rodar com os contatos sumindo em silêncio.

### O que foi exercitado pela interface, no aparelho

| Fluxo | Evidência |
|---|---|
| Projeto novo pela interface | `03`–`05`; projetos anteriores preservados (`projects.json`) |
| Criar Chão e Cubo pelo menu de criação | `06`–`08` |
| **Arquivo de cena v12 com as seções novas** | `cena-v12-dispositivo.aescene`: cabeçalho `AETHER_EDITOR 12 0 3`, `LAYERS 1 "Padrão" 4294967295`, `INPUT 3 "Mover" "Olhar" "Saltar" …` |
| Reabrir projeto salvo pela build nova | `16`: status **"Cena restaurada"**, Chão e Cubo de volta |
| **Seletor de modelos de script** | `10`: os sete modelos com nome e descrição, mais "Arquivo vazio" |
| Criar script a partir do modelo | `18`: classe e `ComponentId` renomeados para `AcenderNoContato`; `Contratos.cs` criado junto |
| **Aplicar compila o modelo no aparelho** | `19`: "Código aplicado ao projeto"; `schema.json` publicado com as seis propriedades tipadas |
| Schema governando o inspetor | `22`: "Corpo físico — Já adicionado", "Personagem — Incompatível com corpo físico", "Olhar — Adicione Câmera a este objeto" |
| Componentes recolhidos e repetíveis | `24`: `Colisor 3D · 3` com contagem de instâncias |
| Campos tipados no inspetor | `26`: enum Movimento, bools, floats; `27`: Movimento = Dinâmico |
| Play com o mundo de execução | `32`: "Em execução"; o cubo cai de Y=5 e repousa |
| **Contato sólido chegando ao comportamento** | `script-log.txt`: `SONDA contato-enter n=1 outro=2 normal=<3.3e-05, 1, 3.3e-05>` — **um** Enter para o par, com o cubo e o chão tendo 3 colisores cada (9 pares de subforma), e normal apontando para cima |
| Escrita de propriedade de componente por C# | `script-log.txt`: `SONDA cor aplicada` após `Component.SetFloat` |
| Identidade de sessão por Play | `SONDA start mundo=2` e, depois da reabertura, `mundo=5` |
| Pausa determinística | `38`/`39`: duas capturas consecutivas com o mesmo SHA256 `6C397E45…` com o corpo em repouso |
| Passo avança um passo | `40`→`42`: em "Pausado", o cubo desce e a sombra cresce |
| **Stop preserva a autoria** | `34`: Y volta a 5.000. Ciclo limpo Play→Stop→Salvar: `ciclo-antes.aescene` e `ciclo-depois.aescene` idênticos, 1127 bytes, SHA256 `1B54CEFE82F830FE519DB37D829C48337C22C28EAB472C186079D466872199B0` |
| Sair, reabrir e rodar de novo no mesmo processo | PID **27995** antes e depois; novo Play repete start, contato e repouso |

A `Sonda` é um script de diagnóstico escrito para esta rodada e empurrado por
`adb push` para o projeto de validação — **não** acompanha o produto nem é
semeada em projeto nenhum.

### Divergência observada e NÃO corrigida

O comportamento escreveu `base_color.*` na Malha com sucesso (a chamada lança se
for recusada, e o log seguinte saiu), mas **a cor do objeto não mudou na tela**.
A amostragem de pixel confirma: o cubo continua cinza antes e depois do contato.

A escrita chega ao componente; o que não acontece é o consumo do override de
material por instância no caminho de desenho das primitivas do projeto
independente, no renderer Android. Isso pertence à **entrega C** (materiais),
que este pacote não implementou, e não é regressão desta rodada — o teste de
host `editor_material_instance_override_is_independent_and_roundtrips` continua
passando, ou seja, o valor chega a `MapDrawState::material`. Fica registrado
aqui como lacuna concreta, com o ponto exato onde a cadeia para.

## Testes acrescentados (host)

### Mundo de execução (`tests/native/test_runtime_world.cpp`, 11)

- carga da cena e **documento autoral intacto** (revisão inalterada) durante
  trinta passos de simulação e depois do Stop;
- handle de outra sessão de Play recusado com `ForeignWorld`;
- destruição: referência vence **imediatamente**, armazenamento sai no ponto
  seguro, pai e filho relatados ao consumidor;
- criação imediata sem corromper iteração; busca recursiva e direta;
- API de componentes obedecendo ao schema: exigência, incompatibilidade,
  singular não duplica, remoção bloqueada por dependente, `NotMutableInPlay`;
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
- máscara de camada e política de sensores;
- **matriz de camadas valendo no solver**: a mesma bola repousa sobre o piso com
  as camadas interagindo e atravessa quando não interagem;
- contatos sólidos com um Enter por par de corpos e normal presente no Enter;
- shape cast e overlap para desvio de obstáculo, com overlap **sem** normal;
- camadas nomeadas: reciprocidade, duplicata recusada, ida e volta, matriz
  assimétrica recusada na leitura.

### Entrada (`tests/native/test_runtime_input.cpp`, 5)

- mapa padrão reproduzindo o controle de toque anterior;
- ações como dado do projeto: renomear leva o papel junto, papel para ação
  inexistente recusado, remover limpa o papel;
- zona morta, sensibilidade e inversão;
- **foco e contexto**: perder o foco solta o botão na hora, a soltura é
  observável, contexto desligado zera sem apagar a configuração;
- ida e volta do mapa no arquivo de cena.

### Play com scripts (`tests/native/test_editor_play_scripts.cpp`, 2)

- contato sólido atravessando `EditorPlayScene` → `ScriptBridge` → ABI, com o
  par correto e a descrição de anexos chegando ao runtime;
- ABI incompleta **recusa** o Play em vez de perder eventos em silêncio.

### Modelos de script (`tests/native/test_editor_script_templates.cpp`, 2)

- metadados e código dos sete modelos;
- criação renomeando classe e `ComponentId`, trazendo os contratos na primeira
  vez e **preservando-os** na segunda; arquivo vazio sem modelo.

### Gerenciados (`tests/Aether.Tests`, 8)

- `AstraWorldTests` (5): hierarquia, referência vencida na destruição,
  referência que não sobrevive ao Stop, componentes por instância com tipo
  verificado, transform local e de mundo;
- `AstraTemplateTests` (2): os sete modelos compilam com o mesmo
  `ProjectCompiler` do projeto do usuário, sem campo `unsupported`;
- `ArquivoDeCenaTests` (1): versão do arquivo de cena alinhada entre o escritor
  nativo, o leitor nativo e o shell Java.

## Problemas corrigidos durante o trabalho

1. **`GameObject` como struct não permitia escrever transform** (CS1612).
   Descoberto pelo primeiro modelo escrito contra a API — motivo pelo qual os
   modelos passaram a ser compilados no teste. `GameObject` virou classe.
2. **Seção de camadas escrita só na leitura**: 23 testes falharam de uma vez.
3. **`operator==` declarado depois do uso**: o libstdc++ aceitava, o libc++ do
   NDK não. Só o build Android revelou.
4. **Versão do arquivo de cena divergente entre C++ e Java** (acima).
5. **Caminho Play→script sem teste de host** (acima).

## Lacunas conhecidas

- **Entrega B** (recursos com GUID, importação GLB pelo Android, renomear/mover/
  reimportar, cenas reutilizáveis): não iniciada.
- **Entrega C** (MaterialAsset compartilhado, slots por submesh, overrides
  chegando ao renderer, Luz anexável, pré-visualização isolada): não iniciada —
  ver a divergência de material registrada acima.
- **Entrega F** (console com histórico e navegação por diagnóstico, estados de
  compilação, busca/substituição, campos tipados novos, ocultar `.astra`): não
  iniciada. O seletor de modelo de script é a única parte de interface deste
  pacote.
- **Entrega G**: a prova de aceitação — duas composições diferentes montadas
  inteiramente pela interface — **não** foi feita. Esta rodada montou UMA cena
  simples (chão, cubo, corpo, colisores, script) para exercitar os caminhos.
- Sensor **por colisor** e `CharacterVirtual` na broadphase continuam fora;
  sensores e consultas não acertam o personagem.
- Teclado e gamepad: fontes implementadas e testadas no host, **sem alimentação
  pelo shell Android**.
- Camada de gameplay é por corpo; não há interface para nomear camadas nem
  editar a matriz — hoje isso é API e arquivo.
- Consultas físicas (`RayCast`, `ShapeCast`, `Overlap`) e ações de entrada
  **não** foram exercitadas por script no aparelho nesta rodada: têm teste de
  host, e no aparelho só o contato sólido e a escrita de propriedade foram
  observados.
- Nenhuma medição de FPS, orçamento de quadro ou sessão prolongada. O aparelho
  tem GameTurbo ativo, que invalidaria qualquer medição feita assim.
