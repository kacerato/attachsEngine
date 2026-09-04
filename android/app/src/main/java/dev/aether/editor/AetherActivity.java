package dev.aether.editor;

import android.app.NativeActivity;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;

public final class AetherActivity extends NativeActivity {
    static { System.loadLibrary("aether_android"); }
    private static native void nativeApplyControls(float renderScale, int shadowQuality,
        boolean dynamicResolution, float bloomIntensity, float sharpen, float waveHeight,
        float waveSpeed, float waveSteepness, float microWaves, float surfaceOpacity,
        float absorption, float foam, float interactionStrength);

    private float renderScale=1, bloom=.08f, sharpen=.12f, waveHeight=1, waveSpeed=1;
    private float steepness=1, micro=1, opacity=.72f, absorption=1, foam=.65f, interaction=.65f;
    private int shadows=3; private boolean dynamic=true;
    private LinearLayout panel;

    private int dp(float value) { return Math.round(value * getResources().getDisplayMetrics().density); }
    private GradientDrawable background(int color, float radius) {
        GradientDrawable drawable=new GradientDrawable(); drawable.setColor(color);
        drawable.setCornerRadius(dp(radius)); drawable.setStroke(dp(1),Color.rgb(65,78,83));
        return drawable;
    }
    private TextView heading(String value) {
        TextView text=new TextView(this); text.setText(value); text.setTextSize(13);
        text.setTextColor(Color.rgb(117,216,190)); text.setLetterSpacing(.10f);
        text.setPadding(0,dp(14),0,dp(5)); return text;
    }
    private void apply() {
        nativeApplyControls(renderScale,shadows,dynamic,bloom,sharpen,waveHeight,waveSpeed,
                            steepness,micro,opacity,absorption,foam,interaction);
    }
    private View slider(String name, int maximum, int initial, java.util.function.IntConsumer changed) {
        LinearLayout row=new LinearLayout(this); row.setOrientation(LinearLayout.VERTICAL);
        TextView label=new TextView(this); label.setText(name); label.setTextColor(Color.rgb(223,230,232));
        label.setTextSize(12); row.addView(label);
        SeekBar seek=new SeekBar(this); seek.setMax(maximum); seek.setProgress(initial);
        seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar bar,int value,boolean user) { if(user){changed.accept(value);apply();} }
            public void onStartTrackingTouch(SeekBar bar) {}
            public void onStopTrackingTouch(SeekBar bar) {}
        }); row.addView(seek,new LinearLayout.LayoutParams(-1,dp(34))); return row;
    }
    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        FrameLayout overlay=new FrameLayout(this);
        Button settings=new Button(this); settings.setText("GRÁFICOS"); settings.setTextSize(11);
        settings.setTextColor(Color.WHITE); settings.setAllCaps(false);
        settings.setBackground(background(Color.argb(220,24,31,34),10));
        settings.setOnClickListener(view -> panel.setVisibility(panel.getVisibility()==View.VISIBLE?View.GONE:View.VISIBLE));
        FrameLayout.LayoutParams settingsParams=new FrameLayout.LayoutParams(dp(112),dp(48),Gravity.TOP|Gravity.END);
        settingsParams.setMargins(0,dp(16),dp(18),0); overlay.addView(settings,settingsParams);

        ScrollView scroll=new ScrollView(this); scroll.setFillViewport(true);
        panel=new LinearLayout(this); panel.setOrientation(LinearLayout.VERTICAL);
        panel.setPadding(dp(18),dp(12),dp(18),dp(18)); panel.setBackground(background(Color.argb(244,18,24,27),12));
        panel.addView(heading("RENDERIZAÇÃO"));
        panel.addView(slider("Escala de render",50,50,v->renderScale=.5f+v/100f));
        panel.addView(slider("Sombras: 0 off / 3 máxima",3,3,v->shadows=v));
        Switch drs=new Switch(this); drs.setText("Resolução dinâmica"); drs.setTextColor(Color.WHITE);
        drs.setChecked(dynamic); drs.setOnCheckedChangeListener((button,value)->{dynamic=value;apply();}); panel.addView(drs);
        panel.addView(slider("Bloom",100,8,v->bloom=v/100f));
        panel.addView(slider("Nitidez",100,12,v->sharpen=v/100f));
        panel.addView(heading("ÁGUA / ONDAS GPU"));
        panel.addView(slider("Altura das ondas",150,50,v->waveHeight=v/50f));
        panel.addView(slider("Velocidade",150,50,v->waveSpeed=v/50f));
        panel.addView(slider("Inclinação / cristas",100,50,v->steepness=v/50f));
        panel.addView(slider("Micro-ondas",150,50,v->micro=v/50f));
        panel.addView(slider("Transparência",95,67,v->opacity=.05f+v/100f));
        panel.addView(slider("Absorção por profundidade",195,45,v->absorption=.1f+v/50f));
        panel.addView(slider("Espuma",100,33,v->foam=v/50f));
        panel.addView(slider("Força da interação",100,33,v->interaction=v/50f));
        Button scenes=new Button(this); scenes.setText("Voltar às cenas"); scenes.setAllCaps(false);
        scenes.setOnClickListener(view->finish()); panel.addView(scenes);
        scroll.addView(panel); panel.setVisibility(View.GONE);
        FrameLayout.LayoutParams panelParams=new FrameLayout.LayoutParams(dp(390),-1,Gravity.END);
        panelParams.setMargins(0,dp(72),dp(12),dp(14)); overlay.addView(scroll,panelParams);

        if(getIntent().getBooleanExtra("aether.ocean_preview",false)) {
            TextView hint=new TextView(this); hint.setText("TOQUE CURTO NA ÁGUA  ·  GERA ONDA");
            hint.setTextColor(Color.rgb(210,224,227)); hint.setTextSize(11);
            hint.setPadding(dp(12),dp(7),dp(12),dp(7)); hint.setBackground(background(Color.argb(180,16,22,25),8));
            FrameLayout.LayoutParams hintParams=new FrameLayout.LayoutParams(-2,-2,Gravity.BOTTOM|Gravity.CENTER_HORIZONTAL);
            hintParams.bottomMargin=dp(16); overlay.addView(hint,hintParams);
        }
        addContentView(overlay,new android.view.ViewGroup.LayoutParams(-1,-1));
        apply();
    }
}
