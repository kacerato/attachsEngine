# Terreno por altura com base sólida — contrato

## Propriedades persistidas

`TerrainRecipe::solidBase` escolhe superfície aberta (`false`) ou terreno fechado
(`true`). A opção só altera `Heightfield`; os modos voxel continuam usando seu
volume de densidade. O editor pode iniciar novos terrenos com a opção ligada;
a estrutura e a migração preservam `false` para receitas antigas.

`TerrainRecipe::baseDepth` é a **espessura mínima**, em unidades locais do terreno,
entre o ponto mais baixo da superfície esculpida e o fundo plano. Padrão 8;
intervalo válido de 0,1 a 512, com rejeição de NaN/infinito. O fundo é calculado
depois das operações de escultura: `menor altura final - baseDepth`. Baixar o
ponto mínimo também baixa a base, mantendo a espessura solicitada. A propriedade
não é a altura absoluta do fundo nem a resolução vertical do modo voxel.

O gerador mantém o topo, acrescenta o fundo subdividido na mesma grade e fecha
somente as quatro bordas externas. Não há paredes nos limites internos dos
chunks. Fundo e paredes usam normais planas voltadas para fora, UVs projetadas
pelo eixo dominante e camada 1 (Rocha); a pintura do topo não substitui essa
camada. Os IDs existentes por coordenada e camada continuam sendo usados.

A colisão Mesh existente recebe a mesma geometria fechada, incluindo as paredes
e a face inferior. Trata-se de uma casca triangular fechada para renderização e
colisão estática, sem simulação de massa/volume e sem escavação de cavernas no
modo por altura; escavar e adicionar volume continuam pertencendo aos voxels.

## Persistência, limites e migração

Novas gravações usam `AETERRAIN2`: os campos `solidBase baseDepth` ficam depois de
`collision` e antes da quantidade de operações. A leitura de `AETERRAIN1`
continua suportada, com superfície aberta e profundidade padrão sem efeito.
Receitas truncadas ou inválidas não substituem o estado de saída.

O GLB mantém a receita incorporada, materiais, chunks e atributos como antes.
Uma base sólida contém `4*cells*cells + 8*cells` triângulos; no limite de 128
células são 66.560, abaixo do orçamento de 200 mil. Não aumenta a grade de
amostras nem exige discretização volumétrica. Geração cancelada continua sem
publicar o resultado parcial.

## Referências oficiais de capacidade

- Godot **4.6**, [SurfaceTool](https://docs.godotengine.org/en/4.6/classes/class_surfacetool.html): construção de geometria com posições, UVs e normais suaves/planas.
- Godot **4.6**, [BoxMesh](https://docs.godotengine.org/en/4.6/classes/class_boxmesh.html): volume retangular alinhado aos eixos, dimensões e subdivisões das faces.

Essas referências fundamentam a capacidade; a malha e o contrato da Astra são
implementação própria. Validação do aparelho e capturas da interface devem ser
registradas no relatório integrado desta entrega.
