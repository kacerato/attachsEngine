# N01 — organização temporal e ciclos

Estado: recurso/editor/SDK aceitos em host; não encerra N01 ou o plano.

## Referências e decisões

- [Unity 6000.0, seleção e manipulação de chaves](https://docs.unity3d.com/6000.0/Documentation/Manual/animeditor-AdvancedKeySelectionAndManipulation.html):
  transformar uma seleção é uma operação temporal sobre identidades existentes.
  A Astra preserva relações dentro de cada canal e recusa colisões explicitamente.
- [Godot 4.5, Animation](https://docs.godotengine.org/en/4.5/classes/class_animation.html)
  e [tipos de trilha](https://docs.godotengine.org/en/4.5/tutorials/animation/animation_track_types.html):
  representação de rotação e interpolação são contratos do recurso, distintos do
  enquadramento da curva e do relógio de loop.
- [Unity 6000.5, loop optimization](https://docs.unity.com/en-us/engine/6000.5/manual/animation-section/animation-mecanim/animation-clips/looping-animation-clips):
  comparar início/fim precede a repetição. A operação Astra descrita aqui fecha
  apenas posição/pose C0; não equivale a correção de velocidade C1 ou root motion.

## Autoria, recurso e runtime

`animation_clip_arrange` amplia os recursos existentes, sem um segundo sampler.
Todos os comandos trabalham em candidato e só publicam depois da validação.
Editor e SDK usam as mesmas funções; persistência e compilação continuam AECLIP 4.

Alinhar início, fim ou centro desloca as chaves selecionadas de cada track como
um bloco. Cues selecionados formam outro bloco. A duração e as diferenças internas
de tempo permanecem. Distribuir ordena os instantes distintos da seleção e os
espaça uniformemente entre início/fim; chaves e cues antes coincidentes continuam
coincidentes. Valores, IDs e handles autorais são preservados. Tangentes automáticas
respondem ao novo intervalo; os handles livres mantêm sua derivada autoral.
Quaternion expande para o grupo sincronizado, incluindo progressão quando presente.
Cruzar chave não selecionada, coincidir na mesma curva ou sair do clipe recusa tudo.

`Início → fim` copia a pose inicial de cada propriedade/camada para a fronteira
final. Não é bake da composição nem mudança da referência aditiva. A operação é
explícita e possui Undo; não se fecha um loop silenciosamente ao repetir.

Repetir exige fronteiras fechadas de todos os tracks, inclusive camadas sem peso
ou silenciadas, para que reativá-las não revele um salto escondido. Tolerância é
absoluta na unidade da propriedade; quaternion usa distância angular em graus.
Uma diferença acima da tolerância informa o canal e o resíduo, sem publicar.
Repetição insere 1–64 ciclos contíguos, sem o quadro extra usado em Duplicar trecho.
Cues usam `[início,fim)`: o cue da fronteira final original se desloca ao fim do
último ciclo; cada ciclo adicional recebe novos IDs para seus cues internos.
Chaves posteriores, duração e referências aditivas acompanham o mesmo ripple.
O orçamento conservador limita o total a 262144 chaves e 86400 segundos.

## Interface móvel e SDK

NÃO IREI SER SIMPLISTA NO DESIGN.

A superfície de intervalo compartilha início/fim e oferece tarefas Trecho,
Ciclo e Organizar. Trocar tarefa substitui os controles, sem acumular painéis
sobre o viewport. Campos numéricos usam o editor existente e verificam a revisão
capturada. Erros aparecem no diagnóstico do clipe; Undo é uma publicação inteira.

Os ícones novos `AnimationCycle` e `AnimationArrange` entram no catálogo e atlas
de produção. Não são imagens conceituais usadas como controles.

ABI 10 anexa `arrangeSelection` e comandos CloseRange/RepeatRange. C# expõe
`ClipArrange`, `ClipEdit.ArrangeSelection`, `CloseRange` e `RepeatRange`.
Os prefixos ABI 1–9 permanecem aceitos; um SDK novo recusa operações novas quando
o editor anuncia ABI anterior. Nenhum painel precisa estar aberto para editar
um draft, e descartar o draft preserva recurso e histórico publicados.

## Aceite e limites

Provar distribuição com coincidências e quaternion, colisão atômica, ciclo aberto,
fechamento explícito, repetição com camada/cues, Undo/Redo, reabertura e Play.
Capturar Trecho/Ciclo/Organizar em 655×300 e conferir controles/diagnósticos reais.
Aceite host: recursos 28/28, editor 49/49 e SDK ABI 10 real 1/1, zero falhas ou
casos pulados. Build gerenciado sem erros/avisos. Logs e capturas de Ciclo e
Organizar em 655×300 estão no registro `docs/validacao/n01-n07-2026-10-10/`.
Host, SDK, pacote Android e exame no aparelho continuam evidências separadas.
As operações acima não fecham regiões de marcadores, inspeção visual ampliada de
curvas ou rig/retargeting; esses itens seguem no universo integral do plano.
