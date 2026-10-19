# Base plana e escultura suave — 19/10/2026

Correção solicitada pelo proprietário após teste do 107: início moldado e elevação pontuda.
Continuação dos 71 itens, sem fechamento do primeiro bloco.

## Implementação

- Somente criação nova no editor: amplitude zero, 64 células em 64 m, base sólida.
- Pincel inicial: raio 8 m, força 0,25 por amostra, dureza zero, espaçamento 0,2.
  Novo terreno reinicia o pincel transitório; reabrir preserva a receita salva.
- Slider de força quadrático para dar precisão ao toque em valores baixos;
  intervalo 0,05–8 e entrada numérica preservados. Elevar/Baixar mostram
  “Altura por amostra (m)”. Não houve alteração do kernel nem dos carimbos salvos.
- Colinas/Montanhas/Ilha permanecem. Células por lado continua ajustável;
  dobrar resolução por eixo tem custo, não é otimização gratuita. Assets intactos.

## Referências estudadas

- Unity 6000.0, Raise or Lower Terrain:
  https://docs.unity.com/en-us/engine/6000.0/manual/creating-environments/script-terrain/terrain-tools/terrain-raise-lower-terrain
- MapMagic 2 Bundle 2.1.11, fontes locais `Assets/MapMagic/Brush/Core/Preset.cs`
  (raio/dureza/espaçamento independentes) e `Trace.cs` (carimbos por distância).
  Os valores da Astra são próprios; não há alegação de port completo do grafo.

## Validação

- 59/59 testes integrados, `build/terrain-evidence/flat-soft-20261019.log`.
- Novo cenário: campo inicial exatamente plano, centro elevado suavemente,
  ombro com 35–65% da altura central a meio raio, exterior intocado, preset
  Montanhas disponível e receita idêntica ao reabrir.
- UI executável em software: `ui108-1280-brush.png`, raio e força visíveis;
  não substitui captura do renderer Android. Nenhum conceito de imagem gerado.
- APK 108 compilado, assinatura/identidade/conteúdo auditados. Sem certificado
  agregado ELF16K, como 107. `build/artifacts/Astra-Terreno-108-arm64.apk`.
  SHA256 `0aae0fcc7dda3316032387be0d94bcb5c50eb03271c17c3155fc1ca1823b6820`.
- Aparelho saiu da Astra durante preparação; solicitada disponibilidade para
  testes sem interferir no uso do proprietário. Instalação/teste 108 pendentes.

## Publicação desta revisão

- Engine: `c0129bcc`, publicada em `codex/terrain-voxel-generator`, sem merge na main.
- AstraDocs: `8145b8e`, build e 594.215 links verificados sem erros.
- Vercel: `dpl_6SZsND31K4ik6qh9GMnmmjK8BCuU`, estado READY, domínio
  `astraengine.com.br`. Nota `2026-10-19-terreno-plano-pincel-suave` publicada
  como desenvolvimento, com validação Android pendente.

## Escopo ainda aberto

Não é qualidade AAA, erosão, composição de biomas ou biblioteca PBR concluída.
Raios pequenos e malhas pouco densas ainda podem gerar facetas. D09 permanece
prioritário para a saturação do projeto antigo; D07, materiais e demais itens
continuam conforme o aceite. Não reduzir propriedades ou assets para contornar.
