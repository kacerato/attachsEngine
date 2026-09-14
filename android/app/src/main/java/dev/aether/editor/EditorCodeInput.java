package dev.aether.editor;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.widget.Toast;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.Layout;
import android.text.Spanned;
import android.text.TextWatcher;
import android.text.style.ForegroundColorSpan;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.graphics.PixelFormat;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputConnectionWrapper;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.PopupWindow;
import android.widget.ScrollView;
import java.nio.charset.StandardCharsets;
import java.io.InputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import org.json.JSONObject;
import org.json.JSONArray;

/** Visible platform projection of EditorCodeBuffer, never a separate document.
 * Native owns revisions and undo. Only changed UTF-8 ranges cross JNI during
 * typing; full snapshots are reserved for open, undo and conflict recovery.
 */
final class EditorCodeInput {
    private static native boolean workspace();
    private static native byte[] takeCopy();
    private static native byte[] takeLanguage();
    private long languageToken, languageBuffer, languageRevision;
    private int languagePosition, languageOperation;
    private long searchRequest;
    private PopupWindow languageMenu;
    private LinearLayout searchTools;
    private EditText findField, replaceField;
    private TextView findStatus;
    private int indentWidth=4;
    private boolean indentTabs;
    private final Runnable completion = () -> requestLanguage(0, "");
    private boolean portraitWorkspace;
    private float appliedScale;
    private int sceneOrientation=ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
    private static native byte[][] poll(long buffer, long revision);
    private static native boolean send(int kind, long buffer, long revision, long serial,
        int start, int erased, byte[] inserted, int end, float x, float y, boolean flag, boolean focus);
    private static final int MAX_BYTES = 512 * 1024;
    private final Activity activity;
    private final Handler main = new Handler(Looper.getMainLooper());
    // No permanently idle thread per destroyed NativeActivity. The one-worker
    // gate below also prevents a queue of stale source snapshots.
    private final ExecutorService lexer = new ThreadPoolExecutor(0, 1, 15, TimeUnit.SECONDS,
        new LinkedBlockingQueue<>(1), r -> {
        Thread thread = new Thread(r, "Astra.CodeHighlight"); thread.setDaemon(true); return thread;
    });
    private FrameLayout root;
    private WindowManager.LayoutParams windowBounds;
    private WindowManager.LayoutParams railBounds;
    private LinearLayout rail;
    private boolean railAttached;
    private PopupWindow editMenu;
    private boolean attached;
    private float editorScale = 1;
    private LinearLayout panel;
    private CodeField field;
    private TextView undo, redo;
    private long id, revision, viewRevision, serial, pending, textEpoch;
    private boolean suppress, bulk, csharp, historyPending, stopped;
    private boolean highlighting, highlightAgain;
    private int batch, background, foreground, muted, line, accent, literal;
    private int editStart, editErased;
    private String inserted = "";
    private byte[] recovery;
    private long recoverySerial;
    private long lastRejected;
    private boolean resync;
    private String removed = "";

    EditorCodeInput(Activity activity) {
        this.activity = activity;
        // JNI outlives a NativeActivity. Do not reuse serial 1 underneath the
        // acknowledgement left by the previous Java client in this process.
        serial = android.os.SystemClock.elapsedRealtimeNanos();
        android.content.SharedPreferences preferences=activity.getPreferences(0);
        indentWidth=preferences.getInt("code.indentWidth",4)==2?2:4;
        indentTabs=preferences.getBoolean("code.indentTabs",false);
    }
    private static String decode(byte[] bytes) { return new String(bytes, StandardCharsets.UTF_8); }
    private static byte[] encode(CharSequence text) { return text.toString().getBytes(StandardCharsets.UTF_8); }
    private int dp(float value) { return Math.round(value * editorScale); }

    // UTF-16 caret offsets stop at code-point boundaries, including surrogate pairs.
    private static int utf8Offset(CharSequence text, int offset) {
        offset = Math.max(0, Math.min(offset, text.length()));
        if (offset > 0 && offset < text.length() && Character.isLowSurrogate(text.charAt(offset))
                && Character.isHighSurrogate(text.charAt(offset - 1))) --offset;
        return encode(text.subSequence(0, offset)).length;
    }
    private static int utf16Offset(String text, int bytes) {
        int at = 0, consumed = 0;
        while (at < text.length()) {
            int point = text.codePointAt(at), width = point < 0x80 ? 1 : point < 0x800 ? 2 : point < 0x10000 ? 3 : 4;
            if (consumed + width > bytes) break;
            consumed += width; at += Character.charCount(point);
        }
        return at;
    }

    void tick() {
        stopped = false;
        boolean code = workspace();
        if (code != portraitWorkspace) {
            if (code) sceneOrientation = activity.getRequestedOrientation();
            portraitWorkspace = code;
            // Change the actual Activity/IME orientation. configChanges keeps
            // the native document and this input projection alive through it.
            activity.setRequestedOrientation(code ? ActivityInfo.SCREEN_ORIENTATION_PORTRAIT : sceneOrientation);
        }
        byte[] copied = takeCopy();
        if (copied != null) {
            ClipboardManager clipboard = (ClipboardManager) activity.getSystemService(Activity.CLIPBOARD_SERVICE);
            if (clipboard != null) {
                clipboard.setPrimaryClip(ClipData.newPlainText("Astra Console", decode(copied)));
                if (Build.VERSION.SDK_INT < 33) Toast.makeText(activity,"Mensagem copiada",Toast.LENGTH_SHORT).show();
            }
        }
        // Do not replace an unacknowledged projection with an older render frame.
        if (recovery != null) {
            if (recoverySerial == 0) {
                long next = ++serial;
                if (send(3, id, revision, next, 0, 0, recovery, 0, 0, 0, false, false)) recoverySerial = next;
            }
        }
        byte[][] snapshot = poll(id, resync ? -1 : revision);
        if (snapshot == null) { hide(); return; }
        String[] meta = decode(snapshot[0]).split(" ");
        // Do not lay out a portrait Android panel using the previous landscape
        // native frame while the swapchain is being recreated.
        View frame = activity.getWindow().getDecorView();
        float nativeAspect=Float.parseFloat(meta[22])/Float.parseFloat(meta[27]);
        if (Math.abs(frame.getWidth()/(float)Math.max(1,frame.getHeight())-nativeAspect)>.03f) {
            hide(); return;
        }
        long nextId = Long.parseLong(meta[0]), nextRevision = Long.parseLong(meta[1]);
        long ack = Long.parseLong(meta[2]), nextView = Long.parseLong(meta[3]);
        if (ack < pending || (recovery != null && (recoverySerial == 0 || ack < recoverySerial))) return;
        if (recovery != null) { recovery = null; recoverySerial = 0; }
        editorScale = activity.getWindow().getDecorView().getWidth() / Float.parseFloat(meta[22]);
        if (root == null) create();
        if (Math.abs(appliedScale-editorScale)>.01f) {
            appliedScale=editorScale;
            field.setTextSize(TypedValue.COMPLEX_UNIT_PX,dp(16));field.setLineSpacing(dp(4),1f);
            field.setPadding(dp(52),dp(8),dp(16),dp(12));
            LinearLayout actions=rail;
            for(int i=0;i<actions.getChildCount();++i) {
                TextView action=(TextView)actions.getChildAt(i);
                action.setTextSize(TypedValue.COMPLEX_UNIT_PX,dp(17));
                for(Drawable drawable:action.getCompoundDrawables()) if(drawable!=null) drawable.setBounds(0,0,dp(23),dp(23));
                action.requestLayout();
            }
        }
        background = (int) Long.parseLong(meta[12]); foreground = (int) Long.parseLong(meta[13]);
        muted = (int) Long.parseLong(meta[14]); line = (int) Long.parseLong(meta[15]);
        accent = (int) Long.parseLong(meta[16]); literal = (int) Long.parseLong(meta[17]);
        boolean newBuffer = id != nextId;
        boolean reset = newBuffer || nextRevision != revision || resync;
        long rejected = Long.parseLong(meta[21]);
        if (id == 0) lastRejected = rejected;
        if (rejected > lastRejected && field != null) {
            // A rejected revision must not erase the text the user just saw.
            recovery = encode(field.getText()); recoverySerial = 0; pending = 0;
            lastRejected = rejected; resync = true;
            field.setEnabled(false); return;
        }
        suppress = true;
        id = nextId; revision = nextRevision;
        if (reset && snapshot[1] != null) { field.setText(decode(snapshot[1])); ++textEpoch; }
        if (reset || nextView != viewRevision) {
            String text = field.getText().toString();
            field.setSelection(utf16Offset(text, Integer.parseInt(meta[4])), utf16Offset(text, Integer.parseInt(meta[5])));
            final int x = (int) Float.parseFloat(meta[6]), y = (int) Float.parseFloat(meta[7]);
            boolean jump = nextView != viewRevision && (!newBuffer || nextView > 1);
            field.post(() -> { if (jump) field.bringPointIntoView(field.getSelectionEnd()); else field.scrollTo(x, y); });
        }
        viewRevision = nextView; pending = 0; historyPending = false;
        resync = false;
        if (reset) {
            batch = 0; field.clearComposingText();
            InputMethodManager manager = (InputMethodManager) activity.getSystemService(Activity.INPUT_METHOD_SERVICE);
            if (manager != null && field.hasFocus()) manager.restartInput(field);
        }
        csharp = meta[18].equals("1");
        undo.setEnabled(meta[19].equals("0")); redo.setEnabled(meta[20].equals("0"));
        undo.setAlpha(undo.isEnabled() ? 1 : .35f); redo.setAlpha(redo.isEnabled() ? 1 : .35f);
        field.setEnabled(true); field.setTextColor(foreground); field.setHighlightColor((accent & 0xffffff) | 0x44000000);
        field.setBackgroundColor(background); panel.setBackgroundColor(background);
        View decor = activity.getWindow().getDecorView();
        int width = Math.max(1, Math.round(Float.parseFloat(meta[10]) * decor.getWidth()));
        int height = Math.max(1, Math.round(Float.parseFloat(meta[11]) * decor.getHeight()));
        int x = Math.round(Float.parseFloat(meta[8]) * decor.getWidth());
        int y = Math.round(Float.parseFloat(meta[9]) * decor.getHeight());
        boolean moved = windowBounds.width != width || windowBounds.height != height || windowBounds.x != x || windowBounds.y != y;
        windowBounds.width = width; windowBounds.height = height; windowBounds.x = x; windowBounds.y = y;
        if (!attached && decor.getWindowToken() != null) {
            windowBounds.token = decor.getWindowToken();
            activity.getWindowManager().addView(root, windowBounds); attached = true;
        } else if (attached && moved) activity.getWindowManager().updateViewLayout(root, windowBounds);
        int railX=Math.round(Float.parseFloat(meta[23])*decor.getWidth());
        int railY=Math.round(Float.parseFloat(meta[24])*decor.getHeight());
        int railW=Math.max(1,Math.round(Float.parseFloat(meta[25])*decor.getWidth()));
        int railH=Math.max(1,Math.round(Float.parseFloat(meta[26])*decor.getHeight()));
        boolean railMoved=railBounds.x!=railX || railBounds.y!=railY || railBounds.width!=railW || railBounds.height!=railH;
        railBounds.x=railX;railBounds.y=railY;railBounds.width=railW;railBounds.height=railH;
        if(!railAttached && decor.getWindowToken()!=null) {
            railBounds.token=decor.getWindowToken();activity.getWindowManager().addView(rail,railBounds);railAttached=true;
        } else if(railAttached && railMoved) activity.getWindowManager().updateViewLayout(rail,railBounds);
        panel.setVisibility(View.VISIBLE);
        suppress = false;
        publishInsets();
        if (reset) { scheduleHighlight(); publishState(); updateFindCount(); }
        long requestedSearch=Long.parseLong(meta[28]);
        if(requestedSearch!=searchRequest) {searchRequest=requestedSearch;toggleSearch(false);}
        byte[] language=takeLanguage();
        if(language!=null) showLanguage(decode(language));
    }

    private void create() {
        root = new FrameLayout(activity); root.setClipChildren(true);
        // NativeActivity presents Vulkan into the main window buffer. Android
        // Views in that same buffer are overwritten. An attached, non-modal
        // panel has its own compositor surface, restricted to the code body.
        windowBounds = new WindowManager.LayoutParams(1, 1,
            WindowManager.LayoutParams.TYPE_APPLICATION_PANEL,
            WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT);
        windowBounds.gravity = Gravity.TOP | Gravity.LEFT;
        windowBounds.softInputMode = WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING
            | WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN;
        windowBounds.setTitle("Astra.Code");
        if (Build.VERSION.SDK_INT >= 30) windowBounds.setFitInsetsTypes(0);
        if (Build.VERSION.SDK_INT >= 28) windowBounds.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        root.setSystemUiVisibility(activity.getWindow().getDecorView().getSystemUiVisibility());
        panel = new LinearLayout(activity); panel.setOrientation(LinearLayout.VERTICAL);
        root.addView(panel, new FrameLayout.LayoutParams(-1, -1));
        field = new CodeField();
        field.setGravity(Gravity.TOP | Gravity.START); field.setTypeface(Typeface.MONOSPACE);
        field.setTextSize(TypedValue.COMPLEX_UNIT_PX, dp(16));
        field.setLineSpacing(dp(4), 1f);
        field.setPadding(dp(52), dp(8), dp(16), dp(12)); field.setIncludeFontPadding(false);
        field.setHorizontallyScrolling(true); field.setHorizontalScrollBarEnabled(true); field.setVerticalScrollBarEnabled(true);
        field.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE
            | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        field.setTypeface(Typeface.MONOSPACE);
        field.setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI | EditorInfo.IME_FLAG_NO_FULLSCREEN | EditorInfo.IME_ACTION_NONE);
        field.setContentDescription("Código do arquivo ativo");
        if (Build.VERSION.SDK_INT >= 26) field.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS);
        field.setFilters(new InputFilter[]{(source, start, end, dest, dstart, dend) -> {
            if (suppress) return null;
            int size = encode(dest).length - encode(dest.subSequence(dstart, dend)).length
                + encode(source.subSequence(start, end)).length;
            return size <= MAX_BYTES && source.subSequence(start, end).toString().indexOf('\0') < 0
                ? null : dest.subSequence(dstart, dend);
        }});
        panel.addView(field, new LinearLayout.LayoutParams(-1, 0, 1));
        createSearchTools();
        rail = new LinearLayout(activity);
        rail.setPadding(dp(8),dp(3),dp(8),dp(3));
        rail.setBackgroundColor(0xff0c1217);
        rail.setSystemUiVisibility(root.getSystemUiVisibility());
        railBounds=new WindowManager.LayoutParams(1,1,WindowManager.LayoutParams.TYPE_APPLICATION_PANEL,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL |
            WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,PixelFormat.TRANSLUCENT);
        railBounds.gravity=Gravity.TOP|Gravity.LEFT;railBounds.setTitle("Astra.CodeAccessories");
        if(Build.VERSION.SDK_INT>=30) railBounds.setFitInsetsTypes(0);
        LinearLayout actions = rail;
        undo = action(actions, "↶", "Desfazer edição", () -> history(false));
        redo = action(actions, "↷", "Refazer edição", () -> history(true));
        action(actions, "Tab", "Aumentar recuo", () -> indent(true));
        action(actions, "{ }", "Inserir chaves", () -> replaceSelection("{}", 1));
        action(actions, "▣", "Copiar seleção", () -> field.onTextContextMenuItem(android.R.id.copy));
        TextView more=action(actions, "•••", "Mais ações de edição", () -> {});
        more.setOnClickListener(view -> {
            View owner=activity.getWindow().getDecorView();
            if(activity.isFinishing() || activity.isDestroyed() || owner.getWindowToken()==null) return;
            if(editMenu!=null) editMenu.dismiss();
            LinearLayout menu=new LinearLayout(activity);menu.setOrientation(LinearLayout.VERTICAL);
            menu.setPadding(dp(8),dp(8),dp(8),dp(8));
            String[] labels={"Sugestões C#","Ir à definição","Buscar e substituir","Buscar nas fontes C#",
                "Recuo: "+(indentTabs?"tabulação":indentWidth+" espaços"),
                "Diminuir recuo","Inserir ( )","Inserir ;","Selecionar tudo","Recortar","Colar","Recolher teclado"};
            for(int i=0;i<labels.length;++i) {
                final int selected=i;
                TextView item=new TextView(activity);item.setText(labels[i]);item.setTextColor(0xfff3f2eb);
                item.setTextSize(TypedValue.COMPLEX_UNIT_PX,dp(13));item.setGravity(Gravity.CENTER_VERTICAL);
                item.setPadding(dp(12),0,dp(12),0);item.setContentDescription(labels[i]);
                GradientDrawable pressed=new GradientDrawable();pressed.setColor(0xff202f23);pressed.setCornerRadius(dp(5));
                StateListDrawable states=new StateListDrawable();states.addState(new int[]{android.R.attr.state_pressed},pressed);
                GradientDrawable normal=new GradientDrawable();normal.setColor(0xff121c23);states.addState(new int[]{},normal);item.setBackground(states);
                item.setOnClickListener(choice -> {
                editMenu.dismiss();
                switch(selected) {
                    case 0: requestLanguage(0, "");break;
                    case 1: requestLanguage(1, "");break;
                    case 2: toggleSearch(false);break;
                    case 3: toggleSearch(true);break;
                    case 4: cycleIndent();break;
                    case 5: indent(false);break;
                    case 6: replaceSelection("()",1);break;
                    case 7: replaceSelection(";",1);break;
                    case 8: field.selectAll();break;
                    case 9: field.onTextContextMenuItem(android.R.id.cut);break;
                    case 10: field.onTextContextMenuItem(android.R.id.paste);break;
                    case 11: dismissKeyboard();break;
                }
                });menu.addView(item,new LinearLayout.LayoutParams(-1,dp(36)));
            }
            ScrollView scroll=new ScrollView(activity);scroll.setFillViewport(true);scroll.addView(menu);
            final int height=Math.min(dp(268),Math.max(dp(100),railBounds.y-dp(56)));
            editMenu=new PopupWindow(scroll,Math.min(dp(220),railBounds.width-dp(16)),height,true);
            GradientDrawable surface=new GradientDrawable();surface.setColor(0xff121c23);surface.setCornerRadius(dp(9));surface.setStroke(dp(1),0xff344651);
            editMenu.setBackgroundDrawable(surface);editMenu.setElevation(dp(10));editMenu.setOutsideTouchable(true);
            scroll.setClipToOutline(true);scroll.setBackground(surface);
            editMenu.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
            // A panel's own token is a subwindow token and cannot parent another
            // TYPE_APPLICATION_PANEL. The Activity decor supplies the valid
            // application window token; position still comes from the rail.
            editMenu.showAtLocation(owner,Gravity.TOP|Gravity.LEFT,railBounds.x+railBounds.width-editMenu.getWidth()-dp(8),Math.max(dp(48),railBounds.y-height-dp(6)));
        });
        field.setOnFocusChangeListener((view, focus) -> publishState());
        field.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence text, int start, int count, int after) {
                if (suppress) return;
                editStart = utf8Offset(text, start); editErased = utf8Offset(text, start + count) - editStart;
                removed = text.subSequence(start, start + count).toString();
            }
            @Override public void onTextChanged(CharSequence text, int start, int before, int count) {
                if (!suppress) inserted = text.subSequence(start, start + count).toString();
            }
            @Override public void afterTextChanged(Editable text) {
                if (suppress) return;
                if (removed.equals(inserted)) return;
                long next = ++serial;
                if (!send(0, id, revision, next, editStart, editErased, encode(inserted), 0, 0, 0, bulk, true)) {
                    recovery = encode(text); resync = true; field.setEnabled(false); return;
                }
                pending = next; ++revision; ++textEpoch;
                publishState(); scheduleHighlight();
                updateFindCount();
                invalidateLanguage();
                if(!bulk && csharp && (inserted.equals(".") || (!inserted.isEmpty() && Character.isJavaIdentifierPart(inserted.charAt(inserted.length()-1)))))
                    main.postDelayed(completion,500);
            }
        });
        root.getViewTreeObserver().addOnGlobalLayoutListener(this::publishInsets);
    }

    private void invalidateLanguage() {
        main.removeCallbacks(completion); languageToken=0;
        if(languageMenu!=null) {languageMenu.dismiss();languageMenu=null;}
    }
    private void requestLanguage(int operation,String query) {
        if(field==null || !attached || !csharp || historyPending || recovery!=null) return;
        if(batch>0 || BaseInputConnection.getComposingSpanStart(field.getText())>=0) return;
        invalidateLanguage();
        long next=++serial;
        int position=Math.max(0,field.getSelectionEnd());
        if(send(4,id,revision,next,operation,position,encode(query),0,0,0,false,true)) {
            languageToken=next;languageBuffer=id;languageRevision=revision;
            languagePosition=position;languageOperation=operation;pending=next;
            if(operation!=0) Toast.makeText(activity,"Consultando fontes C#…",Toast.LENGTH_SHORT).show();
        }
    }
    private GradientDrawable toolSurface() {
        GradientDrawable surface=new GradientDrawable();surface.setColor(0xff121c23);
        surface.setCornerRadius(dp(9));surface.setStroke(dp(1),0xff344651);return surface;
    }
    private void showLanguage(String response) {
        try {
            JSONObject envelope=new JSONObject(response);
            if(!attached || envelope.getLong("Token")!=languageToken || envelope.getLong("Buffer")!=id ||
                envelope.getLong("Revision")!=revision || languagePosition!=field.getSelectionEnd()) return;
            JSONObject result=envelope.getJSONObject("Result");JSONArray items=result.getJSONArray("Items");
            if(items.length()==0) {
                if(languageOperation!=0) Toast.makeText(activity,result.getString("Message"),Toast.LENGTH_LONG).show();
                return;
            }
            LinearLayout menu=new LinearLayout(activity);menu.setOrientation(LinearLayout.VERTICAL);menu.setPadding(dp(8),dp(6),dp(8),dp(6));
            TextView heading=new TextView(activity);heading.setText(result.getString("Message")+(result.optBoolean("Truncated")?" · limite atingido":""));
            heading.setTextColor(0xff97acbb);heading.setTextSize(TypedValue.COMPLEX_UNIT_PX,dp(11));
            heading.setPadding(dp(6),dp(5),dp(6),dp(5));menu.addView(heading);
            final long token=languageToken,buffer=languageBuffer,version=languageRevision;
            final int operation=languageOperation,position=languagePosition;
            for(int i=0;i<items.length();++i) {
                JSONObject item=items.getJSONObject(i);
                TextView row=new TextView(activity);
                row.setText(item.getString("Label")+"\n"+item.getString("Detail"));row.setMaxLines(3);
                row.setTextColor(0xfff3f2eb);row.setTextSize(TypedValue.COMPLEX_UNIT_PX,dp(12));
                row.setPadding(dp(10),dp(7),dp(10),dp(7));
                row.setBackground(toolSurface());
                row.setOnClickListener(view -> {
                    if(token!=languageToken || buffer!=id || version!=revision || position!=field.getSelectionEnd()) {invalidateLanguage();return;}
                    invalidateLanguage();
                    if(operation==0) {
                        int start=item.optInt("Start"),length=item.optInt("Length");String value=item.optString("Insert");
                        if(start<0 || length<0 || start+length>field.length()) return;
                        field.setSelection(start,start+length);replaceSelection(value,value.length());
                    } else {
                        long next=++serial;
                        if(send(5,id,revision,next,item.optInt("Line",1),0,encode(item.optString("File")),item.optInt("Column",1),0,0,false,false)) pending=next;
                    }
                });
                LinearLayout.LayoutParams cell=new LinearLayout.LayoutParams(-1,-2);cell.topMargin=dp(3);menu.addView(row,cell);
            }
            ScrollView scroll=new ScrollView(activity);scroll.addView(menu);scroll.setBackground(toolSurface());scroll.setClipToOutline(true);
            int height=Math.min(dp(Math.min(240,38+items.length()*60)),Math.max(dp(80),windowBounds.height/2));
            languageMenu=new PopupWindow(scroll,Math.min(dp(360),windowBounds.width-dp(12)),height,false);
            languageMenu.setBackgroundDrawable(toolSurface());languageMenu.setElevation(dp(10));languageMenu.setOutsideTouchable(true);
            languageMenu.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
            View owner=activity.getWindow().getDecorView();if(owner.getWindowToken()==null) return;
            int y=windowBounds.y+windowBounds.height-height-dp(4);
            Layout layout=field.getLayout();
            if(operation==0 && layout!=null) {
                int lineIndex=layout.getLineForOffset(position);
                int caretY=windowBounds.y+field.getTop()+field.getPaddingTop()+layout.getLineBottom(lineIndex)-field.getScrollY();
                y=caretY+height<windowBounds.y+windowBounds.height?caretY:Math.max(windowBounds.y,caretY-height-field.getLineHeight());
            }
            languageMenu.showAtLocation(owner,Gravity.TOP|Gravity.LEFT,windowBounds.x+dp(6),y);
        } catch(org.json.JSONException error) {Toast.makeText(activity,"Resposta de linguagem inválida",Toast.LENGTH_SHORT).show();}
    }
    private void cycleIndent() {
        if(indentTabs) {indentTabs=false;indentWidth=2;} else if(indentWidth==2) indentWidth=4; else indentTabs=true;
        activity.getPreferences(0).edit().putInt("code.indentWidth",indentWidth).putBoolean("code.indentTabs",indentTabs).apply();
        Toast.makeText(activity,"Recuo: "+(indentTabs?"tabulação":indentWidth+" espaços"),Toast.LENGTH_SHORT).show();
    }
    private TextView searchButton(LinearLayout row,String label,Runnable callback) {
        TextView button=new TextView(activity);button.setText(label);button.setTextColor(0xffa8ed65);
        button.setTextSize(TypedValue.COMPLEX_UNIT_PX,dp(12));button.setGravity(Gravity.CENTER);
        button.setPadding(dp(8),0,dp(8),0);button.setContentDescription(label);button.setOnClickListener(v -> callback.run());
        row.addView(button,new LinearLayout.LayoutParams(-2,dp(36)));return button;
    }
    private EditText searchField(LinearLayout row,String hint) {
        EditText input=new EditText(activity);input.setSingleLine(true);input.setTextColor(0xfff3f2eb);input.setHintTextColor(0xff97acbb);
        input.setTextSize(TypedValue.COMPLEX_UNIT_PX,dp(12));input.setHint(hint);input.setPadding(dp(8),0,dp(8),0);
        input.setBackground(toolSurface());input.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        row.addView(input,new LinearLayout.LayoutParams(0,dp(36),1));return input;
    }
    private void createSearchTools() {
        searchTools=new LinearLayout(activity);searchTools.setOrientation(LinearLayout.VERTICAL);searchTools.setPadding(dp(6),dp(4),dp(6),dp(4));
        searchTools.setBackgroundColor(0xff121c23);
        LinearLayout search=new LinearLayout(activity);findField=searchField(search,"Buscar texto exato");
        searchButton(search,"↑",() -> findInBuffer(false));searchButton(search,"↓",() -> findInBuffer(true));
        searchButton(search,"×",() -> {searchTools.setVisibility(View.GONE);field.requestFocus();});searchTools.addView(search);
        LinearLayout replace=new LinearLayout(activity);replaceField=searchField(replace,"Substituir por");
        searchButton(replace,"Trocar",() -> replaceMatch(false));searchButton(replace,"Todas",() -> replaceMatch(true));searchTools.addView(replace);
        LinearLayout status=new LinearLayout(activity);findStatus=new TextView(activity);findStatus.setTextColor(0xff97acbb);
        findStatus.setTextSize(TypedValue.COMPLEX_UNIT_PX,dp(11));findStatus.setGravity(Gravity.CENTER_VERTICAL);
        status.addView(findStatus,new LinearLayout.LayoutParams(0,dp(32),1));
        searchButton(status,"No projeto",() -> requestLanguage(2,findField.getText().toString()));searchTools.addView(status);
        panel.addView(searchTools,0,new LinearLayout.LayoutParams(-1,-2));searchTools.setVisibility(View.GONE);
        findField.addTextChangedListener(new TextWatcher() {
            public void beforeTextChanged(CharSequence s,int a,int b,int c) {}
            public void onTextChanged(CharSequence s,int a,int b,int c) {updateFindCount();}
            public void afterTextChanged(Editable s) {}
        });
    }
    private void toggleSearch(boolean project) {
        invalidateLanguage();searchTools.setVisibility(View.VISIBLE);
        int start=Math.max(0,Math.min(field.getSelectionStart(),field.getSelectionEnd())),end=Math.max(field.getSelectionStart(),field.getSelectionEnd());
        if(end>start && end-start<256) findField.setText(field.getText().subSequence(start,end));
        updateFindCount();findField.requestFocus();
        InputMethodManager manager=(InputMethodManager)activity.getSystemService(Activity.INPUT_METHOD_SERVICE);
        if(manager!=null) manager.showSoftInput(findField,InputMethodManager.SHOW_IMPLICIT);
        if(project && findField.length()>0) requestLanguage(2,findField.getText().toString());
    }
    private void updateFindCount() {
        if(findField==null || field==null) return;
        String query=findField.getText().toString(),text=field.getText().toString();int count=0,at=0;
        if(!query.isEmpty()) while((at=text.indexOf(query,at))>=0) {++count;at+=query.length();}
        findStatus.setText(count+" ocorrências · arquivo atual");
    }
    private void findInBuffer(boolean next) {
        String text=field.getText().toString(),query=findField.getText().toString();if(query.isEmpty()) return;
        int start=Math.max(0,Math.min(field.getSelectionStart(),field.getSelectionEnd())),end=Math.max(0,Math.max(field.getSelectionStart(),field.getSelectionEnd()));
        int at=next?text.indexOf(query,end):text.lastIndexOf(query,start-1);
        if(at<0) at=next?text.indexOf(query):text.lastIndexOf(query);
        if(at>=0) {field.setSelection(at,at+query.length());field.bringPointIntoView(at);publishState();}
        updateFindCount();
    }
    private void replaceMatch(boolean all) {
        String query=findField.getText().toString();if(query.isEmpty()) return;
        String replacement=replaceField.getText().toString(),text=field.getText().toString();
        if(all) {
            int count=0,at=0;while((at=text.indexOf(query,at))>=0) {++count;at+=query.length();}
            long bytes=encode(text).length+(long)count*(encode(replacement).length-encode(query).length);
            if(bytes>MAX_BYTES) {findStatus.setText("Substituição excede 512 KiB");return;}
            String changed=text.replace(query,replacement);
            bulk=true;field.getText().replace(0,text.length(),changed);bulk=false;
            field.setSelection(Math.min(Math.max(0,field.getSelectionEnd()),changed.length()));publishState();
        } else {
            int start=Math.max(0,Math.min(field.getSelectionStart(),field.getSelectionEnd())),end=Math.max(start,Math.max(field.getSelectionStart(),field.getSelectionEnd()));
            if(text.substring(start,end).equals(query)) replaceSelection(replacement,replacement.length());
            findInBuffer(true);
        }
        updateFindCount();
    }
    private TextView action(LinearLayout row, String glyph, String label, Runnable run) {
        TextView button = new TextView(activity); button.setText(glyph); button.setGravity(Gravity.CENTER);
        button.setTextSize(TypedValue.COMPLEX_UNIT_PX, dp(17)); button.setTextColor(0xfff3f2eb); button.setContentDescription(label);
        button.setFocusable(false); button.setOnClickListener(view -> run.run());
        StateListDrawable states = new StateListDrawable();
        GradientDrawable pressed = new GradientDrawable(); pressed.setColor(0xff202f23); pressed.setCornerRadius(dp(6));
        states.addState(new int[]{android.R.attr.state_pressed}, pressed);
        GradientDrawable idle = new GradientDrawable(); idle.setColor(glyph.equals("Tab")?0xff1b2730:0xff0c1217);
        idle.setCornerRadius(dp(6));if(glyph.equals("Tab")) idle.setStroke(dp(1),0xff344651);
        states.addState(new int[]{}, idle); button.setBackground(states);
        if (Build.VERSION.SDK_INT >= 26) button.setTooltipText(label);
        if (glyph.equals("↶") || glyph.equals("↷") || glyph.equals("▣")) {
            try (InputStream image = activity.getAssets().open("ui/code-ide/" + (glyph.equals("▣") ? "copy" : "undo") + ".png")) {
                Drawable mark = Drawable.createFromStream(image, label);
                if (mark != null) {
                    mark.setBounds(0, 0, dp(23), dp(23)); button.setText(""); button.setCompoundDrawables(null, mark, null, null);
                    button.setPadding(0, dp(6), 0, 0);
                    if (glyph.equals("↷")) button.setScaleX(-1);
                }
            } catch (IOException ignored) { /* Glyph remains readable in an incomplete development package. */ }
        }
        LinearLayout.LayoutParams cell=new LinearLayout.LayoutParams(0,-1,1);cell.setMargins(dp(3),0,dp(3),0);
        row.addView(button, cell); return button;
    }
    private void replaceSelection(String value, int caret) {
        int start = Math.max(0, Math.min(field.getSelectionStart(), field.getSelectionEnd()));
        int end = Math.max(start, Math.max(field.getSelectionStart(), field.getSelectionEnd()));
        Editable text=field.getText();
        if ((long)encode(text).length-encode(text.subSequence(start,end)).length+encode(value).length>MAX_BYTES) {
            Toast.makeText(activity,"Edição excede 512 KiB",Toast.LENGTH_SHORT).show();return;
        }
        bulk = true;
        try { text.replace(start, end, value); } finally { bulk = false; }
        field.setSelection(Math.min(text.length(),start+Math.max(0,Math.min(caret,value.length())))); publishState();
    }
    private void indent(boolean add) {
        Editable text = field.getText(); int from = Math.max(0, Math.min(field.getSelectionStart(), field.getSelectionEnd()));
        int to = Math.max(from, Math.max(field.getSelectionStart(), field.getSelectionEnd()));
        int start = text.toString().lastIndexOf('\n', Math.max(-1, from - 1)) + 1;
        int end = to;
        String segment = text.subSequence(start, end).toString();
        String[] lines = segment.split("\n", -1); StringBuilder changed = new StringBuilder();
        for (int i = 0; i < lines.length; ++i) {
            if (i > 0) changed.append('\n');
            String value = lines[i];
            if (add) changed.append(indentTabs?"\t":indentWidth==2?"  ":"    ");
            else { int trim = 0; while (trim < Math.min(indentWidth, value.length()) && value.charAt(trim) == ' ') ++trim;
                if (value.startsWith("\t")) trim = 1; value = value.substring(trim); }
            changed.append(value);
        }
        bulk = true; text.replace(start, end, changed); bulk = false;
        field.setSelection(start, start + changed.length()); publishState();
    }
    private void history(boolean again) {
        if (historyPending) return;
        field.clearComposingText(); publishState();
        long next = ++serial;
        if (send(2, id, revision, next, 0, 0, null, 0, 0, 0, again, true)) {
            pending = next; historyPending = true; field.setEnabled(false);
        }
    }
    private void publishState() {
        if (suppress || id == 0 || field == null || historyPending || recovery != null) return;
        Editable text = field.getText();
        boolean composing = batch > 0 || BaseInputConnection.getComposingSpanStart(text) >= 0;
        long next = ++serial;
        if (send(1, id, revision, next, utf8Offset(text, field.getSelectionStart()), 0, null,
            utf8Offset(text, field.getSelectionEnd()), field.getScrollX(), field.getScrollY(), composing, field.hasFocus())) pending = next;
    }
    private void hide() {
        invalidateLanguage();
        if(editMenu!=null) {editMenu.dismiss();editMenu=null;}
        if(railAttached) {activity.getWindowManager().removeViewImmediate(rail);railAttached=false;}
        if (!attached) return;
        publishState(); dismissKeyboard();
        activity.getWindowManager().removeViewImmediate(root); attached = false;
    }
    void stop() { stopped = true; hide(); main.removeCallbacks(highlight); }
    private void dismissKeyboard() {
        if (field == null) return;
        field.clearComposingText(); field.clearFocus();
        InputMethodManager manager = (InputMethodManager) activity.getSystemService(Activity.INPUT_METHOD_SERVICE);
        if (manager != null) manager.hideSoftInputFromWindow(field.getWindowToken(), 0);
        publishState();
    }
    private void publishInsets() {
        if (root == null || root.getHeight() == 0) return;
        View decor = activity.getWindow().getDecorView();
        int covered;
        if (Build.VERSION.SDK_INT >= 30) {
            // Insets of the attached panel are relative to its already-clipped
            // frame and can be zero above the IME. The native layout needs the
            // obstruction of the Activity's full window instead.
            WindowInsets insets = activity.getWindowManager().getCurrentWindowMetrics().getWindowInsets();
            covered = insets.isVisible(WindowInsets.Type.ime()) ? insets.getInsets(WindowInsets.Type.ime()).bottom : 0;
        } else {
            Rect visible = new Rect(); decor.getWindowVisibleDisplayFrame(visible);
            covered = decor.getHeight() - visible.height();
            if (covered < decor.getHeight() * .15f) covered = 0;
        }
        EditorTextInput.reportIme(covered / (float) Math.max(1, decor.getHeight()));
    }

    private final class CodeField extends EditText {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        CodeField() { super(activity); }
        @Override protected void onSelectionChanged(int start, int end) { super.onSelectionChanged(start, end); publishState(); }
        @Override protected void onScrollChanged(int x, int y, int oldX, int oldY) { super.onScrollChanged(x, y, oldX, oldY); publishState(); }
        @Override public boolean onTextContextMenuItem(int action) {
            if (action == android.R.id.undo) { history(false); return true; }
            if (action == android.R.id.redo) { history(true); return true; }
            bulk = true; boolean handled = super.onTextContextMenuItem(action); bulk = false; publishState(); return handled;
        }
        @Override public boolean onKeyDown(int code, KeyEvent event) {
            if (event.isCtrlPressed() && code == KeyEvent.KEYCODE_Z) { history(event.isShiftPressed()); return true; }
            if (event.isCtrlPressed() && code == KeyEvent.KEYCODE_Y) { history(true); return true; }
            if (code == KeyEvent.KEYCODE_TAB) { indent(!event.isShiftPressed()); return true; }
            return super.onKeyDown(code, event);
        }
        @Override public boolean onKeyPreIme(int code, KeyEvent event) {
            if (code == KeyEvent.KEYCODE_BACK && event.getAction() == KeyEvent.ACTION_UP) { dismissKeyboard(); return true; }
            return super.onKeyPreIme(code, event);
        }
        @Override public InputConnection onCreateInputConnection(EditorInfo info) {
            InputConnection input = super.onCreateInputConnection(info); if (input == null) return null;
            return new InputConnectionWrapper(input, false) {
                @Override public boolean beginBatchEdit() { ++batch; publishState(); return super.beginBatchEdit(); }
                @Override public boolean endBatchEdit() { boolean result = super.endBatchEdit(); batch = Math.max(0, batch - 1); publishState(); return result; }
                @Override public boolean setComposingText(CharSequence text, int cursor) { boolean result = super.setComposingText(text, cursor); publishState(); return result; }
                @Override public boolean setComposingRegion(int start, int end) { boolean result = super.setComposingRegion(start, end); publishState(); return result; }
                @Override public boolean finishComposingText() { boolean result = super.finishComposingText(); publishState(); return result; }
                @Override public boolean commitText(CharSequence text, int cursor) {
                    bulk = text.length() > 1 && BaseInputConnection.getComposingSpanStart(getText()) < 0;
                    boolean result = super.commitText(text, cursor); bulk = false; publishState(); return result;
                }
            };
        }
        @Override protected void onDraw(Canvas canvas) {
            Layout layout = getLayout();
            if (layout != null) {
                int first = layout.getLineForVertical(Math.max(0, getScrollY() - getPaddingTop()));
                int last = layout.getLineForVertical(getScrollY() + getHeight());
                int selected = layout.getLineForOffset(Math.max(0, getSelectionEnd()));
                paint.setColor(0xff19242b);
                canvas.drawRect(getScrollX() + getPaddingLeft(), layout.getLineTop(selected) + getPaddingTop(),
                    getScrollX() + getWidth(), layout.getLineBottom(selected) + getPaddingTop(), paint);
                float cell = getPaint().measureText(" "); paint.setColor(line); paint.setStrokeWidth(dp(1));
                for (int row = first; row <= last; ++row) {
                    int start = layout.getLineStart(row), end = layout.getLineEnd(row), spaces = 0;
                    while (start < end) { char c = getText().charAt(start++); if (c == ' ') ++spaces; else if (c == '\t') spaces += 4; else break; }
                    for (int col = 4; col <= spaces; col += 4) {
                        float x = getPaddingLeft() + col * cell;
                        canvas.drawLine(x, layout.getLineTop(row) + getPaddingTop(), x, layout.getLineBottom(row) + getPaddingTop(), paint);
                    }
                }
            }
            super.onDraw(canvas);
            if (layout == null) return;
            // Gutter remains fixed while code scrolls horizontally.
            paint.setColor(background); canvas.drawRect(getScrollX(), getScrollY(), getScrollX() + getPaddingLeft() - dp(6), getScrollY() + getHeight(), paint);
            paint.setColor(line);canvas.drawRect(getScrollX()+getPaddingLeft()-dp(6),getScrollY(),
                getScrollX()+getPaddingLeft()-dp(5),getScrollY()+getHeight(),paint);
            paint.setTypeface(Typeface.MONOSPACE); paint.setTextSize(getTextSize() * .85f); paint.setTextAlign(Paint.Align.RIGHT);
            int first = layout.getLineForVertical(Math.max(0, getScrollY() - getPaddingTop()));
            int last = layout.getLineForVertical(getScrollY() + getHeight());
            int selected = layout.getLineForOffset(Math.max(0, getSelectionEnd()));
            for (int row = first; row <= last; ++row) {
                paint.setColor(row == selected ? accent : muted);
                canvas.drawText(Integer.toString(row + 1), getScrollX() + getPaddingLeft() - dp(14), layout.getLineBaseline(row) + getPaddingTop(), paint);
            }
        }
    }

    private void scheduleHighlight() { main.removeCallbacks(highlight); main.postDelayed(highlight, 280); }
    private final Runnable highlight = () -> {
        if (stopped || field == null) return;
        if (highlighting) { highlightAgain = true; return; }
        highlighting = true;
        final long version = textEpoch, buffer = id;
        final String text = field.getText().toString(); final boolean language = csharp;
        lexer.execute(() -> {
            List<int[]> spans = language ? CSharpColors.scan(text) : new ArrayList<>();
            main.post(() -> {
                highlighting = false;
                if (!stopped && id == buffer && textEpoch == version && field != null) {
                    Editable value = field.getText();
                    for (ForegroundColorSpan old : value.getSpans(0, value.length(), ForegroundColorSpan.class)) value.removeSpan(old);
                    for (int[] token : spans) value.setSpan(new ForegroundColorSpan(token[2] == 1 ? 0xffdca5ed : token[2] == 2 ? 0xffffcc80 : token[2] == 4 ? 0xff68d9ef : muted), token[0], token[1], Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
                }
                if (highlightAgain) { highlightAgain = false; scheduleHighlight(); }
            });
        });
    };

    /** Lexical color only; diagnostics and valid component types still come from Roslyn. */
    private static final class CSharpColors {
        private static final Set<String> WORDS = new HashSet<>(Arrays.asList((
            "abstract as async await base bool break byte case catch char checked class const continue decimal default delegate do double else enum event explicit extern false finally fixed float for foreach goto if implicit in int interface internal is lock long namespace new null object operator out override params private protected public readonly record ref return sbyte sealed short sizeof stackalloc static string struct switch this throw true try typeof uint ulong unchecked unsafe ushort using var virtual void volatile while yield").split(" ")));
        static List<int[]> scan(String text) {
            List<int[]> result = new ArrayList<>(); int i = 0, size = text.length();
            while (i < size && result.size() < 16000) {
                int start = i; char c = text.charAt(i++); int kind = 0;
                if (c == '/' && i < size && text.charAt(i) == '/') { while (i < size && text.charAt(i) != '\n') ++i; kind = 3; }
                else if (c == '/' && i < size && text.charAt(i) == '*') { ++i; while (i < size && !(text.charAt(i - 1) == '*' && text.charAt(i) == '/')) ++i; i = Math.min(size, i + 1); kind = 3; }
                else if (c == '#' && (start == 0 || text.charAt(start - 1) == '\n')) { while (i < size && text.charAt(i) != '\n') ++i; kind = 3; }
                else if (c == '"' || c == '\'') {
                    boolean verbatim = c == '"' && start > 0 && text.charAt(start - 1) == '@';
                    while (i < size) { char next = text.charAt(i++);
                        if (!verbatim && next == '\\') { i = Math.min(size, i + 1); continue; }
                        if (next == c) { if (verbatim && i < size && text.charAt(i) == c) { ++i; continue; } break; }
                    } kind = 2;
                } else if (Character.isDigit(c)) { while (i < size && (Character.isLetterOrDigit(text.charAt(i)) || text.charAt(i) == '.' || text.charAt(i) == '_')) ++i; kind = 2; }
                else if (Character.isJavaIdentifierStart(c)) {
                    while (i < size && Character.isJavaIdentifierPart(text.charAt(i))) ++i;
                    if (WORDS.contains(text.substring(start, i))) kind = 1;
                    else if(Character.isUpperCase(c)) kind = 4;
                }
                if (kind != 0) result.add(new int[]{start, i, kind});
            }
            return result;
        }
    }
}
