# Terrain surface v2 compatibility fixture

`terrain-surface-v2.txt` is emitted by the actual serializer from revision
`03b8f10a`, using the current model header so Snow/residency additions are
ignored by that historical visitor. No V3 output is relabeled or truncated.

Generator input: the same `{7,11}` layer and historical values documented in
README-terrain-v1.md; Geo scale 5 metres and default flat curve enabled;
disabled global Tint retains scale X=-2/Y=0. Other V2 fields keep defaults.
The V3 reader must retain all these values and initialize Snow Off, terrain
residency Original and every map InheritTerrain.

The final separator space is actual historical writer output and is retained
byte-for-byte through a targeted Git whitespace exception and no line conversion.
The fixture contains metadata only, no private project or image data.
