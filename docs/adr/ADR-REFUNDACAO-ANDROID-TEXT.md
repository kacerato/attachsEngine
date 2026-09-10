# Entrada Android na sessão autoral

Data: 2026-09-09. Escopo: M1, plano externo §§6, 7, 8 e 13.

## Problema e decisão

O teclado desenhado não oferece composição, seleção e clipboard Android.
Usamos EditText em AlertDialog como adaptador da plataforma; não criamos outro
documento nem toolkit. O fallback desenhado permanece para execução sem essa
plataforma. Nenhuma biblioteca foi adicionada.

Referências consultadas: [visibilidade e foco do IME](https://developer.android.com/develop/ui/views/touch-and-input/keyboard-input/visibility)
e [tipo de entrada](https://developer.android.com/develop/ui/views/touch-and-input/keyboard-input/style).
Essas APIs orientam a implementação; capacidades da Astra são comprovadas por
seus testes e dispositivo, não inferidas de outras engines.

## Contrato, ownership e threading

EditorSession::pendingTextEdit publica finalidade, entidade, campo, versão e
valor. completeTextEdit valida e aplica pela história existente. Texto de busca
não é mutação autoral. Cancelar não altera documento. Resposta de outra cena ou
revisão não pode sobrescrever edição posterior. Durante o campo aberto, ponteiros
não chegam à navegação/gizmos.

android_editor_text_input mantém pedido e resposta protegidos por mutex. JNI
transporta bytes UTF-8 e token; somente updateEditorTextInput, na thread dona da
sessão, modifica a autoria. A UI consulta a cada 100 ms enquanto a Activity está
ativa e cancela o diálogo em onPause. O renderer não faz chamadas JNI por draw.

Número deve ser finito, consumido integralmente pelo parser e aceito pelo
descritor de propriedade. Vírgula decimal isolada é normalizada. Nome rejeita
vazio e controles. Limites existentes: 63 bytes de texto e 47 de número; excesso
é rejeitado sem truncar UTF-8. Persistência e comandos permanecem existentes.

## Prova e limitações

O primeiro teste revelou extração fullscreen do teclado em landscape; usamos
IME_FLAG_NO_EXTRACT_UI para manter Aplicar/Cancelar acessíveis. Ajustamos tamanho
e altura mínima do campo após captura. Há evidência no relatório de refundação.
Clipboard é delegado ao EditText; não foi exercitado manualmente nesta rodada.
Não há alegação de compatibilidade com todos os IMEs, acessibilidade completa,
campos arbitrariamente longos ou Inspector extensível concluído.

Rollback: desativar usePlatformTextInput e remover a ligação Android retorna ao
adaptador anterior sem mudar formato de cena. Isso não satisfaria o gate de IME
do plano, portanto não é uma conclusão alternativa.
