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
