package dev.aether.editor;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.widget.TextView;
import android.graphics.Color;
import android.view.Gravity;
import dev.aether.export.GamePackage;
import java.io.File;

/** Exported games enter here. The editor launcher is absent from their manifest. */
public final class GameBootstrapActivity extends Activity {
    private static final Object PACKAGE_LOCK=new Object();
    private Thread preparation;
    private volatile boolean stopped;
    private TextView status;
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        status=new TextView(this);status.setTextColor(Color.WHITE);status.setBackgroundColor(Color.rgb(17,21,26));
        status.setGravity(Gravity.CENTER);status.setTextSize(18);status.setPadding(32,32,32,32);
        status.setText("Preparando o jogo…");setContentView(status);
        preparation=new Thread(()->{synchronized(PACKAGE_LOCK){
            if(stopped)return;
            try {
                File apk=new File(getApplicationInfo().sourceDir);
                String identity=GamePackage.sha256(apk);
                File base=new File(getFilesDir(),"game-packages");
                File project=new File(base,identity),complete=new File(project,".package-complete");
                if(!complete.isFile()) {
                    base.mkdirs();File stage=new File(base,identity+".staging");
                    // Only this private, fixed staging directory is discarded after an interrupted extraction.
                    GamePackage.deleteTree(stage);GamePackage.extract(apk,stage);
                    if(!new File(stage,".package-complete").createNewFile())throw new java.io.IOException("Cannot finalize game package");
                    if(project.exists())throw new java.io.IOException("Incomplete game installation; clear this game's data and retry");
                    if(!stage.renameTo(project))throw new java.io.IOException("Cannot publish game package");
                }
                runOnUiThread(()->{
                    if(stopped)return;
                    try {
                        File data=new File(getFilesDir(),"game-data");
                        if(!data.isDirectory()&&!data.mkdirs())throw new java.io.IOException("Cannot create game save directory");
                        // Android may expose /data/user/0 through a system symlink.
                        // Give SaveStore the physical root; its child-path checks
                        // must continue rejecting project-created symbolic links.
                        android.system.Os.setenv("ASTRA_PLAYER_DATA_ROOT",data.getCanonicalPath(),true);
                    } catch(Exception failure) {status.setText("Não foi possível abrir os dados do jogo: "+failure.getMessage());return;}
                    Intent play=new Intent(this,AetherActivity.class);
                    play.putExtra("astra.project_path",project.getAbsolutePath());
                    play.putExtra("astra.project_name",getApplicationInfo().loadLabel(getPackageManager()).toString());
                    play.putExtra("astra.player",true);play.putExtra("aether.start_play",true);
                    startActivity(play);finish();
                });
            } catch(Exception failure) {
                android.util.Log.e("Astra.Player","Game startup failed",failure);
                runOnUiThread(()->{if(!stopped)status.setText("Não foi possível iniciar o jogo.\n\n"+failure.getMessage());});
            }
        }},"Astra game preparation");
        preparation.start();
    }
    @Override public void onDestroy(){stopped=true;if(preparation!=null)preparation.interrupt();super.onDestroy();}
}
