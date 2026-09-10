# Componentes, composição física e C# — rodada autorizada

Autorização direta em 10/09/2026: **“eu autorizo”**, após o pedido de preparar a
adição ampla antes do ADB. Este registro atualiza o estado de execução dos três
blocos anteriores. Não encerra os planos V1/V2, o complemento de componentes e
código, nem o quadro de referências Unity/ItsMagic.

## Correções encontradas na integração

- Nativo: erros reais de compilação sob -Werror; adaptação dos testes de interação
  ao inspetor por instância, cabeçalhos recolhidos e páginas de propriedades.
- Restore: NuGet.Config não oferecia fonte para Roslyn; habilitada a fonte oficial
  com mapeamento restrito aos pacotes necessários. Lock do projeto de scripting.
- Compilador: DLLs nativas copiadas junto ao runtime eram tratadas como referências
  gerenciadas. A seleção agora ignora imagens sem metadata de assembly.
- Reabertura Android: o roteador Java aceitava somente arquivos até v8, embora o
  leitor nativo já gravasse v10. O roteador agora admite 1–10 e recusa futuras;
  validação de conteúdo continua no leitor nativo. Arquivos existentes preservados.
- Aplicar no aparelho: abort dentro da ponte criptográfica do .NET. Investigação
  com pilha LLDB e mapas de bibliotecas apontou ausência de OpenSSL compatível.
  Corrigido com OpenSSL 3.5.8 da fonte oficial, nomes privados, empacotamento,
  hashes e inicialização antes do runtime. Ver [dependência](../../native/third_party/openssl/README.md).
- GC: o pacote linux-bionic usa Mono/SGen sob a ABI CoreCLR. O worker do
  compilador aguardava a thread nativa cooperar com a coleta. O loop e a espera
  final pelo compilador agora delimitam a região nativa com marcador na pilha.
- Lifecycle: uma segunda NativeActivity no mesmo processo precisa associar sua
  nova thread ao domínio Mono existente antes de resolver delegates. Corrigido
  com mono_thread_attach, sem trocar o runtime ou criar outra VM.
- Diagnóstico: Scene.Log mantém o buffer do editor e pode entregar mensagens
  a um callback da plataforma; Android as registra sob a tag Astra.Script.

## Resultados host

| Camada | Resultado | Evidência local |
|---|---|---|
| Nativo completo | 767/767 aprovados, também após o callback de logs | build/validation-components-20260910/native-final-tests.log |
| Managed completo | 513/513 aprovados, zero pulados | build/validation-components-20260910/managed-tests.log |
| Shaders | geração e validação concluídas | tools/generate-embedded-shaders.ps1 -All |
| Android | Debug e Release compilados | build/validation-components-20260910/android-thread-lifecycle.log |
| Java | 16/16, zero falhas/erros/pulados | android/app/build/test-results/testDebugUnitTest/ |

Após as correções do host Android, Debug e Release foram recompilados;
`android-thread-lifecycle.log` registra o build final. Os testes host não foram
usados como substitutos dos fluxos físicos descritos abaixo.

Os testes adicionados exercitam arquivo v10 com múltiplas instâncias,
remapeamento ao duplicar, reparent com owner, undo, origem de compound assimétrico,
formas em filhos, massa/impulsos/torque, recusa de shear, quatro subpassos,
agregação Enter/Stay/Exit e as quatro juntas; motores de dobradiça/deslizante
produzem movimento. Em C#: compilação/schema, múltiplos Behaviors, propriedades,
forças/torques, exceção isolada por instância, reinício em novo Play, incompatibilidade
de schema, aplicação atômica e preservação da geração boa diante de compilação inválida.
Os dois exemplos de fisica-codigo.md foram compilados contra a API efetiva.

## Dispositivo e autoria por toque

ADB Wi-Fi, modelo 25053PC47G, arm64, pacote dev.aether.editor. Instalações com
`install -r`, sem limpar dados. Projeto exclusivo criado pela interface:
`ComponentesValidacao0910i`; projetos anteriores preservados.

Capturas em `build/validation-components-20260910/`:

- 08-creation.png e 09-cube.png: criação de objeto e malha real no viewport.
- 10-add.png e 11-components.png: Add, ícones, corpo e dois colisores separados;
  componentes adicionados aparecem recolhidos.
- 12-collider.png a 14-offset.png: abertura da segunda instância, teclado numérico
  e centro X=0,25 com wireframe deslocado. Undo/Redo e salvar pela barra.
- touch-authored.aescene: arquivo puxado do aparelho, com IDs distintos e offset
  somente no segundo colisor. Reabertura confirmada após a correção Java.
- 15-files.png e 16-code.png: navegação de arquivos e fonte C# aberta no IDE.

17-applied.png mostra o retorno ao shell provocado pela falha original; **não é
evidência de Aplicar aprovado**. Algumas capturas anteriores mostram telas de
transição e não devem ser apresentadas como provas de execução 3D.

## Cena integrada de validação

`tests/native/component_scene_fixture.cpp` é uma ferramenta host explícita,
excluída do build padrão e dos assets do produto. Gera composition.aescene e
FixtureMotion.cs numa saída indicada pelo operador. Usa a biblioteca real de
primitivas, cria chão, corpo assimétrico com duas formas próprias e uma filha,
sensor cinemático, âncora, motor de dobradiça e câmera. O mundo tem cinco corpos
e uma junta. Foi copiada somente para o projeto exclusivo e aberta por Arquivos.

Não é exemplo automático semeado em projetos novos nem evidência de execução
de scripts por ter sido gerada. Aplicar/Play, eventos e retorno após Stop estão
registrados abaixo com suas evidências no aparelho.

## Aplicar, Play e reabertura comprovados

- 25-gc-result.png: primeiro Aplicar concluído após corrigir OpenSSL e GC.
  Arquivos Project.dll, Project.pdb e schema.json publicados em `.astra/code`.
- 27-sensor.png a 29-reference-picker.png: cabeçalhos recolhidos, schema
  SensorProbe, propriedade Target mostrando Composto e seletor por nome/ID/pai.
- 30-play.png e 32-release-play.png: câmera de componente, corpo composto com
  forma filha, chão, âncora e corpo da dobradiça em execução.
- `release-script-log.txt`: Start, TriggerEnter/Exit para o corpo 3, consulta
  GetBodyVelocity e Stop. Nova execução produz novamente Start e eventos.
- 33-paused.png e 34-paused-stable.png possuem o mesmo SHA256
  `2612956174e29ef0cb8afdcd89b0bfcd5677e6e6a59f987ac12fe70a329cdaeb`.
  35-step.png mostra avanço de posição/orientação após tocar Passo, permanecendo
  em Pausado. Retomar voltou a executar e Stop restaurou a autoria.
- composition-before-play.aescene e composition-after-stop.aescene: ambos
  2.264 bytes e SHA256
  `3b6de088845840021a6bb56486d0416607948f917a5cc19e198a6316f60616d1`.
- `components-release-motion.mp4`: gravação de 20 segundos incluindo Play,
  pausa, passo, retomada e Stop. Não é benchmark de FPS.
- 43-release-apply.png e 44-release-reopened.png: Aplicar concluído antes e
  depois de voltar à lista de projetos e abrir outra NativeActivity, mantendo
  PID 29164. `reattach-second.log` mostra a nova thread associada ao Mono.
- `final-release-log.txt`, PID 29164: novo Play após a reabertura confirmou
  Start, Enter, Exit, velocidade e Stop com o APK final.

Identidade aplicada (`applied-current.txt`):
`0c78a97e5c9099193105fecd19a1f73716ea090afc6d11ab201e627ea87409c4`.

APKs finais, SHA256:

| APK | SHA256 |
|---|---|
| Debug | 4126633cf419f529dc9657404fb58dbe597ca78b178bccb927d52954a7e70a9f |
| Release | 3df026651d5f2ebc22e5ab1d03027c0f8fc6623013ae4cbe583bd633e412755a |

O arquivo de log preserva quedas **anteriores às correções**, inclusive os PIDs
7380 e 26455. Elas não devem ser confundidas com o ciclo final do PID 29164.
A captura 41-reopened-applied.png registrou ItsMagic em primeiro plano durante
uma troca de aplicativo e não é evidência Astra. Capturas 38–42 incluem tentativas
de navegação/transições; somente 43–44 comprovam a reabertura final com Aplicar.

## Limites de aceitação

Contagens host não comprovam desempenho Android, estabilidade prolongada,
todos os dispositivos ou as 270 referências do catálogo. Permanecem as lacunas
específicas do [contrato físico](../adr/ADR-REFUNDACAO-COMPOSICAO-FISICA.md):
shapes de malha/convexos, contatos sólidos, sensores com CharacterMotor, juntas
avançadas, mutação de composição em Play, AssetGUID geral, API universal completa,
IDE com debugger/hot reload e player/exportação independente.

Nesta rodada não foram exercitados no Android todos os menus de copiar/colar/
reset/remover, todos os tipos de junta, escalas extremas, projetos grandes,
teclados/dispositivos diversos, tolerância a interrupção durante escrita,
TLS/certificados, validação prolongada de memória ou desempenho. Parte desses
contratos possui cobertura host; isso é distinto de prova no aparelho.
O painel Arquivos ainda mostra `.astra`; ocultar metadados internos da navegação
normal e oferecer console com histórico/navegação de erros continuam refinamentos
pendentes. A captura de Play usa geometria simples para provar comportamento.
