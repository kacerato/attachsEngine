# Biblioteca de terreno fornecida

Catálogo preparado para integração; a presença dos arquivos não significa que o renderer já consome todos os materiais.

- `sources/`: originais sem redução, identificados por SHA-256.
- `textures/`: conversões PNG sem perda apenas para fontes PSD/TIFF/TGA. Os PNGs originais são usados diretamente.
- `thumbnails/`: imagens pequenas apenas para seleção na interface; nunca usadas como textura do terreno.
- `catalog.json`: propriedades de origem, canais resolvidos, variantes de cluster, referências ausentes e caminhos efetivos. Empacotar somente os caminhos referenciados pelo catálogo.
- `profiles/`: derivados opcionais, somente quando solicitados com `--profile`. O perfil padrão é `original`.

Preparação: `tools/prepare-terrain-library.py`; contratos de canais: `tools/terrain_material_contracts.py`. A configuração global de canais MicroSplat tem precedência sobre a configuração da camada, exceto no modo Custom. O mask map de TerrainLayer usa R=metallic, G=AO, B=altura, A=smoothness (Unity 6000.0 Terrain Layers).

Oito referências de mask map do MapMagic não foram encontradas nos pacotes fornecidos; estão registradas em `unresolved_dependencies`.
