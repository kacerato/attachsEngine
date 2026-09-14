# M06 — projeção Android visível do documento de código

Data: 12/09/2026. Estado: implementado; rodada ADB parcial registrada em [validação M06](../validacao/2026-09-12-m06-ide.md).
Substitui a decisão de campo invisível para **código**. O histórico do [adaptador Android anterior](ADR-REFUNDACAO-ANDROID-TEXT.md) permanece datado.

`EditorCodeWorkspace` continua proprietário do documento, revisão, publicação e undo. Android recebe uma projeção visível `Editable` em `EditorCodeInput.CodeField`. A sessão é a única escritora do documento; JNI apenas enfileira intervalo UTF-8/revisão ou estado de seleção/composição.

Snapshots completos não são a mensagem de digitação. A UI recebe snapshot ao trocar buffer/revisão externa, e aplica sem emitir outra edição. Mensagens reconhecidas e revisões impedem que um frame atrasado reverta texto local. Uma divergência é preservada como recuperação separada, sem recompilar uma cópia duplicada de Behavior.

Não existe undo autoral Android independente: comandos locais e físicos vão ao histórico nativo. Composição e batches têm estado explícito e bloqueiam auto-build. Roslyn mantém a autoridade sobre compilação/schema; coloração lexical não informa suporte de API.

Layout, abas, arquivos e console continuam na engine. A plataforma posiciona o EditText no retângulo normalizado publicado; os insets do IME são subtraídos uma única vez.

### Correção exigida pela execução no aparelho

`NativeActivity` apresenta Vulkan no buffer da janela principal. `addContentView`
nessa mesma janela criou um EditText acessível, com texto correto, mas seus pixels
foram sobrescritos pelo renderer. Compilar Java/C++ não detectou esse defeito.

O host agora usa `WindowManager.TYPE_APPLICATION_PANEL` ligado ao token da
Activity, limitado ao retângulo do código e com `FLAG_NOT_TOUCH_MODAL`. É uma
superfície de composição própria, sem diálogo, dimming, janela flutuante de usuário
ou permissão de overlay. Toques externos continuam chegando às abas/console;
toques internos pertencem ao campo Android. A guarda de gesto no consumidor
nativo continua impedindo que um intervalo de transição alcance a câmera.

A troca interna de foco para o painel mantém o processamento da sessão enquanto
a Activity está retomada. `APP_CMD_PAUSE` continua suspendendo normalmente.
Ao sair da IDE/abrir gaveta/menu ou pausar a Activity, o host remove sua janela
e encerra a composição; a volta a anexa novamente sem recriar o documento.

Insets de uma subjanela já recortada podem informar zero para o IME. Em API 30+
o host consulta os insets de `WindowManager.getCurrentWindowMetrics()` da Activity.
O observador dos campos curtos só publica enquanto seu próprio campo está ativo,
evitando sobrescrever os insets do código. Tamanho de fonte, gutter e acessórios
seguem a escala lógica nativa publicada, não uma segunda escala independente por dp.

Referência: [WindowManager.LayoutParams](https://developer.android.com/reference/android/view/WindowManager.LayoutParams)
para janela anexada e encaminhamento de toques externos; a prova de funcionamento
é a captura do aparelho, não a documentação Android.

Limites: 512 KiB por arquivo, 16 buffers, fila JNI de 256 mensagens/1 MiB, um worker lexical com resultado por buffer/revisão e 16 mil spans. O histórico anterior continua limitado por tamanho e quantidade de snapshots por buffer; não se declara rope/piece table implementada.

Nenhuma dependência de UI foi acrescentada. O fallback desenhado continua para consumidores sem Android. As cores vêm do tema Astra e os PNGs de undo/redo são recursos existentes. Não há mudança de formato de cena, serialização de componentes, shaders ou orientação da Activity.

Riscos a exercitar: fallback de insets antes de API 30; barras de seleção fora do campo; retomada/troca de abas durante composição; caracteres combinados; falha de escrita da recuperação; arquivos no teto; orçamento de cópias/conversões UTF-8. Não há alegação de proteção contra morte do processo antes de salvar/reconhecer a recuperação.

Critérios e lacunas: [estado M06](../planos/ESTADO-M06-IDE.md). Reverter a integração visual deve preservar fontes e recuperações; não apaga o projeto para retornar ao fallback.
