package dev.aether.editor;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import java.nio.charset.StandardCharsets;

/** Android owns composition/selection/clipboard; the native session owns edits. */
final class EditorTextInput {
    private static native byte[][] poll();
    private static native void submit(long token, byte[] value, boolean accept);
    private final Activity activity;
    private final Handler handler=new Handler(Looper.getMainLooper());
    private AlertDialog dialog;
    private boolean running;
    private long lastToken;
    EditorTextInput(Activity activity) { this.activity=activity; }
    private static String decode(byte[] value) { return new String(value,StandardCharsets.UTF_8); }
    private final Runnable tick=new Runnable() {
        @Override public void run() {
            if(!running) return;
            byte[][] data=poll();
            if(data!=null && dialog==null) {
                long token=Long.parseLong(decode(data[0]));
                if(token!=lastToken) {lastToken=token;show(token,data);}
            }
            handler.postDelayed(this,100);
        }
    };
    void start() { if(!running) {running=true;handler.post(tick);} }
    void stop() {
        running=false;handler.removeCallbacks(tick);
        if(dialog!=null) {submit(lastToken,new byte[0],false);dialog.dismiss();dialog=null;}
    }
    private void show(long token,byte[][] data) {
        final boolean code=decode(data[1]).equals("code");
        final boolean number=decode(data[1]).equals("number");
        final int limit=Integer.parseInt(decode(data[4]));
        EditText field=new EditText(activity);
        field.setTextSize(code?14:16);
        if(code) {field.setTypeface(android.graphics.Typeface.MONOSPACE);field.setMinLines(14);field.setHorizontallyScrolling(true);}
        field.setMinHeight((int)(48*activity.getResources().getDisplayMetrics().density));
        field.setSingleLine(!code);
        // Keep confirmation and cancellation visible in landscape IMEs.
        field.setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI|(code?EditorInfo.IME_ACTION_NONE:EditorInfo.IME_ACTION_DONE));
        field.setInputType(number?InputType.TYPE_CLASS_NUMBER|InputType.TYPE_NUMBER_FLAG_DECIMAL|
                InputType.TYPE_NUMBER_FLAG_SIGNED:InputType.TYPE_CLASS_TEXT|(code?InputType.TYPE_TEXT_FLAG_MULTI_LINE|InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS:0));
        field.setText(decode(data[3]));if(code) field.setSelection(field.length());else field.selectAll();
        int padding=(int)(20*activity.getResources().getDisplayMetrics().density);
        field.setPadding(padding,padding/2,padding,padding/2);
        AlertDialog current=new AlertDialog.Builder(activity)
                .setTitle(decode(data[2])).setView(field)
                .setNegativeButton("Cancelar",(d,w)->submit(token,new byte[0],false))
                .setPositiveButton(code?"Concluir edição":"Aplicar",null).create();
        dialog=current;
        current.setCanceledOnTouchOutside(false);
        current.setOnCancelListener(d->submit(token,new byte[0],false));
        current.setOnDismissListener(d->{if(dialog==current) dialog=null;});
        current.setOnShowListener(d->{
            current.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v->{
                byte[] text=field.getText().toString().getBytes(StandardCharsets.UTF_8);
                if(text.length>limit) {field.setError("Limite de "+limit+" bytes UTF-8");return;}
                submit(token,text,true);current.dismiss();
            });
            field.requestFocus();
            current.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE|
                    WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        });
        current.show();
        if(code) current.getWindow().setLayout(WindowManager.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.MATCH_PARENT);
    }
}
