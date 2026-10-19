# Retomada integrada no Windows — 19/10/2026

Branch: `codex/terrain-voxel-generator`. Checkout:
`C:/Users/jamaa/Documents/ChatGPT/astra`. Sem ADB. O universo continua sendo
os 71 itens de `plano-implementacao.csv`, com todas as propriedades comprometidas.
O primeiro bloco continua com 28 itens; nenhuma funcionalidade foi retirada.

## Integração

- `09e06e1e` incorpora `origin/main` em `7f480e3c` na branch de terreno.
  O catálogo mantém o gerador de terreno e as seis receitas novas de personagem,
  com 97 entradas. A branch main não foi modificada.
- `10d09abb` incorpora o trabalho do outro PC até `c9a5337d`: LOD autoral
  AETERRAIN6, invalidação regional de caminhos, reserva limitada de buffers
  GPU e amostragem do pincel por comprimento percorrido.
- A evidência anterior está em `PROGRESSO-2026-10-17.md`; não equivale a
  validação da combinação de fontes acima neste checkout.

## Verificação desta retomada

Os contratos de materiais passaram em 6/6 testes e a extração seletiva em 2/2.
O verificador de APK agora exige o marcador AETERRAIN6. Sua sintaxe foi
conferida; nenhum APK novo foi gerado ou auditado nesta retomada.

Suíte integrada: **56/56 passaram**, compilada com
`tools/build-editor.ps1 -Target Terrain -Jobs 1 -RunTests`.
Log local: `build/terrain-evidence/integrated-20261019.log`.
Inclui os três cenários novos: frequência de eventos pelo roteador real,
LOD na publicação de geometria/Undo/Redo/salvar/reabrir e controles por toque
em layouts de 655 e 1280 pixels. O pincel compara 1, 8 e 32 eventos no mesmo
segmento visível, verificando carimbos e campo final em altura e volume.
Não somar testes de rodadas anteriores a esta contagem.

Planejamento de atualização de buffers: **4/4 passaram**, incluindo
crescimento, redução, reativação de cauda e limites. Execução pelo novo alvo
`BufferUpdates` do mesmo script, com o lock comum. Log:
`build/terrain-evidence/buffer-updates-20261019.log`. Este alvo testa o
planejador CPU; não é nova prova de upload Vulkan no aparelho.

A superfície de produção foi executada e capturada em
`build/terrain-evidence/lod-1280-20261019.png` e
`build/terrain-evidence/lod-655-20261019.png`. No compacto, os campos Níveis
e Erro ficam na mesma linha, alcançáveis por rolagem, com Aplicar fixo.
Rótulos e valores foram inspecionados nas imagens. O fundo sem cena pertence
ao rasterizador de teste: não demonstra transições de LOD ou material 3D.

## Próxima dependência de materiais

O catálogo conserva 82 entradas e 146 texturas. O consumidor atual ainda usa
os quatro bindings de `MapMaterialRecord` como albedos e pesos por vértice.
Trocar apenas essas imagens não entrega M01–M08. A implementação precisa de
identidade persistida por camada, mapas de controle, propriedades por camada
e bindings próprios para normal, altura e PBR, compartilhados entre prévia,
GLB/reabertura e renderer. O caminho com descritores limitados também precisa
consumir o mesmo contrato; não anunciar uma capacidade exclusiva do caminho
bindless como suporte geral do editor.

Fontes originais permanecem na resolução original. Perfis opcionais não
substituem fontes, e miniaturas não são texturas de renderização. Permanecem
pendentes as transições, ruídos e clusters completos, interface altura-volume,
checkpoints/GC, vizinhança e resoluções independentes, além do aceite Android.

Referências oficiais já adotadas para o LOD: Godot 4.5,
[Mesh LOD](https://docs.godotengine.org/en/4.5/tutorials/3d/mesh_lod.html), e
Unity 6000.0,
[Terrain.heightmapPixelError](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Terrain-heightmapPixelError.html).
Não há declaração de conclusão do bloco ou de paridade com os plugins.
