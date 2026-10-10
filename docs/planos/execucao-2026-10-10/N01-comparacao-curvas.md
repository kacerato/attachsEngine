# N01 — comparar curvas sem reescrever os valores

Estado: implementação integrada, aceita em host; 50/50 cenários do alvo editor.
Não encerra regiões de marcadores, N01 ou o plano inteiro.

## Decisão e referências

NÃO IREI SER SIMPLISTA NO DESIGN.

A interface anterior mostrava um canal por vez: valores de unidades diferentes
não podiam disputar um eixo sem contexto. A solução mantém um canal editável e
até oito referências fixadas, com normalização exclusivamente visual. Vista e
Comparar compartilham uma folha temporária; a lista de referências tem paginação,
nomes de camada/alvo/propriedade e permite tornar uma referência o canal ativo.
Não há painel adicional permanente sobre a cena.

- [Unity 6000.3, Animation Curves](https://docs.unity.com/en-us/engine/6000.3/manual/animation-section/animation-mecanim/animation-clips/animation-editor-guide/animeditor-animation-curves):
  propriedades selecionadas compartilham o gráfico, sem perder suas identidades.
- [Unity 6000.0, Editing Curves](https://docs.unity3d.com/6000.0/Documentation/Manual/EditingCurves.html):
  enquadramento de seleção e edição numérica são operações distintas de zoom.
- [Godot 4.5, Animation Track Types](https://docs.godotengine.org/en/4.5/tutorials/animation/animation_track_types.html)
  e [editor oficial](https://github.com/godotengine/godot/blob/4.5-stable/editor/animation/animation_bezier_editor.cpp):
  tempo/valor têm navegação independente; tangentes pertencem ao dado autoral.

Essas referências orientam ergonomia e separação de responsabilidades. A Astra
usa sua identidade angular, seu sampler e os mesmos controles de toque.
O vídeo de workflow previamente localizado não pôde ser aberto pelo browser do
ambiente; não é declarado como assistido ou usado como evidência visual.

## Autoria, dados e custo

Pins usam identidade estável de track/componente; troca de camada conserva
referências, remoção do canal aposenta seu pin e fechar/trocar clipe libera tudo.
São estado transitório do editor: AECLIP e a ABI 10 permanecem inalterados.
Os comandos de edição numérica, chave, tangente e histórico continuam os mesmos.

Enquadrar calcula os extremos estacionários reais dos segmentos cúbicos usando
`animationCurveExtremaTimes`, incluindo overshoot entre chaves, mais as fronteiras
e os handles ativos. Esses limites ficam em cache durante o gesto. Normalizar
aplica a transformação afim de cada canal para o eixo `n`; o movimento vertical
e os handles usam a transformação inversa para publicar valores autorais.
Um canal constante recebe faixa finita. Zoom, pan e isolamento não editam o recurso
nem acrescentam Undo. Enquadrar seleção limita o tempo aos IDs selecionados.

Referências são tracejadas e não recebem handles ou hit targets; editar exige
tornar o canal ativo. Render de curvas tem 384 passos por canal, até nove canais,
quatro verificações de fronteira por passo e até 512 representantes de segmentos
degrau. Fontes densas têm representação visual limitada, conservando todas as
chaves nos dados e no acesso numérico. Degraus representados usam fronteiras reais,
sem diagonal que sugira interpolação ausente.

Os glyphs `AnimationCurveCompare` e `AnimationCurveNormalize` foram gerados com
as primitivas Astra e integrados ao atlas de produção de 332 ícones. Não são
imagens conceituais substituindo controles executáveis.

## Aceite

Um cenário integrado cria translação e escala com amplitudes distintas; fixa
referências, normaliza, isola e navega pela UI em 655×300. Confere recurso e
histórico intactos nessas operações, seleção precisa, enquadramento de seleção,
arraste normalizado cancelado/publicado, edição numérica em unidade real,
overshoot, preservação da cena fonte e reabertura do valor autoral.

Build aprovado em `host/build-curves-rename-fix.log`; cenário focado aprovado e
rodada integrada **50/50**, em `host/editor-curves-accepted.log`. A rodada anterior
49/50 revelou a perda do acesso Nome no cabeçalho Curvas; o acesso foi restaurado
e o fluxo de renomear passou novamente. A fixture do cenário novo também foi
corrigida para considerar que criar um clipe já cria canais TRS completos.

Capturas `curves-view-655.png`, `curves-compare-655.png` e
`curves-normalized-655.png`, em `host/capturas/`, foram inspecionadas: lista
identificada, controles de tempo/valor alcançáveis e gráfico com valor numérico
autoral legível em 655×300. São controles/curvas rasterizados pelo executável CPU,
sem certificar malha Vulkan ou Android. ADB continua reservado para o fim do plano.
