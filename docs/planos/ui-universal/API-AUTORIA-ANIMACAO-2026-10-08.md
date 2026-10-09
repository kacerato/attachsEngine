# API de autoria de animação

Implementação de B4 em revisão local; o APK code 19 não contém o SDK abaixo.
O Dev code 20 contém o SDK e passou pelo caminho IDE → publicar → executar →
editar/preview → reabrir no aparelho. A distribuição pública permanece pendente.
O code 21 contém a ABI 2 de bake, passou no host e foi compilado/instalado.
Em 09/10 passou pelo bake por toque, redução 605 → 15 chaves, Undo/Redo e
sonda C# de duas voltas compilada/publicada pelo IDE: 183 → 32 chaves.
Todos os 477 quadros das duas gravações foram examinados; recursos UI/API
reabriram byte idênticos, com fonte GLB preservada. Consulte BAKE.md.
O code 22 contém AECLIP 3/ABI 3 e camadas de autoria; passou por criação e
leitura via SDK embarcado, cold reopen e cinco verificações Animator no
POCO F7. Consulte `docs/validacao/animation-layers-2026-10-09/LAYERS.md`.
O estado dos builds e do aceite fica no relatório de validação. Esta API não
declara B4 nem equivalência UMotion completos.

## Caminho de uso

Selecione um objeto na cena. No IDE, escolha Modelos de código → Ferramenta
de animação, dê um nome e compile. Abra Ferramentas do editor e execute
“Criar giro no objeto”. O clipe é um recurso editável do projeto e aparece
no seletor do Animation Studio. Não exige Behavior, personagem nem Play.

O modelo autorado é um arquivo C# do projeto. Qualquer método
`public static void Nome(EditorContext editor)` com `[EditorCommand("Rótulo")]`
entra no catálogo da **geração publicada**. Editar ou falhar na compilação
não substitui essa geração. Métodos async, genéricos, de instância ou com
assinatura diferente são recusados antes de executar.

```csharp
using Astra.Editor;

public static class MinhasFerramentas
{
    [EditorCommand("Renomear clipe")]
    public static void Renomear(EditorContext editor)
    {
        var clip = editor.Clips[0];
        using var edit = editor.BeginClip(clip);
        edit.SetName("Abertura");
        edit.SetDisplayRate(30);
        edit.Commit();
    }
}
```

## Contratos

| Superfície | Comportamento efetivo |
| --- | --- |
| `EditorContext.SelectedObject`, `Clips`, `ImportedClips` | Seleção e catálogo real do projeto; GUIDs estáveis, sem depender do painel aberto. |
| `CreateClip`, `ExtractClip` | Criação/extração registrada em disco e no histórico; a extração preserva a fonte. |
| `BeginClip(guid, revision)`, `InspectClip` | Rascunho isolado/snapshot; revisão zero usa a atual, revisão explícita vencida é recusada. |
| `AddTrack`, `RemoveTrack` | Bindings relativos canônicos, GUID do nó importado opcional, translação/rotação/escala/morphs. |
| `PutKey`, `SplitKey`, `EraseKey`, `PutPose` | IDs, tangentes, pesos e grupos de rotação usam as mesmas operações nativas da UI. |
| `Retime`, `Reverse`, `Crop`, `SetRotation` | Transformações verificadas; conversão que perderia curvas/voltas exige bake explícito. |
| `Bake(track, ClipBakeSettings)` | Extensão ABI 2 aceita no host e no Dev code 21: amostragem adaptativa, redução opcional, conversão Euler → quaternion/progressivo e relatório de erro medido. Publica somente com `Commit`. Sonda física preservou duas voltas; ausente no code 20. |
| `Copy`, `Paste`, `TransformSelection`, `EraseSelection` | Clipboard proprietário tipado e grupos de pose; substituição/inserção nos tracks ou em todos. |
| `AddLayer`, `ConfigureLayer`, `DuplicateLayer`, `MoveLayer`, `RemoveLayer` | ABI 3: pilha de até 32 camadas incluindo Base fixa; peso, modo Override/Additive, referência neutra/temporal, Mudo/Solo e IDs monotônicos. |
| `AddLayerTrack`, `CopyBaseToTrack` | Máscara esparsa por propriedade; canal neutro/copiado com modo de rotação próprio, ou cópia explícita da Base para o canal selecionado. |
| `Sample`, `SampleCurve`, `SampleComposed` | Sample lê canal cru; SampleComposed (ABI 3) lê a propriedade após a pilha de camadas. Rotação retorna quaternion XYZW; SampleCurve retorna valor/derivada da curva crua. |
| `Snapshot` | AECLIP 2/3 convertido em inspeção tipada: origem/hash, IDs, bindings, camadas, tracks, todas as curvas/chaves/flags. Não publica por alterar uma cópia C#. |
| `Commit`, `Dispose` | Commit publica um passo de recurso no histórico; Dispose descarta o rascunho sem publicação. |

Tempo é em segundos, slopes em valor/segundo, weights em fração do segmento.
Euler é em graus; a velocidade de rotação progressiva é derivada das poses,
não um quinto componente fornecido por `PutPose`. Morphs têm até 64 valores.
`ClipBindingPath.Join("A/B", "Junta")` gera `A%2FB/Junta`; não use nomes como
fallback quando um caminho explícito não resolver. Canais de propriedades
arbitrárias, eventos e drivers de autoria ainda não fazem parte dessa API.

## Vida e falhas

A ABI 2 mantém o prefixo da ABI 1; o SDK novo lê apenas os bytes declarados
por uma engine ABI 1 e recusa `Bake` nesse contexto. Em 64 bits, a tabela
passa de 168 para 176 bytes. Os argumentos do bake têm 32 bytes e o relatório,
24. Não alterar tabela nativa e SDK separadamente no pacote distribuído.

A ABI 3 preserva esses prefixos e acrescenta SampleComposed, totalizando
184 bytes em 64 bits. Comandos de camada usam a estrutura existente de 88
bytes. Hosts antigos recusam essas operações explicitamente. AECLIP 1/2
migra para Base única; versões antigas não leem AECLIP 3. Mudo/Solo salvam
e afetam Play. Edições de tempo ajustam a referência ou recusam a operação
inteira; clipboard entre clipes exige camada por nome/modo não ambíguos.
Snapshot expõe metadados da pilha e a identidade de camada de cada track.

```csharp
using System.Linq;

using var edit = editor.BeginClip(editor.Clips[0]);
var rotation = edit.Snapshot.Tracks.Single(t => t.Property == ClipProperty.Rotation);
var report = edit.Bake(rotation.Id, new ClipBakeSettings(
    SampleRate: 60, Tolerance: 0.05, Reduce: true,
    Rotation: ClipRotation.Quaternion, MaximumFrames: 16384));
edit.Commit();
```

`SampleRate` define a grade inicial, não o número final de chaves: extremos,
degraus e refinamento entram antes da redução. Tolerância usa graus para
rotação e unidades do canal para posição, escala ou morphs. A comparação
percorre as poses amostradas e subdivisões de verificação; não é certificação
de erro contínuo para todo tempo possível. `Rotation: null` conserva o modo.
Quaternion/progressivo → Euler usa XYZ fixo (Rz·Ry·Rx) e exige referência
explícita `EulerReference: new ClipEulerReference(x, y, z)`. A conversão escolhe
o representante equivalente mais próximo da pose anterior, com tratamento da
liberdade X/Z no gimbal. Não oferece ordens Euler alternativas nem recupera
voltas já perdidas pela fonte. Orçamentos de poses, avaliações, comparações e IDs são limites
reais; ultrapassá-los descarta a operação inteira, sem publicar parte dela.
O relatório só descreve essa conversão; não é um benchmark de animação.

A ABI 4 preserva os prefixos 1/2/3 e acrescenta `bakeAdvanced` e `consolidate`:
200 bytes em 64 bits, request avançado de 56 bytes, relatório de 24 bytes.
`ClipBakeSettings.EulerReference` é nullable. `ClipEdit.CreateConsolidated(nome,
settings)` retorna `ClipConsolidation(Clip, Report)`: publica outro GUID/caminho
com uma Base, amostrando a composição real de todas as propriedades, incluindo
peso, referência temporal, Mudo e Solo. Sem formato explícito, rotações saem
como Quaternion. A fonte e o rascunho não são publicados por essa operação;
o novo recurso tem sua própria transação Undo/Redo e não é atribuído ao Animator
automaticamente. Revisão vencida, limite ou candidato inválido recusam a criação.
O relatório agrega contagens; MaximumError é o maior valor numérico medido nos
canais, em suas unidades ou graus. Não é uma norma global entre unidades.

```csharp
using var edit = editor.BeginClip(editor.Clips[0]);
var rotation = edit.Snapshot.Tracks.First(t => t.Property == ClipProperty.Rotation);
edit.Bake(rotation.Id, new ClipBakeSettings(Rotation: ClipRotation.Euler,
    EulerReference: new ClipEulerReference(0, 360, 0)));
var result = edit.CreateConsolidated("Versão consolidada");
// A publicação acima cria só result.Clip; Commit publicaria também o rascunho.
```

Editor: Edição → Bake e redução → Canal/Clipe novo; a página XYZ troca a
primeira linha por referência e campos numéricos. Confirmar Ramo XYZ habilita
uma conversão que precisa dessa referência. As duas linhas preservam a timeline
e usam os novos ícones vetoriais `animation/consolidate` e `rotation-branch`,
integrados a SVG, PNG, enum e atlas. Jobs assíncronos, FK/IK/root motion e exportação
FBX continuam pendentes, separados da consolidação do avaliador existente.

O comando é síncrono na thread do editor e só roda em Edit. O contexto,
rascunhos e clipboards pertencem à invocação; reter/usá-los depois ou em uma
worker é recusado antes de acessar ponteiros nativos. Clipboards possuem
Dispose, permitindo liberar a cópia e reutilizar o limite de oito cópias.
Há até oito rascunhos com 262144 chaves no conjunto deles, e um orçamento
separado de 262144 chaves nas cópias.
Catálogos têm até 65536 clipes; projetos expõem até 256 comandos.

Cada `Commit` e cada criação/extração são transações próprias. Um comando
que cria um recurso, faz commit e depois lança exceção não desfaz retroativamente
essas publicações: elas permanecem no histórico para Undo. Rascunhos sem commit
somem ao encerrar o comando. Não há transação conjunta entre vários recursos
nem comando assíncrono nesta entrega. Falhas comuns chegam ao console e à API
por diagnóstico do editor; a política nativa de falta de memória permanece
a da engine, compilada sem exceções C++.

O snapshot não duplica matemática C#: sampling, normalização de grupos,
conflitos, persistência e reversão vêm do backend real. Compilação do clipe e
catálogo são caches da invocação, invalidados pelas operações de autoria.
Não é uma afirmação de desempenho medido nem um merge de reimportação.

## Origem das decisões

Unity 6000.0 `AnimationUtility.SetEditorCurve` separa edição de curvas da
janela; `MenuItem` permite ações de editor por métodos estáticos. A adaptação
Astra utiliza recurso revisionado e rascunho nativo, com catálogo contextual
no IDE mobile, sem copiar o visual da Unity.

- https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AnimationUtility.SetEditorCurve.html
- https://docs.unity3d.com/6000.0/Documentation/ScriptReference/MenuItem.html

Godot 4.5 `EditorScript` é referência para ferramenta explícita em Edit,
separada do comportamento de Play. A Astra fecha o contexto no fim do comando
e usa o histórico e a publicação dos recursos já existentes.

- https://docs.godotengine.org/en/4.5/classes/class_editorscript.html

Godot 4.5 `Animation.optimize` fundamenta a redução com tolerância configurável;
Unity 6000.0 `AnimationUtility.GetEditorCurve` distingue curvas de autoria das
curvas reunidas pelo runtime. A Astra compara pelo sampler compartilhado,
preserva a fonte importada e publica o resultado no histórico do recurso.

- https://docs.godotengine.org/en/4.5/classes/class_animation.html#class-animation-method-optimize
- https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AnimationUtility.GetEditorCurve.html
