package dev.aether.editor.shell;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.text.format.DateUtils;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;

/**
 * Configurações do shell: hoje, a telemetria de desempenho.
 *
 * Duas colunas em paisagem: à esquerda o que é e o que nunca é enviado, à
 * direita o controle, o estado da fila e as ações. Na primeira abertura a mesma
 * folha vira o pedido de consentimento, com a escolha explícita nos dois botões
 * de baixo; nada é coletado enquanto a pessoa não escolher.
 */
public final class SettingsSheet extends FrameLayout {

    private final Telemetry telemetry;
    private final boolean firstRun;
    private final Switch share;
    private final TextView status;
    private final Button sendNow, forget;

    SettingsSheet(Context context, Telemetry telemetry, boolean firstRun) {
        super(context);
        this.telemetry = telemetry;
        this.firstRun = firstRun;
        setBackgroundColor(0xC4000000);
        setClickable(true);

        LinearLayout card = new LinearLayout(context);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setBackground(panel(Design.SURFACE, 16f, Design.LINE));
        card.setPadding(dp(24), dp(18), dp(24), dp(18));
        LayoutParams cardParams = new LayoutParams(dp(760), LayoutParams.MATCH_PARENT, Gravity.CENTER);
        cardParams.topMargin = dp(16);
        cardParams.bottomMargin = dp(16);
        addView(card, cardParams);

        card.addView(label(firstRun ? "ANTES DE COMEÇAR" : "CONFIGURAÇÕES", 9f, Design.MUTED, .28f));
        TextView title = label("Telemetria de desempenho", 17f, Design.TEXT, 0f);
        title.setTypeface(Design.ui(true));
        title.setPadding(0, dp(6), 0, dp(4));
        card.addView(title);
        TextView lead = label("Ajude a Astra a rodar melhor em aparelhos como o seu. Opcional; muda quando quiser.",
                12.5f, Design.DIM, 0f);
        lead.setPadding(0, 0, 0, dp(12));
        card.addView(lead);

        LinearLayout columns = new LinearLayout(context);
        columns.setOrientation(LinearLayout.HORIZONTAL);
        card.addView(columns, new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, 0, 1f));

        // Coluna da esquerda: o contrato do que sai do aparelho.
        LinearLayout facts = new LinearLayout(context);
        facts.setOrientation(LinearLayout.VERTICAL);
        facts.addView(section("ENVIADO", Design.ACCENT));
        facts.addView(item(true, "Quadros por segundo e tempo de CPU e GPU, em médias de 10 s"));
        facts.addView(item(true, "Modelo do aparelho, GPU, memória e tela"));
        facts.addView(item(true, "Versão do app, nível de qualidade e temperatura"));
        facts.addView(item(true, "Um código aleatório desta instalação, que você pode apagar"));
        View gap = new View(context);
        facts.addView(gap, new LinearLayout.LayoutParams(1, dp(10)));
        facts.addView(section("NUNCA SAI DO APARELHO", Design.MUTED));
        facts.addView(item(false, "Nomes, arquivos e conteúdo dos seus projetos"));
        facts.addView(item(false, "Conta, localização e identificadores do aparelho"));
        ScrollView factsScroller = new ScrollView(context);
        factsScroller.addView(facts);
        LinearLayout.LayoutParams factsParams = new LinearLayout.LayoutParams(0, LayoutParams.MATCH_PARENT, 1.25f);
        factsParams.rightMargin = dp(18);
        columns.addView(factsScroller, factsParams);

        // Coluna da direita: controle e estado.
        LinearLayout control = new LinearLayout(context);
        control.setOrientation(LinearLayout.VERTICAL);
        control.setBackground(panel(Design.CANVAS, 12f, Design.LINE_SOFT));
        control.setPadding(dp(16), dp(14), dp(16), dp(14));
        columns.addView(control, new LinearLayout.LayoutParams(0, LayoutParams.MATCH_PARENT, 1f));

        LinearLayout toggleRow = new LinearLayout(context);
        toggleRow.setGravity(Gravity.CENTER_VERTICAL);
        TextView toggleLabel = label("Compartilhar desempenho", 13.5f, Design.TEXT, 0f);
        toggleLabel.setTypeface(Design.ui(true));
        toggleRow.addView(toggleLabel, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f));
        share = new Switch(context);
        int[][] states = {{android.R.attr.state_checked}, {}};
        share.setThumbTintList(new ColorStateList(states, new int[] {Design.ACCENT, Design.DIM}));
        share.setTrackTintList(new ColorStateList(states, new int[] {Design.withAlpha(Design.ACCENT, .45f), Design.TRACK}));
        share.setChecked(telemetry.consent() == Telemetry.Consent.GRANTED);
        toggleRow.addView(share);
        control.addView(toggleRow);
        control.addView(divider(), dividerParams());

        status = label("", 12f, Design.MUTED, 0f);
        status.setLineSpacing(0f, 1.15f);
        // O estado pode somar a mensagem da última ação a duas linhas de fila:
        // rola dentro do painel em vez de ser cortado pelos botões.
        ScrollView statusScroller = new ScrollView(context);
        statusScroller.addView(status);
        control.addView(statusScroller, new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, 0, 1f));
        LinearLayout buttons = new LinearLayout(context);
        LinearLayout.LayoutParams buttonsParams = new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        buttonsParams.topMargin = dp(10);
        control.addView(buttons, buttonsParams);

        sendNow = new Button(context);
        style(sendNow, "Enviar agora", Design.DIM, panel(Color.TRANSPARENT, 9f, Design.LINE));
        sendNow.setOnClickListener(view -> {
            sendNow.setEnabled(false);
            status.setText("Enviando…");
            telemetry.upload(message -> { sendNow.setEnabled(true); refresh(message); });
        });
        buttons.addView(sendNow, new LinearLayout.LayoutParams(0, dp(40), 1f));

        forget = new Button(context);
        style(forget, "Apagar dados", 0xFFFF8A80, panel(Color.TRANSPARENT, 9f, 0x66FF8A80));
        forget.setOnClickListener(view -> confirmForget());
        LinearLayout.LayoutParams forgetParams = new LinearLayout.LayoutParams(0, dp(40), 1f);
        forgetParams.leftMargin = dp(8);
        buttons.addView(forget, forgetParams);

        LinearLayout actions = new LinearLayout(context);
        actions.setGravity(Gravity.END);
        LinearLayout.LayoutParams actionParams = new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        actionParams.topMargin = dp(14);
        card.addView(actions, actionParams);

        if (firstRun) {
            Button decline = new Button(context);
            style(decline, "Agora não", Design.DIM, panel(Color.TRANSPARENT, 9f, Design.LINE));
            decline.setOnClickListener(view -> { telemetry.setConsent(false); dismiss(); });
            actions.addView(decline, new LinearLayout.LayoutParams(dp(130), dp(42)));
            Button accept = new Button(context);
            style(accept, "Compartilhar", Design.ACCENT_INK, panel(Design.ACCENT, 9f, Design.ACCENT));
            accept.setOnClickListener(view -> { telemetry.setConsent(true); dismiss(); });
            LinearLayout.LayoutParams acceptParams = new LinearLayout.LayoutParams(dp(150), dp(42));
            acceptParams.leftMargin = dp(10);
            actions.addView(accept, acceptParams);
            // No primeiro contato a escolha é pelos dois botões de baixo; um
            // interruptor desligado e inerte ao lado deles só confundiria.
            share.setVisibility(GONE);
        } else {
            Button close = new Button(context);
            style(close, "Fechar", Design.ACCENT_INK, panel(Design.ACCENT, 9f, Design.ACCENT));
            close.setOnClickListener(view -> dismiss());
            actions.addView(close, new LinearLayout.LayoutParams(dp(130), dp(42)));
            share.setOnCheckedChangeListener((view, checked) -> {
                telemetry.setConsent(checked);
                refresh(checked ? "Coleta ligada. Vale a partir da próxima abertura do editor." : "Coleta desligada.");
            });
        }
        refresh(null);
    }

    private void refresh(String message) {
        boolean granted = telemetry.consent() == Telemetry.Consent.GRANTED;
        StringBuilder text = new StringBuilder();
        if (message != null) text.append(message).append("\n\n");
        if (!granted) {
            text.append(firstRun ? "Nada é coletado até você escolher." : "Coleta desligada. Nada é registrado.");
        } else {
            int pending = telemetry.pendingWindows();
            text.append(pending == 0 ? "Nenhuma janela aguardando envio."
                    : pending + (pending == 1 ? " janela aguardando envio." : " janelas aguardando envio."));
            long last = telemetry.lastUploadMillis();
            long now = System.currentTimeMillis();
            text.append("\nÚltimo envio: ").append(last == 0 ? "ainda não houve"
                    : now - last < DateUtils.MINUTE_IN_MILLIS ? "agora há pouco"
                    : DateUtils.getRelativeTimeSpanString(last, now, DateUtils.MINUTE_IN_MILLIS));
        }
        status.setText(text);
        sendNow.setVisibility(granted && !firstRun ? VISIBLE : GONE);
        forget.setVisibility(firstRun ? GONE : VISIBLE);
    }

    private void confirmForget() {
        if (!"confirm".equals(forget.getTag())) {
            forget.setTag("confirm");
            forget.setText("Confirmar");
            postDelayed(() -> { forget.setTag(null); forget.setText("Apagar dados"); }, 4000);
            return;
        }
        forget.setTag(null);
        forget.setEnabled(false);
        forget.setText("Apagando…");
        telemetry.forget(message -> {
            forget.setEnabled(true);
            forget.setText("Apagar dados");
            refresh(message);
        });
    }

    public void dismiss() {
        if (getParent() instanceof android.view.ViewGroup) ((android.view.ViewGroup) getParent()).removeView(this);
    }

    private TextView section(String value, int color) {
        TextView view = label(value, 9f, color, .24f);
        view.setTypeface(Design.ui(true));
        view.setPadding(0, 0, 0, dp(6));
        return view;
    }

    /** Linha de lista com marcador desenhado: traço de "sim" ou barra de "não". */
    private View item(boolean sent, String value) {
        LinearLayout row = new LinearLayout(getContext());
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(0, dp(3), 0, dp(3));
        row.addView(new Marker(getContext(), sent), new LinearLayout.LayoutParams(dp(18), dp(18)));
        TextView text = label(value, 12.5f, sent ? Design.DIM : Design.MUTED, 0f);
        text.setPadding(dp(10), 0, 0, 0);
        row.addView(text, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f));
        return row;
    }

    private static final class Marker extends View {
        private final boolean sent;
        private final Paint paint;
        Marker(Context context, boolean sent) {
            super(context);
            this.sent = sent;
            paint = Design.stroke(sent ? Design.ACCENT : Design.FAINT, context.getResources().getDisplayMetrics().density * 1.6f);
        }
        @Override protected void onDraw(Canvas canvas) {
            float w = getWidth(), h = getHeight();
            canvas.drawCircle(w / 2f, h / 2f, Math.min(w, h) / 2f - paint.getStrokeWidth(), paint);
            if (sent) {
                canvas.drawLine(w * .30f, h * .52f, w * .45f, h * .66f, paint);
                canvas.drawLine(w * .45f, h * .66f, w * .71f, h * .36f, paint);
            } else {
                canvas.drawLine(w * .32f, h * .68f, w * .68f, h * .32f, paint);
            }
        }
    }

    private View divider() {
        View line = new View(getContext());
        line.setBackgroundColor(Design.LINE_SOFT);
        return line;
    }

    private LinearLayout.LayoutParams dividerParams() {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, Math.max(1, dp(1)));
        params.topMargin = dp(12);
        params.bottomMargin = dp(12);
        return params;
    }

    private TextView label(String value, float size, int color, float tracking) {
        TextView view = new TextView(getContext());
        view.setText(value);
        view.setTextSize(size);
        view.setTextColor(color);
        view.setLetterSpacing(tracking);
        return view;
    }

    private void style(Button button, String text, int color, GradientDrawable background) {
        button.setText(text);
        button.setAllCaps(false);
        button.setTextColor(color);
        button.setTextSize(13f);
        button.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
        button.setBackground(background);
        button.setStateListAnimator(null);
        button.setSingleLine(true);
        button.setEllipsize(android.text.TextUtils.TruncateAt.END);
        button.setPadding(dp(10), 0, dp(10), 0);
    }

    private GradientDrawable panel(int fill, float radius, int border) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(fill);
        drawable.setCornerRadius(dp(radius));
        drawable.setStroke(Math.max(1, dp(1f)), border);
        return drawable;
    }

    private int dp(float value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    @Override public boolean performClick() { return super.performClick(); }
}
