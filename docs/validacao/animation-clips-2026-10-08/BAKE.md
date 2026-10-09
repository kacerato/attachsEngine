# Bake e redução de trilhas — revisão B4

Estado em 09/10: backend, faixa redesenhada e SDK validados no host e no aparelho.
Dev code 21 aceito por toque, reprodução, API, histórico e reabertura; 477/477
quadros revisados. O histórico de 08/10 abaixo registra o bloqueio então existente.
Isto fecha o recorte de bake por trilha, não B4 inteiro.

## Cadeia funcional

Trilha tipada AECLIP 2 → curvas/extremos → sampler real compartilhado → grade
inicial e refinamento → redução → reconstrução de rotação progressiva →
verificação de pose → transação revisionada → journal/registro/biblioteca →
preview e runtime. O SDK usa um rascunho isolado e só publica no Commit; a
interface usa a mesma operação nativa e um passo no histórico.

Os IDs existentes no mesmo componente/tempo são preservados. Chaves novas
consomem o alocador; Undo restaura curvas, tangentes, bindings e metadados,
mantendo revisão e fronteira de IDs monotônicas. Uma recusa não publica parte
do resultado. Recurso fonte, hash de origem e demais trilhas ficam intactos.

Taxa de 1 a 240 amostras/s é a grade inicial, não quantidade final. Extremos de
Bezier, chaves originais, descontinuidades e refinamento entram antes da
redução. Degraus conservam tempos representáveis adjacentes. Rotação Euler
limita o percurso angular entre amostras para evitar aliasing de voltas
completas. Redução considera erro de pose do sampler real; o relatório traz
chaves antes/depois, poses da grade, pontos verificados e erro máximo medido.
Verificação por amostras não é prova matemática de erro em todo tempo contínuo.

Há limites reais de poses, avaliações, comparações, profundidade e IDs;
cancelamento do backend ou falha preservam o recurso. A operação de UI e o
comando C# ainda são síncronos, sem job interativo com progresso/cancelamento.
Não alegar desempenho de clipes grandes sem medir esse caso.

## Interface e proposta

NÃO IREI SER SIMPLISTA NO DESIGN.

A primeira captura executável mostrou uma folha vertical cobrindo a cena e
parte do gráfico. `bake-design-concept.png` é uma hipótese gerada com imagegen,
usando `clip-bake-result.png` como referência. Seu mecanismo 3D e seus números
são ilustração; não são asset disponível nem resultado medido da Astra.

Decisão aplicada: a edição do bake substitui temporariamente o transporte
inferior. Taxa, tolerância, formato e redução ficam numa faixa; resultado,
Aplicar e fechar, na segunda. A curva e a cena permanecem sem folha sobreposta.
Propriedades de chave/seleção cedem essa região à operação atual. Fechar
retorna ao transporte, mantendo trilha e tempo. Undo/Redo invalidam o relatório
da revisão anterior. A captura posterior da UI executável foi examinada nos
tamanhos 853×394 e 655×300: todos os ajustes ficam acessíveis, com gráfico e
área de cena livres da folha. A área de cena vazia dessa rasterização não é
prova de renderer Vulkan ou de aparelho.

Ícones `animation/bake` e `animation/reduce` são fontes SVG autoradas no
gerador existente, rasterizadas em PNG e integradas ao enum/atlas binário de
produção. A folha conceitual não substitui esses recursos executáveis.

## API e compatibilidade

ABI 2: prefixo ABI 1 preservado; tabela 176 bytes em 64 bits, ajustes 32 bytes,
relatório 24 bytes. O SDK novo aceita uma tabela ABI 1 com 168 bytes sem ler o
campo novo. `ClipEdit.Bake` recusa um host antigo. API e APK precisam distribuir
SDK e tabela compatíveis juntos.

Preservar formato, Euler → quaternion/progressivo e quaternion/progressivo →
quaternion/progressivo usam conversão explícita. Quaternion/progressivo →
Euler continua recusado: ainda falta escolha de ramo/eixos e continuidade.
Bake FK/IK, root motion, retargeting e exportação de Unity não são comprovados
por esta implementação. Eventos, camadas de autoria, drivers, mirror/auto-key
e reimportação com merge permanecem na cadeia B4–B6.

## Evidências disponíveis antes da revisão visual final

- Backend: os dois cenários novos passam no conjunto de 18 cenários de recurso,
  incluindo curvas ponderadas/overshoot, degraus, cancelamento/orçamento,
  comparação independente em 4001 tempos e Euler de 720° em 1001 tempos.
- SDK C#: compilação Release sem avisos/erros; integração com a sessão nativa,
  publicação, história e sampler passa, incluindo leitura da tabela ABI 1.
- A primeira verificação de UI encontrou uma expectativa de teste incorreta:
  comparava serialização inteira depois de Undo, apesar das revisões/IDs
  monotônicos exigidos pelo contrato. Corrigida para comparar curvas/bindings
  preservando e verificando esses dois campos. Nova execução: 27/27 cenários
  passaram, incluindo bake por roteamento real, configuração, Undo/Redo e
  invalidação do relatório. Não classificar a falha como correção de runtime.
- O novo link falhou por falta de espaço. Compactação NTFS do cache do host
  recuperou cerca de 2,3 GB: 3724837787 bytes lógicos armazenados em 1388936036.
  Fontes e dados não foram apagados. O link foi refeito com sucesso; não usar
  o archive incompleto da tentativa sem espaço como evidência válida.
- Aceite físico SDK do code 20 está no REPORT: 474/474 quadros revisados,
  editar tempo de pose, salvar e reabrir, fonte GLB intacta. Não prova o bake.

## Pacote e instalação code 21

Dev `0.2.9-dev.bake.20261008.5`, `dev.aether.editor.u07`, ARM64 Release.
Build Android: 7m18s, 53 tarefas (27 executadas). APK de 104445346 bytes,
SHA-256 `0769ac1f2d2d329a7e69c2d8f9d7b6d8bc682616a7fdc2155e75954cd6c7c57c`.
Instalação com atualização foi aceita; versão/code foram lidos no package
manager, e o hash do base.apk instalado é igual ao do pacote local. O APK Dev
é local; a publicação editorial não representa distribuição desse APK.

O arquivo existente `Clipes/Clipe 2.aeclip` conserva o hash
`c357a277954b2c90d0a31f1562bae058a56f67400109ae5466c5f69d9686b315`
antes/depois da instalação. Isso prova preservação dessa instalação, não
aceite de bake ou reabertura da revisão 21. Em 08/10 a sonda `BakeDuasVoltas.cs`
estava apenas no projeto isolado AnimatorClips-20261008, ainda sem execução.
Ela cria 61 poses Euler num objeto genérico, converte duas voltas para quaternion,
exige redução e relatório dentro da tolerância. Em 09/10 foi publicada pelo
compilador do aparelho e executada pelo menu; o recurso e o aceite estão abaixo.

Em 08/10 a tela bloqueada impediu o aceite. As capturas de bloqueio/notificações
particulares não integram as evidências publicáveis. Em 09/10 o aparelho foi
encontrado desbloqueado, ADB transport 107, e o aceite abaixo foi executado.

## Aceite físico code 21 — 09/10/2026

POCO F7, projeto isolado AnimatorClips-20261008, Mecanismo A e B, sem jogador,
Behavior ou Play para autoria. Package manager confirmou code 21 e versão
0.2.9-dev.bake.20261008.5. Nenhuma recompilação/reinstalação foi necessária
neste aceite: o APK validado é o pacote já identificado acima.

1. Abrir Giro do objeto, selecionar rotação Y, Edição → Bake e redução.
   Alterar amostragem de 60 para 24 e converter Quaternion, depois Progressivo.
   A UI publicou 12 → 15 chaves, 49 poses, 193 pontos; erro exibido 9,639e-6°.
2. Undo restaurou curvas/bindings Quaternion; Redo restaurou Progressivo.
   Arquivos puxados confirmam igualdade dessas estruturas, revisão crescente
   e fronteira de IDs monotônica. O relatório antigo sumiu nos dois casos.
3. Fechar a faixa e reproduzir. Gravação UI: 240/240 quadros examinados em
   oito folhas; movimento de ida/volta no A e B fixo. Salvar, encerrar processo,
   reabrir e conferir o mesmo recurso. Primeiro round-trip byte idêntico,
   SHA-256 9faa0af401c088516ab5ba187bf32257cfc79cfb3aa89630e6e38ec70c418f5a.
4. No IDE, Recompilar projeto publicou a sonda BakeDuasVoltas.cs. Catálogo
   exibiu o comando; executar criou Duas voltas · Bake SDK. Seus guards reais
   passaram antes de SetName/Commit: mais de cinco poses amostradas, pontos
   verificados, erro ≤ 0,01° e menos chaves que a entrada. O recurso publicado
   contém oito poses Quaternion / 32 chaves escalares, ante 61 poses Euler /
   183 chaves escalares autoradas. Não inferir o número exato de pontos do
   relatório SDK: a sonda não o persistiu.
5. Reprodução SDK: 237/237 quadros examinados em oito folhas, com duas voltas
   por clipe de dois segundos e B fixo. Salvar/encerrar/reabrir manteve arquivo
   byte idêntico, SHA-256 d2c3e10778d58e7676992b87cc2fe2aaf0cd2867eac32cfbe1a63fb4891874c6.
6. Conferir o toggle Redução no Giro progressivo, taxa padrão 60 após reabrir:
   desligado 15 → 605 chaves; ligado 605 → 15. Ambos 121 poses / 481 pontos,
   erro exibido 9,604e-6°. Novo salvar/encerrar/reabrir manteve o resultado
   reduzido byte idêntico, SHA-256 7043432ffa941612a2d79b3e82aa464a25f70b1e8ab8bb99dc4061ee2bb64320.

Fonte Mechanism.glb permaneceu com SHA-256
ca06483178d84b27428605065b3709e41dc70e3756c540f2df589d386b6db86b.
Capturas reais antes/depois, denso/reduzido, histórico, catálogo SDK e reabertura
foram examinadas. A faixa é legível, mantém viewport/curvas e controles dentro
da tela; não há folha sobre o mecanismo. Configuração de bake é estado da
ferramenta e retorna ao padrão após reabrir; o recurso convertido é persistente.

Vídeos device-code21-ui-preview.mp4 e device-code21-sdk-preview.mp4: **477/477
quadros**, 16 folhas completas, hashes RGB24 e PTS, zero quadros omitidos.
Revisão humana registrada separadamente da extração em code21-ui-review e
code21-sdk-review. As folhas mostram ROI dos dois mecanismos; screenshots
completas comprovam o contexto de UI. Não é medição de FPS, latência, temperatura
ou garantia matemática de erro contínuo. Não prova skin, retargeting, IK ou
biblioteca de assets dos pacotes.

O arquivo device-code21-acceptance.json registra os checks estruturais e hashes.
O recorte de bake por trilha/UI/API tem aceite físico; **B4 continua aberto**
nos limites descritos neste documento e no plano. Publicação do guia/nota
de desenvolvimento no AstraDocs é registrada separadamente do APK público.

## Publicação editorial — 09/10/2026

AstraDocs main publicado no commit `332e3e5fc3417c4d4c65836675cd76132144acde`,
confirmado no remoto. Vercel `dpl_4QrffurHV18w8iCjQpeeVyUNSGrx`, projeto
astra-docs/equipe lucas-df5f8b19, produção READY e alias astraengine.com.br.
Build gerou HTML/Markdown/JSON; check:links conferiu 951 páginas, 218854 links,
zero erros. Guia e imagem real conferidos no domínio, navegação de Atualizações
para o guia e detalhes/migração funcionando; leitura móvel de 390×844 sem
overflow horizontal (conteúdo 375 px). Imagem carregada em 2772×1280; Markdown
HTTP 200 e feed JSON contendo a nota. Sem publicar pacotes, fontes privadas ou APK.

Guia: https://astraengine.com.br/pt-br/snapshot-2026-10-07/sistemas/animation-clips/

Nota: https://astraengine.com.br/atualizacoes/#2026-10-09-animation-clips-bake-sdk

O download público permanece 0.2.3. O aceite físico refere-se ao Dev code 21
instalado localmente. Publicação editorial não fecha B4/B5/B6 nem distribui o Dev.

## Referências concretas

- UMotion Pro 1.29p04: `ClipSettings.html`, `RotationModes.html`, exportação e
  propriedades de compressão do manual fornecido. Estudo de precisão e
  workflow; o núcleo DLL Unity não foi executado como backend Android.
- Unity 6000.0:
  https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AnimationUtility.GetEditorCurve.html
- Godot 4.5:
  https://docs.godotengine.org/en/4.5/classes/class_animation.html#class-animation-method-optimize
  e fonte 4.5-stable `scene/resources/animation.cpp`.

Princípio extraído: separar curva autorada da representação otimizada e
controlar redução pela precisão. A adaptação usa formatos, convenção Euler,
histórico, propriedade de pose e sampler da Astra, sem copiar a UI das engines.
