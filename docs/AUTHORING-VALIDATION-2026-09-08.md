# Validação de autoria — 08/09/2026

## Continuação: importação fonte independente

`tools/export-authoring-assets.py` agora normaliza GLB e glTF 2.0, incluindo
múltiplos buffers externos, dados base64 e imagens externas PNG/JPEG, em um
buffer compartilhado alinhado. Não altera os transforms das instâncias.
Dependências externas ficam registradas com SHA-256 no catálogo. URIs remotas
ou caminhos fora da pasta fonte são rejeitados; o importador não acessa rede.
Esta normalização não substitui validação/decodificação completa das imagens.

Recursos glTF e buffer usam nomes por conteúdo; IDs autorais continuam estáveis.
Assim, reimportar não sobrescreve recursos referenciados por um catálogo antigo.
Todos os dados são preparados antes da primeira escrita. Cada arquivo usa
temporário, flush/fsync e replace; o catálogo é publicado por último. Falhas
podem deixar recursos imutáveis não referenciados, mas preservam o catálogo
anterior. Limpeza desses recursos exige rastrear referências e não foi adicionada.

Validação desta continuação: oito testes do exportador e cinco do container glTF
passaram; py_compile e diff --check passaram. Importação real de
`samples/boat/Source/boat.glb` executada; teste compara exatamente todos os
atributos de vértices e índices exportados com os originais. Há testes de
reimportação, buffers externos, imagem, truncamento e falha na publicação.
Nenhum C++/Java foi alterado nesta continuação; builds Android anteriores não
constituem prova da integração desses recursos ao runtime.

Uso offline:
`python tools/export-authoring-assets.py modelo.gltf pasta_saida --source-id identidade`

Limite: o catálogo ainda não é consumido pelo Asset Browser/renderer Android.
Este avanço corrige o importador fonte, não entrega importação pelo touchscreen
nem encerra o teste de reconstrução das demos. Skins continuam recusadas e
animações não são exportadas por este caminho estático.

Status: implementação parcial. O critério de reconstruir as duas cenas do zero
sem alterar código **não foi atingido**. Esta execução não usou subagentes.
O checkout main já continha alterações locais extensas; elas foram preservadas.

## Estado encontrado e decisões

O caminho Android usa editor nativo C++ e renderer Vulkan próprio. A pesquisa
nos módulos nativos, managed e Android não encontrou integração Filament.
Não foi introduzido outro backend. EditorDocument/EditorHistory/EditorSession
já constituíam a fronteira de autoria; EditorMapScene extrai draws do documento.
O documento e o histórico existentes foram estendidos, sem uma segunda UI de
água ou caminhos especiais para presets.

A grade era fixa em [-20,20]. A câmera aceitava distâncias de .01 a 1e6, mas
os planos de corte continuavam vinculados ao pacote. O Inspector já usava
metadata numérica e persistia 13 valores de água. O espectro possuía API real
no backend, mas suas propriedades não estavam no documento do editor.

## Alterações desta execução

- `native/editor/editor_grid.h`: grade alinhada ao mundo com cobertura móvel,
  subdivisões decimais e transição de opacidade entre escalas. Limite de 258
  segmentos, sem alocação dinâmica na geração. Usa o overlay existente;
  não participa do depth test, portanto aparece também sobre objetos.
  É aproximação finita adaptativa, não shader de plano infinito.
- `editor_camera.cpp`: near adapta-se à distância para edição de centímetros;
  far aumenta para enquadrar grandes extensões. `setSceneClipPlanes` transporta
  esses mesmos valores ao renderer, culling e push constants; Play restaura
  os planos do pacote.
- `editor_document.*`, `editor_properties.h`, `editor_archive.cpp`: mais 21
  propriedades de água, no histórico e formato AETHER_EDITOR 4. Leitura de
  v1/v2/v3 preservada; ativação espectral separada mantém herança anterior.
- `android_main.cpp`: os valores autorados alimentam a API espectral existente
  e os controles de espuma. A comparação dos parâmetros evita regeneração por
  alterações de transform ou seleção. A regeneração espectral continua síncrona;
  seu custo durante arrastes precisa de medição em hardware.

Propriedades novas: vento, fetch em metros, profundidade, swell, espalhamento,
amortecimento curto; vento/direção/fetch/swell/espalhamento/peso do swell cruzado;
amplitude e choppiness das três bandas atuais; limiar, crescimento e decaimento
da espuma. Não foram adicionadas cascatas dinâmicas, novos shaders ou spray.

## Referências consultadas

- [Godot water.gd](https://github.com/krautdev/GodotOceanWaves/blob/main/assets/water/water.gd):
  parâmetros de cascatas, atualização no editor, mapas e consultas de altura.
- [Godot wave_generator.gd](https://github.com/krautdev/GodotOceanWaves/blob/main/assets/water/wave_generator.gd):
  separação da geração espectral, FFT e recursos de compute.
- Projeto Unity local `Downloads/extracted/extracted`: inventário dos passes,
  shaders FFT e leitura de `KWS_FFT_ToHeightMap.cs` para lifecycle dos recursos
  de consulta de altura. A auditoria integral de scripts, materiais, cenas e
  texturas pedida no escopo ainda não foi realizada. Nenhum código foi copiado.
- O acesso ao vídeo informado retornou erro; não houve comparação visual dele.

## Validação executada

Build host CMake e **697/697 testes** passaram. Testes adicionados exercitam
cobertura remota da grade, escala, entradas inválidas, planos da câmera,
roundtrip dos 21 campos, migração v3, carga truncada atômica, undo/redo de
ativação e edição de vento pelo roteamento real de toque do Inspector.

Android `:app:assembleDebug :app:assembleRelease`: BUILD SUCCESSFUL.
`git diff --check` nos caminhos alterados: sem erro de whitespace.
Preview de UI 853x394 rasterizado e inspecionado: 523 instâncias, zero descartes,
zero runs sem fonte. Esta imagem usa cena de UI sintética, não captura Vulkan.
Evidências locais: `build/editor-host/authoring-tests.log`,
`build/authoring-android-build.log`, `build/editor-host/adaptive-grid.png`.

ADB executado: lista de aparelhos vazia. Não houve instalação, medição GPU,
captura de água, teste de lifecycle nem soak térmico nesta execução.

## Fluxo disponível e teste de reconstrução

No projeto Ocean existente, abrir Settings e avançar as páginas Water até
Wind m/s; editar vento, fetch e demais parâmetros; confirmar pelo teclado
numérico. Undo/redo restaura valores e ativação. Salvar e reabrir conserva os
campos no arquivo de cena v4. O teste host valida persistência e entrada;
o efeito visual desses novos controles precisa ser observado no aparelho.

Não há passo a passo honesto de criação completa da água em uma cena vazia:
a criação de malha/corpo de água e a importação independente ainda impedem
esse fluxo. Ocean depende do pacote existente. Forest depende da biblioteca
de lotes AEMAP. Barco/física ainda dependem da montagem de runtime da demo.

| Capacidade | Antes | Depois | Validado? | Evidência |
|---|---|---|---|---|
| Parenting/TRS/hierarquia | Existia parcialmente | Preservado | Host, sem nova prova Android | Suíte de documento/mapa/histórico |
| Hierarchy criar/duplicar/reparentar | Existia parcialmente | Preservado | Host | Suíte de sessão |
| Câmera independente | Existia | Corte adapta à escala | Matemática host; Android compilado | test_editor_view.cpp |
| Grade | Fixa ±20 | Adaptativa, cobertura móvel | Host e preview UI | adaptive-grid.png |
| Picking/gizmos | Esferas, ferramentas parciais | Preservado | Host; precisão por triângulo pendente | Suíte existente |
| Inspector | Metadata numérica | Mais 21 propriedades | Sim, host e toque roteado | test_editor_session.cpp |
| Água espectral | Backend existente, fora do documento | Controles persistidos ligados ao backend | Host/build; GPU pendente | android_main.cpp e testes |
| Cascatas adicionáveis/removíveis | Não disponíveis na UI | Ainda pendentes | Não | Três bandas fixas na integração |
| Componentes genéricos | Parcial | Sem alteração | Não nesta execução | EditorEntity ainda é registro fixo |
| Assets/importação independente/DnD | Parcial/desconectado | Sem alteração | Não nesta execução | EditorMapScene usa pacote |
| Material reutilizável como asset | Overrides por instância | Sem alteração | Não como workflow completo | Sem biblioteca autoral independente |
| Play reconstrói física autorada | Demo | Ainda demo | Não | Integração runtime pendente |
| Persistência | v3 | v4, migração v1–v3 | Sim, host | test_editor_map_scene.cpp |
| Equivalência visual à referência | Não demonstrada | Não demonstrada | Não | Aparelho ausente |
| Recriar ambas as demos do zero | Falha arquitetural | Ainda falha | Não | Importação, composição e criação de água pendentes |

## Continuação: projetos reais e arraste por toque

ProjectStore não cria mais os quatro cartões fictícios na primeira abertura nem
após um índice corrompido. Entradas sem diretório são descartadas do catálogo;
diretórios existentes do usuário são preservados. Novos projetos publicam cena
e descritor antes do índice; falhas não retornam sucesso. O índice inválido é
preservado em backup antes de recuperar descritores existentes. Cinco testes
JUnit de filesystem passaram, incluindo falha de criação e recuperação.

O preview host começa vazio ou carrega explicitamente um AEMAP real. A cena
Empty carrega a biblioteca sem instanciar lotes e mantém a câmera de edição
inicial em vez de herdar a câmera distante do pacote. Referências sem malha
continuam reparáveis na hierarquia, mas não produzem esferas fictícias de picking.

Arrastar uma malha do browser para o viewport cria uma instância no plano
horizontal do alvo da câmera; soltar numa linha da hierarquia cria sob aquele
pai. Cancelamento não cria entidade nem comando. Arraste horizontal de uma
linha da hierarquia muda o pai preservando a transformação mundial; ciclos e
shear incompatível são rejeitados. Ambos os fluxos usam o histórico existente.
Isso ainda usa lotes do pacote AEMAP: o catálogo glTF independente não está
integrado ao consumidor do renderer.

Validação: 703/703 testes nativos, 5/5 JUnit, assembleDebug e assembleRelease
passaram. Evidências: build/editor-host/authoring-tests.log e
build/authoring-projects-android.log. Preview vazio: empty-preview.png;
preview de pacote: package-preview.ppm, ambos em build/editor-host.

No Android foi instalado o APK e criado AuthoringValidation pela interface.
A cena inicial salvou somente a raiz. O arraste persistiu uma referência real
de malha. Capturas device-launcher.png, device-empty.png, device-assets.png e
device-drop.png estão em build/editor-host. A geometria não ficou identificável
na última captura; o ADB desconectou antes do enquadramento. A correção posterior
da câmera Empty foi compilada, mas ainda não instalada/validada no aparelho.
Não há aceitação visual completa deste fluxo nem da reconstrução das demos.

Continuam pendentes: recursos autorais independentes no runtime/browser,
materiais reutilizáveis, criação de corpos de água e cascatas editáveis,
composição de componentes e reconstrução da física em Play a partir da cena.

## Continuação posterior: geração de água

A criação de superfícies de água e a edição estrutural das cascatas avançaram
após o registro acima. Estado atualizado, workflow e limitações em
[WATER-AUTHORING.md](WATER-AUTHORING.md). São 709 testes host aprovados e builds
Debug/Release aprovados; lifecycle e visual GPU ainda sem aceitação em aparelho.
Volumes físicos autorais de água continuam pendentes.

## Continuação: rios, volumes físicos e composição da UI

O estado anterior de pendência de volumes foi superado pela implementação descrita
em WATER-AUTHORING.md: rotas de 2–16 pontos, física Jolt extraída da cena,
profundidade finita, esteiras, camadas e UI em resolução de apresentação.
A suíte passou a 715 testes. Debug/Release e testes Java passaram. A última compilação
foi instalada com sucesso no aparelho (Package Manager: 2026-09-08 20:39:25).
A versão intermediária foi inspecionada visualmente; o teste final de Play ficou
com o usuário. Não há medição final GPU desta etapa.
