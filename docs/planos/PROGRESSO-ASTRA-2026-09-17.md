# Progresso da Astra — 17/09/2026

## Resposta direta

**Aproximadamente 30% do plano ampliado P01–P18 implementado**, com faixa de incerteza de **25–35%**. **P03, câmera e representação editorial: aproximadamente 70%.** Esses números estimam cobertura de implementação; não significam 30% de paridade Unity nem 70% de validação no dispositivo.

Base: plano de universalidade de 15/09, registro P01–P04, continuações P03 de 17/09, registros M06, M08/M09 e R1–R4. Código conferido nesta continuação: sessão/tela/câmera, frustum, caminho do renderer Android e interfaces de recursos. As outras famílias não receberam auditoria exaustiva nesta rodada. As evidências antigas permanecem históricas, não foram todas reexecutadas.

## Como foi estimado

Índice orientativo por pacote, com pesos iguais entre os 18 pacotes: 0 = sem entrega identificada; 25 = base/capacidade parcial; 50 = parte relevante dos percursos integrada; 75 = maior parte utilizável com lacunas; 100 = escopo e aceitação fechados. Interpolações são julgamento técnico, não contagem automática de linhas, arquivos ou nomes do atlas. Pesos iguais não representam esforço, duração ou dificuldade equivalente. O índice arredondado abaixo é 30%; a média das estimativas centrais atualizada é 28,3%.

| Pacote | Estimativa | Base existente e principal falta |
|---|---:|---|
| P01 Propriedades | 45% | Grupos, condições, IDs, campos compostos RGB/XYZ e escrita atômica nativa; edição conjunta com undo; faltam tipos ricos restantes e matriz completa |
| P02 Composição/impacto | 75% | Add transitivo/atômico, relações e reparos navegáveis, condições reais de execução física e presets persistentes com prévia/Undo; faltam grafo indexado, cobertura geral de provedores e presets compostos/seletivos |
| P03 Câmera/representação | 70% | Marcadores, seleção, volume, projeções, Ver/Pilotar/Alinhar, alças, histórico e PiP independente básico; fidelidade do preview e cobertura gráfica ainda abertas |
| P04 Casca/painéis | 30% | Identidade IDE e grupos/abas presentes; Estúdio/Foco e layout persistente geral incompletos |
| P05 Bancada | 30% | Painéis de recursos e inspeção parcial; documentos integrados e preview 3D ainda faltam |
| P06 Reimportação visual | 35% | Identidade/reconcile e dock; faltam diff visual, preview e escolhas por item completas |
| P07 Recursos/residência | 30% | Cache/compartilhamento parcial; separação imagem/sampler, revision/epoch e orçamento completo pendentes |
| P08 Texturas/materiais | 55% | Recursos, viewer, bindings e edição/reparo de material com journal e undo compartilhado; faltam simetria total, KTX2 viewer e ciclo completo de criação/exclusão de recursos |
| P09 Malha autorável | 25% | Importação estrutural/slots; autoria de atributos, sockets, variantes e derivados ainda parcial |
| P10 Física/input | 40% | Corpos, formas, juntas e capacidades existentes; falta fechar extensão/editor/eventos e percursos universais |
| P11 Animação/rig | 10% | Família ainda sem percurso autoral completo demonstrado; clips, skin/morph, mixer e Timeline abertos |
| P12 Câmera avançada/luz/mídia | 15% | Câmera/luz básicas; óptica/targets e consumidores de mídia incompletos |
| P13 UI/texto autorais | 15% | UI do editor não equivale ao sistema autoral completo de UI de jogo; canvas/eventos/fontes/localização abertos |
| P14 Navegação/splines | 5% | Não foi identificado fechamento de bake/agentes/links e edição geral de curvas |
| P15 2D | 5% | Infraestrutura não equivale a sprites/tilemap/rig/ferramentas 2D completas |
| P16 Ambiente/autoria gráfica | 20% | Renderer/água/qualidade existentes; autoria universal de terrain/partículas/volumes/shaders incompleta |
| P17 Extensões | 0% | Sem pacote de release XR/rede/endereçamento certificado nesta base |
| P18 Paridade/distribuição | 5% | Atlas e rastreabilidade iniciados; não há fechamento geral por propriedade, migração e distribuição |

Os valores baixos das famílias não auditadas são provisórios. Novos commits de outros agentes devem ser revisados antes de recalcular. Nenhum pacote foi declarado 100% com base na mera existência de um backend ou de um tipo no catálogo.

M00–M15 e R0–R10 são outros recortes do mesmo trabalho. Não somar seus percentuais aos P01–P18 nem usar 30% como auditoria independente de cada marco M. IDE, importação e materiais têm entregas anteriores relevantes; o escopo ampliado continua muito maior.

## Etapa anterior — contrato de agendamento

`renderer/render_view.h` define snapshot de vista com projeção, dimensões, câmera, epoch/revisão e token de solicitação, além de orçamento explícito de resolução/pixels/frequência. `editor/editor_camera_preview.h`, pertencente à sessão, implementa fixação de câmera, uma solicitação em voo, atualização sob demanda de cena estática, limite de frequência para animação e publicação condicionada à revisão correta. Mudança de cena fecha a prévia; invalidação do alvo permite recuperação sem mexer na câmera principal.

**Este é o contrato de preparação/agendamento, não um preview renderizado.** Não há novo botão público anunciando imagem pronta, render target Vulkan alocado por esse contrato ou segunda cena de simulação. A próxima integração precisa receber a conclusão real do backend; os testes apenas simulam essa conclusão para verificar a proteção contra publicação antiga.

Host compilado, filtro `p03_` 5/5. O novo caso cobre orçamento 1920×1080 → 320×180, projeção ortográfica/roll, invisibilidade, frequência, revisão alterada durante trabalho, conclusão antiga, recuperação do alvo e preservação da câmera principal. Nenhum APK novo foi instalado nesta rodada: ainda não há um fluxo visual novo para conferir por ADB.

## Sequência de integração prevista naquela etapa

1. Backend da vista independente: attachments de cor/depth, framebuffer e descritores por vista; reutilizar recursos da cena sem reconstruir o mundo.
2. Isolar constantes, listas de visibilidade e históricos; não reaproveitar resultados de oclusão/sombra da câmera principal sem validade demonstrada.
3. Gravar os draws com projeção/aspect próprios e publicar a textura somente após conclusão válida. Falhas e mudança de surface devem liberar/recriar recursos com sincronização correta.
4. Integrar PiP fixável/fechável com orçamento e estado de indisponibilidade; selecionar outro objeto deve manter a câmera fixada.
5. Conferir no aparelho diferença real entre as duas vistas, alteração de objetos, desfazer, fechamento, troca de cena e recuperação; medir custo antes de encerrar preview.
6. Retomar tipos ricos P01, painel de impacto P02 e casca Estúdio/Foco P04. A câmera avançada de P12 não deve bloquear indefinidamente essas frentes.

## Atualização — preview renderizado e conferido no aparelho

O contrato acima agora tem consumidor Vulkan real: cor, profundidade, framebuffer, UBO e descritores próprios; geometria e materiais são compartilhados com a cena. A ação **Câmera → Prévia** abre uma janela fixada à câmera, independente da seleção e da órbita principal, com fechamento explícito. A imagem só é publicada após conclusão do trabalho e aceitação da revisão pelo agendador.

Host `p03_`: **6/6**. Android Debug compilado e instalado. ADB confirmou enquadramentos distintos, ocultação do cubo nas duas vistas, restauração por desfazer e navegação da vista principal sem mudar a câmera fixada. A conferência encontrou e corrigiu o uso do relógio parado da simulação no agendador: a limitação de frequência agora usa tempo monotônico.

Esta entrega cobre o preview básico em edição, não fidelidade completa de Play: água é omitida; sombras e pós-processamento estão desativados e isso aparece no rodapé. Orçamento padrão de 640×360 e até 15 Hz, sem painel de ajuste de orçamento ou redimensionamento livre nesta etapa. Troca de cena/recuperação de surface possuem invalidação no código, mas ainda precisam de percurso físico dedicado, assim como medição de custo e corpus de materiais complexos.

Registro e evidências: [continuação P03](P03-PROJECAO-ORTOGRAFICA-2026-09-16.md). A estimativa global continua aproximadamente 30%; não houve nova auditoria das 18 famílias que justifique recalcular o índice. P03 permanece parcial. Próxima sequência: completar estados de falha/ciclo de vida e configuração do preview, depois retomar tipos ricos P01, impacto P02 e casca P04, mantendo fidelidade avançada da câmera como trabalho explícito.


## Continuação — configuração e falhas da prévia, 17/09

Implementados dois controles compactos no painel: resolução alterna 320×180, 640×360 e 960×540; frequência máxima alterna 5, 15 e 30 Hz. São limites reais do alvo/agendador, não somente rótulos. Alterar configuração invalida a imagem e o trabalho pendente; conclusão da configuração anterior não pode publicar. Preferências são da sessão, ainda sem persistência entre reinicializações. A proporção permanece 16:9 e o painel não ganha redimensionamento livre nesta etapa.

Falha do backend agora interrompe tentativas automáticas e apresenta uma ação por toque para tentar novamente. Recuperação do alvo ou mudança de configuração também limpam o erro. Isso evita realocar recursos repetidamente quando o backend não consegue renderizar. Revisões antigas são rejeitadas sem registrar um falso erro para a revisão atual.

Android Debug compilado e instalado por ADB. Os controles novos ainda não receberam conferência visual física nesta rodada; as imagens anteriores comprovam a prévia básica, não estes controles. Água, sombras, pós-processamento, lifecycle físico completo e custo medido continuam pendentes.

Percentual atualizado: **plano ampliado aproximadamente 30% (faixa 25–35%); P03 aproximadamente 70%**. Estimativas mantidas: configuração e erro melhoram uma entrega parcial, mas não encerram as lacunas de aceitação. Cada continuação deve informar novamente o percentual e a base da alteração ou manutenção; não somar pontos automaticamente por quantidade de commits.

Validação complementar: alvo host `aether_tests` compilado, filtro `p03_` **6/6**. Caso do agendador ampliado para falha, bloqueio de repetição automática, nova tentativa e rejeição de conclusão após mudança de resolução. A compilação host geral falhou em `tests/native/ui_preview_main.cpp`, alvo auxiliar `aether_ui_preview`; não declarar o build host inteiro aprovado. Android e alvo de testes passaram separadamente.


## Continuação — suspensão, recuperação e conferência física, 17/09

A prévia agora libera seu alvo ao sair do workspace Cena ou entrar em Play, mantendo a câmera fixada para solicitar uma imagem nova ao retornar. A validação da entidade/epoch acontece antes do bloqueio por falha: câmera removida ou cena substituída não deixa um painel preso em erro. Uma solicitação sem conclusão por mais de cinco segundos, observada pelo agendador enquanto a aplicação avança frames, vira falha com nova tentativa explícita; conclusões atrasadas não publicam. Isso não detecta um bloqueio completo da thread/GPU.

Android Debug compilado e instalado. No projeto P01Camera0915i foram conferidos os controles passando de 640×360/15 Hz para 960×540/30 Hz e o percurso fechar/reabrir com a imagem e configuração preservadas na sessão. Evidências: `camera-preview-settings.png` e `camera-preview-reopened.png` em `docs/validacao/evidencias/p01-p04-20260915/`. Não foram alteradas propriedades da cena. Troca de projeto, recuperação de surface e suspensão por Play ainda precisam de percurso físico específico; não estão comprovadas pelas capturas.

Host: corrigido namespace ausente nas chamadas `editWaterRoute` da ferramenta `ui_preview_main.cpp`; build geral agora concluído. Filtro P03: **6/6**, com cobertura adicional de timeout, conclusão atrasada e suspensão/retorno do modelo. Nenhum benchmark de custo gráfico foi executado.

**Percentual atualizado: plano ampliado ≈30% (25–35%); P03 ≈70%.** Houve avanço de robustez e evidência da prévia, mas a estimativa arredondada permanece: faltam fidelidade gráfica, lifecycle físico completo e cobertura de câmeras/hierarquias. Próximo bloco de implementação: tipos ricos P01 (cor/HDR, vetores e referências com consumidores reais), seguido do painel de impacto P02; fidelidade avançada da câmera permanece explicitamente aberta.


## Continuação P01 — propriedades compostas, 17/09

O descritor comum agora declara `ComponentTriple`: identidade persistente do conjunto, nome, três IDs de canais e semântica Vector/LinearColor. Não há armazenamento duplicado. A serialização antiga permanece escalar, na mesma ordem e versão; leitores existentes continuam compatíveis.

Adoção real: luz `color` (RGB linear); colisor `half_extents`, `center`, `rotation`; junta `anchor_a`, `anchor_b`, `axis_a`, `axis_b`. O inspetor monta uma linha com três células identificadas R/G/B ou X/Y/Z, a partir do mesmo descritor, e só agrupa quando os três canais estão visíveis no grupo atual. Cada célula encaminha para a edição numérica existente, preservando ID da instância, validação e histórico por canal. Componentes sem descritor composto mantêm os campos anteriores.

A API nativa `setComponentTriple` resolve canais por ID, clona uma candidata, escreve todos os valores e valida o componente completo antes de substituir a instância. Recusa componentes ausentes, identidades ambíguas, canais desconhecidos, limites inválidos, valores não finitos e invariantes cruzadas. Isto permite alterar um vetor de direção em uma operação sem publicar estados intermediários inválidos. Não há ainda ligação dessa operação composta à API C# ou um editor modal de três valores; a UI desta entrega continua editando um canal por transação.

Consumidores são os existentes: iluminação, pose/forma do colisor e configuração física das juntas. Não foram presumidos recursos da Unity. Luz mantém RGB linear 0–1 e intensidade separada; não foi anunciado suporte novo a HDR, conversão sRGB ou seletor visual. Recursos, máscaras, curvas, listas, ajuda e unidades dos grupos compostos ainda exigem expansão.

Validação: alvo host `aether_tests` compilado; filtro P01 **3/3**. Novo caso cobre direção válida, rejeição atômica de eixo nulo, ambiguidade entre instâncias, rejeição parcial de cor e leitura da serialização existente. APK Android Debug compilado e instalado. O layout novo e a edição agrupada ainda não foram conferidos visualmente no aparelho; instalação não equivale a essa prova.

Percentual atualizado: **P01 ≈45% (antes 40%); P03 ≈70%; plano geral ≈30% (faixa 25–35%)**. O avanço de cinco pontos em um de 18 pacotes não altera o arredondamento global. Próximas entregas: edição composta com histórico único, unidades/apresentação adaptativa e cor visual, depois referências de recursos e painel de impacto P02.


## Continuação P01 — edição conjunta e amostra de cor, 17/09

Tocar no nome do grupo RGB/XYZ abre a edição dos três canais. Células individuais preservam o caminho escalar. O editor conjunto aceita três números separados por espaços ou ponto e vírgula, inclusive vírgula decimal; teclado interno oferece Separar canais. A ponte Android solicita teclado textual para permitir os separadores. Os dados capturam entidade, instância, propriedade e versão da cena; confirmar aplica `setComponentTriple` em uma candidata e uma única operação de histórico. Cancelar, valores inválidos ou revisão alterada não publicam uma mudança parcial.

A cor da luz apresenta uma amostra calculada por conversão linear→sRGB. Os valores autorados e usados na iluminação permanecem lineares. Trata-se de amostra visual com edição numérica, não um seletor HSV, conta-gotas, HDR ou biblioteca de cores.

Validação: alvo host compilado; P01 **4/4**, incluindo abertura do grupo pela UI, entrada localizada, rejeição atômica, um undo para três canais e cancelamento. Android Debug compilado e instalado. Teclado/layout e a amostra ainda não receberam conferência visual no aparelho nesta rodada.

**Percentual atualizado: P01 ≈45%; P03 ≈70%; plano geral ≈30% (25–35%).** Mantidos os valores arredondados: esta entrega completa a edição dos grupos introduzidos na rodada anterior, mas tipos ricos restantes, seletor de cor completo, recursos e painel de impacto continuam abertos. A operação composta ainda não foi exposta à API C#.


## Continuação — contrato futuro de execução e ajustes P01, 17/09

Adendo [Aba Jogo e recarga](ABA-JOGO-RECARGA-2026-09-17.md) ligado ao plano mestre e ao plano de universalidade: distingue mini prévia sob demanda, renderização contínua e recarga de propriedades/assets/código. Define ownership de mundo/vista/input, gerações, publicação segura, rollback, preservação de estado por capacidade e aceitação. **Planejado, não implementado**, sem mudar a prioridade P01/P02.

Implementação P01 desta rodada: título Android específico para três canais, limite de texto coerente com buffer nativo, conferência do tipo da edição antes de aplicar resposta e unidade exibida no título de grupos compostos quando comum aos três canais. Sem alterar valores ou unidades persistidas. Host compilado, P01 4/4; APK Debug compilado. Este APK não foi reinstalado nem conferido visualmente nesta rodada.

**Percentual atualizado: geral ≈30% (25–35%), P01 ≈45%, P03 ≈70%.** Mantidos: arquitetura futura e ajustes de apresentação não encerram novos pacotes. Permanecem pendentes seletor de cor completo, demais tipos ricos, referências de recursos e painel de impacto.


## Continuação P01 — seletor visual de cor, 17/09

A amostra de cor abre um painel nativo com 24 escolhas de matiz e grade de 11×11 de saturação/brilho, amostra do rascunho e Aplicar/Cancelar. O painel usa o descritor LinearColor, sem comparação por nome de componente. O nome do grupo mantém a edição numérica conjunta e as células RGB mantêm edição escalar.

O rascunho visual usa HSV sobre RGB sRGB; Aplicar converte para RGB linear, valida pelo setter composto e publica uma única operação de histórico. Escolhas intermediárias não modificam documento nem luz. Revisão alterada recusa aplicação; troca de cena limpa o painel. Limites atuais: seleção discreta por células, sem arraste contínuo, HEX, HDR, conta-gotas ou biblioteca de cores. A precisão arbitrária permanece nos campos numéricos. A iluminação não é atualizada ao vivo enquanto se escolhe o rascunho.

Host compilado, P01 **5/5**. Novo percurso percorre widgets reais: abrir amostra, selecionar verde, confirmar sem alterar antes de Aplicar, um undo e cancelamento. O primeiro teste encontrou o painel num ramo incorreto do layout; corrigido antes da aprovação. Conferência visual física ainda pendente.

**Percentual atualizado: geral ≈30% (25–35%); P01 ≈45%; P03 ≈70%.** Mantido o arredondamento: há seletor básico real, mas faltam acabamento/interação contínua, demais tipos ricos e painel de impacto. Arquitetura da aba Jogo/hot reload segue futura, sem mudança nesta entrega.

APK final desta entrega compilado e instalado por ADB. Instalação não substitui a conferência visual pendente do seletor.


## Continuação P01 — percurso físico de cor, 17/09

ADB no projeto P01Camera0915i: anexada Luz temporariamente ao objeto da câmera, aberto o grupo Emissão e a amostra de cor; escolhidos matiz verde e saturação/brilho máximos. Aplicar publicou RGB linear 0/1/0 no inspetor; um desfazer restaurou 1/1/1. Reaberto o seletor, alterado o rascunho e cancelado; o próximo desfazer removeu a adição temporária da luz, confirmando que Cancelar não acrescentou alteração de cor ao histórico. Cena salva ao final com Câmera/Olhar originais e sem a luz temporária.

Evidências em `docs/validacao/evidencias/p01-p04-20260915/`: `color-applied.png`, `color-undo.png`, `color-scene-restored.png`. O percurso comprova autoria pelo seletor e histórico. Não certifica a contribuição fotométrica da luz nos objetos: neste enquadramento não foi estabelecida uma comparação controlada de iluminação.

Melhoria decorrente: contorno claro/escuro identifica matiz e célula de saturação/brilho selecionados. APK final compilado e instalado; a melhoria dos contornos ainda não recebeu captura física após reinstalação. Testes host anteriores P01 5/5 não foram repetidos para essa mudança apenas visual. Permanecem seleção discreta, ausência de HDR/conta-gotas/HEX e falta de conferência em retrato.

**Percentual atualizado: geral ≈30% (25–35%), P01 ≈45%, P03 ≈70%.** Mantida a cobertura de implementação; esta rodada ampliou evidência física e legibilidade. Próximo bloco: referências de recursos e painel de impacto P02, sem confundir a aba Jogo futura com a mini prévia atual.


## Continuação P02 — painel de relações, 17/09

Menu do componente → Dependências abre painel no inspetor, com paginação. Enumera requisitos no mesmo objeto, referências de saída (incluindo vazias/inválidas) e componentes que exigem ou referenciam o tipo selecionado. Tocar numa relação válida seleciona o objeto e expande a instância relacionada quando conhecida. Os dados vêm dos schemas/referências existentes e são recalculados do documento; não há cópia persistida do grafo.

Uma relação de tipo não significa que remover aquela instância esteja bloqueado quando existem outras instâncias equivalentes. O bloqueio de remoção permanece no resolvedor anterior; o painel é de consulta/navegação, não executa exclusão em cascata. Referências implícitas, assets, serviços, reparo transacional e grafo indexado/transitivo ainda estão fora deste bloco. Não presumir completude por exibir uma lista.

Host compilado; P02 1/1 (abrir menu/painel e navegar do requisito da câmera para Olhar), regressões P01 5/5. Android Debug compilado. Instalação não concluída: o transporte ADB desconectou antes da instalação. Conferência visual do novo painel no aparelho ainda pendente; referências cruzadas extensas e paginação não foram exercitadas fisicamente nesta rodada.

**Percentual atualizado: P02 ≈50% (antes 45%), P01 ≈45%, P03 ≈70%; geral ≈30% (25–35%).** A média orientativa dos 18 pacotes passa a 26,4%; arredondamento global mantido. Avanço atribuído ao percurso real de consulta/navegação, sem marcar como concluído o grafo universal.


## Continuação P02 — navegação no aparelho e referências tipadas, 17/09

ADB reconectado; instalado APK do painel e conferido no projeto P01Camera0915i o percurso menu da Câmera → Dependências → Olhar. A dependência aparece e tocar abre a instância Olhar. Capturas `impact-camera.png` e `impact-navigate.png` em `docs/validacao/evidencias/p01-p04-20260915/`. Percurso somente de consulta, sem alteração autoral.

Implementação adicional: requisitos usam nomes do catálogo; referências de saída incluem o tipo requerido; referência válida com uma única instância compatível navega diretamente ao componente de destino. Mais de uma instância não escolhe arbitrariamente: navega apenas ao objeto e informa multiplicidade. Referências recebidas distinguem validade de escopo; IDs acima do domínio de ObjectId não são truncados para outro objeto. Painel ganhou identificação do componente consultado e cartões com fundo/recorte arredondado.

Host compilado, P02 **2/2**, incluindo junta→corpo em outro objeto e relação reversa. Android Debug recompilado e instalado. As capturas físicas pertencem ao painel anterior aos ajustes de cartões/rótulos; essas mudanças finais e o percurso cruzado têm evidência de build/host, ainda sem conferência visual própria. Não declarar grafo universal fechado: assets, serviços, requisitos condicionais, índice persistente e reparo continuam pendentes.

**Percentual atualizado: geral ≈30% (25–35%), P01 ≈45%, P02 ≈50%, P03 ≈70%.** Mantidos os valores arredondados; houve ampliação da precisão e da evidência de P02, sem encerramento de outro pacote.


## Continuação P02 — impacto sobre remoção, 17/09

O painel distingue relações informativas, referências inválidas e relações que bloqueiam remover a instância consultada. Requisitos/referências tipadas recebidas só bloqueiam quando a instância é a última daquele tipo; referências inválidas não são apresentadas como bloqueios válidos. Referências opcionais vazias mantêm a semântica existente (por exemplo, proprietário implícito do colisor).

Resumo de bloqueio no cabeçalho usa diretamente `componentInstanceRemovalBlockedBy` e `componentRemovalReferenceUse`, os mesmos resolvedores do menu de remoção. A frase Sem bloqueio de dependências não promete ausência de outras restrições operacionais, como Play ou transação aberta. As linhas bloqueantes/ inválidas recebem destaque; não foi implementada exclusão em cascata nem reparo automático.

Host compilado; P02 **3/3**, cobrindo câmera requerida por Olhar, referência junta→corpo e colisor com proprietário vazio válido. Android Debug compilado e instalado. Indicadores novos ainda sem conferência visual física; evidência ADB da navegação anterior não é reapresentada como prova dos novos rótulos.

**Percentual atualizado: geral ≈30% (25–35%); P01 ≈45%; P02 ≈50%; P03 ≈70%.** Mantidos: melhora a precisão da consulta existente, sem fechar recursos, requisitos condicionais, grafo indexado ou reparo. Próximo incremento relevante é ampliar a consulta para recursos e seus consumidores reais, preservando a distinção entre dependência de tipo, referência de objeto e uso de recurso.

## Continuação P02 — recursos por slot e usos compartilhados, 17/09

O painel Dependências agora enumera GUIDs persistentes de malhas, materiais e texturas explicitamente atribuídos aos slots de MeshRenderer, incluindo oclusão. A consulta encontra outros componentes da cena com o mesmo recurso e tipo de vínculo; tocar no consumidor seleciona o objeto e abre a instância correspondente. As linhas recebem recorte para impedir que identificadores longos invadam outros controles. Índices numéricos locais de pacote não são interpretados como referências persistentes.

Limites: o provedor cobre MeshRenderer, não todos os componentes. Texturas herdadas de materiais e o marcador de ausência de textura não viram usos explícitos. Não há consulta de existência ao AssetRegistry, resolução transitiva, índice de consumidores, medição de desempenho ou bloqueio de exclusão de assets. As relações representam atribuições autorais, não comprovam os bindings efetivos da GPU. Linhas diretas de GUID são informativas; a navegação implementada é para componentes consumidores.

Host e Android Debug compilados; P02 **4/4**, incluindo slots adicionais, exclusão de marcadores e atualização dos consumidores após trocar um material. APK instalado com sucesso por ADB. Não houve conferência visual física do novo percurso de recursos nesta rodada; instalação não é validação visual.

**Percentual atualizado: geral ≈30% (25–35%); P01 ≈45%; P02 ≈50%; P03 ≈70%.** Mantidos os arredondamentos: recursos explícitos ampliam P02, mas resolução pelo registro, herança, demais provedores e reparo ainda impedem fechar o pacote. Próximo bloco: conectar a consulta ao registro de assets e resolver materiais/texturas herdados com diagnóstico de recursos ausentes.

## Continuação P02 — registro e herança de materiais, 17/09

Consulta de recursos integrada ao AssetRegistry da sessão e à biblioteca EditorMapScene, tanto no desenho do painel quanto na navegação por toque. Mostra caminho cadastrado em vez de apenas GUID, diagnostica tipo incompatível e identidade não registrada; malha presente no pacote sem registro é identificada como recurso do pacote, sem falso erro de ausência. Material registrado mas indisponível na biblioteca aparece como não carregado. Registro existente não comprova existência do arquivo nem residência GPU.

Texturas locais e herdadas usam os mesmos resolvedores slotTexture/slotOcclusionTexture da extração de materiais. Overrides locais vencem e MaterialTextureNone interrompe a herança. Os bindings receberam nomes Cor base, Normal, Metal / rugosidade, Emissão e Oclusão. A busca de consumidores usa essa mesma resolução; uma textura local de um objeto pode ser relacionada à textura herdada de outro, e a relação desaparece quando removida por override.

Limites: provedor ainda restrito a MeshRenderer; texturas da fonte do pacote sem GUID explícito/compartilhado, dependências transitivas do registro, existência física dos arquivos, reparo, navegação ao documento do asset e índice de consumidores continuam pendentes. A consulta não modifica arquivos ou cenas e não implementa exclusão em cascata. Identificadores/caminhos longos continuam sujeitos ao recorte do cartão, sem detalhe expandido próprio.

Validação: host compilado, P02 **5/5**; caso novo cobre herança, consumidor reverso, tipo errado, identidade não registrada, bloqueio de herança por none e material não carregado. Android Debug compilado e instalado por ADB. Não foi realizada conferência visual física desse novo fluxo.

**Percentual atualizado: geral ≈30% (25–35%); P01 ≈45%; P02 ≈50%; P03 ≈70%.** Mantidos os arredondamentos: o vínculo recurso/consumidor ficou mais preciso, mas P02 ainda não entrega requisitos condicionais, reparo e grafo completo. Próximo bloco: navegação e detalhes de recurso no inspetor, com retorno ao componente e diagnóstico acionável, antes de ampliar operações de reparo.

## Continuação P02 — navegação de recursos e relações do registro, 17/09

Linhas de recurso em Dependências agora abrem detalhes no próprio inspetor, inclusive quando o GUID não está registrado. O painel apresenta identidade, tipo registrado, caminho, fonte e versão do importador; materiais e malhas apresentam também seu estado na biblioteca quando consultável. Não confundir registro com arquivo existente ou residência GPU.

As relações Depende de / Usado por do AssetRegistry são navegáveis por GUID e separadas dos consumidores de componentes da cena. Uma pilha de navegação guarda recurso e página anteriores; Voltar retorna pelas relações até o componente original. O limite de 64 níveis impede crescimento ilimitado em percursos repetidos. Troca de cena e abertura de outro componente limpam o contexto. Abrir um consumidor da cena seleciona e expande sua instância, encerrando essa navegação de recursos.

Sem novo registro paralelo ou mutação autoral. Ainda faltam reparo/relink, existência física dos arquivos, preview de recurso, detalhes expansíveis para caminhos longos, provedores além de MeshRenderer e indexação do grafo. O painel é consulta navegável, não a Bancada P05 completa. A navegação de relações permite seguir cadeias manualmente; não é uma análise transitiva automática.

Validação: host e Android Debug compilados; P02 **7/7**, incluindo abrir GUID não registrado, retornar ao componente preservando vínculo e distinguir direção das arestas do registro. APK instalado por ADB. Não houve conferência visual física do novo painel nesta rodada.

**Percentual atualizado: geral ≈30% (25–35%); P01 ≈45%; P02 ≈50%; P03 ≈70%.** Mantidos os arredondamentos enquanto faltam requisitos condicionais, conflitos e reparo transacional. Próximo bloco: operações de reparo tipadas com prévia de impacto e histórico, aproveitando os seletores existentes, sem substituir referências silenciosamente.

## Continuação P02 — reparo local com prévia e histórico, 17/09

Detalhes do recurso → Reparar usos locais abre seleção tipada por registro e disponibilidade. O usuário escolhe candidato, examina a lista de slots/bindings afetados e aplica explicitamente; cancelar não altera o documento. O alcance é todos os usos explícitos daquele GUID no componente aberto, não todos os objetos do projeto. A troca mantém instância, demais slots, overrides de material, sampler, canais e transforms. Para malhas, o índice temporário é resolvido novamente pela identidade de destino.

O comando repairComponentResource recebe identidade da cena e revisão esperadas, recusa Play/transação aberta/cena alterada, revalida candidato e aplica uma única alteração pelo histórico. Undo/Redo cobre todos os slots do componente em um passo. Materiais candidatos precisam estar carregados, malhas precisam existir no pacote e texturas precisam estar carregadas no catálogo do projeto ao aplicar. Falha de publicação da textura após autoria é informada como publicação pendente; não se anuncia atomicidade GPU.

Texturas herdadas do material compartilhado não são convertidas silenciosamente em overrides; só vínculos locais entram nesse reparo. GUID usado em categorias incompatíveis é recusado pelo seletor. Não há reparo de arquivo de material compartilhado, substituição global, troca de fonte, geração de variante nem migração automática de topologia. O usuário pode trocar um recurso válido deliberadamente; não é restrito a IDs ausentes.

Validação: host e Android Debug compilados, P02 **8/8**. Percurso novo usa widgets reais para escolher malha, preparar e aplicar reparo de dois slots, verifica ausência de mutação na prévia, preservação de material/textura, índice resolvido, um Undo, Redo e rejeição de revisão obsoleta. Os caminhos material/textura foram compilados, mas não receberam exercício dedicado nesta rodada. APK instalado por ADB; conferência visual física do novo fluxo ainda pendente.

**Percentual atualizado: P02 ≈55% (antes 50%); geral ≈30% (25–35%); P01 ≈45%; P03 ≈70%.** O avanço corresponde ao primeiro percurso real de reparo com prévia/histórico, somado à navegação de recursos já entregue. A média orientativa dos 18 pacotes é 26,7%; nenhum pacote foi encerrado. Próximo bloco: ampliar o contrato de reparo para usos compartilhados com transação de recurso e impacto por consumidor, além de conferir fisicamente este fluxo antes de considerá-lo validado no aparelho.

## Continuação P02/P08 — publicação consistente de material compartilhado, 17/09

A investigação do próximo reparo revelou um pré-requisito: cinco rotas de edição compartilhada gravavam o material antes de atualizar o AssetRegistry e publicavam dependências vazias. Centralizadas em commitSharedMaterial as edições de texturas, superfície, canais, amostragem e números. Material e registro agora usam o journal existente de EditorImportTransaction, sem segundo registro paralelo. A biblioteca em memória só recebe o candidato depois do commit dos arquivos; falha solicita rollback e mantém backups quando recuperação falha.

O candidato exige revisão consecutiva, valida tipos das texturas, deduplica dependências, inclui oclusão, ignora herança/none e preserva dependências não relacionadas a texturas, parâmetros/versão do importador e derivados. Retirar o último binding de uma textura remove a aresta correspondente. Antes de gravar, o material do disco é comparado semanticamente com a versão carregada; uma alteração externa é recusada. O begin do journal também confere hash dos bytes lidos. Continua válido o contrato de um único escritor do projeto; não é controle geral de concorrência entre processos.

Validação: host compilado; filtro R4 **18/18**. Ampliado percurso compartilhado para confirmar dependências em memória/disco, recusar arquivo externamente alterado sem sobrescrever a biblioteca e remover a última dependência de textura. Android Debug compilado e instalado por ADB. Não houve injeção de falha de energia, conferência visual ou novo percurso físico nesta rodada.

Limites: criação de material ainda usa a rota anterior; não foi implementado Undo/Redo de arquivos de recurso nem o novo seletor de reparo compartilhado. Journal de recuperação não equivale a histórico autoral. Publicação GPU continua etapa posterior e pode informar falha depois da gravação; não há atomicidade entre disco e GPU. Próximo bloco continua sendo histórico de recurso compartilhado e prévia por consumidor, agora sobre a gravação consistente.

**Percentual atualizado: geral ≈30% (25–35%); P01 ≈45%; P02 ≈55%; P03 ≈70%.** Mantidos: este incremento corrige a fundação de recursos, mas não fecha o percurso compartilhado planejado nem amplia automaticamente P08.

## Bloco P02/P08 — histórico de recursos e reparo em três escopos, 17/09

### Contratos implementados

- Histórico único: EditorHistory aceita transações autônomas de recurso com replay atômico. Edições de material intercalam corretamente com comandos da cena; falha no replay mantém o cursor, em vez de consumir o passo. Transações de recurso não se misturam com comandos de cena dentro de uma transação aberta.
- As cinco rotas compartilhadas (textura, superfície, canais, amostragem e números) passam a registrar snapshots antes/depois. Undo/Redo grava novamente material e AssetRegistry pelo journal, incrementa revisão e republica a biblioteca. Alteração externa no material ou conteúdo divergente recusa replay sem sobrescrever nem perder histórico. É histórico da sessão, não persistido entre reinícios.
- Reparo compartilhado: recurso de material → textura do material → Reparar no material. Candidato tipado, revisão do material e epoch/revisão da cena, prévia de consumidores e aplicação explícita. Todos os bindings daquele material que usam a textura são substituídos, incluindo oclusão. Overrides dos objetos são preservados e aparecem separados dos consumidores afetados na prévia. Outros usos em cenas não abertas também recebem o material alterado, mas não são enumerados como se tivessem sido examinados.
- Reparo local da cena: seletor de alcance Componente/Cena aberta, prévia dos usos explícitos e preparação em cópia do documento/histórico. Um consumidor incompatível recusa o conjunto antes da publicação. Aplicação reúne todos os objetos/slots em um único Undo; herança de materiais não vira override automaticamente.
- Malhas do pacote sem registro próprio entram no seletor por GUID persistente carregado; nenhuma identidade é inventada. Destinos de textura passam pela decodificação antes do reparo. Um binding originalmente ausente pode ser restaurado por Undo sem fabricar uma aresta válida no registro.

### Evidência e limites

Host compilado; P02 **10/10**, R4 **18/18**, filtro history **29/29** (filtros sobrepostos, não somar como casos únicos). Testes cobrem rejeição integral de consumidores incompatíveis, reparo multiobjeto/slots, fallback de candidatos do pacote, três usos herdados e um override preservado, troca incluindo oclusão, gravação em disco, intercalamento de ação de cena e recurso, restauração de GUID ausente e conflito externo mantendo cursor. Android Debug compilado e instalado.

Permanecem: criação/exclusão de arquivos de material fora desse novo histórico, reparo multiasset numa transação única, enumeração de consumidores em cenas fechadas, busca textual no seletor, grafo indexado e requisitos condicionais. A prévia de consumidor é autoral e não certifica contribuição visual de um binding desativado por shader/canais. Falha de publicação GPU é reportada depois da autoria persistida; disco/GPU não são uma transação única. Recuperação após queda de energia ainda não recebeu injeção física nesta rodada.

### Reestimativa

**P02 ≈65% (55% → 65%); P08 ≈55% (45% → 55%).** O aumento corresponde aos percursos completos de reparo e histórico, sem declarar encerramento. P01 ≈45% e P03 ≈70% mantidos. Média orientativa dos 18 pacotes: **26,7% → 27,8%**; estimativa global arredondada permanece **≈30% (25–35%)**. O avanço concentrado em dois pacotes não equivale a dez pontos globais. Próxima fronteira: ampliar referências/provedores e requisitos condicionais, e completar apresentação/navegação dos inspetores sob P01/P04.

### Conferência ADB deste bloco

APK final no aparelho 25053PC47G, projeto P01Camera0915i. Percurso: Cubo → Malha → Dependências → recurso → Reparar usos locais → alcance Cena aberta → malha de esfera do pacote → prévia → Aplicar substituição. A geometria do Cubo mudou para esfera e a consulta passou a indicar os dois consumidores. Um Undo restaurou o cubo; cena restaurada salva. Evidências: repair-scene-preview.png, repair-scene-applied.png e repair-scene-undo.png em docs/validacao/evidencias/p01-p04-20260915. Esta conferência encontrou e motivou a inclusão de malhas carregadas sem registro próprio no seletor.

O caso físico teve um uso afetado; atomicidade com múltiplos objetos foi exercitada no host. Reparo de textura compartilhada, override por binding e conflito de arquivo possuem evidência de host/persistência, ainda sem percurso visual físico dedicado. Nenhuma equivalência de validação é presumida entre esses casos. Percentuais finais deste bloco: P02 ≈65%, P08 ≈55%, geral ≈30% (índice central 27,8%).

## Continuação — requisitos condicionais de referências

Implementado no contrato `ComponentObjectReference` um predicado `requiredForExecution`, independente da visibilidade e da validade autoral. O resolvedor comum distingue Ready, OptionalEmpty, RequiredEmpty, Incompatible e Inactive. Referências opcionais vazias continuam válidas; rascunhos incompletos continuam editáveis e serializáveis.

Primeiro consumidor real: Junta exige o corpo conectado quando `enabled=true`, conforme o comportamento existente em `ScenePhysics`. O runtime passa a consumir o resolvedor comum; o inspetor destaca campo obrigatório/incompatível/inativo, e o painel de impacto distingue requisito de execução ausente e destino inativo. A relação estrutural Junta → Corpo no mesmo objeto permanece obrigatória. Desativar uma junta não remove referências explícitas nem libera a remoção de seus destinos: essas referências autorais continuam protegidas.

Base: plano de universalidade §4.2/P02, sem presumir contratos de outra engine. Um colisor estático não recebeu requisito de corpo físico. O predicado é metadado de código, sem mudança no formato persistido ou migração de arquivos.

Validação: build host concluído; P02 11/11 e composição 6/6, incluindo simulação dos quatro tipos de junta. Cobertura nova: rascunho ativo incompleto, desativação, destino ativo/inativo e autorreferência incompatível. Não houve build Android ou nova conferência ADB nesta continuação; a instalação do bloco anterior não contém estas mudanças.

Limites: prontidão de referência não certifica prontidão física completa. Pelo menos um corpo dinâmico, geração de shape, residência e demais condições ainda têm verificadores próprios. Não foi concluído um sistema geral de requisitos condicionais entre tipos, nem a exposição de todas as condições do solver no painel.

Percentuais mantidos conservadoramente: P02 ≈65%, P08 ≈55%, geral ≈30% (índice central 27,8%; faixa 25–35%). Esta entrega fecha uma condição real dentro da estimativa atual, sem inflar o pacote.

## Continuação — condições físicas das juntas compartilhadas

`runtime/joint_requirements.h` concentra a auditoria autoral de juntas ativas: validade dos campos, corpo no objeto, destino compatível e ativo, pelo menos um corpo dinâmico e matrizes globais finitas. Os diagnósticos possuem código estável, objeto e instância. ScenePhysics consome o mesmo verificador; o painel de impacto apresenta condições adicionais sem duplicar as linhas de composição/referência. A condição de movimento permite navegar até o Corpo físico do objeto da junta. Junta desativada ou objeto inativo não executa essa auditoria.

Evidência: build host aprovado; P02 12/12; composição 6/6, incluindo simulação das quatro juntas. Caso novo cobre par estático/cinemático recusado, navegação ao corpo, destino dinâmico satisfazendo a condição e suspensão por desativação. Sem instalação Android ou conferência ADB nesta rodada.

Correção de precisão da documentação: o runtime atual exige Corpo físico proprietário para Colisor ativo, inclusive estático. O corpo pode ser Static: não existe exigência de corpo DINÂMICO para collider estático. A referência vazia de proprietário é um rascunho válido e significa o próprio objeto na execução, que precisa conter o corpo. A documentação anterior sobre não acrescentar requisito deve ser lida como ausência de NOVA restrição de composição, não como suporte já implementado a collider ativo sem corpo.

Limites: a auditoria de junta não certifica criação de shapes, ausência de shear, compatibilidade completa de hierarquia dos corpos nem aceitação pelo Jolt; esses caminhos continuam na inicialização física. Propriedades autorais não são alteradas automaticamente. Percentual mantido: P02 ≈65%, P08 ≈55%, geral ≈30% (índice 27,8%, faixa 25–35%).

## Continuação — corpos, formas e transformações físicas

Extraídas para `runtime/physics_requirements.h` as regras efetivamente usadas pelo runtime: proprietário próprio/ancestral explícito sem atravessar outro corpo, hierarquia de corpo independente, decomposição da transformação do corpo, composição da pose do colisor, rejeição de shear e escala uniforme para esfera/cápsula. ScenePhysics usa essas funções para construir os corpos; o painel de impacto consulta as mesmas funções, evitando uma segunda interpretação geométrica.

O painel de Corpo físico enumera formas ativas vinculadas, navega até suas instâncias e sinaliza ausência de formas, excesso de 256 formas, problemas de hierarquia e transformação. O painel de Colisor resolve o proprietário efetivo (inclusive proprietário vazio significando o próprio objeto), permite navegar ao corpo e aponta problemas geométricos. Componentes/objetos que não executam respeitam a desativação. Consultas não alteram autoria, IDs ou histórico.

Validação: host compilado; P02 13/13 e composição 6/6. Novo caso verifica ausência de formas, esfera sob escala não uniforme, caixa sem rotação válida, caixa rotacionada com shear, desativação e conservação de identidade. Casos existentes exercitam composição assimétrica, shapes filhos, rejeição de shear e quatro juntas no solver. Sem novo APK/ADB nesta continuação.

Limites: consulta do corpo percorre sua subárvore, sem índice incremental; diagnósticos de transformação podem repetir quando várias formas compartilham o corpo inválido. Não certifica disponibilidade de memória, aceitação pelo backend, nem todos os requisitos de Personagem. Corpo próprio continua necessário para collider ativo com proprietário vazio; rascunho sem corpo continua editável. Não foi criado requisito de corpo dinâmico para colisor estático.

Percentuais: P02 ≈65%, P08 ≈55%, geral ≈30% (índice central 27,8%, faixa 25–35%), mantidos até fechar mais percursos e conferir a apresentação no aparelho.

## Reestimativa após presets e condições de execução

P02 passa de aproximadamente 65% para **75%**. A base do avanço é o conjunto das condições reais de execução (referências, juntas, corpos e colisores) com o novo percurso persistente de presets: capturar, nomear, listar, prévia, aplicar, adicionar com requisitos, Undo da cena, renomear e excluir. Não se atribui esse mesmo avanço novamente a P01/P08.

O índice central dos 18 pacotes passa de **27,8% para 28,3%**; o arredondamento global permanece ≈30% (faixa 25–35%). Faltam no P02 grafo indexado, cobertura de todos os provedores futuros, presets compostos/seletivos e integração dos presets ao grafo de recursos. Detalhes e limites em [Presets de componentes](P02-PRESETS-COMPONENTES-2026-09-17.md).

### Conferência final no aparelho

APK final instalado em 25053PC47G. Projeto P01Camera0915i: Malha de EsferaDraco → Presets → Salvar atual → nome EsferaDraco_Preset com barra visível acima do teclado. Biblioteca sobreviveu à reinstalação/reabertura. No Cubo, seleção do preset manteve a geometria durante a prévia; Aplicar valores mudou de cubo para esfera azul. Um Undo restaurou o cubo e a cena foi salva restaurada. O preset de demonstração permanece na biblioteca desse projeto.

A conferência encontrou e corrigiu dois defeitos antes do fechamento: barra de nome ausente e conversão incorreta de índice da malha (o slot MeshRenderer é 1-based; assetGuid recebe índice 0-based). O teste de presets passou a verificar explicitamente GUID e slot 1, incluindo captura de malha legada sem GUID. P02 final 14/14. Evidências em docs/validacao/evidencias/p01-p04-20260915: presets-keyboard.png, presets-preview.png, presets-applied.png, presets-undo.png. Renomear/excluir, proteção de referência e adição Olhar+Câmera possuem evidência de host; não foram todos repetidos fisicamente. Sem commit/push nesta entrega.
