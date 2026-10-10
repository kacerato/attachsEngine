# N01 — regiões autorais e reprodução por intervalo

Estado: implementação integrada, validação do novo conjunto pendente. A baseline
pública anterior permanece AECLIP 4 / ABI 10; este documento não anuncia APK.

NÃO IREI SER SIMPLISTA NO DESIGN.

## Contrato e referências

[Godot 4.5 AnimationPlayer](https://docs.godotengine.org/en/4.5/classes/class_animationplayer.html)
separa reprodução por seção do clipe inteiro;
[Animation 4.5](https://docs.godotengine.org/en/4.5/classes/class_animation.html)
possui marcadores nomeados. O princípio extraído é nomear uma faixa de tempo
reutilizável e limitar sua amostragem. Astra armazena fronteiras explícitas,
independentes dos eventos/cues. Criar uma região não cria uma chamada de gameplay.

Modelo → validação → AECLIP → operações temporais → compilação → sampler
composto/preview → journal → editor/SDK. A seleção da região e o modo de preview
pertencem ao editor, sem serem necessários para ler o recurso no runtime.

## Dados, compatibilidade e operações

AECLIP 5 acrescenta uma lista de até 1.024 regiões após os cues. Cada região tem
ID global do alocador do recurso, nome de 1–256 bytes e início/fim finitos com
`0 <= início < fim <= duração`. Ordenação estável por início/ID; nomes repetidos
não confundem identidade. ID de chave/cue/camada não pode ser reaproveitado como
região; identidades retiradas não são recicladas.

Leitores atuais aceitam versões 1–4 e produzem lista vazia. Leitores antigos
recusam AECLIP 5: salvar nesta revisão é uma migração de formato; preservar uma
cópia anterior é necessário para voltar a um APK antigo.

- Inserção antes da região desloca ambas as fronteiras. Inserção dentro estende
  o fim. Inserção exatamente no fim fica fora da região.
- Remoção comprime o relógio; região sem duração restante é retirada.
- Crop conserva a interseção. Retiming e reverse transformam ambas as fronteiras.
- Duplicar/repetir copia as interseções com IDs novos, preservando o original.
- Mover intervalo que corta apenas parte de uma região é recusado atomicamente;
  conter as duas fronteiras permite mover a região inteira.

As mesmas regras são chamadas pelo editor e SDK. Nomes e fronteiras usam a
publicação de recursos existente, incluindo Undo/Redo e revisão otimista.

## Fluxo mobile

Edição → Intervalo → Regiões abre uma superfície temporária. Lista paginada
(três regiões por página) e detalhes compartilham o espaço: não coexistem com
uma segunda lista permanente. “+ Região do intervalo atual” publica o intervalo
configurado; selecionar permite nomear, editar início/fim, alternar preview,
reproduzir, reiniciar, controlar loop e excluir.

Seleção destaca as duas fronteiras no gráfico. Preview selecionado limita o
relógio; reinício usa seu início. Desligá-lo volta ao clipe inteiro. Exclusão
da região ativa cancela sua reprodução. Scrub/preview amostram poses sem emitir
eventos externos. Campos inválidos permanecem abertos para correção e não
publicam alterações parciais. Mudança de revisão recusa um campo antigo.

O ícone `animation/region` usa primitivas Astra e integra o atlas executável,
com 335 entradas na revisão conjunta de regiões, opacidade e políticas de
textura. Conceito visual não substitui captura de execução.
Proposta gerada: `N01-regioes-conceito.png`. Os botões ± ilustrados na hipótese
não serão presumidos implementados: a UI usa o campo numérico existente. As
ações de usar a região e voltar ao clipe inteiro compartilham um controle com
estado explícito, evitando dois controles para a mesma transição.

## API e aceite

ABI 11 acrescenta PutRegion, RemoveRegion e SampleRegion preservando o prefixo
ABI 10. O SDK C# lê ClipRegion no snapshot e usa o sampler composto nativo.
SampleRegion recebe relógio relativo à região e Once/Loop/PingPong/ClampForever;
tempo/modo inválido é recusado. Essa consulta não emite cues nem publica recurso.

Cenários integrados: identidade/migração/cold round-trip; operações temporais e
recusa de movimento parcial; autoria pelo roteador de toque, teclado, correção
de campo inválido, preview/loop/reinício, exclusão, Undo/Redo, reabertura e
compilação runtime. O cenário SDK exercita criação, consulta composta e remoção,
além da compatibilidade com a tabela anterior. Resultados serão registrados após
o build conjunto com a Biblioteca. Android permanece pendente de liberação final.
