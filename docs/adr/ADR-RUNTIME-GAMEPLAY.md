# ADR — Mundo de execução, schema de componentes e ABI de scripts

Data: 10/09/2026. Contexto: entregas A, D e E de
[Próximo pacote — criação de gameplay](../PROXIMO-PACOTE-GAMEPLAY.md).
Base: `a34d0325d6c4302942c14442a9d900a199d4cdd0`, branch `codex/gameplay-runtime`.

## Problema

O Play executava sobre uma **cópia de `EditorDocument`**. Física e scripts liam o
modelo do editor, então:

- não havia como ligar o mesmo runtime a um consumidor sem editor;
- a API em C# só endereçava objetos por `ObjectId` cru, sem geração nem mundo;
- as regras de "este componente pode ser anexado aqui" existiam duas vezes — uma
  no catálogo do inspetor, outra implícita no caminho de execução;
- consultas físicas e contatos sólidos não chegavam ao projeto de forma alguma.

## Decisão

### 1. O grafo de cena sai do editor

`runtime::SceneGraph` (native/runtime/scene_graph.h) guarda os objetos sem
conhecer editor, UI, histórico ou seleção. `EditorDocument` passou a **ser** esse
grafo, acrescentando só as invariantes autorais do Inspector por um ponto de
extensão (`acceptObject`). Não existem duas definições de "objeto de cena".

Consequência aceita: `runtime::SceneObject` carrega campos que só o editor usa
hoje (`castShadow`, `environment`, `rigidBody` legado). Eliminá-los é trabalho de
migração de arquivo, não desta decisão.

### 2. `GameWorld` possui a sessão de execução

Identidade é `{mundo, id, geração}`:

- **mundo** é um contador do processo. Um handle guardado por um script de uma
  sessão anterior de Play é **recusado**, não reinterpretado no mundo novo;
- **geração** zera na destruição. Ids nunca são reciclados dentro de um mundo,
  então um handle antigo nunca acerta um objeto novo.

### 3. Ponto seguro assimétrico — e por quê

| Operação | Momento | Motivo |
|---|---|---|
| Criar objeto/componente | **Imediato** | Não invalida referência nenhuma. O handle devolvido já funciona, e o script configura o objeto na mesma linha |
| Destruir objeto | Referência **vence na hora**, armazenamento sai no `flush()` | Invalida referências: quem guardou o handle precisa ser recusado ainda dentro do callback, mas a iteração em curso não pode ver o armazenamento sumir |
| Reparentear | `flush()` | Muda a ordem de travessia |
| Remover componente | `flush()` | Mesmo motivo da destruição, em escala menor |

`flush()` relata os ids removidos, e é assim que o adaptador de física solta os
corpos do Jolt correspondentes.

### 4. Autoridade de pose

`TransformAuthority` diz quem publica a pose: corpo físico, personagem ou
ninguém. Escrever transform por script sobre uma pose simulada — **ou sobre um
ancestral dela** — é recusado com motivo (`TransformOwnedByPhysics`), em vez de
aceitar a escrita e deixá-la voltar no passo seguinte.

### 5. Schema comum de componentes

`scene/component_schema.h` centraliza nome, descrição, categoria, exigências,
incompatibilidades e mutabilidade em Play. O catálogo do inspetor **deriva** dele
e acrescenta apenas o que é decisão de interface (ícone, grupo de propriedades).
O mundo de execução aplica o mesmo contrato, e a API em C# o herda por
consequência. Um teste nativo verifica que catálogo e schema não voltaram a ser
duas listas.

### 6. Camadas de gameplay valem no solver

A `ObjectLayer` do Jolt passou a codificar **camada de gameplay + classe de
movimento**; a matriz de interação do mundo é consultada pelo
`ObjectLayerPairFilter`. Um par proibido **não gera contato**, em vez de gerar e
ser descartado depois. A matriz precisa ser recíproca — o Jolt consulta o par uma
vez só, em ordem não especificada — e uma matriz assimétrica é recusada.

Limite declarado: 32 camadas (uma máscara de 32 bits por linha). A camada é do
**corpo**, não do colisor: o Jolt filtra por corpo, e prometer camadas por
subforma seria uma propriedade sem efeito.

### 7. Consultas devolvem contato real

`RayCastClosestV2`/`RayCastAllV2` fazem uma **segunda consulta ao corpo acertado**
(`Body::GetWorldSpaceSurfaceNormal`) para devolver a normal de superfície. Zero
não é usado como "normal" em lugar nenhum: `hasNormal` diz quando ela existe.
Sobreposição parada não tem direção ao longo de quê e sai sem normal; o fim de um
contato também, porque o Jolt não informa geometria em `OnContactRemoved`.

A instância do **colisor** que respondeu é resolvida pelo dado de usuário da
subforma, que a criação do composto já gravava com o índice da parte.

### 8. Entrada é dado do projeto

O núcleo não conhece "Mover" nem "Saltar". `InputActionMap` declara as ações e
diz quais **papéis** elas cumprem; renomear a ação leva o papel junto. Perder o
foco para a interface zera as ações e **solta os botões na hora** — é a regra que
impede o personagem de continuar andando porque o dedo saiu para um painel.

### 9. ABI de scripts versionada em v5

Campos de v2 permanecem nas mesmas posições; v3 acrescentou hierarquia, ciclo de
vida, componentes por instância, propriedades tipadas e transform de mundo; v4,
consultas físicas e contatos sólidos; v5, ações de entrada. O lado gerenciado
exige a versão corrente e confere `size`: uma struct maior do que a acordada
seria lida além do fim do que o nativo alocou.

## Alternativas descartadas

- **Manter a cópia de `EditorDocument` e só renomear o arquivo.** Continuaria
  importando editor no caminho de execução; o texto do pacote pede exatamente o
  contrário.
- **Filtrar camadas só nas consultas.** Seria uma matriz sem efeito na simulação:
  dois objetos "que não colidem" continuariam empurrando um ao outro.
- **Nomes de ação fixos no núcleo (`Move`, `Jump`).** Transformaria em contrato
  do motor algo que o pacote define como dado editável.
- **`GameObject` como struct em C#.** `Object.WorldTransform = ...` não compila
  (CS1612). Quem revelou isso foi o primeiro modelo de comportamento escrito
  contra a API — e é por isso que os modelos são compilados no teste.

## Consequências

- O arquivo de cena foi para **v12**: v11 acrescentou as camadas, v12 as ações.
  Um arquivo v10 abre com camadas e ações padrão, que são o comportamento
  anterior a estes recursos.
- `editor_scene_physics.*` e `editor_script_bridge.*` deixaram de existir; o
  conteúdo vive em `runtime/scene_physics.*` e `runtime/script_bridge.*`.
- Sensor **por colisor** continua fora: o sensor pertence ao corpo inteiro, e o
  backend filtra por corpo. Um checkbox por colisor seria propriedade sem efeito.
- `CharacterVirtual` continua sem participar da broadphase como corpo rígido:
  sensores e consultas **não** acertam o personagem. Está no pendente, não
  resolvido.
