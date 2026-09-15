# R4 — gerenciador de texturas, perfil por textura e publicação incremental (15/09/2026)

Projeto M08Recursos0913k.

| Arquivo | APK | O que mostra |
|---|---|---|
| `01-gerenciador-19-texturas.png` | `8F52C91B…DB0D` (commit `c10ba36b`) | Arquivos > Texturas abre em Propriedades a grade com as 19 texturas, busca e seis filtros, 3 páginas |
| `02-filtro-sem-alfa-16.png` | idem | "Sem alfa" lista 16: a `imagem-1.png`, que tem transparência, sai |
| `03-textura-em-propriedades-perfil-usuarios.png` | idem | `imagem-1.png` em Propriedades: visualizador, perfil (interpretação, tamanho, mipmaps, bordas, anisotropia), "Na GPU: não usada" e "Usuários (0)" |
| `04-perfil-mipmaps-nao-publicacao-incremental.png` | `0F7D8E4D…5D2D` (commit `c3703e19`) | "Mipmaps: não" aplicado; o log dessa republicação: `[Import] texturas importadas: 122 publicadas (122 reaproveitadas, 0 enviadas), 122 slots bindless; geometria reaproveitada.` |

Logs filtrados pela tag do editor na mesma sessão:
- abertura: `[Import] texturas importadas: 122 publicadas (0 reaproveitadas, 122 enviadas) … geometria enviada.`
- política antes da correção de `device.cpp`: `aniso=1.0` e `textures.samplerAnisotropy reduzido por capability.`
- depois da correção (APK `1AD975F7…FCEA`, commit `c60f34dc`): `[RenderPolicy] … aniso=8.0 … clamps=3` e `[Textures] anisotropia de material=8.0`.

Depois da conferência, o perfil da `imagem-1.png` voltou a "Mipmaps: sim".
