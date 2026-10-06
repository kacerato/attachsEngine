# Análise visual — Colliders, Unity Official Tutorials

Repositório alvo: https://github.com/kacerato/attachsEngine. Vídeo oficial: https://www.youtube.com/watch?v=bh9ArKrPY8w, canal Unity, publicado em 2013. A versão exata do editor exibido não foi identificada; este vídeo serve como referência de interação histórica. API e arquitetura atuais são fundamentadas na Unity 6000.0 e Godot 4.5 abaixo.

## Método e cobertura real

O vídeo foi aberto no navegador, com o player reportando 1920 × 1080. As capturas foram feitas na superfície de 1280 × 720 do navegador. Foi usado o avanço de um quadro do player pausado, confirmado por incrementos de aproximadamente 0,033333 s. Não houve download do vídeo nem interpretação de título/transcrição como evidência visual.

Foram capturados e examinados **100 quadros consecutivos**, de **90,033333 s a 93,333300 s**, sem saltos, para a passagem de seleção múltipla das peças para seleção da raiz. Os timestamps reais estão em [adjacent.json](../../validacao/ui-universal-2026-10-06/video-colliders/adjacent.json), e [adjacent-reviewed.json](../../validacao/ui-universal-2026-10-06/video-colliders/adjacent-reviewed.json) acrescenta hash e observação de cada quadro; os 100 PNGs completos permanecem ao lado. Quatro folhas de contato permitem examinar todos esses quadros, com recorte de Hierarchy/Inspector: [000–024](../../validacao/ui-universal-2026-10-06/video-colliders/contact-000.png), [025–049](../../validacao/ui-universal-2026-10-06/video-colliders/contact-025.png), [050–074](../../validacao/ui-universal-2026-10-06/video-colliders/contact-050.png) e [075–099](../../validacao/ui-universal-2026-10-06/video-colliders/contact-075.png).

**Isso não equivale a analisar todos os quadros dos 4 minutos.** Outros tempos foram examinados por amostragem para contexto; esse escopo é separado abaixo. Capturas pretas, cortadas ou com buffering foram descartadas como evidência de workflow. Um `currentTime` atualizado não garante que a imagem apresentada já seja daquele tempo.

## Leitura dos 100 quadros

| Quadros | Tempo real aproximado | Observação visual | Consequência para a engine |
|---|---|---|---|
| 000–009 | 90,033333–90,333330 | Várias peças da hierarquia selecionadas; Inspector mostra Mesh Filter, Mesh Renderer e Mesh Collider. Cursor se desloca pela seleção. | Visual e colisão precisam conservar suas identidades próprias. |
| 010–039 | 90,366663–91,333320 | Cursor chega às linhas superiores da árvore; seleção múltipla e Inspector permanecem. Não aparece novo painel. | Troca contextual deve ser rápida e não exigir abrir uma ferramenta diferente. |
| 040–042 | 91,366653–91,433319 | Destaque da árvore passa à raiz `prop_robotArm`; o conteúdo anterior do Inspector ainda aparece. | Separar mudança de seleção e atualização da superfície de propriedades. |
| 043 | 91,466652 | Quadro intermediário contém mistura visual de conteúdo anterior e novo no Inspector. Não é prova de animação intencional nem de atomicidade interna. | Nunca inferir comportamento de código apenas de um quadro de transição. |
| 044–099 | 91,499985–93,333300 | Raiz selecionada; Inspector mostra Transform e Add Component, sem Mesh Collider/Renderer da seleção anterior. Filhos continuam visíveis na árvore. | O objeto raiz não precisa ter malha. A inspeção de uma parte não deve converter a raiz em uma primitiva. |

O clique específico e seus modificadores não foram identificados como eventos de entrada no vídeo. O resultado da seleção é visível. Não se presume atalhos, contagem de cliques ou composição física não demonstrados.

## Quadros de contexto, examinados por amostragem

| Captura | Tempo | Observação |
|---|---|---|
| [clear-035](../../validacao/ui-universal-2026-10-06/video-colliders/clear-035.png) | 35 s | PowerCube selecionado com Box Collider, Center e Size, visual e contorno diferentes. |
| [clear-050](../../validacao/ui-universal-2026-10-06/video-colliders/clear-050.png) | 50 s | Bancada composta, filhos Handle/Leg/LowerShelf na hierarquia e Box Collider no Inspector. Não foram verificadas todas as propriedades dos filhos. |
| [clear-065](../../validacao/ui-universal-2026-10-06/video-colliders/clear-065.png) | 65 s | Bancada visual com Mesh Collider e campo Mesh; Convex aparece desmarcado. |
| [full-080](../../validacao/ui-universal-2026-10-06/video-colliders/full-080.png) | ver overview.json | Peças do braço selecionadas e recursos com nomes de collision no Asset Browser. Nome de recurso não prova sua participação no solver. |
| [full-110](../../validacao/ui-universal-2026-10-06/video-colliders/full-110.png) | ver overview.json | Linhas verdes de geometria mais simples dentro do braço visual. O gesto que as produziu não foi analisado continuamente neste trecho. |
| [full-160](../../validacao/ui-universal-2026-10-06/video-colliders/full-160.png) | ver overview.json | Rigidbody expandido separadamente do Sphere Collider: massa, drag, gravity, kinematic e collision detection visíveis. |
| [clear-206](../../validacao/ui-universal-2026-10-06/video-colliders/clear-206.png) | 206,739671 s | Play ativo, Sphere Collider e Rigidbody inspecionados; Scene e Game mostram vistas distintas. |
| [clear-221](../../validacao/ui-universal-2026-10-06/video-colliders/clear-221.png) | 221,739671 s | Esfera próxima ao cubo; Console exibe Enter/Stay. Uma imagem estática não prova a sequência inteira de callbacks. |
| [clear-236](../../validacao/ui-universal-2026-10-06/video-colliders/clear-236.png) | 236,739671 s | Editor fora de Play e Console com Enter/Stay/Exit; não se deduz persistência de alterações em Play desse quadro. |

## Referências atuais e decisão aplicada

- Unity **6000.0**, [compound colliders](https://docs.unity3d.com/6000.0/Documentation/Manual/compound-colliders-introduction.html): formas separadas participam de um corpo e podem ter poses independentes. Aqui usamos a coleção e IDs persistentes de Collider já existentes; não duplicamos Transform/Body.
- Godot **4.5**, [CollisionObject3D](https://docs.godotengine.org/en/4.5/classes/class_collisionobject3d.html): ownership de formas é separado da representação visual. Esse princípio ajuda a manter foco de componente distinto da seleção do objeto.
- Godot **4.5-stable**, [CollisionShape3DGizmoPlugin](https://github.com/godotengine/godot/blob/4.5-stable/editor/scene/3d/gizmos/physics/collision_shape_3d_gizmo_plugin.cpp): o gizmo usa linhas/triângulos de colisão da forma e registra segmentos de seleção; alteração de forma segue o histórico. Extraímos o princípio de selecionar o contorno realmente desenhado. Nenhum código da Godot foi copiado.

**NÃO IREI SER SIMPLISTA NO DESIGN.** A proposta estrutural aplicada é reutilizar o viewport como superfície contextual de escolha de partes: com um Collider aberto, um toque em outro contorno do mesmo objeto muda a instância inspecionada e seu destaque. Não cria uma janela de partes nem move o visual. O Inspector existente continua sendo a superfície de edição; seus consumidores, histórico e serialização são reutilizados.

O picking desse pacote é de **contornos do overlay**, que já são desenhados sem oclusão, e não uma consulta física ao solver. Seleção por face/profundidade, seleção cruzada de colliders em descendentes e diagnóstico completo de centro de massa/apoio ainda são pendências de U11. Coincidência exata de contornos desempata por ID persistente; as setas do Inspector continuam permitindo alcançar partes sobrepostas.

## Auditoria de capturas rejeitadas

A busca nos documentos desta cadeia encontrou também o vídeo `Mzt1rEEdeOI` em `CONTRATOS-E-EXECUCAO.md`. A análise visual desse vídeo de World Space continua pendente e foi sinalizada no próprio documento. Este relatório não atribui seus quadros, interações ou capacidades à análise de Colliders.

`overview-000` e `full-065` estão pretas. `overview-020/035/050/065` tiveram recorte do player ou gaveta de recomendações. `full-175/190` estavam com buffering e imagem anterior, embora o timestamp tivesse avançado. Esses arquivos são mantidos para rastreabilidade, não sustentam as conclusões acima. `overview.json` guarda os tempos reportados, inclusive de tentativas rejeitadas. As conclusões usam apenas as capturas nomeadas na tabela e os 100 quadros consecutivos examinados.
