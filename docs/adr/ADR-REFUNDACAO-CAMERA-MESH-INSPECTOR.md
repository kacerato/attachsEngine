# Câmera, malha e inspetor por composição

Data: 10/09/2026. Estado atualizado: compilação e rodada integrada autorizadas e executadas. Ver [resultados e limites](../validacao/2026-09-10-componentes-codigo.md); shaders regenerados, autoria por toque e câmera em Play comprovadas.

Este bloco segue V2 §§18.1–18.4 e o complemento `ASTRA_COMPONENTES_CODIGO_EDITOR.md`. Durante sua escrita, o usuário suspendeu builds, testes e ADB. A autorização posterior “eu autorizo” permitiu a rodada vinculada acima. Ela não encerra os marcos de componentes, runtime ou scripting.

## Problema e resultado no código

Antes, `EditorEntityKind::Camera` escolhia a câmera do jogo. `assetId` e `material` eram campos de qualquer entidade, fora da coleção de componentes. O catálogo Add não podia anexar essas capacidades a um objeto vazio. Transformação, Material e Propriedades ocupavam abas independentes.

Agora `scene::Camera` e `scene::MeshRenderer` possuem descritores e armazenamento na coleção `Components`. `EditorEntity` deixa de possuir `assetId` e `material`. Os consumidores consultam capacidades: resolver câmera, controlar olhar, extrair desenhos, selecionar geometria, enquadrar e ajustar colisor. Criar Câmera passa a compor objeto e componente; instanciar geometria faz o mesmo com Malha.

`kind` continua como metadado histórico e no editor opcional de água. Não escolhe a câmera nem habilita a extração de malha. Um objeto criado como grupo pode receber malha, aparecer e ser selecionado no viewport. Isso não transforma a atual cópia Play de `EditorDocument` em um player independente; essa separação continua aberta.

## Referências consultadas e limite da comparação

As imagens enviadas pelo usuário são referência visual, não especificação de APIs que existiriam na Astra:

| Arquivos em Downloads | Observação visível | Adaptação escrita |
|---|---|---|
| `photo_4979250647223962777_y.jpg` | Hierarquia à esquerda, cena central, arquivos/propriedades na lateral | Manter a cena com retângulo próprio e painéis ajustáveis; a área central de código já existente continua acessível |
| `photo_4979250647223962776_y.jpg`, cópia `(1)` e `photo_4979250647223962781_y.jpg` | Transform e componentes em cabeçalhos recolhíveis | Lista única de cabeçalhos compactos com nomes curtos, ícones PNG e abertura por toque |
| `photo_4979250647223962779_y.jpg`, `photo_4979250647223962778_y.jpg` | Adição dividida em famílias | Busca por nome, descrição e TypeId; categorias do catálogo e tipos C# do schema aplicado |
| `photo_4979250647223962774_y.jpg`, `photo_4979250647223962775_y.jpg` e cópia `(1)` | Criação de objetos separada da composição | Preservar a criação de objetos como atalhos; não adicionar menus UI/NPC/Voxel sem implementação |
| `photo_4979250647223962782_y.jpg` | Geometria/material dentro do renderer, referência e busca | Duas páginas internas de Malha, seletor de geometria real do pacote e edição dos parâmetros do material |

A Astra conserva carvão, branco e acento lima, com os próprios ícones. As imagens da referência não foram copiadas para o produto. As miniaturas 3D das imagens não foram simuladas por ilustrações: o seletor apresenta identificação, material e contagem de triângulos lidos da biblioteca.

Consulta primária nesta rodada: [Camera da Unity 6](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Camera.html), [Mesh Renderer da Unity 6](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshRenderer.html) e [índice fornecido pelo usuário](https://github.com/kacerato/reposplugins). Projeções, materiais, render targets e opções dessas referências não constituem evidência de suporte Astra. A correspondência por tipo continua em [componentes](../componentes/README.md).

## Contrato de Câmera

TypeId `astra.camera`, versão 1, uma por objeto. Nenhuma restrição por nome, tipo histórico, malha, corpo ou movimento. Transform local e hierarquia determinam a pose. `astra.camera.look` passa a requerer essa capacidade, em vez de requerer um tipo especial de objeto.

| PropertyId | Valor inicial | Limite / semântica |
|---|---|---|
| `enabled` | true | Candidata à câmera do Play |
| `vertical_fov` | 60 | Graus; 1 a 170 |
| `near_plane` | 0,1 | Metros; 0,001 a 10.000 |
| `far_plane` | 2.000 | Metros; 0,01 a 1.000.000, estritamente maior que near |
| `priority` | 0 | Inteiro de −10.000 a 10.000 |

Entre as câmeras habilitadas em hierarquias ativas, vence a maior prioridade; o menor ObjectId desempata. Existe uma saída de câmera nesta implementação. Ao não haver candidata, a sessão mantém o comportamento anterior de usar a vista do editor como fallback. Remover Câmera não apaga os valores de Olhar; o inspetor informa a dependência ausente, e o consumidor de olhar não o executa como câmera.

O Android recebe pose, FOV e planos da câmera escolhida. O FOV foi ligado aos fontes de projeção de vértices, raios do céu, cascatas de sombra, frustum, seleção de LOD e reprojeção temporal. Mudanças de FOV, planos ou proporção invalidam históricos de oclusão/cor e cache de sombras. Os valores configurados da vista do editor são restaurados fora do Play.

O transporte de focal reutiliza campos transientes reservados, sem aumentar os push constants de 128 bytes: `shadowTransitionParameters.z` no uniforme, `materialFactors.w` no passe do céu e `sourceTransform.z` no pós-processamento. Essa última posição não tinha consumidor GLSL para seu antigo valor de escala. Os fontes GLSL foram editados; **SPIR-V e headers derivados ainda precisam ser regenerados quando builds forem autorizados**. Binários antigos não implementam esse caminho.

Ortográfica, roll, lente física, viewport por câmera, masks, múltiplas saídas, render targets e alternância por API pública C# permanecem pendentes. O editor expõe somente os campos cujo consumidor foi escrito.

## Contrato de Malha

TypeId `astra.render.mesh`, versão 1, uma por objeto. Possui `mesh` (referência local ao pacote), `enabled` e `MaterialParameters`. A referência zero significa sem geometria. A ausência do componente também significa ausência dessa capacidade; não há cubo implícito.

O atual identificador de geometria é qualificado pelo fingerprint da cena/pacote. Ainda não é um AssetGuid de projeto. O seletor usa a biblioteca que a sessão realmente possui; não interpreta nomes de arquivos inexistentes. Água usa um fluxo especializado anterior: o seletor comum não aceita recursos de água e não troca a geometria de uma entidade de água.

| PropertyId do material | Inicial | Limite |
|---|---|---|
| `base_color.r`, `.g`, `.b` | 1 | 0–1 |
| `roughness` | 0,5 | 0–1 |
| `metallic` | 0 | 0–1 |
| `normal_scale` | 1 | 0–16 |
| `specular` | 1 | 0–1 |
| `emission.r`, `.g`, `.b` | 0 | 0–1 |
| `emission_strength` | 1 | 0–10.000 |

Editar um parâmetro ativa o override da instância. Referenciar outra geometria mantém overrides explícitos; quando não existem, carrega parâmetros da nova origem. Restaurar material da origem desativa o override e recupera os parâmetros do recurso. Texturas e classe de pipeline continuam pertencendo ao material de origem. Não foi inventado um MaterialAsset compartilhado.

`scene::MaterialParameters` é independente do renderer. `renderer::MaterialOverride` passa a ser um alias usado pelos adaptadores existentes. Renderização e picking respeitam a presença da malha e sua habilitação. Colisor continua independente da visibilidade: ajustar à malha lê a geometria mesmo quando o renderer está desabilitado. Trocar a referência visual não recalcula silenciosamente o colisor; Ajustar à malha permanece uma operação explícita e desfazível.

## Inspetor e comandos

O inspetor normal apresenta nome, atividade e quantidade de componentes, seguido de cabeçalhos recolhidos. Transformação é o bloco de autoria TRS já existente, não um novo componente removível. As abas globais Transformação/Material/Propriedades deixam de dividir o fluxo normal.

Cabeçalhos fechados usam 44 dp e a ordem de anexação. A lista pagina quando necessário. Ao abrir um componente, ele ocupa a área de campos; tocar novamente no cabeçalho retorna à lista. Essa adaptação evita espremer vários formulários numa tela de telefone. O menu contextual oferece remoção sem uma barra permanente de ações por componente. Novos componentes ficam fechados, na última página da lista.

Add tem busca e categorias Todos/Câmera/Visual/Física/Código. A categoria nativa vem de `EditorComponentEntry`, não de uma convenção de nomes. Tipos já anexados ou incompatíveis exibem a razão e não registram uma ação de adição. Scripts continuam vindo do schema C# aplicado; classes utilitárias não entram no menu.

Campos numéricos nativos agora usam `ComponentType::numbers`. O diálogo conserva ObjectId, instanceId e PropertyId, e o setter tipado recusa valor fora do intervalo, conflito entre campos ou resposta obsoleta. Os antigos índices de propriedades ficam nos adaptadores de transformação/arquivos históricos; não são necessários para acrescentar números aos novos componentes. Campos, paginação e cabeçalhos compartilham os retângulos que são desenhados.

`EditorAction::AssignMesh` e `RestoreMaterial` passam pela versão da cena e pelo histórico. Referência inválida não altera o documento. Adicionar, remover, editar parâmetros, restaurar material e trocar geometria continuam operações de autoria, com Undo/Redo. A habilitação do objeto e sua visibilidade são independentes da capacidade Malha.

## Arquivo v10 e migração

`AETHER_EDITOR 10` grava câmera e malha somente na coleção de componentes. Os slots históricos de asset/material no cabeçalho ficam zerados, e os números 9–19 não são gravados na seção escalar. O leitor v10 recusa uma segunda representação nesses slots.

O leitor das versões 1–9 reconstrói Malha a partir de referência, enabled e valores antigos de material. Preserva parâmetros não padrão mesmo quando o override estava desligado. Um objeto historicamente Camera recebe o novo componente durante essa migração. Em v10, remover Câmera ou Malha é persistente: recarregar não recria a capacidade pelo `kind`.

Identidades das instâncias existentes continuam sob o contrato v9; componentes introduzidos pela migração recebem novos IDs. Dados desconhecidos mantêm TypeId/versão/payload. Migração e leitura preparam um documento candidato antes de substituir o documento aberto. A compatibilidade foi implementada em fonte, **não comprovada por execução**.

## Pendências preservadas

| Área | Trabalho restante |
|---|---|
| Aceitação deste bloco | Regenerar shaders, compilar nativo/managed/Android, adaptar expectativas antigas de UI, executar casos e usar ADB somente após autorização |
| Câmera | Projeções adicionais, roll, masks, targets, várias saídas, API de runtime independente |
| Recursos | GUIDs, importação geral, assets separados de material, submeshes/slots, instâncias de recursos, miniaturas renderizadas e locate no projeto |
| Componentes | Transform opcional de domínio; descritor completo de unidades/defaults/referências/eventos; binding nativo/C# comum |
| Física | Compound, owner, pose/rotação, sensor por corpo, quatro juntas e eventos C# escritos na continuação; filtros, ShapeAsset, contatos sólidos, eventos por subforma e juntas avançadas pendentes |
| Código | Autocomplete, destaque semântico, debugger, migração de schemas, seleção de AssetReference genérica, hot reload e restauração de buffers |
| Layout | Calibrar a densidade real nas proporções do aparelho, navegação por teclado, rolagem contínua e miniaturas; ícones exclusivos dos novos tipos ainda usam os PNGs Astra existentes de câmera/objeto |
| Runtime e produto | Mundo independente de EditorDocument, módulos de água separados, exportação, cena vazia e gerenciamento completo do projeto conforme os demais marcos |

Nenhum build, teste, APK, instalação, captura ou comando ADB foi executado nesta rodada. Os fixtures existentes que acessavam campos removidos tiveram apenas a adaptação mecânica à API de componentes; não foram executados nem ampliados em uma bateria de testes. Resultados históricos continuam históricos.


Continuação em [Composição física e referências](ADR-REFUNDACAO-COMPOSICAO-FISICA.md): os novos consumidores/seletores existem em fonte, sem compilação nem execução. As pendências acima foram atualizadas para distinguir implementação e aceitação.
