# Validação — mundo de execução, física consultável e ações de entrada

Rodada de host em 10/09/2026 e rodada de aparelho em 11/09/2026, branch
`codex/gameplay-runtime`, a partir de `a34d0325d6c4302942c14442a9d900a199d4cdd0`.
Escopo: entregas A, D e E de
[Próximo pacote — criação de gameplay](../PROXIMO-PACOTE-GAMEPLAY.md), mais o que
foi entregue em 11/09: da **C**, a Luz anexável e o override de material por
instância chegando à tela; da **B**, o registro de recursos com GUID e a
importação de GLB pelo aparelho.

O usuário autorizou ADB. **A rodada no aparelho exercitou o caminho autoral
completo** — criar projeto, compor objetos, criar script a partir de modelo,
Aplicar, anexar, Play, pausa, passo, Stop e reabertura — e encontrou dois
defeitos que nenhum teste de host pegava. As entregas B, C, F e a prova de
aceitação com duas composições continuam **fora** desta rodada.

## Comandos e resultados

| Comando | Resultado |
|---|---|
| `cmake --build build/editor-host --target aether_tests --parallel 6` | compila sem warning (`-Werror` ligado) |
| `./build/editor-host/aether_tests.exe` | **794/794** em 10/09; **820/820** em 11/09, com luz, material em execução, registro de recursos e importação de GLB |
| `dotnet run --project tests/Aether.Tests/Aether.Tests.csproj -c Release -p:AetherNativeBuildDir=…/build/editor-host` | **521/521** em 10/09; em 11/09, 451 aprovados e 70 pulados (os pulados dependem da lib nativa que este build de host não produz) |
| `android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain` | BUILD SUCCESSFUL; 16 testes Java (10 `ProjectSceneSourceTest` + 6 `ProjectStoreTest`), 0 falhas |
| `android/gradlew.bat :app:assembleRelease --console=plain` | BUILD SUCCESSFUL — `app-release.apk`, 30.705.870 bytes |

Linha de base antes das alterações: 767 nativos e 513 gerenciados.

A rodada de 11/09 reinstalou o APK: a anterior tinha ficado com um APK
**instrumentado** ainda no aparelho (o processo não havia sido substituído), e a
primeira leitura de pixels foi feita contra ele. Conferido pelo tamanho de
`libaether_android.so` — 12.027.008 bytes instrumentado contra 12.025.712 do
artefato limpo — e repetido do zero contra o limpo.

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

Evidências em `build/validation-runtime-20260910/` (não versionadas — `build/` é
ignorado).

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
3. **Material escrito por código não chegava à tela durante o Play.** Durante a
   execução só as poses eram publicadas para o renderer; material, visibilidade
   e sombra por instância ficavam parados no valor de autoria. Corrigido em
   `InstancedRenderer::queueAuthoredPoses` e guardado pelo teste
   `play_scene_material_written_by_code_reaches_the_draw_state`. A seção
   "Divergência observada" abaixo tem a causa, a correção e a amostragem de
   pixel no aparelho.

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

### Divergência observada — causa achada e corrigida

O comportamento escrevia `base_color.*` na Malha com sucesso (a chamada lança se
for recusada, e o log seguinte saía), mas **a cor do objeto não mudava na tela**.
A escrita chegava ao componente e chegava a `MapDrawState::material`; o que não
acontecia era a publicação desse estado para o renderer.

Causa: durante o Play o documento autoral é deliberadamente **não** escrito,
então `changed` é sempre falso em `android_main.cpp` e o único caminho de
publicação que roda é `InstancedRenderer::queueAuthoredPoses`, que — como o nome
diz — publicava **apenas matrizes de modelo**. Material, visibilidade e sombra
por instância ficavam congelados no valor do último `queueMapScene`, isto é, no
valor de autoria. Não era lacuna da entrega C: o override por instância já
existia e já funcionava no documento; ele simplesmente não alcançava a tela
enquanto a cena estava em execução.

Correção: `queueAuthoredPoses` passou a acumular também o estado por instância
que não é pose (material, visibilidade, sombra) num lote transacional, publicado
no quadro junto com as poses e invalidando o cache de cascatas de sombra quando
algum deles muda. As camadas de água ficaram **de fora** de propósito: são estado
de autoria de água e a simulação escreve nos mesmos lotes depois da extração —
republicá-las aqui sobrescreveria o que ela acabou de calcular.

Fixado em teste de host:
`play_scene_material_written_by_code_reaches_the_draw_state` escreve
`base_color.{r,g,b}` pelo mundo de execução, exige que a escrita **ligue** o
override, que o verde chegue a `MapDrawState::material` e que
`applyMaterialOverride` o entregue ao registro de material do desenho — e que o
Stop devolva a autoria intacta.

Verificação no aparelho, em APK Debug **sem** nenhuma instrumentação
(`libaether_android.so` de 12.025.712 bytes, o mesmo do artefato local; a rodada
anterior tinha sido feita contra um APK instrumentado ainda instalado):

| Momento | Amostra | Resultado |
| --- | --- | --- |
| Autoria (antes do Play) | cubo | `(141,141,141)` cinza |
| Em execução, depois do contato | cubo frente / topo | `(41,175,73)` / `(101,229,150)` verde |
| Em execução | chão em três pontos | `(211,211,211)` — inalterado |
| Depois do Stop | cubo | `(141,141,141)` cinza: autoria preservada |

Evidências: `build/validation-runtime-20260910/57-play-final.png` e
`58-stop-final.png`.

**Correção de registro sobre a rodada anterior.** O "chão ficou ciano" relatado
durante a investigação não era defeito da engine: eram valores que eu mesmo
havia deixado no inspetor por toques às cegas (`cor_ligada`/`cor_desligada` com
`"0 9 0"` e `alvo` apontando para o chão). O descritor recusou `base_color.g=9`
por estar fora de `[0,1]`, a exceção desligou o comportamento e sobrou
`(0, 0.55, 0.55)` — ciano. A engine se comportou como especificado. O componente
foi removido do objeto pelo menu do inspetor e a cena voltou ao estado limpo.

### Entrega C — Luz anexável, validada no aparelho (11/09)

Montado inteiramente pela interface, no APK Debug sem instrumentação, sobre o
projeto `RuntimeGameplay0910`: selecionar o Cubo, `Add` → **Luz**, e editar as
propriedades nos campos do inspetor (teclado numérico do aparelho).

O inspetor desenhou o componente inteiro a partir do descritor, sem nenhuma tela
específica de luz: interruptor **Acesa**, seletor **Modalidade**, **Cor R/G/B**,
**Intensidade**, **Alcance**, **Cone interno/externo**.

| Passo | Evidência | Amostra de pixel |
| --- | --- | --- |
| Luz anexada (Pontual, intensidade 8) | `63-luz-anexada.png` | chão `(211,211,211)` — luz fraca a 5 m, sem efeito visível |
| Intensidade 300 | `66-intensidade-300.png` | chão `(221,221,221)`: clareia e a sombra do cubo enfraquece |
| Cor G e B em 0 | `67-luz-vermelha.png` | chão `(238,211,211)` — poça vermelha com queda suave |
| Modalidade → Spot, objeto girado X=90° | `70-spot-para-baixo.png` | cone vermelho de borda suave no chão, centrado sob a luz |
| Modalidade → Direcional | `71-direcional.png` | chão inteiro `(255,171,171)` e **sombra projetada do cubo** — a modalidade com passe de sombra |
| Salvar, encerrar o processo e reabrir | `72-reaberto-luz.png` | `(255,171,171)` e `(252,170,171)`, idênticos: a aparência sobrevive ao arquivo |

A sombra em `71-direcional.png` é a prova de que a matriz de capacidades é real e
não decorativa: a direcional projeta porque o passe de cascatas existe, e é a
única que projeta. Pontual e spot iluminam e não projetam — está documentado, e a
Luz não oferece interruptor de sombra que não teria shader atrás.

O que a entrega C **não** tem: MaterialAsset compartilhado, slots por submesh,
pré-visualização isolada de material. A luz e o override de material por
instância são a parte implementada.

### Entrega B — GLB importado pela interface, no aparelho (11/09)

`Adicionar objeto → Geometria → Importar modelo` abre o seletor do sistema. O
arquivo usado foi um GLB de três nós (`Torre`, `Base`, `Braco`) apontando para a
mesma malha — instâncias, que é o caso que separa "a importação funciona" de "a
importação duplica tudo". Gerado para esta rodada em
`build/validation-runtime-20260910/torre.glb`, 1.692 bytes; **não** acompanha o
produto.

| Passo | Evidência |
| --- | --- |
| A entrada existe no catálogo de criação | `82-geometria.png`: "Importar modelo · Abre um .glb do aparelho e traz suas malhas" |
| O seletor do sistema abre | `88`, `90`: o seletor de documentos do Android, com busca |
| Importa e aparece na cena | `92-importado.png`: `Torre`, `Base`, `Braco` na hierarquia e a geometria na tela, com a cor do material do arquivo |
| A fonte é copiada para dentro do projeto | a pasta `Fontes` aparece no painel de arquivos; `Fontes/torre.glb`, 1.692 bytes |
| O registro é gravado ao lado da cena | `.astra/assets.astra`: `AETHER_ASSETS 1 1` e o recurso com caminho, fonte e SHA-256 `1096e79b…` |
| Reabrir o projeto relê a fonte | log: `[Import] fonte reaberta: Fontes/torre.glb`; `94-reaberto-com-registro.png` com a geometria de volta |

Log do aparelho na importação:
`[Import] pacote absorvido: 4 desenhos, 48 vertices` (uma primitiva interna mais
três nós) e
`[Import] Fontes/torre.glb objetos=3 reimport=0 texturas_ignoradas=0 animacoes=0 peles=0`.

Dois defeitos foram encontrados e corrigidos nesta rodada, nenhum deles visível
no host:

1. **O seletor cancelava a própria importação.** Abrir o seletor pausa a
   Activity, e o `onPause` encerrava o pedido — a importação era cancelada no
   exato instante em que o usuário começava a escolher o arquivo. Status
   observado: "Importação interrompida.". Corrigido: `stop()` para só o laço de
   consulta, e o pedido sobrevive à pausa.
2. **A troca do pacote gráfico era recusada.** O alocador não sobrescreve uma
   alça de buffer viva, então recriar o buffer de instâncias falhava com o
   pacote novo já publicado. Corrigido soltando os buffers antes de recriar, e
   recriando também os descritores de culling e compactação — eles apontavam
   para o buffer indireto substituído, e deixá-los velhos faria a GPU escrever
   contagens de desenho em memória liberada.

Estado do projeto de validação ao fim: `Torre`/`Base`/`Braco` aparecem **em
duplicata**. São duas importações feitas antes de a gravação do registro existir
na build instalada; as duas apontam para as mesmas três identidades, e é por
isso que se sobrepõem exatamente. Não é defeito do caminho atual — uma
reimportação com o registro presente devolve `reimport=1` e cria zero objetos.

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

### Luzes (`tests/native/test_runtime_lights.cpp`, 6)

- o componente existe para as três bocas — schema, catálogo do inspetor e
  arquivo — e sobrevive à ida e volta da serialização; cone interno maior que o
  externo é recusado na autoria;
- coleta com pose de MUNDO pela hierarquia, e ancestral desativado ou interruptor
  apagado tiram a luz do quadro;
- orçamento determinístico: mais influente primeiro, empate pelo menor id, o que
  não coube é **contado**; direcional não ocupa vaga pontual e a segunda
  direcional é declarada excedente; intensidade zero não rouba vaga;
- janela do cone pré-calculada — a mesma conta que o fragmento faz — dá 1 no eixo
  e 0 no ângulo externo, e a pontual dá 1 em qualquer direção pela mesma conta;
- a matriz de capacidades de sombra é publicada e a Luz **não** expõe
  interruptor de sombra enquanto pontual e spot não tiverem passe;
- luz alterada por código durante o Play chega ao quadro coletado, e o Stop
  devolve a autoria intacta.

### Recursos e importação (`test_asset_registry.cpp` 6, `test_gltf_import.cpp` 6, `test_editor_model_import.cpp` 4, `test_editor_map_scene.cpp` +3)

- SHA-256 conferido contra os vetores publicados, incluindo a borda de 56 bytes
  do padding — um hash que muda de implementação entre plataformas não serve
  para dizer "este arquivo é o mesmo";
- identidade sobrevive a renomear/mover e a editar o conteúdo; caminho ocupado é
  recusado; dependência pendurada é recusada na entrada; apagar recurso com
  dependente é recusado; caminho que escapa do projeto é recusado; leitura
  inválida deixa o registro anterior intacto;
- o leitor de JSON aceita documentos reais e falha fechado em doze formas de
  arquivo quebrado, com limite de profundidade;
- GLB montado byte a byte no teste: geometria, hierarquia e fatores de material;
  instâncias com **uma** cópia da geometria; o que ficou para trás é contado;
  arquivo quebrado, `.gltf` de texto, extensão exigida, índice fora da faixa e
  ciclo na hierarquia recusados **com motivo**; limites e cancelamento;
- **chave estável na reimportação**: o teste reexporta o arquivo com um nó novo
  ANTES do existente e exige que a chave do existente não mude — ele pegou uma
  ordem de travessia invertida na raiz, corrigida;
- referência de malha sobrevive ao pacote ser reordenado; cena sem identidade a
  adota; identidade ausente vira slot zero em vez de apontar para outra malha;
  arquivo do componente v1 continua carregando;
- importação cria objetos apontando para a geometria nova, reimportar não
  duplica, importação recusada deixa a cena e o registro exatamente como
  estavam, e a identidade sobrevive a salvar e reabrir o projeto.

### Play com scripts (`tests/native/test_editor_play_scripts.cpp`, 3)

- contato sólido atravessando `EditorPlayScene` → `ScriptBridge` → ABI, com o
  par correto e a descrição de anexos chegando ao runtime;
- ABI incompleta **recusa** o Play em vez de perder eventos em silêncio;
- material escrito por código durante a execução chega ao estado de desenho,
  liga o override e não contamina a autoria depois do Stop.

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

- **Entrega B**: registro com GUID, identidade de malha, leitor de GLB e
  importação pelo seletor do Android estão implementados e validados no
  aparelho. Continuam **fora**: renomear/mover/apagar recursos pelo painel de
  arquivos com confirmação e dependentes, cenas reutilizáveis com remapeamento
  de IDs, texturas na importação, e `.gltf` com arquivos externos.
- **Entrega C**: a Luz anexável (direcional, pontual e spot) e o override de
  material por instância estão implementados, integrados ao renderer e validados
  no aparelho. Continuam **fora**: MaterialAsset compartilhado, slots por
  submesh, pré-visualização isolada de material, e sombra de luz pontual e de
  spot (não existe passe para elas — a matriz de capacidades diz isso).
- Luzes pontuais/spot têm teto de **8** por quadro. O excedente é contado e
  avisado no log do shell; ainda **não** aparece no console do editor, porque o
  console da entrega F não existe.
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
