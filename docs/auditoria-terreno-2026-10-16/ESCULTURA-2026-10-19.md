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

### Validação Android posterior à autorização

APK 108 instalado por atualização no POCO F7. Projeto isolado `TerrainQA108`:
criação plana com base sólida, raio 8 e altura por amostra 0,25 conferidos;
dois gestos produziram 33 operações. Capturas `device108-created.png`,
`device108-held.png` e `device108-hill-held.png` mostram a base e a prévia
texturizada antes de soltar. Desfazer voltou a 9 operações; Refazer restaurou
o segundo gesto. Salvar, reiniciar o processo, selecionar a raiz e tocar
Editar terreno reabriu a receita com 33 operações e o relevo preservado.
`device108-reedit.png` mostra a elevação com topo arredondado.

O teste encontrou um defeito: reiniciar o editor restabelecia raio 4/força 1
ao abrir terreno existente. A inicialização do estado do editor foi alinhada
a raio 8/força 0,25, sem alterar o formato ou os carimbos salvos. O teste host
de escultura agora cobre também o estado de uma aplicação recém-iniciada.
Validação dessa correção registrada abaixo após o novo APK.

### Correção e aceite específico do APK 109

- Código `e8cc94fa`; 59/59 testes host passaram, incluindo sessão nova.
- APK 109 / `0.3.1-dev.terrain.20261019.4`, compilado e auditado, instalado
  por atualização. SHA256 `8b99546abccbc12ebd25ae5944af79d1cf54bd833e60499fc9abb1c7debefeae`.
- Após nova inicialização e Editar terreno, `device109-reopen-defaults.png`
  confirmou raio 8 e altura por amostra 0,25. Receita carregada com 1034
  operações: o proprietário confirmou ter editado o projeto durante o build;
  esse total e o buraco visível não pertencem aos gestos automatizados acima.
- Nenhum gesto novo foi aplicado sobre essas edições na conferência 109.
  O teste específico valida inicialização/reedição, não repete todo o roteiro 108.
- `device109-reopen-logcat.log`: sem FATAL EXCEPTION/Fatal signal.
- Nota nova `2026-10-19-terreno-pincel-reinicio`, AstraDocs `b409b13`:
  build e 594.217 links sem erros; Vercel `dpl_C4kLR8rZJPZTHfbTS5f1PdY39Zhw`
  READY, nota e versão 109 confirmadas em `astraengine.com.br/atualizacoes/`.
- Capturas/logs permanecem locais em `build/terrain-evidence`; APK em
  `build/artifacts/Astra-Terreno-109-arm64.apk`. Limite ELF16K anterior permanece.

No log do processo 108 não houve FATAL EXCEPTION/Fatal signal. Há mensagens
de erro do framework/vendor; não se afirma log inteiramente limpo. A textura
continua repetitiva e o raio/força exigem rolagem na seção Ferramentas;
esta revisão não representa aceite global dos materiais ou de toda a UI.

Não é qualidade AAA, erosão, composição de biomas ou biblioteca PBR concluída.
Raios pequenos e malhas pouco densas ainda podem gerar facetas. D09 permanece
prioritário para a saturação do projeto antigo; D07, materiais e demais itens
continuam conforme o aceite. Não reduzir propriedades ou assets para contornar.
