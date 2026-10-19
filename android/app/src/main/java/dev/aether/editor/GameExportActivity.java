package dev.aether.editor;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.widget.*;
import dev.aether.editor.shell.Design;
import dev.aether.export.*;
import java.io.*;
import java.util.Arrays;
import java.util.concurrent.*;

/** Focused export route; signing material stays outside project payloads. */
public final class GameExportActivity extends Activity {
    private static final int IMPORT_KEY=10,SAVE_KEY=11,SAVE_APK=12;
    private final ExecutorService worker=Executors.newSingleThreadExecutor();
    private LinearLayout body,actions;private TextView status,stepLabel;
    private EditText label,id,version,code,alias,password;
    private String gameLabel,applicationId="com.studio.game",versionName="1.0",keyAlias="game";
    private int versionCode=1,step;private File keyFile,apkFile;private boolean busy,keyBackedUp;
    private volatile boolean destroyed;
    private final String[] steps={"1  Identidade","2  Assinatura","3  Gerar APK"};

    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);gameLabel=getIntent().getStringExtra("astra.project_name");
        if(gameLabel==null)gameLabel="Meu jogo";
        LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setPadding(dp(24),dp(12),dp(24),dp(12));root.setBackgroundColor(Design.CANVAS);
        LinearLayout header=new LinearLayout(this);header.setGravity(Gravity.CENTER_VERTICAL);
        Button back=button("Projetos",()->finish());header.addView(back,new LinearLayout.LayoutParams(dp(120),dp(48)));
        TextView title=text("EXPORTAR JOGO  /  "+gameLabel,20,Design.TEXT);header.addView(title);root.addView(header);
        LinearLayout content=new LinearLayout(this);
        LinearLayout rail=new LinearLayout(this);rail.setOrientation(LinearLayout.VERTICAL);rail.setPadding(0,dp(20),dp(24),0);
        stepLabel=text("",19,Design.ACCENT);rail.addView(stepLabel);
        rail.addView(text("Cena inicial\nscenes/editor.aescene\n\nExporta o projeto salvo, incluindo recursos e scripts. Salve no editor antes de exportar.",14,Design.DIM));
        content.addView(rail,new LinearLayout.LayoutParams(dp(230),-1));
        ScrollView scroll=new ScrollView(this);body=new LinearLayout(this);body.setOrientation(LinearLayout.VERTICAL);scroll.addView(body);
        content.addView(scroll,new LinearLayout.LayoutParams(0,-1,1));root.addView(content,new LinearLayout.LayoutParams(-1,0,1));
        status=text("",14,Design.DIM);root.addView(status);
        actions=new LinearLayout(this);actions.setGravity(Gravity.END);root.addView(actions);setContentView(root);showIdentity();
    }
    private int dp(int value){return Math.round(value*getResources().getDisplayMetrics().density);}
    private TextView text(String value,int size,int color){TextView view=new TextView(this);view.setText(value);view.setTextSize(size);view.setTextColor(color);view.setPadding(0,dp(8),dp(12),dp(8));return view;}
    private Button button(String title,Runnable action){Button button=new Button(this);button.setText(title);button.setAllCaps(false);button.setOnClickListener(v->{if(!busy)action.run();});return button;}
    private EditText field(String title,String value,int type){body.addView(text(title,13,Design.DIM));EditText field=new EditText(this);field.setTextColor(Design.TEXT);field.setSingleLine(true);field.setInputType(type);field.setText(value);body.addView(field,new LinearLayout.LayoutParams(-1,dp(48)));return field;}
    private void page(int page){step=page;stepLabel.setText(steps[page]);body.removeAllViews();actions.removeAllViews();status.setText("");}
    private void showIdentity(){
        page(0);label=field("Nome no Android",gameLabel,InputType.TYPE_CLASS_TEXT);
        id=field("Identificador exclusivo — permanece igual nas atualizações",applicationId,InputType.TYPE_CLASS_TEXT);
        version=field("Versão visível",versionName,InputType.TYPE_CLASS_TEXT);code=field("Número da versão — aumente a cada atualização",Integer.toString(versionCode),InputType.TYPE_CLASS_NUMBER);
        actions.addView(button("Assinatura →",()->{
            try {
                ApkExporter.Settings settings=new ApkExporter.Settings(id.getText().toString().trim(),label.getText().toString().trim(),Integer.parseInt(code.getText().toString()),version.getText().toString().trim());
                applicationId=settings.applicationId;gameLabel=settings.label;versionCode=settings.versionCode;versionName=settings.versionName;showSigning();
            }catch(Exception error){status.setText(error.getMessage());}
        }));
    }
    private void showSigning(){
        page(1);body.addView(text("Use a mesma chave para atualizar o jogo. A chave pode ser criada aqui ou importada de um arquivo PKCS#12 (.p12).",16,Design.TEXT));
        alias=field("Alias da chave",keyAlias,InputType.TYPE_CLASS_TEXT);
        password=field("Senha da chave — não é salva", "",InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_VARIATION_PASSWORD);
        password.setSaveEnabled(false);
        body.addView(button("Importar chave .p12",()->{
            Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("*/*").addCategory(Intent.CATEGORY_OPENABLE);startActivityForResult(intent,IMPORT_KEY);
        }));
        body.addView(button("Criar chave",this::createKey));
        body.addView(button("Guardar cópia da chave",()->{if(keyFile==null){status.setText("Crie ou importe a chave primeiro.");return;}saveDocument(SAVE_KEY,"application/x-pkcs12",applicationId+".p12");}));
        status.setText(keyFile==null?"Nenhuma chave selecionada.":"Chave selecionada. Informe a senha novamente.");
        actions.addView(button("← Identidade",this::showIdentity));
        actions.addView(button("Gerar APK →",this::export));
    }
    private void createKey(){
        if(keyFile!=null){status.setText("Uma chave já está selecionada. Volte aos projetos para iniciar uma nova exportação.");return;}
        keyAlias=alias.getText().toString().trim();char[] secret=password.getText().toString().toCharArray();
        submit(()->{
            File directory=new File(getFilesDir(),"export-keys");if(!directory.isDirectory()&&!directory.mkdirs())throw new IOException("Cannot create signing key directory");
            File created=new File(directory,applicationId+"-"+System.currentTimeMillis()+".p12");GameSigningKey.create(created,keyAlias,secret);
            keyFile=created;keyBackedUp=false;return "Chave criada. Guarde uma cópia antes de gerar o APK.";
        },secret);
    }
    private void export(){
        if(keyFile==null||!keyBackedUp){status.setText("Importe uma chave ou guarde uma cópia da chave criada antes de exportar.");return;}
        keyAlias=alias.getText().toString().trim();char[] secret=password.getText().toString().toCharArray();password.setText("");
        ApkExporter.Settings settings=new ApkExporter.Settings(applicationId,gameLabel,versionCode,versionName);
        File project=new File(getIntent().getStringExtra("astra.project_path"));
        page(2);body.addView(text(gameLabel+"\n"+applicationId+"\nVersão "+versionName+" ("+versionCode+")",22,Design.TEXT));
        body.addView(text("Nesta versão o player compartilha as bibliotecas da engine. Os scripts salvos são incluídos e compilados ao iniciar; não há remoção automática de recursos sem uso.",14,Design.DIM));
        ProgressBar progress=new ProgressBar(this);body.addView(progress);
        submit(()->{
            File folder=new File(getExternalFilesDir(null),"Exports");if(!folder.isDirectory()&&!folder.mkdirs())throw new IOException("Cannot create exports folder");
            File output=new File(folder,applicationId+"-"+versionCode+"-"+System.currentTimeMillis()+".apk");
            ApkExporter.export(new File(getApplicationInfo().sourceDir),project,output,getCacheDir(),settings,
                ApkExporter.signer(keyFile,"PKCS12",secret,keyAlias,secret),message->runOnUiThread(()->{if(!destroyed)status.setText(message);}));
            apkFile=output;return "APK assinado e verificado. Salve o arquivo para instalar ou compartilhar.";
        },secret,()->{
            progress.setVisibility(View.GONE);actions.removeAllViews();
            if(apkFile!=null)actions.addView(button("Salvar APK",()->saveDocument(SAVE_APK,"application/vnd.android.package-archive",applicationId+"-"+versionCode+".apk")));
            else actions.addView(button("← Corrigir assinatura",this::showSigning));
        });
    }
    private interface Work {String run() throws Exception;}
    private void submit(Work work,char[] secret){submit(work,secret,()->{});}
    private void submit(Work work,char[] secret,Runnable done){
        busy=true;status.setText("Processando…");
        worker.submit(()->{String message;try{message=work.run();}catch(Exception error){message="Falha: "+error.getMessage();android.util.Log.w("Astra.Export","Export operation failed",error);}finally{if(secret!=null)Arrays.fill(secret,'\0');}
            final String result=message;runOnUiThread(()->{if(destroyed)return;busy=false;status.setText(result);done.run();});});
    }
    private void saveDocument(int request,String type,String name){startActivityForResult(new Intent(Intent.ACTION_CREATE_DOCUMENT).setType(type).addCategory(Intent.CATEGORY_OPENABLE).putExtra(Intent.EXTRA_TITLE,name),request);}
    @Override protected void onActivityResult(int request,int result,Intent data){
        super.onActivityResult(request,result,data);if(result!=RESULT_OK||data==null||data.getData()==null)return;Uri uri=data.getData();
        if(request==IMPORT_KEY)submit(()->{
            File directory=new File(getFilesDir(),"export-keys");directory.mkdirs();File copy=File.createTempFile("import-",".p12",directory);
            try(InputStream in=getContentResolver().openInputStream(uri);OutputStream out=new FileOutputStream(copy)){copy(in,out,4*1024*1024);}
            keyFile=copy;keyBackedUp=true;return "Chave importada. Informe o alias e a senha.";
        },null);
        else if(request==SAVE_KEY||request==SAVE_APK)submit(()->{
            File source=request==SAVE_KEY?keyFile:apkFile;if(source==null)throw new IOException("No file to save");
            try(InputStream in=new FileInputStream(source);OutputStream out=getContentResolver().openOutputStream(uri,"wt")){copy(in,out,Long.MAX_VALUE);}
            if(request==SAVE_KEY)keyBackedUp=true;return request==SAVE_KEY?"Cópia da chave salva. Pronto para gerar APK.":"APK salvo.";
        },null);
    }
    private static void copy(InputStream in,OutputStream out,long limit)throws IOException{
        if(in==null||out==null)throw new IOException("Cannot access selected file");byte[] buffer=new byte[65536];int n;long count=0;
        while((n=in.read(buffer))!=-1){count+=n;if(count>limit)throw new IOException("File too large");out.write(buffer,0,n);}
    }
    @Override public void onBackPressed(){if(busy){status.setText("Aguarde a operação atual terminar.");return;}super.onBackPressed();}
    @Override protected void onDestroy(){destroyed=true;worker.shutdownNow();super.onDestroy();}
}
