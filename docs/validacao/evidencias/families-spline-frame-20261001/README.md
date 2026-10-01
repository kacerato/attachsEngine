# Frame autorado de spline — 2026-10-01

`native-final.log`: 13/13 cenários, incluindo Play/ABI, frame mundial, offset, up/roll mutáveis, migração v1/v2, emenda, identidade, Undo/Redo, arquivo, input e igualdade da seta de viewport com o consumidor sob escala não uniforme. O cenário C# em `managed-transport-final.log` passou 1/1; seu recorder é apenas prova do transporte SDK, não do runtime nativo. Fachada gerada confere com o schema no alvo nativo.

`tangents-before.png` é captura anterior; `concept-orientation.png` é hipótese visual gerada. `orientation-final.png` é UI executável host 853×394 revisada; `ui-final.log` registra zero descartes, fonte ausente e recortes. O ícone orientation é SVG/raster integrado ao atlas real (252 entradas).

O APK é aceite somente após BUILD SUCCESSFUL em `android-build.log` e a conferência `package-manifest.json`, com ABI35, SHA-256, fontes e igualdade dos assets gerados/empacotados. Não houve instalação ou execução física Android. Layout raster host não substitui Vulkan/aparelho.
