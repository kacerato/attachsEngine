# Assets e código de terceiros

Código, shaders e assets portados de pacotes de terceiros para a Astra. Cada
arquivo portado também cita a origem num comentário. Os pacotes originais
(.unitypackage, pastas inteiras) não entram no repositório, e nada disto vai
para o AstraDocs nem para páginas públicas.

| Pacote | Versão | Titular da licença | Arquivos portados (origem → Astra) |
|---|---|---|---|
| UniStorm — Volumetric Clouds Sky, Modular Weather and Cloud Shadows (Black Horizon Studios) | 5.3.1 | Dono do projeto (licença company) | `Resources/Clouds/cloudsInclude.cginc` (ambiente topo/base de `renderCloudsInternal`) → `native/rhi/shaders/dirt_road_sky.frag`. O perfil de altura de `cloudMapBase`/`cloudGradient` foi usado na v3 e substituído na v4 pelo cúmulo do HZD. Valores dos gradientes `CloudLightColor`/`CloudBaseColor` do prefab `UniStorm VR System` orientaram a base fria das nuvens na hora dourada (sem copiar o gradiente). Nenhuma textura ou volume do pacote: os ruídos são gerados por `native/renderer/cloud_noise.cpp`. |
| Enviro 3 — Sky and Weather (Hendrik Haupt) | 3.2.1a | Dono do projeto (licença company) | Estudado, nada copiado: a curva `sceneExposure` de `Default Lighting Preset.asset` (EV100 por hora solar) orientou o limite de exposição das variantes de hora dourada e pôr do sol na validação do G4d (`docs/validacao/g4-nuvens-curvas-2026-10-21/README.md`). |
