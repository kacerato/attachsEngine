# Eventos e marcadores de clipe — bloco de 10/10/2026

## Contrato e caminho vertical

AECLIP 4 preserva a leitura das versões 1–3. Cues tipados, ordenados por tempo/ID,
compartilham o alocador monotônico dos canais e chaves. Marker é um ponto de autoria;
Event emite `clip_event(tag: Integer, value: Number, cue: Integer)` no componente
Animation ou Animator, pela ComponentEventQueue existente. `cue` identifica o ponto
dentro do clipe; `tag` é o contrato semântico escolhido pelo autor, inteiro de
0 a 16777215. `value` é double finito; IDs são positivos até INT64_MAX. Nenhuma chamada
por nome de função, execução em scrub ou novo barramento é necessário.

Dados → arquivo → biblioteca imutável → travessia temporal → fila adiada → scripts
e conexões de cena. Draft ABI 6 e superfície contextual usam as mesmas operações,
jornal de recursos, undo/redo e recusa de revisões vencidas.

## Semântica de reprodução

- Intervalos `(antes, depois]` ao avançar e `[depois, antes)` ao reverter.
- Play/seek/rewind não reproduzem o trecho saltado. Um cue em zero não dispara
  automaticamente no início: dispara quando esse instante é atravessado.
- Loop e PingPong enumeram ocorrências em ordem temporal. Na inversão do ping-pong,
  endpoints têm uma ocorrência, sem duplicar a mesma identidade. Cues diferentes
  em zero e duração permanecem eventos distintos mesmo coincidindo no loop.
- Filtros por sentido local, enabled e peso efetivo maior que zero. Cada motion
  ativo de um blend emite seus próprios eventos; tags iguais não são deduplicadas.
- Pose congelada de interrupção e pose de referência aditiva não geram eventos.
- Limite compartilhado de 256 por avanço do avaliador, com mesclagem ordenada por clipe e
  contagem analítica de supressões, sem iterar milhões de ciclos.
  `clip_events_lost(count)` informa a perda. A fila mantém seu diagnóstico próprio.
  Não existe ordenação global por tempo entre motions/componentes; a precedência
  do orçamento segue a travessia existente. Peso considerado é estado × motion × camada,
  antes de atenuação espacial por máscaras/overrides superiores do compositor.
- Preview e seek de autoria não recebem contexto de eventos.

## Edição e preservação

Faixa contextual Eventos/Marcadores substitui o corpo da timeline. Selecionar,
arrastar com snap de frame, nomear, editar tempo/código/valor, ativar, filtrar
sentido, duplicar e excluir. Propriedades avançadas sob demanda. Retime e corte
acompanham cues; inverter também troca filtros de sentido. Bake/consolidação
preservam cues; operações de seleção de chaves não alteram cues globais.
Conexão de evento versão 2 adiciona filtro `clip_tag`, -1 para qualquer código;
leitura da versão 1 mantém esse default. Códigos são serializados como inteiros,
sem arredondamento do formatador float. Argumentos de método continuam autorados,
não são substituídos implicitamente pelo payload.

## Referências concretas

- Godot 4.5, [Call Method Track](https://docs.godotengine.org/en/4.5/tutorials/animation/animation_track_types.html):
  ações temporais e preview sem execução; adaptar à fila adiada tipada existente.
- Godot 4.5, [Animation markers](https://docs.godotengine.org/en/4.5/classes/class_animation.html):
  posições nomeadas no recurso, separadas de ações de gameplay.
- Godot 4.5-stable, [AnimationMixer source](https://github.com/godotengine/godot/blob/4.5-stable/scene/animation/animation_mixer.cpp):
  travessia de métodos, seek e entrega adiada são caminhos distintos da amostragem.
- Unity 6.6/6000.6, [Animation Event workflow](https://docs.unity.com/en-us/engine/6000.6/manual/animation-section/animation-mecanim/animation-clips/animation-editor-guide/script-animation-window-event):
  faixa temporal e propriedades contextuais do ponto selecionado. A Astra usa
  códigos tipados e conexões, sem depender de mensagens por string.

## Observação do workflow em vídeo

Vídeo oficial Synty Studios, [How to use Animation Events](https://www.youtube.com/watch?v=92P2Zz6K9vA):
inspeção de pares de quadros consecutivos via avanço de 1/30 s em torno de
30, 35, 40, 55 e 70 s. Timeline/canais, criação do script e feedback no Console
foram observados; a mensagem de execução aparece entre 70,0667 e 70,1000 s.
Não é auditoria integral frame a frame do vídeo. Princípio extraído: separar
marcação temporal, configuração da reação e feedback da execução. A adaptação
mobile substitui o corpo da timeline e usa a fila/Console existentes, sem
manter outro Inspector fixo ou executar métodos por nome na prévia.

## Aceite e limites

Criar eventos/marcadores → editar pela UI/API → desfazer/refazer → salvar/reabrir
→ reproduzir pelo Animation e Animator → entregar payload pelo barramento existente
→ verificar loops, reverso, ping-pong, offsets, orçamento e preview isolado.

Estado final: bloco implementado e validado no host, 42/42 cenários da família
de clipes. Integração C# com a ponte nativa real: 1 passou, zero falhas/pulados,
incluindo compatibilidade de prefixos ABI 1–5 e operações ABI 6. SDK Release:
zero erros, dois avisos CS8981 preexistentes. Dois exemplos completos do guia:
zero erros/avisos. Artefatos, hashes e capturas em
`docs/validacao/animation-cues-2026-10-10/`.

Animation e Animator foram exercitados com consumidor Scripts e conexão real
que alterna a atividade de outro objeto somente para o código 7. Peso zero não
entrega eventos. Salto de 200 s: 256 entregas e diagnóstico de 144 supressões
em cada avaliador. Travessia de um milhão de loops validou prefixo ordenado e
contagem sem iterar ciclos; teste de tempo acima de 1 milhão protege graphs
duradouros. Migração AECLIP 3→4 e EventConnection 1→2 preserva contratos antigos.
Retime/reverse/crop, consolidação, payload double, códigos máximos, drafts vencidos,
histórico e reabertura foram conferidos.

UI executável rasterizada, inspecionada em 853×394 e 655×300: seleção,
propriedades, criação, opções, arraste e cancelamento. Revisão visual corrigiu
o badge + de criação e identificou o payload como Valor; captura posterior
confirma as duas resoluções. Não inclui renderer 3D/Vulkan. A superfície usa
contexto da timeline e folha temporária; não adiciona Inspector fixo. Ícones
determinísticos usam as primitivas angulares existentes, PNG e atlas de produção.
O resultado não exigiu substituir o paradigma já estabelecido por um mockup.

Sem ADB nesta sessão, por instrução do usuário. Não é uma nova distribuição APK.
Este bloco não inclui IK, retargeting, drivers de propriedades nem bake FK/IK/root motion.
Seleção múltipla, clipboard de cues e regiões de marcadores também não são
anunciados como suporte; o clipboard anterior é de chaves.

## Adendo: aceite Android após liberação do ADB — 10/10/2026

O parágrafo acima registra a entrega anterior do host. Após autorização explícita,
Dev code 33 (0.3.1-dev.cues.20261010.1) foi compilado e atualizado no POCO F7.
UI por toque, SDK compilado pelo próprio IDE, persistência após force-stop e seis
checks reais de Animation/Animator passaram. APK instalado, SDK e atlas conferem
com o build; 13 cenas preexistentes preservadas. Vídeo: 1.538 frames inspecionados
em 52 folhas, com lacuna de PTS explicitada. 594/594 C#, 42/42 clipes, 36/36
regressões nativas, 1/1 ponte SDK e 17/17 Java. Evidência em
`docs/validacao/animation-cues-android-2026-10-10/README.md`.
Aceite fecha este bloco de eventos/marcadores; os limites funcionais anteriores
permanecem explícitos. APK público 0.3.0/code 28 inalterado.
