# Auditoria dos pacotes de terreno fornecidos

Data: 15/10/2026. Esta matriz separa extração, estudo e implementação. Código de Unity/C# ou shader de Unity não é um plugin carregável diretamente no renderer C++/Vulkan da Astra. A equivalência exige consumidor, persistência, UI e teste próprios. **Extração não significa port completo.**

## Universo físico verificado

| Distribuição | Arquivos de conteúdo | Versão interna / observação |
|---|---:|---|
| Digger – Terrain Caves Overhangs v6.0 | 151 | 73 C#, 14 shaders, 11 HLSL, 17 PNG, 15 prefabs; fontes de ações e formas presentes |
| MicroSplat – Terrain Collection v3.9.41 | 517 | 11 módulos; todos os `package.json` declaram **3.9.0**, Unity 2019.4. 53 C#, 95 snippets TXT, 34 shaders, 151 PNG, 24 TGA, 39 PDFs |
| Path Painter II v2.1.10 | 85 | Changelog 2.1.10; manual 2.1.2; quatro DLLs em variantes 000/010, XML de API, três shaders, 42 PNG, quatro PSD |
| Total | **753** | Não inclui `.meta`, diretórios e previews extraídos separadamente |

Manifestos com SHA-256 por pacote/arquivo estão em `build/terrain-packages/*/manifest.json`. Inventário recontado em `build/terrain-research/inventory.json`. As fontes e vídeos de terceiros não são copiados para a documentação pública.

## Cobertura de entrada (APK 78, antes da revisão solicitada)

I = implementado no recorte descrito; P = parcial; N = ausente. Nenhuma linha abaixo autoriza anunciar paridade com o pacote.

| ID | Família / capacidades efetivamente encontradas | Entrada | Dependências e lacuna |
|---|---|:---:|---|
| D01 | Dig, Add; opacidade e tamanho | P | Esfera finita; sem prévia ao vivo, dimensão por eixo ou alvo de opacidade |
| D02 | Sphere, HalfSphere, RoundedCube, Stalagmite e inversão | P | Apenas esfera; `BrushType.cs`, `ModificationParameters.cs` |
| D03 | Paint e seleção de textura, opacidade-alvo | P | Quatro materiais por região dura; sem mistura por peso |
| D04 | PaintHoles | N | Máscara de buracos no heightfield, restauração e colisão |
| D05 | Reset, Smooth, BETA_Sharpen | P | Smooth existe; faltam restauração local e sharpening |
| D06 | Cavernas, overhangs, fechamento e colisão | P | Volume finito funcional; falta edição robusta de paredes/cursor |
| D07 | Chunks afetados, LOD e geração paralela Burst | P | Chunks 16 existem; regeneração integral, sem LOD de terreno |
| D08 | Persistência/Undo/Redo, recarga de dados | P | Receita/GLB versionados; custo de revisões retidas precisa gestão |
| D09 | Integração de materiais Terrain/MicroSplat/URP/HDRP | N | Astra usa Vulkan próprio; adaptar semântica de superfície |
| D10 | API de modificação e integração de navegação/runtime | N | API de editor C++ parcial; sem equivalente runtime público completo |
| P01 | Tamanho do caminho e acostamento, curvas de transição | P | Raio único/falloff fixo; sem perfil de acostamento separado |
| P02 | Shaping, Texturing e Vegetation Clearing independentes | P | Caminho combina nível+pintura; vegetação de terreno ausente |
| P03 | Auto Ramp, limite de inclinação, elevação e Terrain Follow | N | Path força Y fixo; faltam rampa e acompanhamento |
| P04 | Superfície/acostamento, opacidade, Smart Texture Paint | P | Uma camada dura; falta transição e semântica de cruzamento |
| P05 | Paint/Edit, alterar traço após desenho, continuar caminho | P | Receita reaberta; sem seleção de traço como objeto de edição |
| P06 | Retas, curvas, linhas mistas, pontos e caps | P | Interpolação simples entre amostras; grupo de gesto necessário |
| P07 | Limpar grama/árvores, ruído e faixa gradual | N | Requer vegetação persistida, não apenas meshes decorativos |
| P08 | Terrenos vizinhos, grupos e propagação de camadas | N | Terreno finito independente; chunks internos não são vizinhos autorais |
| P09 | MapScaler e resoluções de height/control/detail maps | N | Redimensionar célula não equivale a reamostrar dados independentes |
| P10 | API Paint/Paint3D/linhas/bulk, inclusão opcional runtime | N | DLLs/API específicas Unity; consumidores Astra necessários |
| M01 | Core: splats, texture arrays, propriedades por camada | P | Quatro texturas, sem pesos/arrays nem propriedades por camada |
| M02 | Core: height blend, normals, AO, smoothness, metal, emissão | N | Material básico roughness fixa; mapas fornecidos precisam integração |
| M03 | Triplanar: local/mundo e mistura dos eixos | N | Projeção pelo eixo dominante gera emendas; não é triplanar |
| M04 | Anti-tile: detail/distance noise e até três normal noises | N | Repetição uniforme de quatro PNG |
| M05 | Anti-tile: distance resampling, fade/noise, height blend | N | Requer caminho de sombreamento e controles verificáveis |
| M06 | Texture clusters: 2/3 variantes, stochastic, rotação/escala | N | Sem variantes ou randomização contínua |
| M07 | Global texture: tint/normal/SAOM/emissão, geomap | N | Sem mapa global e respectivo compositor |
| M08 | Snow: altura, inclinação, ruído, máscara, glitter | N | Sem sistema de neve/cobertura persistida |
| M09 | Streams: wetness, puddles, streams, lava e pintura FX | N | Água geral da engine não equivale à integração de terreno |
| M10 | Terrain blending: mesh/terreno, blend height/normal | N | Sem amostragem de superfície entre objetos |
| M11 | Tessellation, displacement, parallax e POM | N | Requer variantes GPU e orçamento móvel; não prometer tessellation por haver PDF |
| M12 | Alpha-hole: corte por alfa e sombras | N | Precisa máscara, sombra e política de colisão coerentes |
| M13 | Wind/glitter: variação animada, reflexão pontual | N | Sem consumidor/controles de terreno |

Módulos **fornecidos**: core, alpha-hole, anti-tile, global-texture, snow, streams, terrain-blending, tessellation, texture-clusters, triplanar, wind. PDFs de Procedural Texturing, Scatter, Trax, Decals, Mesh Terrains, Object Shader, Low Poly e 256 Textures estão no diretório do core, mas **não há módulos correspondentes no pacote**. Não confundir manual de catálogo com fonte fornecida.

## Critério de fechamento de cada linha

Modelo e limites → serialização/migração → consumidor de geometria/shader/runtime → criação/edição/estados → desfazer/cancelar/reabrir → teste de efeito → captura real. Uma implementação com recorte permanece P e declara o recorte. A tabela de resultados desta revisão deve ser preenchida somente após testes.

## Revisão 3 — código e testes no host, aceite Android pendente

Suíte integrada: **33/33 cenários** em Windows. Não representa aceite no aparelho da revisão 3. O telefone estava offline ao fechar estes testes.

| Famílias | Incremento efetivo | Cobertura restante |
|---|---|---|
| D01/D02 | Prévia de geometria durante o gesto; círculo/esfera, caixa, meia esfera e cone; raio, força, núcleo rígido, rotação e proporção X/Z. Geometria e cursor usam a mesma orientação. | Dimensão Y independente, cubo arredondado e cone invertido não implementados. Não há equivalência completa de parâmetros Digger. |
| D03/M01 | Quatro pesos normalizados por vértice, pintura suave local em 3D, restauração dos pesos naturais e miniaturas reais. | Sem arrays, mapas de controle de resolução independente ou biblioteca arbitrária de camadas. |
| D04/M12 | Buracos geométricos no relevo; no modo base sólida, cortam teto/fundo e fecham as bordas. Restaurar preenche novamente. | Não é recorte por alfa do MicroSplat. Não há máscara alfa separada. |
| D05 | Restaurar, suavizar e aguçar; ruído e terraços no campo de altura. Testes distinguem efeito real de cada operador. | Equivalência numérica aos algoritmos Unity não declarada. |
| D06/D08 | Cursor de volume, modos de escultura/navegação exclusivos, prévia descartável em memória, um gesto por Undo e reedição pelo Inspector. | Sem teste de conforto/latência desta versão no telefone; revisões finais ainda se acumulam. |
| P01/P06 | Núcleo/falloff ajustáveis; separação de gestos impede ligar caminhos independentes por engano. | Acostamento independente, rampa automática, seguir terreno, edição de pontos/curvas e vegetação continuam pendentes. |
| M03/M04/M06 | Shader com mistura triplanar de albedo e combinação de duas escalas/rotações com variação macro; gradientes explícitos para mipmaps. Quatro variantes SPIR-V geradas e validadas. | Sem normais por camada, clusters de texturas distintas, height blend, POM, resampling por distância ou paridade Anti-Tile. Aparência/desempenho GPU ainda sem aceite desta revisão. |

**Arquitetura atual:** o worker de prévia produz apenas TerrainMesh. O consumidor publica uma malha temporária sem GLB, registro de recurso, arquivo ou alteração no documento. A fila mantém um trabalho e reúne as amostras mais recentes. Ao soltar, a publicação definitiva gera a revisão persistida; cancelar repõe o pacote anterior. A prévia ainda recalcula todo o terreno e republica o pacote gráfico agregado; não é remalhamento por chunks afetados nem garantia de 60 fps.

**Persistência:** AETERRAIN3 guarda formas/parâmetros/grupos de gesto, mistura e variação de material. AETERRAIN1/2 continuam legíveis, com aparência legada por padrão; Dig/Add antigos migram com núcleo rígido para preservar sua influência. O teto passa a 4.096 amostras por receita; não equivale a 4.096 gestos.

**Ainda não implementados nesta revisão:** M02, M05 e M07–M13 nos recortes próprios dos módulos, D07 incremental/LOD, D10 API runtime, P02–P05 completos e P07–P10. Não existe port completo de nenhum dos três pacotes. A auditoria está concluída como inventário; a implementação de todas as capacidades solicitadas permanece em andamento.

Referências: [Digger oficial](https://ofux.github.io/Digger-Documentation/), fontes locais do Digger 6.0 e PDFs/XML locais dos outros pacotes. A documentação online pode descrever versões posteriores; o universo físico acima é o que foi recebido. Referências Unity e análise visual: [revisão de design](design/terrain-2026-10-15/revision3/README.md).
