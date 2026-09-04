# ADR-017 — Água nativa mobile, data-driven e por capacidades

Status: aceita; implementação incremental com cada capacidade validada antes de
ser marcada como disponível.

## Contexto e referência funcional

O pacote autorizado pelo proprietário foi inspecionado em
`C:/Users/donod/Downloads/extracted/extracted/Assets/KriptoFX/WaterSystem/WaterResources`.
Ele é uma referência de amplitude funcional, não uma dependência de runtime: os
scripts, materiais, command buffers e serialização dependem de UnityEngine e não
podem ser carregados diretamente pela Aether.

Foram mapeados `WaterSystemScriptableData`, `WaterSystem`, os passes de render e
os módulos de buoyancy, flow map, fluids, dynamic waves, shoreline, spline,
planar/cubemap reflection e interação. A cena Unity poderá alimentar um
importador futuro de dados conhecidos, mas componente desconhecido deve produzir
diagnóstico explícito; nunca desaparecer silenciosamente.

## Decisão

Água é composta por `WaterSurface` (instância ECS), `WaterProfile` (Resource
versionado), dados opcionais de costa/fluxo e uma pipeline resolvida pelas
capacidades do device. Nenhuma opção depende do nome da cena ou do material.

O caminho mobile de base usa dois subpasses Vulkan:

1. opacos, alpha coverage e céu escrevem cor e profundidade;
2. água e transparências são ordenadas back-to-front, a água lê profundidade
   opaca como input attachment e compõe sobre a cor já residente no tile.

Assim, espessura, absorção Beer–Lambert e espuma de interseção não exigem copiar
o depth full-resolution para DRAM. HZB/TAA continuam sendo leitores externos e
preservam o depth somente quando realmente ativos. Refração distorcida de cor é
uma capacidade separada: pode usar imagem de cor amostrada no perfil de maior
qualidade; o fallback tile-local transmite a cor de fundo sem inventar uma
captura falsa.

Ondas têm dois domínios distintos. Ondas longas deslocam vértices e fornecem a
mesma superfície determinística às consultas de gameplay. Ondulações menores
alteram apenas a normal no fragment shader e são filtradas por derivadas/footprint
para não produzir moiré à distância. O detalhe usa um espectro de normais
seamless, com mip chain completa e duas amostras rotacionadas; não há grade
periódica codificada no fragment shader. Uma futura simulação FFT substitui o
provedor de espectro sem alterar `WaterSurface`, buoyancy ou a pipeline óptica.

Perturbações locais entram por `WaterInteractionField`, uma fila circular fixa,
sem alocação por frame. Toque, chuva, casco, rigid body ou script publicam o
mesmo `WaterImpulse`; o campo usa uma onda radial amortecida tanto na consulta
CPU quanto no deslocamento GPU. O upload compacta apenas eventos ativos e o
shader não percorre os oito slots quando o campo está vazio.

O aplicativo de validação apresenta seleção de cenas antes do runtime. O painel
de gráficos altera eixos globais via snapshot versionado thread-safe; a UI Java
não conhece Vulkan nem materiais. Escala, resolução dinâmica, sombras, bloom,
nitidez, espectro, micro-ondas, opacidade, absorção, espuma e força de interação
continuam dados resolvidos pelos subsistemas nativos e podem migrar para a mesma
metadata futura do Inspector/NoCode.

## Matriz de capacidades

Estados: **operacional** significa que há consumidor real e teste; **fundação**
significa contrato implementado, mas sem todos os passes; **planejado** não é
anunciado ao runtime como disponível.

| Família da referência | Modelo Aether | Estado atual | Gate de conclusão |
| --- | --- | --- | --- |
| oceano infinito, caixa finita, rio spline, custom mesh | `WaterDomain`, clipmap camera-snapped e superfície ECS | fundação | gerar clipmap/river mesh no runtime, costuras e bounds animados |
| FFT, vento, rotação, turbulência, time scale | provedor de espectro do `WaterProfile` | ondas analíticas operacionais; FFT planejada | compute FP16 validado, cascatas espectrais e fallback determinístico |
| micro waves/normal detail | espectro normal seamless, mips e intensidade no perfil | operacional no shader e UI | validar sweep longo sem shimmer em Adreno/Mali |
| transparência, cor, turbidez, absorção, IOR | óptica versionada | operacional para depth/Beer–Lambert/Fresnel | prova visual rasa/profunda e cena com objetos submersos |
| refraction simple/physical, dispersion | `WaterRefractionMode` resolvido por capacidade | transmissão tile-local operacional; distorção planejada | cena color amostrável, bordas seguras e custo medido |
| environment, SSR, planar e cubemap reflection | `WaterReflection` + fallback explícito | environment operacional; demais fundação | passes, culling mask, resolução/cadência e preenchimento fora da tela |
| reflected sun e anisotropic reflection | lóbulo solar e normal de onda | sol operacional; anisotropia planejada | perfil próprio e comparação energética |
| shoreline waves e shoreline foam | recurso de costa + thickness/depth | espuma de interseção operacional; ondas de costa planejadas | baker/editor, LOD costeiro e transição estável |
| foam color/size/fade/shadows | módulo de espuma | crest + contato operacionais | textura/partículas, shadow receive e distância configurável |
| flow map | recurso vetorial versionado | planejado | painter/import, amostragem e velocidade por superfície |
| fluid simulation around objects | domínio compute local | planejado | timestep fixo, atlas limitado, interação e fallback |
| dynamic waves/rain | `WaterInteractionField` com impulsos radiais | analítico operacional em CPU/GPU; compute planejado | atlas compute local para interferência complexa e budget independente |
| caustics, dispersion e ortho depth | passe de caustics por volume | planejado | projeção, LOD, resolução e máscara de profundidade |
| volumetric lighting | extensão do volume da água | planejado | resolução/iterações/filtro e composição temporal |
| underwater e queue before/after transparent | volume/detector + passe underwater | planejado | câmera/point/sphere tests, transição e ordenação configurável |
| water holes/boat masks | `WaterExclusionVolume` círculo/caixa feathered | fundação testada em CPU | upload/buffer de volumes, shader e picking/editor |
| buoyancy assíncrona | `sampleWaterSurface` determinístico | consulta CPU operacional | batch/job API, validade temporal e provedor FFT |
| network time | tempo externo monotônico do perfil | fundação | fonte sincronizada sem acoplar rede ao renderer |
| active software culling/world bounds | frustum, draw bounds e distância máxima | operacional no render scene | clipmap runtime e telemetria específica de água |
| filtros, anisotropia, wireframe, depth pós-efeitos | eixos globais resolvidos | fundação | Inspector/ProjectSettings e consumidores por passe |

## Conteúdo e proveniência da cena de validação

`samples/ocean` contém malha oceânica, fundo com batimetria e espectro normal
procedural próprios. O céu profissional é um HDR equiretangular CC0; o cooker
preserva HDR na prefiltragem GGX, gera panorama seam/pole-safe, BRDF LUT e grava
hash, autor, licença e URL no manifesto. O runtime só carrega os artefatos Aether
pré-processados e não decodifica HDR durante o frame.

## Contratos de qualidade e performance

- Qualidade próxima não é reduzida para alcançar FPS; frequências abaixo do
  pixel são filtradas porque já não carregam informação visível.
- Cada módulo caro possui resolução, distância, cadência e fallback próprios.
- Solicitado, resolvido, indisponível e fallback são estados distintos.
- Uma propriedade serializada sem consumidor não conta como feature entregue.
- O alvo de 120 Hz usa 8,333 ms de frame e 7,333 ms de budget GPU; capturas curtas
  ou clock elevado não substituem soak e movimento no hardware real.
- O demo `samples/ocean` é validação Aether nativa, não conversão da cena Unity.

## Compatibilidade e evolução

`WaterProfile` é backend-neutral e versionado. Mudanças incompatíveis exigem
migração explícita. Vulkan só aparece no backend; Inspector, NoCode, scripting,
buoyancy e serialization consomem a mesma metadata/recurso. Dispositivos sem
compute, FP16 storage, sampled color/depth ou tessellation recebem composição
analítica correta, nunca crash nem ativação presumida.
