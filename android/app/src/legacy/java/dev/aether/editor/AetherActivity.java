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
    private EditorTextInput editorTextInput;
    @Override protected void onResume() {
        super.onResume();
        if(getIntent().getBooleanExtra("aether.editor_ui",false)) {
            if(editorTextInput==null) editorTextInput=new EditorTextInput(this);
            editorTextInput.start();
        }
    }
    private static final String WATER_PREFERENCES="water_controls_v3";
    static { System.loadLibrary("aether_android"); }
    private static native int nativeWaterProviderStatus();
    private static native void nativeApplyVisibilityControls(float solid,float foliage,float transition);
    private float solidLodError=0,foliageLodError=0,lodTransition=0;
    private static native void nativeApplyControls(float renderScale, int shadowQuality,
        boolean dynamicResolution, float bloomIntensity, float sharpen, float waveHeight,
        float waveSpeed, float waveSteepness, float microWaves, float surfaceOpacity,
        float absorption, float foam, float interactionStrength, float bodyRippleGain, float roughness,
        float turbidity, float ior, float direction, float foamCompression,float foamGrowth,float foamDecay,
        float specularAntialiasing,float contactFoamWidth,
        float foamElevation,float foamCoverage,float microDisplacement,float microWavelength,
        float wakeStrength,float wakeMinimumSpeed,float wakeSpacing,float wakeWidthScale,
        float wakeMaximumImpulse,
        float fluidDensity,boolean waterPaused,
        float swellLength,float directionalSpread,float crossSwell,
        float waterLevel,float longWaveAmplitude,float longWaveLength,
        float spectralWindSpeed,float spectralFetchKm,float spectralDepth,
        float spectralSwell,float spectralSpread,float spectralDamping,
        float crossWindSpeed,float crossDirection,float crossFetchKm,
        float crossSwellShape,float crossSpread,float crossWeight,
        float nearDisplacement,float midDisplacement,float farDisplacement,
        float nearChoppiness,float midChoppiness,float farChoppiness);

    private float renderScale=1, bloom=.08f, sharpen=.12f, waveHeight=1, waveSpeed=1;
    private float steepness=1, micro=1.6f, opacity=.72f, absorption=1, foam=1.05f, interaction=.65f;
    private float bodyRippleGain=.75f;
    private int shadows=3; private boolean dynamic=true;
    private float roughness=.22f, turbidity=.10f, ior=1.333f, direction=0;
    private float foamCompression=.8f,foamGrowth=4,foamDecay=.5f;
    private float specularAntialiasing=.5f,contactFoamWidth=1.35f;
    private float foamElevation=.14f,foamCoverage=1.35f;
    private float microDisplacement=.10f,microWavelength=.85f;
    private float wakeStrength=1.2f,wakeMinimumSpeed=.1f,wakeSpacing=2,wakeWidthScale=.22f;
    private float wakeMaximumImpulse=1.2f;
    private float fluidDensity=1400;
    private boolean waterPaused=false;
    private float swellLength=1,directionalSpread=1,crossSwell=0;
    private float waterLevel=0,longWaveAmplitude=0,longWaveLength=320;
    private float spectralWindSpeed=10,spectralFetchKm=100,spectralDepth=20;
    private float spectralSwell=.8f,spectralSpread=.2f,spectralDamping=.1f;
    private float crossWindSpeed=0,crossDirection=65,crossFetchKm=100;
    private float crossSwellShape=1,crossSpread=.1f,crossWeight=.35f;
    private float nearDisplacement=1,midDisplacement=1,farDisplacement=1;
    private float nearChoppiness=1,midChoppiness=1,farChoppiness=1;
    private LinearLayout panel;
    private TextView waterProviderLabel;
    private LinearLayout spectralFoamControls;
    private void refreshWaterProvider() {
        int status=nativeWaterProviderStatus();
        if(waterProviderLabel!=null) waterProviderLabel.setText(status==2?"FFT GPU + física espectral":
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
    private ScrollView settingsScroll;

    private float savedWater(android.content.SharedPreferences values,String key,float fallback) {
        float value=values.getFloat(key,fallback);
        return Float.isFinite(value)?value:fallback;
    }
    private void loadWaterControls() {
        android.content.SharedPreferences p=getSharedPreferences(WATER_PREFERENCES,MODE_PRIVATE);
        waveHeight=savedWater(p,"waveHeight",waveHeight); waveSpeed=savedWater(p,"waveSpeed",waveSpeed);
        steepness=savedWater(p,"steepness",steepness); micro=savedWater(p,"micro",micro);
        opacity=savedWater(p,"opacity",opacity); absorption=savedWater(p,"absorption",absorption);
        foam=savedWater(p,"foam",foam); interaction=savedWater(p,"interaction",interaction);
        bodyRippleGain=savedWater(p,"bodyRippleGain",bodyRippleGain);
        roughness=savedWater(p,"roughness",roughness); turbidity=savedWater(p,"turbidity",turbidity);
        ior=savedWater(p,"ior",ior); direction=savedWater(p,"direction",direction);
        foamCompression=savedWater(p,"foamCompression",foamCompression);
        foamGrowth=savedWater(p,"foamGrowth",foamGrowth); foamDecay=savedWater(p,"foamDecay",foamDecay);
        specularAntialiasing=savedWater(p,"specularAntialiasing",specularAntialiasing);
        contactFoamWidth=savedWater(p,"contactFoamWidth",contactFoamWidth);
        foamElevation=savedWater(p,"foamElevation",foamElevation);
        foamCoverage=savedWater(p,"foamCoverage",foamCoverage);
        microDisplacement=savedWater(p,"microDisplacement",microDisplacement);
        microWavelength=savedWater(p,"microWavelength",microWavelength);
        wakeStrength=savedWater(p,"wakeStrength",wakeStrength);
        wakeMinimumSpeed=savedWater(p,"wakeMinimumSpeed",wakeMinimumSpeed);
        wakeSpacing=savedWater(p,"wakeSpacing",wakeSpacing);
        wakeWidthScale=savedWater(p,"wakeWidthScale",wakeWidthScale);
        wakeMaximumImpulse=savedWater(p,"wakeMaximumImpulse",wakeMaximumImpulse);
        fluidDensity=savedWater(p,"fluidDensity",fluidDensity);
        swellLength=savedWater(p,"swellLength",swellLength);
        directionalSpread=savedWater(p,"directionalSpread",directionalSpread);
        crossSwell=savedWater(p,"crossSwell",crossSwell); waterLevel=savedWater(p,"waterLevel",waterLevel);
        longWaveAmplitude=savedWater(p,"longWaveAmplitude",longWaveAmplitude);
        longWaveLength=savedWater(p,"longWaveLength",longWaveLength);
        spectralWindSpeed=savedWater(p,"spectralWindSpeed",spectralWindSpeed);
        spectralFetchKm=savedWater(p,"spectralFetchKm",spectralFetchKm);
        spectralDepth=savedWater(p,"spectralDepth",spectralDepth);
        spectralSwell=savedWater(p,"spectralSwell",spectralSwell);
        spectralSpread=savedWater(p,"spectralSpread",spectralSpread);
        spectralDamping=savedWater(p,"spectralDamping",spectralDamping);
        crossWindSpeed=savedWater(p,"crossWindSpeed",crossWindSpeed);
        crossDirection=savedWater(p,"crossDirection",crossDirection);
        crossFetchKm=savedWater(p,"crossFetchKm",crossFetchKm);
        crossSwellShape=savedWater(p,"crossSwellShape",crossSwellShape);
        crossSpread=savedWater(p,"crossSpread",crossSpread); crossWeight=savedWater(p,"crossWeight",crossWeight);
        nearDisplacement=savedWater(p,"nearDisplacement",nearDisplacement);
        midDisplacement=savedWater(p,"midDisplacement",midDisplacement);
        farDisplacement=savedWater(p,"farDisplacement",farDisplacement);
        nearChoppiness=savedWater(p,"nearChoppiness",nearChoppiness);
        midChoppiness=savedWater(p,"midChoppiness",midChoppiness);
        farChoppiness=savedWater(p,"farChoppiness",farChoppiness);
    }
    private void persistWaterControls() {
        if(!getIntent().getBooleanExtra("aether.ocean_preview",false)) return;
        getSharedPreferences(WATER_PREFERENCES,MODE_PRIVATE).edit()
            .putFloat("waveHeight",waveHeight).putFloat("waveSpeed",waveSpeed)
            .putFloat("steepness",steepness).putFloat("micro",micro)
            .putFloat("opacity",opacity).putFloat("absorption",absorption)
            .putFloat("foam",foam).putFloat("interaction",interaction)
            .putFloat("bodyRippleGain",bodyRippleGain).putFloat("roughness",roughness)
            .putFloat("turbidity",turbidity).putFloat("ior",ior).putFloat("direction",direction)
            .putFloat("foamCompression",foamCompression).putFloat("foamGrowth",foamGrowth)
            .putFloat("foamDecay",foamDecay).putFloat("specularAntialiasing",specularAntialiasing)
            .putFloat("contactFoamWidth",contactFoamWidth).putFloat("foamElevation",foamElevation)
            .putFloat("foamCoverage",foamCoverage).putFloat("microDisplacement",microDisplacement)
            .putFloat("microWavelength",microWavelength).putFloat("wakeStrength",wakeStrength)
            .putFloat("wakeMinimumSpeed",wakeMinimumSpeed).putFloat("wakeSpacing",wakeSpacing)
            .putFloat("wakeWidthScale",wakeWidthScale).putFloat("wakeMaximumImpulse",wakeMaximumImpulse)
            .putFloat("fluidDensity",fluidDensity).putFloat("swellLength",swellLength)
            .putFloat("directionalSpread",directionalSpread).putFloat("crossSwell",crossSwell)
            .putFloat("waterLevel",waterLevel).putFloat("longWaveAmplitude",longWaveAmplitude)
            .putFloat("longWaveLength",longWaveLength).putFloat("spectralWindSpeed",spectralWindSpeed)
            .putFloat("spectralFetchKm",spectralFetchKm).putFloat("spectralDepth",spectralDepth)
            .putFloat("spectralSwell",spectralSwell).putFloat("spectralSpread",spectralSpread)
            .putFloat("spectralDamping",spectralDamping).putFloat("crossWindSpeed",crossWindSpeed)
            .putFloat("crossDirection",crossDirection).putFloat("crossFetchKm",crossFetchKm)
            .putFloat("crossSwellShape",crossSwellShape).putFloat("crossSpread",crossSpread)
            .putFloat("crossWeight",crossWeight).putFloat("nearDisplacement",nearDisplacement)
            .putFloat("midDisplacement",midDisplacement).putFloat("farDisplacement",farDisplacement)
            .putFloat("nearChoppiness",nearChoppiness).putFloat("midChoppiness",midChoppiness)
            .putFloat("farChoppiness",farChoppiness).apply();
    }

    @Override protected void onNewIntent(android.content.Intent intent) {
        super.onNewIntent(intent);
        android.content.Intent previous=getIntent();
        boolean changed=!java.util.Objects.equals(previous.getStringExtra("astra.project_path"),
                                                   intent.getStringExtra("astra.project_path"));
        for(String key:new String[]{"aether.ocean_preview","aether.map_preview",
                                   "aether.material_preview","aether.free_camera","aether.water_fft",
                                   "aether.empty_workspace","aether.editor_empty","aether.editor_ui"}) {
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
        nativeApplyVisibilityControls(solidLodError,foliageLodError,lodTransition);
        nativeApplyControls(renderScale,shadows,dynamic,bloom,sharpen,waveHeight,waveSpeed,
                            steepness,micro,opacity,absorption,foam,interaction,bodyRippleGain,
                            roughness,turbidity,ior,direction,foamCompression,foamGrowth,foamDecay,
                            specularAntialiasing,contactFoamWidth,
                            foamElevation,foamCoverage,microDisplacement,microWavelength,
                            wakeStrength,wakeMinimumSpeed,wakeSpacing,wakeWidthScale,wakeMaximumImpulse,
                            fluidDensity,waterPaused,
                            swellLength,directionalSpread,crossSwell,
                            waterLevel,longWaveAmplitude,longWaveLength,
                            spectralWindSpeed,spectralFetchKm,spectralDepth,
                            spectralSwell,spectralSpread,spectralDamping,
                            crossWindSpeed,crossDirection,crossFetchKm,
                            crossSwellShape,crossSpread,crossWeight,
                            nearDisplacement,midDisplacement,farDisplacement,
                            nearChoppiness,midChoppiness,farChoppiness);
    }
    private View slider(String name, float minimum, float maximum, float initial,
                        java.util.function.Consumer<Float> changed) {
        return slider(name,minimum,maximum,initial,1000,changed);
    }
    private View slider(String name, float minimum, float maximum, float initial,
                        int steps, java.util.function.Consumer<Float> changed) {
        return slider(name,minimum,maximum,initial,steps,true,changed);
    }
    private View deferredSlider(String name,float minimum,float maximum,float initial,
                                java.util.function.Consumer<Float> changed) {
        return slider(name,minimum,maximum,initial,1000,false,changed);
    }
    private View slider(String name,float minimum,float maximum,float initial,int steps,
                        boolean live,java.util.function.Consumer<Float> changed) {
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
                    changed.accept(resolved); if(live) apply();
                }
            }
            public void onStartTrackingTouch(SeekBar bar) {}
            public void onStopTrackingTouch(SeekBar bar) { if(!live) apply(); persistWaterControls(); }
        }); row.addView(seek,new LinearLayout.LayoutParams(-1,dp(48))); return row;
    }
    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        android.content.SharedPreferences visibility=getSharedPreferences("render_visibility_v1",MODE_PRIVATE);
        solidLodError=savedWater(visibility,"solidError",0);
        foliageLodError=savedWater(visibility,"foliageError",0);
        lodTransition=savedWater(visibility,"transition",0);
        // Test-scene defaults, not renderer policy: the user can change every
        // axis live. Keep the forest's existing defaults untouched.
        if(getIntent().getBooleanExtra("aether.ocean_preview",false)) {
            dynamic=false;
            waveHeight=3;
            crossSwell=.3f;
            roughness=.14f;
            loadWaterControls();
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
            else {
                // A raiz recebe o foco, não um SeekBar arbitrário. Isso impede
                // o Android de rolar até um slider e evita um painel que parece
                // ter perdido as configurações acima dele.
                settingsScroll.scrollTo(0,0);
                settingsScroll.requestFocus();
                settingsWindow.showAtLocation(getWindow().getDecorView(), Gravity.TOP|Gravity.END, dp(12), dp(72));
                settingsScroll.post(() -> settingsScroll.scrollTo(0,0));
            }
        });

        ScrollView scroll=new ScrollView(this); scroll.setFillViewport(true);
        settingsScroll=scroll;
        settingsScroll.setFocusableInTouchMode(true);
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
        panel.addView(heading("DETALHE À DISTÂNCIA · GLOBAL"));
        panel.addView(heading("0 herda projeto · menor erro preserva mais detalhe"));
        panel.addView(deferredSlider("Erro geométrico · pixels",0,16,solidLodError,v->solidLodError=v));
        panel.addView(deferredSlider("Erro de vegetação · pixels",0,128,foliageLodError,v->foliageLodError=v));
        panel.addView(deferredSlider("Margem de transição LOD",0,.99f,lodTransition,v->lodTransition=v));
        if(getIntent().getBooleanExtra("aether.ocean_preview",false)) {
        boolean spectral=getIntent().getBooleanExtra("aether.water_fft",false);
        panel.addView(heading("ÁGUA / ONDAS GPU"));
        panel.addView(heading("LABORATÓRIO FÍSICO · BARCO E CORPOS"));
        panel.addView(slider("Nível da água · metros",-20,40,waterLevel,v->waterLevel=v));
        if(!spectral) {
            panel.addView(heading("ONDAS ANALÍTICAS"));
            panel.addView(slider("Comprimento do swell",.5f,4,swellLength,v->swellLength=v));
            panel.addView(slider("Dispersão direcional",0,2,directionalSpread,v->directionalSpread=v));
            panel.addView(slider("Swell cruzado",0,2,crossSwell,v->crossSwell=v));
            panel.addView(slider("Onda longa · amplitude em metros",0,20,longWaveAmplitude,v->longWaveAmplitude=v));
            panel.addView(slider("Onda longa · comprimento em metros",120,2000,longWaveLength,v->longWaveLength=v));
            panel.addView(heading("ESTRESSE: ONDA LONGA, NÃO INUNDAÇÃO COSTEIRA"));
        } else {
            panel.addView(heading("ESPECTRO FÍSICO · REBUILD AO SOLTAR"));
            panel.addView(deferredSlider("Vento principal · m/s",0,40,spectralWindSpeed,v->spectralWindSpeed=v));
            panel.addView(deferredSlider("Fetch principal · km",.1f,2000,spectralFetchKm,v->spectralFetchKm=v));
            panel.addView(deferredSlider("Profundidade espectral · m",1,500,spectralDepth,v->spectralDepth=v));
            panel.addView(deferredSlider("Organização do swell",0,2,spectralSwell,v->spectralSwell=v));
            panel.addView(deferredSlider("Dispersão angular",0,1,spectralSpread,v->spectralSpread=v));
            panel.addView(deferredSlider("Amortecimento de ondas curtas · m",0,2,spectralDamping,v->spectralDamping=v));
            panel.addView(heading("MAR CRUZADO · SEGUNDO TREM"));
            panel.addView(deferredSlider("Vento cruzado · m/s",0,40,crossWindSpeed,v->crossWindSpeed=v));
            panel.addView(deferredSlider("Direção cruzada relativa · graus",-180,180,crossDirection,v->crossDirection=v));
            panel.addView(deferredSlider("Fetch cruzado · km",.1f,2000,crossFetchKm,v->crossFetchKm=v));
            panel.addView(deferredSlider("Organização do swell cruzado",0,2,crossSwellShape,v->crossSwellShape=v));
            panel.addView(deferredSlider("Dispersão do mar cruzado",0,1,crossSpread,v->crossSpread=v));
            panel.addView(deferredSlider("Energia do mar cruzado",0,1,crossWeight,v->crossWeight=v));
            panel.addView(heading("BANDAS FFT · 2–8 / 8–32 / 32–2048 M"));
            panel.addView(deferredSlider("Deslocamento da banda próxima",0,3,nearDisplacement,v->nearDisplacement=v));
            panel.addView(deferredSlider("Deslocamento da banda média",0,3,midDisplacement,v->midDisplacement=v));
            panel.addView(deferredSlider("Deslocamento da banda longa",0,3,farDisplacement,v->farDisplacement=v));
            panel.addView(deferredSlider("Cristas da banda próxima",0,4,nearChoppiness,v->nearChoppiness=v));
            panel.addView(deferredSlider("Cristas da banda média",0,4,midChoppiness,v->midChoppiness=v));
            panel.addView(deferredSlider("Cristas da banda longa",0,4,farChoppiness,v->farChoppiness=v));
        }
        panel.addView(slider("Densidade do fluido · kg/m³",500,2000,fluidDensity,v->fluidDensity=v));
        Switch pause=new Switch(this); pause.setText("Pausar água e corpos");
        pause.setTextColor(Color.WHITE); pause.setChecked(waterPaused);
        pause.setOnCheckedChangeListener((button,value)->{waterPaused=value;apply();});
        panel.addView(pause);
        waterProviderLabel=heading("Água inativa / carregando"); panel.addView(waterProviderLabel);
        panel.addView(slider("Altura · multiplicador",0,spectral?3:12,waveHeight,v->waveHeight=v));
        panel.addView(slider("Velocidade · multiplicador",0,3,waveSpeed,v->waveSpeed=v));
        panel.addView(slider("Direção do espectro · graus",-180,180,direction,v->direction=v));
        panel.addView(slider("Inclinação / cristas",0,2,steepness,v->steepness=v));
        panel.addView(slider("Micro-ondas · normal",0,4,micro,v->micro=v));
        panel.addView(slider("Relevo micro geométrico · metros",0,1,microDisplacement,
            v->microDisplacement=v));
        panel.addView(slider("Comprimento do relevo micro · metros",.2f,8,microWavelength,
            v->microWavelength=v));
        panel.addView(heading("ÓPTICA / MATERIAL"));
        panel.addView(slider("Rugosidade",.025f,1,roughness,v->roughness=v));
        panel.addView(slider("Filtro de brilho especular",0,1,specularAntialiasing,v->specularAntialiasing=v));
        panel.addView(slider("Índice de refração",1,2,ior,v->ior=v));
        panel.addView(slider("Turbidez",0,1,turbidity,v->turbidity=v));
        panel.addView(slider("Opacidade da água",.05f,1,opacity,v->opacity=v));
        panel.addView(slider("Absorção por profundidade",.1f,4,absorption,v->absorption=v));
        panel.addView(heading("ESPUMA / INTERAÇÃO"));
        panel.addView(slider("Espuma · intensidade",0,3,foam,v->foam=v));
        panel.addView(slider("Cobertura da espuma",0,4,foamCoverage,v->foamCoverage=v));
        panel.addView(slider("Elevação física visual da espuma · metros",0,1,foamElevation,
            v->foamElevation=v));
        panel.addView(slider("Espuma de contato · largura em metros",0,10,contactFoamWidth,v->contactFoamWidth=v));
        if(spectral) {
            spectralFoamControls=new LinearLayout(this); spectralFoamControls.setOrientation(LinearLayout.VERTICAL);
            spectralFoamControls.addView(heading("ESPUMA ESPECTRAL"));
            spectralFoamControls.addView(slider("Limiar de compressão",0,2,foamCompression,v->foamCompression=v));
            spectralFoamControls.addView(slider("Crescimento / segundo",0,20,foamGrowth,v->foamGrowth=v));
            spectralFoamControls.addView(slider("Dissipação / segundo",0,5,foamDecay,v->foamDecay=v));
            panel.addView(spectralFoamControls);
        }
        panel.addView(slider("Força da interação",0,2,interaction,v->interaction=v));
        panel.addView(slider("Resposta da água aos corpos",0,4,bodyRippleGain,
            v->bodyRippleGain=v));
        panel.addView(heading("ESTEIRA NAVAL · EMISSÃO POR DISTÂNCIA"));
        panel.addView(slider("Força da esteira",0,8,wakeStrength,v->wakeStrength=v));
        panel.addView(slider("Velocidade mínima da esteira · m/s",0,10,wakeMinimumSpeed,
            v->wakeMinimumSpeed=v));
        panel.addView(slider("Espaçamento entre seções · metros",.5f,100,wakeSpacing,
            v->wakeSpacing=v));
        panel.addView(slider("Largura relativa da esteira",.05f,2,wakeWidthScale,
            v->wakeWidthScale=v));
        panel.addView(slider("Limite de impulso da esteira · metros",.05f,5,wakeMaximumImpulse,
            v->wakeMaximumImpulse=v));
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
        if(focused && !getIntent().getBooleanExtra("aether.editor_ui",false) && toolbarWindow!=null && !toolbarWindow.isShowing())
            toolbarWindow.showAtLocation(getWindow().getDecorView(),Gravity.TOP|Gravity.START,dp(18),dp(16));
    }

    @Override protected void onPause() {
        if(editorTextInput!=null) editorTextInput.stop();
        getSharedPreferences("render_visibility_v1",MODE_PRIVATE).edit()
            .putFloat("solidError",solidLodError).putFloat("foliageError",foliageLodError)
            .putFloat("transition",lodTransition).apply();
        persistWaterControls();
        if(settingsWindow!=null) settingsWindow.dismiss();
        if(toolbarWindow!=null) toolbarWindow.dismiss();
        super.onPause();
    }
}
