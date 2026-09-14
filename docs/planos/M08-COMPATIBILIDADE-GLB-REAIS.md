# M08 — correções com GLBs reais

13/09/2026. Continuação de [M06/M08](M06-M08-RECUPERACAO-RECURSOS.md), motivada pelo relato de que nenhum GLB era aceito. A implementação é comum ao formato, sem caminhos ou nomes de carros no código de produção. M08 e M09 permanecem abertos.

## Causas reproduzidas

1. **Ford Lotus:** no aparelho, o seletor preparou 42 nós e 21 desenhos; publicação e instanciação funcionaram na versão anterior. Porém publicar apenas registrava a fonte e instanciar não selecionava nem enquadrava o resultado. O usuário continuava vendo o objeto anterior; a árvore de Arquivos podia esconder a fonte selecionada numa pasta fechada. Isso era um defeito funcional do fluxo, não prova de que o arquivo do usuário estava errado.
2. **Porsche:** o parser recusava qualquer `extensionsRequired`. Este GLB exige `KHR_texture_transform`, que afeta aparência. Depois de admitir explicitamente o perfil geométrico, surgiu outro bloqueio: seus 286.310 triângulos ultrapassavam o limite global de 262.144 usado pela seleção/BVH. Esse erro ocorria depois da publicação gráfica e era apresentado como recusa genérica do pacote.
3. **Contrato de saída:** o caminho legado passava `report.source` por referência para uma função que limpava o próprio `report`. O GUID era zerado antes da procura do recurso. A função agora recebe o GUID por valor; o caminho legado também separa os relatórios e conserva as omissões da importação.
4. **Reabrir o projeto:** a carga guardava um `span` do registro e a publicação substituía seu armazenamento. O acesso seguinte usava memória liberada, causando SIGSEGV no Android. A carga agora possui uma cópia dos registros durante toda a iteração. O documento anterior dizia que já havia uma cópia, mas `const auto` copiava somente o `span`; essa afirmação foi corrigida pela implementação e pela investigação no aparelho.

## Mudanças

- **Entrada na cena explícita:** a revisão oferece `Só recurso` e `Importar na cena`. São operações distintas: a segunda publica e depois instancia em um passo Undo. Falha na instanciação informa que o recurso ficou guardado; não anuncia reversão de uma publicação concluída.
- **Descoberta do resultado:** instanciação seleciona a primeira raiz e enquadra o conjunto de raízes criado, usando bounds mundiais. Não altera escala, origem nem transformações do arquivo. Publicação expande os ancestrais da fonte em Arquivos.
- **Diagnóstico persistente:** falha de leitura, preparação ou publicação aparece em painel, com quebra de texto e páginas, além do console. Codecs obrigatórios ausentes são identificados pelo nome.
- **Acessores:** leitura de sparse com base zero ou buffer intercalado, validação de offsets/contagens/ordenação, tipos inteiros normalizados e não normalizados e `KHR_mesh_quantization`. Escalas positivas de dequantização menores que 0,001 deixam de ser tratadas como singulares; zero continua recusado.
- **Topologia:** TRIANGLE_STRIP e TRIANGLE_FAN viram triângulos com orientação correta e remoção de degenerados da conversão. Índices são lidos como inteiros, sem passar por float; atributos opcionais precisam ter a mesma contagem de POSITION. Malhas sem normais recebem normais acumuladas por área nos vértices compartilhados — não uma implementação de normais flat por face.
- **Nomes repetidos:** labels iguais deixam de bloquear o GLB. Chaves únicas existentes são preservadas; colisões ganham qualificador do nó. Isso não substitui o mapa de identidade persistente e a reconciliação estrutural de M08.2: reordenar nós ambíguos ainda pode alterar a ligação.
- **Seleção:** orçamento alinhado a 4 Mi triângulos, com compartilhamento de BVH para faixas geométricas idênticas. A reserva da árvore deixa de ser duas vezes o número de triângulos. A alteração aumenta o teto de memória; não representa orçamento dinâmico baseado na RAM disponível nem prova com modelos de quatro milhões de triângulos.
- **Instância inteira selecionável:** a instanciação interativa de um arquivo com várias raízes cria um grupo com o nome da fonte. As raízes e suas poses locais são preservadas abaixo dele. Mover o grupo move o conjunto; Undo remove a instância inteira. Arquivos com uma raiz e cenas existentes mantêm sua hierarquia. O caminho legado de importação conserva a forma anterior da cena; isso não implementa SceneAsset ou reconciliação de prefab.

## Aparência: fronteira explícita

O perfil continua sendo **geometria estática com fatores materiais básicos**. Extensões conhecidas de aparência podem entrar nesse perfil, com suas perdas listadas antes da publicação. O GLB original é preservado. Não foi implementado consumidor de texturas PNG/JPEG/KTX2, transformação UV por slot, transmissão, clearcoat, variantes, skins ou animações. Aceitar geometria de um arquivo que usa BasisU não significa decodificar BasisU.

Extensões desconhecidas ou de geometria comprimida, incluindo Draco e meshopt obrigatórios, continuam recusadas com nome e motivo. `.gltf` com dependências externas, reflexão/escala negativa e shear local continuam fora deste caminho. Portanto esta rodada corrige bloqueios gerais e amplia compatibilidade, mas **não deve ser descrita como suporte completo a qualquer GLB**.

## Fontes e evidência

Ford, Porsche e Torre foram copiados dos Downloads do próprio aparelho sem modificar os originais. [CarConcept](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/CarConcept/GLB/CarConcept.glb) foi baixado do repositório oficial Khronos para `build/glb-real`; modelos do usuário não foram incorporados aos arquivos versionados da entrega.

Referências primárias: [KHR_mesh_quantization](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_mesh_quantization/README.md) e [KHR_texture_transform](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_transform/README.md). A primeira usa tipos de acessor e transformações existentes para dequantização; a segunda define transformações das coordenadas de textura. O fallback geométrico da Astra é uma limitação declarada do produto, não uma alegação de conformidade visual dessas extensões.

No host, o modo `aether_tests --import-glb caminho` executa o parser real, publicação com consumidor gráfico de teste, instanciação e extração dos desenhos:

| Arquivo original | Bytes | Nós | Malhas/desenhos importados | Objetos criados | Resultado host |
|---|---:|---:|---:|---:|---|
| Ford Lotus Cortina | 8.171.096 | 42 | 21 | 42 | Passou |
| Porsche 911 Turbo | 90.461.732 | 195 | 113 | 195 | Passou |
| CarConcept | 10.267.996 | 101 | 109 | 125 | Passou |
| Torre | 1.692 | 3 | 3 | 3 | Passou |

Os 125 objetos do CarConcept incluem filhos por primitiva, comportamento atual da Astra; slots materiais autorais continuam pendentes. O probe inclui uma primitiva interna no total de desenhos extraídos.

Regressão focada `glb_`: **12/12**. Inclui acessores esparsos inválidos, contagens incompatíveis, winding, nomes repetidos, escala de dequantização, extensão geométrica sem decoder e o caminho legado. Build host e Android concluídos.

Logs: [regressão](../validacao/evidencias/glb-compat-20260913/regression.log), [arquivos reais](../validacao/evidencias/glb-compat-20260913/real-assets.log), [Android](../validacao/evidencias/glb-compat-20260913/android-build.log).

### Continuação no aparelho — 13/09/2026

O APK `2847E572839FC168D2715057C3C543CE0620D60646EE57C24E9E04B7617315A2` foi instalado e o fluxo **Importar na cena** foi exercitado com Ford e Porsche. Ambos apareceram no viewport Vulkan. A instância adicionada do Ford foi desfeita antes de continuar; as fontes permaneceram registradas.

A rodada seguinte acrescentou o grupo para múltiplas raízes. O teste focado de instâncias independentes passou: mover o grupo altera a pose mundial dos filhos e Undo remove só a última instância. O probe do Porsche passou com **196 objetos** (195 nós da fonte mais o grupo), substituindo a contagem de 195 da tabela anterior para o caminho interativo. [Teste](../validacao/evidencias/glb-compat-20260913/group-test.log).

Durante a reabertura apareceu SIGSEGV por memória invalidada no registro. A simbolização apontou `reimportProjectSources` em `android_main.cpp`, no log que lia `record.source` após publicação. Foi aplicada a cópia proprietária descrita acima. APK final **instalado sem apagar dados**, SHA-256 `08A781D00450431702186DAA08F3DF5467AEA39B4D07B75A49255E053E1B2D78`. [Build Android](../validacao/evidencias/glb-compat-20260913/reload-android-build.log).

No projeto de conferência `M08Recursos0913k`, o Porsche foi importado pela UI, selecionado pelo grupo e movido pelo gizmo para X = 3,78177238. Após Salvar, force-stop e reabertura, as três fontes foram carregadas e o Porsche voltou deslocado. Salvar novamente produziu cena byte a byte igual: SHA-256 `FD9D6D50FF8625018F425F3E676A41ABBE4EF5597E4482DF7F735DE21D2FED8E`, 242 entidades incluindo a raiz da cena, sem duplicação nessa abertura. O registro contém três fontes. A cena de conferência já tinha painéis e uma instância anterior do Ford; as imagens não são comparação de fidelidade material.

Evidência: [Ford na cena](../validacao/evidencias/glb-compat-20260913/ford-na-cena-adb.png), [Porsche movido como grupo](../validacao/evidencias/glb-compat-20260913/porsche-grupo-movido-adb.png), [Porsche reaberto](../validacao/evidencias/glb-compat-20260913/porsche-reaberto-adb.png), [carga das fontes](../validacao/evidencias/glb-compat-20260913/reopen-import.log), [cena salva](../validacao/evidencias/glb-compat-20260913/saved-editor.aescene), [cena reaberta e salva](../validacao/evidencias/glb-compat-20260913/reopened-editor.aescene), [registro](../validacao/evidencias/glb-compat-20260913/saved-assets.astra). A conferência cobre esses fluxos, não soak, falha de energia ou todos os GLBs existentes.

Próxima cobertura necessária: texturas e UV por slot (M09), codecs de geometria com dependências versionadas/licenciadas, transformação refletida sem perda, identidade estrutural e medição de memória/latência com arquivos grandes. Nenhuma regra especial para Ford/Porsche deve ser usada nesses trabalhos.
