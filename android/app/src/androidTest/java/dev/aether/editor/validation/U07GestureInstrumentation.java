package dev.aether.editor.validation;

import android.app.Instrumentation;
import android.content.Intent;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import java.io.FileInputStream;
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;

/** Device acceptance helper, packaged only in the separate test APK.
 * Injects genuine two-pointer touchscreen events; no gameplay/debug shortcuts.
 * Stops immediately if the target app loses foreground. Coordinates are the
 * validated landscape viewport proportions, not an engine input back door. */
public final class U07GestureInstrumentation extends Instrumentation {
    private Bundle arguments;
    private float width,height;
    private long down;
    @Override public void onCreate(Bundle args) { arguments=args;start(); }
    @Override public void onStart() {
        Bundle result=new Bundle();
        String recorder="";
        ParcelFileDescriptor recording=null;
        try {
            Intent launch=new Intent().setClassName(getTargetContext(),"dev.aether.editor.shell.AstraShellActivity")
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK).putExtra("astra.open_project","U07 Laboratório de Controle")
                .putExtra("aether.start_play",true);
            long launchEpoch=System.currentTimeMillis();
            getTargetContext().startActivity(launch);SystemClock.sleep(1500);
            boolean ready=false;
            for(int attempt=0;attempt<60&&!ready;++attempt) {
                guard();
                for(String line:shell("logcat -d -v epoch -s Astra.Script").split("\n")) {
                    if(!line.contains("U07 READY:"))continue;
                    String stamp=line.trim().split("\\s+",2)[0];
                    try {if(Double.parseDouble(stamp)*1000>=launchEpoch)ready=true;}catch(NumberFormatException ignored){}
                }
                if(!ready)SystemClock.sleep(500);
            }
            if(!ready)throw new IllegalStateException("Play scripts did not report readiness after launch");
            SystemClock.sleep(1200); // Finish the physical spawn->landing cycle.
            width=getTargetContext().getResources().getDisplayMetrics().widthPixels;
            height=getTargetContext().getResources().getDisplayMetrics().heightPixels;
            if(width<height)throw new IllegalStateException("Landscape viewport required");
            guard();
            String scenario="camera".equals(arguments.getString("scenario"))?"camera":"locomotion";
            if(!shell("pidof screenrecord").trim().isEmpty())throw new IllegalStateException("Another recording is active");
            recording=getUiAutomation().executeShellCommand("screenrecord --size 1386x640 --bit-rate 4000000 --time-limit 12 /sdcard/u07-"+scenario+"-accepted.mp4");
            SystemClock.sleep(250);recorder=shell("pidof screenrecord").trim();
            if(!recorder.matches("[0-9]+"))throw new IllegalStateException("Recorder PID unavailable");
            SystemClock.sleep(250);
            if("camera".equals(scenario))camera();else locomotion();
            shell("kill -2 "+recorder);recorder="";SystemClock.sleep(600);
            result.putString("result","actual touchscreen sequence completed");finish(-1,result);
        } catch(Exception error) { result.putString("error",error.toString());finish(0,result); }
        finally {
            if(recorder.matches("[0-9]+"))try{shell("kill -2 "+recorder);}catch(Exception ignored){}
            if(recording!=null)try{recording.close();}catch(Exception ignored){}
        }
    }
    private void guard() throws Exception {
        String state=shell("dumpsys activity activities");
        String own=getTargetContext().getPackageName()+"/";
        boolean focused=false;
        for(String line:state.split("\n"))if(line.contains("topResumedActivity")&&line.contains(own))focused=true;
        if(!focused)throw new IllegalStateException("Target app lost foreground; no input sent");
    }
    private String shell(String command) throws Exception {
        ParcelFileDescriptor fd=getUiAutomation().executeShellCommand(command);
        try(FileInputStream input=new FileInputStream(fd.getFileDescriptor());ByteArrayOutputStream out=new ByteArrayOutputStream()) {
            byte[] buffer=new byte[8192];int n;
            while((n=input.read(buffer))>=0) {out.write(buffer,0,n);if(out.size()>1048576)throw new IllegalStateException("Shell result too large");}
            return new String(out.toByteArray(),StandardCharsets.UTF_8);
        } finally {fd.close();}
    }
    private void event(int action,float... points) throws Exception {
        guard();int count=points.length/2;
        MotionEvent.PointerProperties[] properties=new MotionEvent.PointerProperties[count];
        MotionEvent.PointerCoords[] coordinates=new MotionEvent.PointerCoords[count];
        for(int i=0;i<count;++i) {
            properties[i]=new MotionEvent.PointerProperties();properties[i].id=i;properties[i].toolType=MotionEvent.TOOL_TYPE_FINGER;
            coordinates[i]=new MotionEvent.PointerCoords();coordinates[i].x=points[2*i]*width;coordinates[i].y=points[2*i+1]*height;
            coordinates[i].pressure=1;coordinates[i].size=1;
        }
        MotionEvent motion=MotionEvent.obtain(down,SystemClock.uptimeMillis(),action,count,properties,coordinates,0,0,1,1,0,0,InputDevice.SOURCE_TOUCHSCREEN,0);
        try {if(!getUiAutomation().injectInputEvent(motion,true))throw new IllegalStateException("Touch injection rejected");}
        finally {motion.recycle();}
    }
    private void locomotion() throws Exception {
        SystemClock.sleep(500);down=SystemClock.uptimeMillis();event(MotionEvent.ACTION_DOWN,.093f,.789f);
        event(MotionEvent.ACTION_MOVE,.093f,.676f);SystemClock.sleep(750);
        event(MotionEvent.ACTION_POINTER_DOWN|(1<<MotionEvent.ACTION_POINTER_INDEX_SHIFT),.093f,.676f,.916f,.834f);
        SystemClock.sleep(80);event(MotionEvent.ACTION_POINTER_UP|(1<<MotionEvent.ACTION_POINTER_INDEX_SHIFT),.093f,.676f,.916f,.834f);
        SystemClock.sleep(150);event(MotionEvent.ACTION_MOVE,.160f,.789f);SystemClock.sleep(450);
        event(MotionEvent.ACTION_UP,.160f,.789f);SystemClock.sleep(1400);
        // A second cycle explicitly proves stop->idle and repeatable air state.
        down=SystemClock.uptimeMillis();event(MotionEvent.ACTION_DOWN,.916f,.834f);SystemClock.sleep(80);
        event(MotionEvent.ACTION_UP,.916f,.834f);SystemClock.sleep(1600);
    }
    private void camera() throws Exception {
        down=SystemClock.uptimeMillis();event(MotionEvent.ACTION_DOWN,.82f,.62f);
        for(int n=1;n<=8;++n){event(MotionEvent.ACTION_MOVE,.82f,.62f-.045f*n);SystemClock.sleep(45);}
        event(MotionEvent.ACTION_UP,.82f,.26f);SystemClock.sleep(500);
        down=SystemClock.uptimeMillis();event(MotionEvent.ACTION_DOWN,.82f,.45f);
        for(int n=1;n<=12;++n){event(MotionEvent.ACTION_MOVE,.82f-.025f*n,.45f);SystemClock.sleep(40);}
        event(MotionEvent.ACTION_UP,.52f,.45f);SystemClock.sleep(500);
    }
}
