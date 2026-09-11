package dev.aether.editor;

import android.app.Activity;
import android.app.AlertDialog;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.InputType;
import android.text.Selection;
import android.text.SpannableStringBuilder;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.WindowManager;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import java.nio.charset.StandardCharsets;

/**
 * A ponte de texto: o teclado e do sistema, o CAMPO e do editor.
 *
 * <p>Esta classe nao desenha campo nenhum. Ela mantem uma {@link View} de um
 * pixel, invisivel, so para segurar o foco e a {@link InputConnection} — e o
 * que faz o IME abrir e entregar composicao, correcao e area de transferencia.
 * O texto vai para o lado nativo a cada tecla, e e o editor que desenha o campo
 * com o cursor, na sua propria superficie.
 *
 * <p>Antes disso a edicao acontecia num {@link AlertDialog} que cobria a tela:
 * o usuario nao via o objeto que estava renomeando nem o valor que estava
 * mudando enquanto digitava, e a busca so filtrava depois de confirmar.
 *
 * <p>O editor de codigo continua no dialogo. Ele e multi-linha com rolagem e
 * selecao propria, e entra no marco seguinte — trocar os dois de uma vez seria
 * anunciar uma coisa e entregar outra.
 */
final class EditorTextInput {
    private static native byte[][] poll();
    private static native void submit(long token, byte[] value, boolean accept);
    private static native void push(long token, byte[] value, int caretBytes);
    private static native void ime(float fraction);

    private final Activity activity;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private AlertDialog dialog;
    private InlineField field;
    private boolean running;
    private long lastToken;
    private int limit;

    EditorTextInput(Activity activity) { this.activity = activity; }

    private static String decode(byte[] value) { return new String(value, StandardCharsets.UTF_8); }

    private final Runnable tick = new Runnable() {
        @Override public void run() {
            if (!running) return;
            byte[][] data = poll();
            if (data == null) {
                // O pedido acabou: pode ter sido confirmado pelo IME ou fechado
                // pelo proprio editor. Fechar o campo aqui e o que mantem os
                // dois lados de acordo sem um segundo protocolo.
                if (field != null) closeField();
            } else {
                long token = Long.parseLong(decode(data[0]));
                if (token != lastToken) { lastToken = token; open(token, data); }
            }
            handler.postDelayed(this, 50);
        }
    };

    void start() { if (!running) { running = true; handler.post(tick); } }

    void stop() {
        running = false;
        handler.removeCallbacks(tick);
        if (dialog != null) { submit(lastToken, new byte[0], false); dialog.dismiss(); dialog = null; }
        if (field != null) { submit(lastToken, new byte[0], false); closeField(); }
    }

    private void open(long token, byte[][] data) {
        final String kind = decode(data[1]);
        limit = Integer.parseInt(decode(data[4]));
        if (kind.equals("code")) { showDialog(token, data); return; }
        showField(token, kind.equals("number"), decode(data[3]));
    }

    // ---- campo embutido -----------------------------------------------------

    private void showField(long token, boolean number, String initial) {
        if (field == null) {
            field = new InlineField(activity);
            // Um pixel, transparente, por cima de tudo. Ela existe para o foco e
            // para a InputConnection; quem desenha e o editor.
            activity.addContentView(field, new ViewGroup.LayoutParams(1, 1));
            field.getViewTreeObserver().addOnGlobalLayoutListener(imeWatcher);
        }
        field.begin(token, number, initial);
    }

    private void closeField() {
        if (field == null) return;
        field.end();
        ime(0.0f);
    }

    private final ViewTreeObserver.OnGlobalLayoutListener imeWatcher =
        new ViewTreeObserver.OnGlobalLayoutListener() {
            @Override public void onGlobalLayout() {
                if (field == null) return;
                final View root = field.getRootView();
                if (root == null || root.getHeight() <= 0) return;
                final Rect visible = new Rect();
                root.getWindowVisibleDisplayFrame(visible);
                final float covered = root.getHeight() - visible.height();
                // Barras do sistema tambem entram nessa conta. O corte de 15%
                // separa "teclado aberto" de "barra de navegacao", sem precisar
                // de API nova.
                final float fraction = covered / (float) root.getHeight();
                ime(fraction > 0.15f ? fraction : 0.0f);
            }
        };

    private final class InlineField extends View {
        private final Editable buffer = new SpannableStringBuilder();
        private long token;
        private boolean number;
        private boolean active;

        InlineField(Activity host) {
            super(host);
            setFocusable(true);
            setFocusableInTouchMode(true);
        }

        void begin(long id, boolean numeric, String initial) {
            token = id;
            number = numeric;
            active = true;
            buffer.clear();
            buffer.append(initial);
            Selection.setSelection(buffer, buffer.length());
            requestFocus();
            final InputMethodManager manager =
                (InputMethodManager) activity.getSystemService(Activity.INPUT_METHOD_SERVICE);
            if (manager != null) {
                manager.restartInput(this);
                manager.showSoftInput(this, InputMethodManager.SHOW_IMPLICIT);
            }
            publish();
        }

        void end() {
            active = false;
            final InputMethodManager manager =
                (InputMethodManager) activity.getSystemService(Activity.INPUT_METHOD_SERVICE);
            if (manager != null) manager.hideSoftInputFromWindow(getWindowToken(), 0);
            clearFocus();
        }

        @Override public boolean onCheckIsTextEditor() { return active; }

        // O cursor vai em BYTES UTF-8: e assim que o lado nativo indexa o texto.
        // Mandar o indice UTF-16 do Android poria o traco no meio de um glifo
        // acentuado.
        private void publish() {
            final String text = buffer.toString();
            final int selection = Math.max(0, Math.min(Selection.getSelectionEnd(buffer), text.length()));
            final byte[] bytes = text.getBytes(StandardCharsets.UTF_8);
            if (bytes.length > limit) return;
            push(token, bytes, text.substring(0, selection).getBytes(StandardCharsets.UTF_8).length);
        }

        private void accept() {
            final byte[] bytes = buffer.toString().getBytes(StandardCharsets.UTF_8);
            if (bytes.length > limit) return;
            submit(token, bytes, true);
            end();
        }

        private void cancel() { submit(token, new byte[0], false); end(); }

        /**
         * Uma tecla, venha de onde vier.
         *
         * <p>O IME entrega texto pela {@link InputConnection}, mas teclado
         * fisico e eventos injetados chegam direto na View, sem passar por ela.
         * Os dois caminhos existem de verdade — tablet com teclado e o proprio
         * `adb shell input text` — e tratar so um deixaria metade das teclas sem
         * efeito.
         */
        private boolean handleKey(KeyEvent event) {
            if (!active || event.getAction() != KeyEvent.ACTION_DOWN) return false;
            if (event.getKeyCode() == KeyEvent.KEYCODE_ENTER) { accept(); return true; }
            if (event.getKeyCode() == KeyEvent.KEYCODE_DEL) {
                final int end = Selection.getSelectionEnd(buffer);
                final int start = Selection.getSelectionStart(buffer);
                if (start != end) buffer.delete(Math.min(start, end), Math.max(start, end));
                else if (end > 0) buffer.delete(end - 1, end);
                publish();
                return true;
            }
            final int unicode = event.getUnicodeChar();
            if (unicode == 0) return false;
            final int end = Math.max(0, Selection.getSelectionEnd(buffer));
            buffer.insert(end, String.valueOf((char) unicode));
            Selection.setSelection(buffer, end + 1);
            publish();
            return true;
        }

        @Override public boolean onKeyDown(int code, KeyEvent event) {
            return handleKey(event) || super.onKeyDown(code, event);
        }

        @Override public boolean onKeyPreIme(int code, KeyEvent event) {
            // Voltar fecha o campo, nao a tela. Sem isto o gesto de voltar sairia
            // do projeto com uma edicao pela metade aberta.
            if (active && code == KeyEvent.KEYCODE_BACK && event.getAction() == KeyEvent.ACTION_UP) {
                cancel();
                return true;
            }
            return super.onKeyPreIme(code, event);
        }

        @Override public InputConnection onCreateInputConnection(EditorInfo out) {
            out.inputType = number
                ? InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL
                      | InputType.TYPE_NUMBER_FLAG_SIGNED
                : InputType.TYPE_CLASS_TEXT;
            out.imeOptions = EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI
                | EditorInfo.IME_FLAG_NO_FULLSCREEN;
            out.initialSelStart = Selection.getSelectionStart(buffer);
            out.initialSelEnd = Selection.getSelectionEnd(buffer);
            return new BaseInputConnection(this, true) {
                @Override public Editable getEditable() { return buffer; }
                @Override public boolean commitText(CharSequence text, int position) {
                    final boolean done = super.commitText(text, position);
                    publish();
                    return done;
                }
                @Override public boolean setComposingText(CharSequence text, int position) {
                    final boolean done = super.setComposingText(text, position);
                    publish();
                    return done;
                }
                @Override public boolean finishComposingText() {
                    final boolean done = super.finishComposingText();
                    publish();
                    return done;
                }
                @Override public boolean deleteSurroundingText(int before, int after) {
                    final boolean done = super.deleteSurroundingText(before, after);
                    publish();
                    return done;
                }
                @Override public boolean setSelection(int start, int end) {
                    final boolean done = super.setSelection(start, end);
                    publish();
                    return done;
                }
                @Override public boolean performEditorAction(int action) { accept(); return true; }
                @Override public boolean sendKeyEvent(KeyEvent event) {
                    if (handleKey(event)) return true;
                    if (event.getAction() != KeyEvent.ACTION_DOWN) return true;
                    return super.sendKeyEvent(event);
                }
            };
        }
    }

    // ---- editor de codigo: ainda no dialogo ---------------------------------

    private void showDialog(long token, byte[][] data) {
        final int bytes = limit;
        EditText input = new EditText(activity);
        input.setTextSize(14);
        input.setTypeface(android.graphics.Typeface.MONOSPACE);
        input.setMinLines(14);
        input.setHorizontallyScrolling(true);
        input.setMinHeight((int) (48 * activity.getResources().getDisplayMetrics().density));
        input.setSingleLine(false);
        input.setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI | EditorInfo.IME_ACTION_NONE);
        input.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE
            | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        input.setText(decode(data[3]));
        input.setSelection(input.length());
        int padding = (int) (20 * activity.getResources().getDisplayMetrics().density);
        input.setPadding(padding, padding / 2, padding, padding / 2);
        AlertDialog current = new AlertDialog.Builder(activity)
            .setTitle(decode(data[2])).setView(input)
            .setNegativeButton("Cancelar", (d, w) -> submit(token, new byte[0], false))
            .setPositiveButton("Concluir edicao", null).create();
        dialog = current;
        current.setCanceledOnTouchOutside(false);
        current.setOnCancelListener(d -> submit(token, new byte[0], false));
        current.setOnDismissListener(d -> { if (dialog == current) dialog = null; });
        current.setOnShowListener(d -> {
            current.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v -> {
                byte[] text = input.getText().toString().getBytes(StandardCharsets.UTF_8);
                if (text.length > bytes) { input.setError("Limite de " + bytes + " bytes UTF-8"); return; }
                submit(token, text, true);
                current.dismiss();
            });
            input.requestFocus();
            current.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE
                | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        });
        current.show();
        current.getWindow().setLayout(WindowManager.LayoutParams.MATCH_PARENT,
            WindowManager.LayoutParams.MATCH_PARENT);
    }
}
