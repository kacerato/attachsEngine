package dev.aether.editor;

import android.app.NativeActivity;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;

public final class AetherActivity extends NativeActivity {
    static { System.loadLibrary("aether_android"); }
    private static native int nativeWaterProviderStatus();
    private static native void nativeApplyControls(float renderScale, int shadowQuality,
        boolean dynamicResolution, float bloomIntensity, float sharpen, float waveHeight,
        float waveSpeed, float waveSteepness, float microWaves, float surfaceOpacity,
        float absorption, float foam, float interactionStrength, float bodyRippleGain, float roughness,
        float turbidity, float ior, float direction, float foamCompression,float foamGrowth,float foamDecay,
        float specularAntialiasing,float contactFoamWidth,float fluidDensity,boolean waterPaused,
        float swellLength,float directionalSpread,float crossSwell,
        float waterLevel,float longWaveAmplitude,float longWaveLength);

    private float renderScale=1, bloom=.08f, sharpen=.12f, waveHeight=1, waveSpeed=1;
    private float steepness=1, micro=1, opacity=.72f, absorption=1, foam=.65f, interaction=.65f;
    private float bodyRippleGain=.75f;
    private int shadows=3; private boolean dynamic=true;
    private float roughness=.22f, turbidity=.10f, ior=1.333f, direction=0;
    private float foamCompression=.8f,foamGrowth=4,foamDecay=.5f;
    private float specularAntialiasing=.5f,contactFoamWidth=1.35f;
    private float fluidDensity=1400;
    private boolean waterPaused=false;
    private float swellLength=1,directionalSpread=1,crossSwell=0;
    private float waterLevel=0,longWaveAmplitude=0,longWaveLength=320;
    private LinearLayout panel;
    private TextView waterProviderLabel;
    private LinearLayout spectralFoamControls;
    private void refreshWaterProvider() {
        int status=nativeWaterProviderStatus();
        if(waterProviderLabel!=null) waterProviderLabel.setText(status==2?"FFT GPU · experimental ativo":
            status==3?"Analítico · FFT indisponível (fallback)":status==1?"Ondas analíticas GPU":"Água inativa / carregando");
        if(spectralFoamControls!=null) {
            spectralFoamControls.setAlpha(status==2?1f:.4f);
            setControlsEnabled(spectralFoamControls,status==2);
        }
    }
    private void setControlsEnabled(View view,boolean enabled) {
        view.setEnabled(enabled);
        if(view instanceof android.view.ViewGroup) {
            android.view.ViewGroup group=(android.view.ViewGroup)view;
            for(int i=0;i<group.getChildCount();++i) setControlsEnabled(group.getChildAt(i),enabled);
        }
    }
    private android.widget.PopupWindow toolbarWindow;
    private android.widget.PopupWindow settingsWindow;

    @Override protected void onNewIntent(android.content.Intent intent) {
        super.onNewIntent(intent);
        android.content.Intent previous=getIntent();
        boolean changed=false;
        for(String key:new String[]{"aether.ocean_preview","aether.map_preview",
                                   "aether.material_preview","aether.free_camera"}) {
            changed |= previous.getBooleanExtra(key,false)!=intent.getBooleanExtra(key,false);
        }
        if(changed) {
            // The shell's asset selection is immutable. Reject a conflicting
            // singleTask launch rather than pretend that its assets were loaded.
            android.util.Log.w("Aether.Android", "[SceneSelection] conflicting live launch rejected");
            android.widget.Toast.makeText(this,
                "Encerre a cena atual e selecione a próxima no menu.",
                android.widget.Toast.LENGTH_LONG).show();
        }
    }

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
                            steepness,micro,opacity,absorption,foam,interaction,bodyRippleGain,
                            roughness,turbidity,ior,direction,foamCompression,foamGrowth,foamDecay,
                            specularAntialiasing,contactFoamWidth,fluidDensity,waterPaused,
                            swellLength,directionalSpread,crossSwell,
                            waterLevel,longWaveAmplitude,longWaveLength);
    }
    private View slider(String name, float minimum, float maximum, float initial,
                        java.util.function.Consumer<Float> changed) {
        return slider(name,minimum,maximum,initial,1000,changed);
    }
    private View slider(String name, float minimum, float maximum, float initial,
                        int steps, java.util.function.Consumer<Float> changed) {
        LinearLayout row=new LinearLayout(this); row.setOrientation(LinearLayout.VERTICAL);
        TextView label=new TextView(this);
        label.setText(String.format(java.util.Locale.ROOT,"%s  ·  %.3f",name,initial));
        label.setTextColor(Color.rgb(223,230,232));
        label.setTextSize(12); row.addView(label);
        SeekBar seek=new SeekBar(this); seek.setMax(steps);
        seek.setProgress(Math.round((initial-minimum)/(maximum-minimum)*steps));
        seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar bar,int value,boolean user) {
                if(user) {
                    float resolved=minimum+(maximum-minimum)*value/steps;
                    label.setText(String.format(java.util.Locale.ROOT,"%s  ·  %.3f",name,resolved));
                    changed.accept(resolved); apply();
                }
            }
            public void onStartTrackingTouch(SeekBar bar) {}
            public void onStopTrackingTouch(SeekBar bar) {}
        }); row.addView(seek,new LinearLayout.LayoutParams(-1,dp(48))); return row;
    }
    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        // Test-scene defaults, not renderer policy: the user can change every
        // axis live. Keep the forest's existing defaults untouched.
        if(getIntent().getBooleanExtra("aether.ocean_preview",false)) {
            dynamic=false;
            waveHeight=3;
            crossSwell=.3f;
            roughness=.14f;
        }
        // Diagnostic launch overrides are authoritative in the native policy.
        // Reflect them here instead of displaying editable values ignored by it.
        boolean fixedScale=getIntent().hasExtra("aether.resolution_scale");
        boolean fixedDynamic=getIntent().getBooleanExtra("aether.dynamic_resolution",false) ||
            getIntent().getBooleanExtra("aether.disable_dynamic_resolution",false);
        if(fixedScale) {
            float requested=getIntent().getFloatExtra("aether.resolution_scale",1f);
            renderScale=Float.isFinite(requested)?Math.max(.5f,Math.min(1f,requested)):1f;
        }
        if(fixedDynamic) dynamic=!getIntent().getBooleanExtra("aether.disable_dynamic_resolution",false);
        Button settings=new Button(this); settings.setText("GRÁFICOS"); settings.setTextSize(11);
        settings.setTextColor(Color.WHITE); settings.setAllCaps(false);
        settings.setBackground(background(Color.argb(220,24,31,34),10));
        settings.setOnClickListener(view -> {
            refreshWaterProvider();
            if (settingsWindow.isShowing()) settingsWindow.dismiss();
            else settingsWindow.showAtLocation(getWindow().getDecorView(), Gravity.TOP|Gravity.END, dp(12), dp(72));
        });

        ScrollView scroll=new ScrollView(this); scroll.setFillViewport(true);
        panel=new LinearLayout(this); panel.setOrientation(LinearLayout.VERTICAL);
        panel.setPadding(dp(18),dp(12),dp(18),dp(18)); panel.setBackground(background(Color.argb(244,18,24,27),12));
        panel.addView(heading("RENDERIZAÇÃO"));
        View scaleControl=slider("Escala de render",.5f,1f,renderScale,v->renderScale=v);
        if(fixedScale) {setControlsEnabled(scaleControl,false);scaleControl.setAlpha(.5f);}
        panel.addView(scaleControl);
        if(fixedScale || fixedDynamic) panel.addView(heading("Overrides de lançamento · controles bloqueados"));
        panel.addView(slider("Sombras: 0 off / 3 máxima",0,3,shadows,3,v->shadows=Math.round(v)));
        Switch drs=new Switch(this); drs.setText("Resolução dinâmica"); drs.setTextColor(Color.WHITE);
        drs.setChecked(dynamic); drs.setOnCheckedChangeListener((button,value)->{dynamic=value;apply();}); panel.addView(drs);
        drs.setEnabled(!fixedDynamic);
        panel.addView(slider("Bloom",0,1,bloom,v->bloom=v));
        panel.addView(slider("Nitidez",0,1,sharpen,v->sharpen=v));
        if(getIntent().getBooleanExtra("aether.ocean_preview",false)) {
        panel.addView(heading("ÁGUA / ONDAS GPU"));
        if(!getIntent().getBooleanExtra("aether.water_fft",false)) {
            panel.addView(heading("LABORATÓRIO FÍSICO · BARCO E CORPOS"));
            panel.addView(slider("Comprimento do swell",.5f,4,swellLength,v->swellLength=v));
            panel.addView(slider("Dispersão direcional",0,2,directionalSpread,v->directionalSpread=v));
            panel.addView(slider("Swell cruzado",0,2,crossSwell,v->crossSwell=v));
            panel.addView(slider("Nível da água · metros",-20,40,waterLevel,v->waterLevel=v));
            panel.addView(slider("Onda longa · amplitude em metros",0,20,longWaveAmplitude,v->longWaveAmplitude=v));
            panel.addView(slider("Onda longa · comprimento em metros",120,2000,longWaveLength,v->longWaveLength=v));
            panel.addView(heading("ESTRESSE: ONDA LONGA, NÃO INUNDAÇÃO COSTEIRA"));
            panel.addView(slider("Densidade do fluido · kg/m³",500,2000,fluidDensity,v->fluidDensity=v));
            Switch pause=new Switch(this); pause.setText("Pausar água e corpos");
            pause.setTextColor(Color.WHITE); pause.setChecked(waterPaused);
            pause.setOnCheckedChangeListener((button,value)->{waterPaused=value;apply();});
            panel.addView(pause);
        }
        waterProviderLabel=heading("Água inativa / carregando"); panel.addView(waterProviderLabel);
        panel.addView(slider("Altura · multiplicador",0,getIntent().getBooleanExtra("aether.water_fft",false)?3:12,waveHeight,v->waveHeight=v));
        panel.addView(slider("Velocidade · multiplicador",0,3,waveSpeed,v->waveSpeed=v));
        panel.addView(slider("Direção do espectro · graus",-180,180,direction,v->direction=v));
        panel.addView(slider("Inclinação / cristas",0,2,steepness,v->steepness=v));
        panel.addView(slider("Micro-ondas",0,3,micro,v->micro=v));
        panel.addView(heading("ÓPTICA / MATERIAL"));
        panel.addView(slider("Rugosidade",.025f,1,roughness,v->roughness=v));
        panel.addView(slider("Filtro de brilho especular",0,1,specularAntialiasing,v->specularAntialiasing=v));
        panel.addView(slider("Índice de refração",1,2,ior,v->ior=v));
        panel.addView(slider("Turbidez",0,1,turbidity,v->turbidity=v));
        panel.addView(slider("Opacidade da água",.05f,1,opacity,v->opacity=v));
        panel.addView(slider("Absorção por profundidade",.1f,4,absorption,v->absorption=v));
        panel.addView(heading("ESPUMA / INTERAÇÃO"));
        panel.addView(slider("Espuma",0,2,foam,v->foam=v));
        panel.addView(slider("Espuma de contato · largura em metros",0,10,contactFoamWidth,v->contactFoamWidth=v));
        if(getIntent().getBooleanExtra("aether.water_fft",false)) {
            spectralFoamControls=new LinearLayout(this); spectralFoamControls.setOrientation(LinearLayout.VERTICAL);
            spectralFoamControls.addView(heading("ESPUMA ESPECTRAL · EXPERIMENTAL"));
            spectralFoamControls.addView(slider("Limiar de compressão",0,2,foamCompression,v->foamCompression=v));
            spectralFoamControls.addView(slider("Crescimento / segundo",0,20,foamGrowth,v->foamGrowth=v));
            spectralFoamControls.addView(slider("Dissipação / segundo",0,5,foamDecay,v->foamDecay=v));
            panel.addView(spectralFoamControls);
        }
        panel.addView(slider("Força da interação",0,2,interaction,v->interaction=v));
        panel.addView(slider("Resposta da água aos corpos",0,4,bodyRippleGain,
            v->bodyRippleGain=v));
        TextView hint=heading("Toque curto na água gera uma onda local.");
        panel.addView(hint);
        }
        Button scenes=new Button(this); scenes.setText("Voltar às cenas"); scenes.setAllCaps(false);
        scenes.setOnClickListener(view->finish()); panel.addView(scenes);
        scroll.addView(panel);
        // NativeActivity owns the main window surface; ordinary content views
        // share that surface with Vulkan. Attached windows compose above it.
        toolbarWindow=new android.widget.PopupWindow(settings,dp(112),dp(48),false);
        toolbarWindow.setBackgroundDrawable(new android.graphics.drawable.ColorDrawable(Color.TRANSPARENT));
        settingsWindow=new android.widget.PopupWindow(scroll,
            Math.min(dp(390),getResources().getDisplayMetrics().widthPixels-dp(24)),
            Math.max(dp(160),getResources().getDisplayMetrics().heightPixels-dp(86)),false);
        settingsWindow.setBackgroundDrawable(new android.graphics.drawable.ColorDrawable(Color.TRANSPARENT));
        settingsWindow.setOutsideTouchable(true);
        apply();
    }

    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if(focused && toolbarWindow!=null && !toolbarWindow.isShowing())
            toolbarWindow.showAtLocation(getWindow().getDecorView(),Gravity.TOP|Gravity.START,dp(18),dp(16));
    }

    @Override protected void onPause() {
        if(settingsWindow!=null) settingsWindow.dismiss();
        if(toolbarWindow!=null) toolbarWindow.dismiss();
        super.onPause();
    }
}
