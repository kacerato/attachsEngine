# G4: sol, lua e estrelas no céu (19/10/2026)

Pedido do proprietário: "o sol que tem hoje nem sol parece, e a lua também, se é que
dá para dizer que tem".

## O que mudou

- **Sol** (`rhi/shaders/sky_celestial.glsl`): disco com escurecimento de limbo por canal,
  pela lei aproximada I(μ) = 1 − u(1 − μ) (Neckel & Labs 1994), com u maior no azul.
  Em volta, uma auréola em HDR: núcleo de poucos raios solares e uma cauda estreita.
  Vale nos céus procedural e físico; substitui o disco chapado e a corona em `cos^256`.
- **Lua**: esfera de albedo baixo iluminada pela direção real do sol, então a fase sai da
  geometria. Tem mares procedurais, luz cinérea e halo proporcional à parte acesa. A
  direção sai da do sol e da fase (oposta na cheia), então quem move o sol move a lua.
- **Estrelas**: campo procedural em células na direção, com brilho em lei de potência,
  cor por temperatura e cintilação lenta. Só aparecem com o sol abaixo do horizonte e
  com o céu escuro.
- **Environment v14**: `sun_glare`, `moon_phase`, `moon_size`, `moon_intensity` e
  `star_intensity`, no grupo "Sol e lua" do Inspector e na API C#. Cenas v13 recebem os
  padrões novos. Perfil de ambiente v10.

## Validado

- Host: `aether_environment_authoring_tests` 30/30. Os 4 novos de céu (validação,
  migração v13→v14, perfil v9→v10, mistura de volumes) passam, e foram corrigidos dois
  testes antigos de migração que não excluíam os campos da gradação (v13) nem os do céu.
- POCO F7, Astra Dev 106 e 107, Vila em edição com câmera fixa
  (`aether.camera_*` + `aether.profile_frames`):
  - `01-vila-edicao.png`: a cena abre normal;
  - `02-sol-viewport.png`: primeira versão; o halo largo (`cos^6`) lavava meio céu de bege;
  - `03-sol-calibrado-viewport.png`: halo fechado (metade em ~3°), céu azul de volta,
    disco legível.

## Não validado ainda

- Lua e estrelas no aparelho: a Vila é de dia e a lua cheia fica oposta ao sol, abaixo
  do horizonte. Precisa de cena noturna ou de mudar a fase pelo Inspector.
- Inspector "Sol e lua" sem captura. Os campos são os da tabela genérica, sem a prévia
  em cartões que o AGENTS.md pede.
