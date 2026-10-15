# Revisão de autoria de terreno — hipótese visual

`concept.png` foi gerado com a ferramenta integrada de imagem a partir de `device78-voxel-add-frame.png`. É uma hipótese de layout. A vegetação, o cenário e a qualidade fotográfica inventados na imagem **não são recursos implementados nem metas de validação deste desenho**.

Prompt: reorganizar a captura real em uma área de autoria exclusiva; liberar a hierarquia/arquivos durante edição; manter a identidade Astra; mostrar influência externa/interna, centro deslocado do dedo, modo Esculpir/Navegar e confirmação do gesto; controles de raio, força, forma e suavidade; remover gizmo de transformação durante escultura. Ferramentas adicionais precisam de modelo, receita e consumidor antes de aparecer.

## Evidência visual estudada

- Unity, **Build Beautiful Terrains with Unity 2019!**, 01/07/2019: https://www.youtube.com/watch?v=aExdxF4OKBo. Vídeo de 263 s, amostragem geral a cada 15 s, sequência 135–158 s a cada 1 s, mais **60 quadros consecutivos** a 30000/1001 fps a partir de 149 s e 150 s. Não alegamos análise de todos os 7.882 quadros do vídeo.
- 135–141 s: seleção da família da ferramenta no Inspector; menus permanecem distintos da manipulação de objetos.
- 142–147 s: forma/máscara e parâmetros visíveis antes do traço.
- 149,000–149,968 s (30 quadros): influência roxa acompanha o alvo e mantém o terreno legível. A máscara tem forma e borda, não apenas um ponto.
- 150,000–150,968 s (30 quadros): surgem detalhes de relevo ao longo do percurso; a influência permanece presente enquanto o resultado se acumula. O vídeo não mede latência nem prova frequência interna do editor.
- 155–158 s: máscara grande mostra a área de carimbo antes de confirmar; o relevo continua visível ao redor.
- Jason Booth, **MicroSplat Anti Tiling Module**, 09/08/2017: https://www.youtube.com/watch?v=DJbT5-ckIDM. Vídeo de 651 s, visão geral a cada 50 s e sequência 155–195 s a cada 5 s. Mudança de escala/intensidade da normal cria variação de relevo aparente sem depender só da cor. Não é prova de desempenho Android nem da versão fornecida.
- Path Painter II: PDF local versão 2.1.2, pp. 10–17 e 20–28 (a distribuição contém changelog 2.1.10): centro e acostamento distintos, raio/força visuais, edição do traço, caminho e transição configuráveis.

Os vídeos e quadros de estudo ficam em `build/terrain-research/` (ignorados), sem redistribuição no site. A conclusão é estrutural: alvo visível, resposta durante o gesto, estados exclusivos e materiais com variação em escalas diferentes.

## Contratos antes de implementar

| Controle proposto | Contrato necessário |
|---|---|
| Prévia ao vivo | Malha temporária em memória; sem alterar documento, arquivos ou histórico até Up; cancelamento restaura a cena |
| Círculo externo / interno | Mesmo raio, forma e falloff usados pelo gerador; projetados na superfície selecionada |
| Esculpir / Navegar | Um proprietário do gesto; sem regiões de gizmo ou aparência ativa de mover enquanto esculpe |
| Raio / força / suavidade | Validação, serialização por amostra e efeito determinístico; controles grandes com valores numéricos |
| Ruído / terraços / restauração | Algoritmos reais e determinísticos, Undo e compatibilidade de receitas; ausência de implementação impede menu |
| Pintar | Miniaturas reais; pesos interpoláveis e parâmetros persistidos; não trocar uma cor chapada por outra |
| Confirmar traço | Um gesto = um comando; GLB/recurso definitivo somente ao concluir |
| Aplicar forma/material | Apenas quando a configuração muda; não pedir confirmação redundante para cada gesto |

Referências oficiais versionadas: [Unity 6.0 Paint Texture](https://docs.unity3d.com/6000.0/Documentation/Manual/terrain-PaintTexture.html), [Unity Terrain Tools 5.0 brushes](https://docs.unity3d.com/Packages/com.unity.terrain-tools@5.0/manual/brushes.html), [Unity Terrain Tools 5.3 sculpt](https://docs.unity3d.com/Packages/com.unity.terrain-tools@5.3/manual/sculpt.html).
