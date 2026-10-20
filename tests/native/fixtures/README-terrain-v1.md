# Terrain surface v1 compatibility fixture

`terrain-surface-v1.txt` is emitted by the actual serializer at Git revision
`2750745c`, using the current model header so new fields are ignored by that
historical visitor. No v2 output is relabeled or truncated to manufacture v1.

Generator input: one layer `{7,11}`, name `Historic schema fixture`, tile sizes
3/5 metres, offsets -2/4 metres, rotation 17 degrees, saturation .7, tint
(.2,.4,.6,.8), local detail-noise scale 2 and global detail participation .3.
All other historical fields retain their existing neutral defaults.

The fixture tests actual v1 reading, neutral disabled M09/M10 defaults, retained
M01–M08 fields and a canonical v2 write/read after migration. It is metadata
only and contains no private project or image data.

The final separator space is historical writer output, retained byte-for-byte;
the fixture has a targeted Git whitespace exception and no line conversion.
