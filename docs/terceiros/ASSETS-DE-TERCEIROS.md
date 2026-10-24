# Assets e código de terceiros

Código, shaders e assets portados de pacotes de terceiros para a Astra. Cada
arquivo portado também cita a origem num comentário. Os pacotes originais
(.unitypackage, pastas inteiras) não entram no repositório, e nada disto vai
para o AstraDocs nem para páginas públicas.

| Pacote | Versão | Titular da licença | Arquivos portados (origem → Astra) |
|---|---|---|---|
| UniStorm — Volumetric Clouds Sky, Modular Weather and Cloud Shadows (Black Horizon Studios) | 5.3.1 | Dono do projeto (licença company) | `Resources/Clouds/cloudsInclude.cginc` (ambiente topo/base de `renderCloudsInternal`) → `native/rhi/shaders/dirt_road_sky.frag`. O perfil de altura de `cloudMapBase`/`cloudGradient` foi usado na v3 e substituído na v4 pelo cúmulo do HZD. Nenhuma textura ou volume do pacote: os ruídos são gerados por `native/renderer/cloud_noise.cpp`. |
