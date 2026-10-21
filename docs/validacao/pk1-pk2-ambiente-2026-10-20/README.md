# PK-1 + PK-2 — Iluminação, Skybox, luz ambiente, neblina e Criar › Volume (20–21/10/2026)

Branch `claude/laughing-morse-57f44a` (base `9fbbbe57`). Auditoria: `docs/planos/auditoria-full-unity-2026-10-19.md`,
itens A-01..A-06, A-08, V-03, E-01.

## Como funciona na Unity 6 e o que a Astra reproduz

| Item | Na prática, na Unity 6 | Na Astra agora |
|---|---|---|
| A-01 Exposure do céu | O Skybox/Procedural tem `Exposure` (1,3). Mudar a intensidade da luz direcional não clareia céu nem ambiente. | `skyScale` removido. Céu procedural: **Exposição** (1,3). HDRI: **Exposição** em EV. A intensidade do sol só ilumina. |
| A-02 Environment Lighting | Lighting › Environment Lighting › Source = Skybox, Gradient ou Color, + Intensity Multiplier. Skybox vira uma sonda SH. | **Luz ambiente › Fonte** = Céu (Skybox), Gradiente (Céu/Equador/Chão) ou Cor, + **Intensidade**. As três fontes viram a MESMA sonda SH L2 que o shader avalia por normal (antes: média plana 0,4 céu + 0,6 equador). Reflexos: **Intensidade dos reflexos**. |
| A-03 Fog | Lighting › Other Settings › Fog: Color, Mode (Linear/Exponential/Exponential Squared), Density ou Start/End. A cor é a do autor. | **Neblina**: Modo Linear (Início/Fim) / Exponencial / Exponencial ao quadrado, Densidade, **Cor da neblina** = Cor (padrão) ou Acompanhar o céu (comportamento antigo, explícito). |
| A-04 Multiplicadores da raiz | Não existe "sol × N" global. | "Sol potência ×", "Ambiente ×", "Exposição ×" e "Rotação do céu" saíram da UI e do renderer; a migração leva cada um ao campo dono. |
| A-05 Cena sem Ambiente | Toda cena tem Default-Skybox procedural, ambiente do Skybox e Volume padrão. | Sem Ambiente, editor **e** Play usam o mesmo padrão (céu procedural, ambiente do céu, sem neblina). A janela Iluminação mostra o padrão e **Personalizar** cria o Ambiente da cena na raiz (Volume global, prioridade −1000). |
| A-06 Skybox Material | Skybox Material (Procedural/Panoramic/Cubemap) escolhido na janela Lighting; editar o material muda todo uso. | **Material do céu** (`.skybox`, recurso `SkyboxMaterial`): shader Procedural, Panorâmico (HDRI) ou Físico. Escolhido no Ambiente (janela Iluminação); "Novo material do céu" nasce do céu atual; o runtime lê o recurso a cada coleta (compartilhado, não copiado). Com material, os campos de céu da instância ficam só de leitura. |
| A-08 Sun / Sun Size / Convergence | Sun = None/Simple/High Quality; Sun Size 0,04; Sun Size Convergence 5; Atmosphere Thickness 1. | **Sol** Nenhum/Simples/Alta qualidade, **Tamanho do sol**, **Convergência do sol**, **Espessura da atmosfera** (0–5), com as fórmulas do shader da Unity (`calcSunAttenuation`, fase de Mie focada). |
| V-03/E-01 Criar › Volume | GameObject › Volume › Global/Box/Sphere; Volume novo com perfil vazio não muda nada. | Nova categoria **Criar › Volume e céu**: Volume global, em caixa, em esfera (nascem com todos os grupos desligados, prioridade 1), **Ambiente** (volume global completo) e Volume de nuvens. |

## Referência Unity (frames de vídeo, `referencia-unity/`)

| Arquivo | Vídeo | Tempo | O que mostra |
|---|---|---|---|
| u01-lighting-window-environment | https://www.youtube.com/watch?v=NOv31HSqs6U | 0:44 | Lighting › Environment: Skybox Material, Sun Source, Environment Lighting (Source, Intensity Multiplier), Reflections |
| u02-skybox-procedural-material | https://www.youtube.com/watch?v=NOv31HSqs6U | 0:09 | Skybox/Procedural: Sun High Quality, Sun Size 0,04, Convergence 5, Atmosphere Thickness 1, Sky Tint, Ground, Exposure 1,3 |
| u03-ceu-procedural-padrao | https://www.youtube.com/watch?v=YrvhyTnqT8k | 0:53 | Céu padrão do URP numa cena nova + Global Volume |
| u04/u05/u06/u07 neblina | https://www.youtube.com/watch?v=RBI9hQWD_xY | 0:29, 0:31, 0:35, 0:51 | Sem neblina; Fog ligada (Exponential Squared, densidade 0,04); cor vermelha; cena com árvores |
| u08-volume-global-modo | https://www.youtube.com/watch?v=YrvhyTnqT8k | 1:29 | Volume: Mode Global/Local, Weight, Priority, Profile |
| u09-criar-volume-menu | https://www.youtube.com/watch?v=YrvhyTnqT8k | 0:37 | GameObject › Volume |
| u10/u11 panorâmico | https://www.youtube.com/watch?v=arGL_eCqHFQ | 0:02, 1:57 | Skybox Panoramic (HDRI) e o material |

As pranchas lado a lado (`prancha-*.png`) e o julgamento estão na seção de validação no aparelho.

## Migração

- Ambiente `astra.render.environment` **v16 → v17**: 4 enums + GUID do Material do céu depois de `override_clouds`, 16 números no fim.
  Cena antiga: espessura 1, disco High Quality se havia disco, ambiente do céu, neblina exponencial que acompanha o céu.
- Documento (`runtime/environment_migration.cpp`, ao abrir): com sol fotométrico, a escala antiga `sol/2` vai para
  Exposição do céu (1,3 × escala) e EV do HDRI (+log2 escala); "Ambiente ×" → Intensidade; "Exposição ×" → +log2 EV;
  "Rotação do céu" → rotação do HDRI; a raiz volta ao neutro. Idempotente.
- Perfil de ambiente **v11 → v12** (21 palavras no fim); cabeçalho `ASTRA_SKYBOX_MATERIAL 12` para o material do céu.
- Formato da cena (`AETHER_EDITOR 17`) não mudou.

## Limites declarados

- Reflexos do céu procedural ainda amostram o mapa de ambiente padrão (cinza de autoria ou o HDRI do pacote): a
  captura do céu numa sonda é do PK-6 (Reflection Probe).
- Gradiente/Cor não têm cor HDR: a intensidade vai até 8×; cena fotométrica (sol em lux) deve usar a fonte Céu.
- Sem Cubemap de 6 faces nem Skybox por câmera; Sun Source continua no componente Iluminação da cena (PK-3).

## Validação

### Host
- `aether_environment_authoring_tests`: 53/53. `aether_tests`: 1610/1610. API C# conferida (`--check-api`).
- Capturas da UI no host (`ui-host/`, `aether_ui_preview`): janela Iluminação sem Ambiente (padrão + Personalizar),
  grupos Céu / Luz ambiente (Gradiente) / Neblina (Linear) e Criar › Volume e céu.

### POCO F7 (Astra Dev 220, Vila Astra, 21/10 11:15–11:40)
| Arquivo | O que mostra | Julgamento |
|---|---|---|
| `prancha-01-ceu-procedural.png` | Vila com Shader do céu = Procedural, Play × céu padrão URP | Mesma forma: zênite azul escuro clareando até um horizonte quase branco, chão marrom-acinzentado. O da Astra sai ~1 EV mais claro e mais saturado no zênite (95,134,195 × 83,104,136); a cena da Unity tem Tonemapping/Bloom no Volume, e a Vila tem a própria exposição. Não recalibrei a exposição interna 2,4 (decisão do G4) — fica registrado. |
| `prancha-02-neblina-branca.png` | Neblina branca densa × Fog Exponential Squared 0,04 | Proporção parecida: casas ao fundo somem primeiro, primeiro plano limpo. |
| `prancha-03-neblina-vermelha-antes.png` | Neblina vermelha × Unity | **Divergência encontrada**: na Unity o Skybox não recebe Fog; na Astra o céu ficava vermelho. Corrigido em `eaf0818d`: no Modo de cor = Cor o céu fica limpo; "Acompanhar o céu" (cenas antigas) mantém o visual antigo. Validado no host; a captura no aparelho com o 221/240 não foi possível (ver abaixo). |
| `prancha-04-janela-iluminacao.png` | Janela Lighting × janela Iluminação no POCO | Mesma hierarquia: Material do céu no topo, depois o shader; Luz ambiente e Neblina em abas. A Astra pagina o cartão focado (1/2) — padrão atual do Inspector focado. |
| `prancha-05-criar-volume.png` | GameObject › Volume × Criar › Volume e céu | Global/Caixa/Esfera + Ambiente e Nuvens numa categoria só, com ícones novos. |
| `06-vila-antiga-220-x-135.png` | Vila original (céu físico) no 220 (cima) × APK 135 (baixo) | Mesma imagem; a sombra fica levemente mais clara (luz ambiente do céu físico agora vem da irradiância integrada). |
| `07-cena-sem-ambiente-padrao.png` | Projeto sem Ambiente em Play no 220 | Céu procedural padrão, como a cena nova da Unity (A-05). |

Vila devolvida a cada troca (md5 `2e2e996841f1f0eb5af1572c68dec3fc` conferido no aparelho).

### Não testado no aparelho
- A neblina com o céu limpo (correção do `eaf0818d`) e a troca de opções pela janela Iluminação (o 220 ignorava o toque
  nos seletores fora do workspace Cena — corrigido e coberto por teste de host). O POCO ficou disputado: o 221 e o 240
  foram substituídos por APKs do PK-9 (230/232) segundos depois da instalação.
- Material do céu criado no aparelho, Gradiente/Cor no aparelho, Criar › Volume no aparelho.
