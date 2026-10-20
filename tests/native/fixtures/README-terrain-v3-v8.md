# SurfaceV3 and Recipe8 historical writer fixtures

Both files were emitted and reopened by the complete production serializers at
engine checkpoint `a3d27687354994a1408e8fa8d846a82324014f7a`, before the FX and
Flow format extensions. They are real writer output; no newer serialization was
renamed or trimmed to produce these historical fixtures.

- `terrain-surface-v3.txt`: 3194 bytes, SHA256
  `d080d9f44f5660b61f5e97a6d3ebd8f38343bef7e7d54ee5ba9b93e1a06300ff`.
- `terrain-recipe-v8.txt`: 6673 bytes, SHA256
  `b0c641e0eee4f37678fe371961835413776fba09e8e4b346215fab45378c382f`.

The generation step checked historical writer/reader/writer equality before
saving. SurfaceV4 must preserve the complete positional V3 body and initialize
Flow neutrally; Recipe9 must preserve all old geometry, paths, PBR and quality
settings while initializing independent FX neutrally. Tests also reopen the new
serialization and compare its complete authoring data. These metadata fixtures
contain no private project, image assets or credentials.
