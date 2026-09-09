package dev.aether.editor.shell;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.Gravity;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.GridLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.List;

/**
 * Folha de criação de projeto.
 *
 * Esta é a única parte do shell feita com Views: o campo de nome precisa do
 * teclado do sistema, e reimplementar edição de texto sobre Canvas seria trocar
 * uma caixa de texto que funciona por um bug de IME.
 *
 * A lista de cenas rola e as ações ficam fora do scroll — num aparelho em
 * paisagem, com o teclado aberto, um layout de altura livre empurra "Criar
 * projeto" para fora da tela.
 */
public final class NewProjectSheet extends FrameLayout {

    public interface Listener {
        void onCreate(String name, SceneTemplate template);
        void onUnavailable(SceneTemplate template);
    }

    private final Listener listener;
    private final ProjectStore store;
    private final EditText nameField;
    private final TextView errorLabel;
    private final Button confirm;
    private final GridLayout templates;
    private SceneTemplate picked;

    public NewProjectSheet(Context context, ProjectStore store, Listener listener) {
        super(context);
        this.store = store;
        this.listener = listener;
        setBackgroundColor(0xC4000000);
        setClickable(true);

        LinearLayout card = new LinearLayout(context);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setBackground(panel(Design.SURFACE, 16f, Design.LINE));
        card.setPadding(dp(24), dp(18), dp(24), dp(18));
        LayoutParams cardParams = new LayoutParams(dp(620), LayoutParams.MATCH_PARENT, Gravity.CENTER);
        cardParams.topMargin = dp(16);
        cardParams.bottomMargin = dp(16);
        addView(card, cardParams);

        card.addView(label("NEW PROJECT", 9f, Design.MUTED, .28f));

        TextView title = new TextView(context);
        title.setText("Nomeie e escolha o ponto de partida");
        title.setTextColor(Design.TEXT);
        title.setTextSize(17f);
        title.setTypeface(Design.ui(true));
        title.setPadding(0, dp(6), 0, dp(10));
        card.addView(title);

        nameField = new EditText(context);
        nameField.setHint("Nome do projeto");
        nameField.setHintTextColor(0xFF3D3D3D);
        nameField.setTextColor(Design.TEXT);
        nameField.setTextSize(14f);
        nameField.setSingleLine(true);
        nameField.setBackground(panel(0xFF0A0A0A, 9f, Design.LINE));
        nameField.setPadding(dp(14), dp(8), dp(14), dp(8));
        card.addView(nameField, new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, dp(42)));

        errorLabel = new TextView(context);
        errorLabel.setTextColor(0xFFFF6B6B);
        errorLabel.setTextSize(11f);
        errorLabel.setVisibility(GONE);
        errorLabel.setPadding(0, dp(5), 0, 0);
        card.addView(errorLabel);

        TextView group = label("SCENE TEMPLATE", 9f, Design.MUTED, .28f);
        group.setPadding(0, dp(14), 0, dp(8));
        card.addView(group);

        ScrollView scroller = new ScrollView(context);
        scroller.setFillViewport(true);
        templates = new GridLayout(context);
        templates.setColumnCount(4);
        scroller.addView(templates);
        LinearLayout.LayoutParams scrollParams =
                new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, 0, 1f);
        card.addView(scroller, scrollParams);
        buildTemplates();

        LinearLayout actions = new LinearLayout(context);
        actions.setGravity(Gravity.END);
        LinearLayout.LayoutParams actionParams =
                new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        actionParams.topMargin = dp(14);
        card.addView(actions, actionParams);

        Button cancel = new Button(context);
        style(cancel, "Cancelar", Design.DIM, panel(Color.TRANSPARENT, 9f, Design.LINE));
        cancel.setOnClickListener(view -> dismiss());
        actions.addView(cancel, new LinearLayout.LayoutParams(dp(120), dp(42)));

        confirm = new Button(context);
        style(confirm, "Criar projeto", Design.ACCENT_INK, panel(Design.ACCENT, 9f, Design.ACCENT));
        confirm.setEnabled(false);
        confirm.setAlpha(.35f);
        confirm.setOnClickListener(view -> submit());
        LinearLayout.LayoutParams confirmParams = new LinearLayout.LayoutParams(dp(150), dp(42));
        confirmParams.leftMargin = dp(10);
        actions.addView(confirm, confirmParams);

        nameField.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int a, int b, int c) { }
            @Override public void onTextChanged(CharSequence s, int a, int b, int c) { }
            @Override public void afterTextChanged(Editable s) { validate(); }
        });
    }

    private void buildTemplates() {
        templates.removeAllViews();
        List<SceneTemplate> all = SceneTemplate.all();
        for (SceneTemplate template : all) {
            if (!template.ready) continue;
            LinearLayout tile = new LinearLayout(getContext());
            tile.setOrientation(LinearLayout.VERTICAL);
            boolean selected = picked != null && picked.id.equals(template.id);
            tile.setBackground(panel(selected ? Design.WASH : Design.RAISED, 6f,
                    selected ? Design.ACCENT : Design.LINE));
            tile.setPadding(dp(11), dp(9), dp(11), dp(9));
            tile.setAlpha(template.ready ? 1f : .48f);

            if (!template.ready) tile.addView(label("EM BREVE", 8f, Design.ACCENT, .2f));

            TextView name = new TextView(getContext());
            name.setText(template.name);
            name.setTextColor(Design.TEXT);
            name.setTextSize(12.5f);
            name.setTypeface(Design.ui(true));
            tile.addView(name);

            TextView note = new TextView(getContext());
            note.setText(template.note);
            note.setTextColor(Design.MUTED);
            note.setTextSize(10.5f);
            note.setMaxLines(2);
            note.setEllipsize(android.text.TextUtils.TruncateAt.END);
            tile.addView(note);

            tile.setOnClickListener(view -> {
                if (!template.ready) { listener.onUnavailable(template); return; }
                picked = template;
                buildTemplates();
                validate();
            });

            GridLayout.LayoutParams params = new GridLayout.LayoutParams();
            params.width = 0;
            params.height = dp(92);
            params.columnSpec = GridLayout.spec(GridLayout.UNDEFINED, 1f);
            params.setMargins(dp(3), dp(3), dp(3), dp(3));
            templates.addView(tile, params);
        }
    }

    private void validate() {
        String name = nameField.getText().toString().trim();
        boolean duplicate = !name.isEmpty() && store.exists(name);
        boolean legal = name.matches("[^\\\\/:*?\"<>|]+");
        if (duplicate) showError("Já existe um projeto com esse nome.");
        else if (!name.isEmpty() && !legal) showError("O nome não pode conter \\ / : * ? \" < > |");
        else showError(null);

        boolean valid = !name.isEmpty() && !duplicate && legal && picked != null;
        confirm.setEnabled(valid);
        confirm.setAlpha(valid ? 1f : .35f);
    }

    private void showError(String message) {
        if (message == null) { errorLabel.setVisibility(GONE); return; }
        errorLabel.setText(message);
        errorLabel.setVisibility(VISIBLE);
    }

    private void submit() {
        listener.onCreate(nameField.getText().toString().trim(), picked);
    }

    public void dismiss() {
        if (getParent() instanceof android.view.ViewGroup) {
            ((android.view.ViewGroup) getParent()).removeView(this);
        }
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
