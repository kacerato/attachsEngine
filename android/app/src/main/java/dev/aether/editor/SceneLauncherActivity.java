package dev.aether.editor;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

public final class SceneLauncherActivity extends Activity {
    private int dp(float value) { return Math.round(value * getResources().getDisplayMetrics().density); }

    private GradientDrawable panel(int color, float radius) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color); drawable.setCornerRadius(dp(radius));
        drawable.setStroke(dp(1), Color.rgb(56, 68, 74));
        return drawable;
    }

    private Button sceneButton(String title, String subtitle, boolean ocean) {
        Button button = new Button(this);
        button.setAllCaps(false); button.setText(title + "\n" + subtitle);
        button.setTextSize(16); button.setTextColor(Color.rgb(232, 239, 241));
        button.setGravity(Gravity.START | Gravity.CENTER_VERTICAL);
        button.setPadding(dp(22), dp(12), dp(22), dp(12));
        button.setBackground(panel(Color.rgb(31, 39, 43), 12));
        button.setOnClickListener(view -> {
            Intent intent = new Intent(this, AetherActivity.class);
            intent.putExtra("aether.ocean_preview", ocean);
            intent.putExtra("aether.map_preview", !ocean);
            intent.putExtra("aether.free_camera", ocean);
            intent.putExtra("aether.water_fft", ocean);
            intent.putExtra("aether.target_fps", 120.0f);
            startActivity(intent);
        });
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(dp(420), dp(82));
        params.topMargin = dp(14); button.setLayoutParams(params);
        return button;
    }

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setStatusBarColor(Color.rgb(13, 17, 19));
        getWindow().setNavigationBarColor(Color.rgb(13, 17, 19));

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL); root.setGravity(Gravity.CENTER);
        root.setPadding(dp(28), dp(24), dp(28), dp(24));
        root.setBackgroundColor(Color.rgb(15, 20, 22));
        TextView title = new TextView(this);
        title.setText("AETHER  /  CENAS"); title.setTextSize(26);
        title.setTextColor(Color.WHITE); title.setLetterSpacing(.12f);
        root.addView(title);
        TextView help = new TextView(this);
        help.setText("Escolha uma cena de validação. O painel de gráficos permanece disponível durante a execução.");
        help.setTextColor(Color.rgb(148, 164, 170)); help.setTextSize(13);
        help.setPadding(0, dp(8), 0, dp(10)); root.addView(help);
        root.addView(sceneButton("Floresta", "LOD, iluminação, sombras e estabilidade", false));
        root.addView(sceneButton("Oceano · laboratório físico", "FFT multicascata · corpos Jolt · toque", true));
        TextView build=new TextView(this);
        build.setText("Água FFT + física multicascata · 2026-09-06");
        build.setTextColor(Color.rgb(148,164,170)); build.setPadding(0,dp(14),0,0);
        root.addView(build);
        setContentView(root);
    }

    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if (focused && android.os.Build.VERSION.SDK_INT >= 30) {
            android.view.WindowInsetsController controller = getWindow().getDecorView().getWindowInsetsController();
            if (controller != null)
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
        }
    }
}
