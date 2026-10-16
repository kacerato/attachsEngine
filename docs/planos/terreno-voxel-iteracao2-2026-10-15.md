# Terreno e voxel — edição prática e aceite no aparelho

Branch `codex/terrain-voxel-generator`. Continuação autorizada em 15/10/2026, sem merge na main.

**Direção de 16/10/2026:** o proprietário suspendeu o uso de ADB e solicitou primeiro a auditoria direta dos cinco plugins. O escopo detalhado está na [matriz de 71 itens](../auditoria-terreno-2026-10-16/auditoria.md), com inventário, propriedades, assets, transições e três blocos de execução. Essa auditoria substitui o universo de apenas três plugins; não marca a implementação ampliada como concluída.

1. **Reeditar e dar volume.** Ação contextual no Inspector da raiz/chunks, recuperação de receita e fechamento do menu de criação. Terreno por altura com superfície ou base sólida, fundo plano e espessura persistida. Preservar receitas anteriores.
2. **Autoria por toque.** Forma, Pincel, Camadas e Gerir; propriedades agrupadas, seleção direta dos modos, predefinições de relevo, rolagem e ajustes numéricos rápidos. Corrigir toque/arraste do pincel, acesso a XYZ, desfazer/refazer, aplicar/reverter e salvar. Novos ícones no atlas real.
3. **Validar e entregar.** Testes nativos de geometria/receitas/UI e fluxo Android nos três modos: criar, editar, pintar/esculpir, desfazer/refazer, salvar e reabrir. Corrigir falhas, compilar/instalar APK Dev preservando dados, capturar resultado, publicar branch e nota nova no AstraDocs com evidência e limites reais.

## Referências

- Godot 4.6 SurfaceTool: https://docs.godotengine.org/en/4.6/classes/class_surfacetool.html — geometria com atributos, normais e UVs.
- Unity 6.0 Heightmaps: https://docs.unity3d.com/6000.0/Documentation/Manual/terrain-Heightmaps.html — dimensões, resolução e altura.
- Unity 6.0 Raise or Lower Terrain: https://docs.unity3d.com/6000.0/Documentation/Manual/terrain-RaiseLowerTerrain.html — ferramenta, pincel e intensidade com aplicação na cena.

Em 15/10 o proprietário ampliou o pedido: auditar todas as capacidades dos três pacotes, estudar vídeos de referência quadro a quadro e revisar arquitetura, pincel, materiais e fluxo completo. O recorte anterior acima é histórico, não encerra esse pedido ampliado.

## Revisão solicitada — execução em três frentes

1. **Auditoria e referência verificável.** Inventário dos 753 arquivos, módulos realmente fornecidos, documentação local e demonstrações oficiais. Matriz de capacidades com dependências e cobertura real; proposta visual baseada na captura do aparelho.
2. **Autoria e superfície.** Prévia temporária durante o gesto, cursor e influência visíveis, modo exclusivo de escultura/navegação, parâmetros úteis e persistidos, materiais com transições e variação. Preservar Undo, cancelamento, fontes antigas e reedição.
3. **Validação e publicação.** Exercitar cada capacidade entregue, comparar capturas antes/depois, build/APK/toque/logs, publicar somente a branch e atualizar o site. Recursos auditados sem consumidor real permanecem explicitamente pendentes; a matriz impede declarar port completo por ter extraído arquivos.

## Achados iniciais

- A receita persistia, mas sua reabertura estava escondida em Gerir → Abrir seleção.
- A ação de terreno não fechava o menu de contexto da criação.
- O pincel esperava `routing.tapped` no viewport, embora esse sinal seja exclusivo de widgets.
- Propriedades dos diferentes modos apareciam em linhas uniformes, sem distinção clara entre forma e edição.

Estado: implementação e aceite Android em andamento.
