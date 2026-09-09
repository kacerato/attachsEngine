package dev.aether.editor.shell;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;

import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;

/**
 * Tokens do sistema visual ASTRA.
 *
 * O palco tem 1672x941 porque essa é a resolução dos masters em
 * assets/astra-visual/reference. Toda coordenada do shell é escrita nesse
 * espaço e a tela só o escala, então o layout não muda de aparelho para
 * aparelho — o que muda é a densidade de pixels sob o mesmo desenho.
 */
public final class Design {
    public static final float STAGE_W = 1672f;
    public static final float STAGE_H = 941f;

    public static final int VOID       = 0xFF000000;
    public static final int CANVAS     = 0xFF191C21;
    public static final int SILHOUETTE = 0xFF20242B;
    public static final int SURFACE    = 0xFF242830;
    public static final int RAISED     = 0xFF2E343E;
    public static final int LINE       = 0xFF414954;
    public static final int LINE_SOFT  = 0xFF292E36;
    public static final int TRACK      = 0xFF535F70;
    public static final int TEXT       = 0xFFEEF1F5;
    public static final int DIM        = 0xFFBCC4CF;
    public static final int MUTED      = 0xFF929CAB;
    public static final int FAINT      = 0xFF687485;
    public static final int ACCENT     = 0xFF70ACF5;
    public static final int ACCENT_INK = 0xFF191C21;
    public static final int WASH       = 0x1770ACF5;
    public static final int TAG        = 0xFFB8B8B8;

    private static final Map<String, Bitmap> CACHE = new HashMap<>();

    private Design() { }

    /** Fonte da interface. Roboto é a geométrica que todo aparelho já tem. */
    public static Typeface ui(boolean medium) {
        return Typeface.create(medium ? "sans-serif-medium" : "sans-serif", Typeface.NORMAL);
    }

    public static Paint fill(int color) {
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(color);
        return paint;
    }

    public static Paint stroke(int color, float width) {
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(color);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(width);
        paint.setStrokeCap(Paint.Cap.ROUND);
        paint.setStrokeJoin(Paint.Join.ROUND);
        return paint;
    }

    /**
     * Texto medido em altura de caixa alta, não em corpo da fonte.
     *
     * Os masters foram medidos assim (a altura do "P" de PROJECTS, por
     * exemplo); converter aqui evita espalhar fatores mágicos pelas telas.
     */
    public static Paint text(float sizePx, int color, boolean medium, float tracking) {
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(color);
        paint.setTypeface(ui(medium));
        paint.setTextSize(sizePx);
        paint.setLetterSpacing(tracking);
        return paint;
    }

    /** Linha de base para um texto cuja caixa alta deve começar em capTop. */
    public static float baselineForCapTop(Paint paint, float capTop) {
        return capTop - paint.getFontMetrics().ascent * 0.72f;
    }

    public static Bitmap asset(Context context, String path) {
        Bitmap cached = CACHE.get(path);
        if (cached != null) return cached;
        try (InputStream stream = context.getAssets().open("astra/" + path)) {
            Bitmap bitmap = BitmapFactory.decodeStream(stream);
            if (bitmap != null) CACHE.put(path, bitmap);
            return bitmap;
        } catch (IOException error) {
            return null;
        }
    }

    /** Tinge uma máscara branca com alfa — usado pela silhueta de fundo. */
    public static Paint tint(int color) {
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        paint.setColorFilter(new android.graphics.PorterDuffColorFilter(
                color, android.graphics.PorterDuff.Mode.SRC_IN));
        return paint;
    }

    public static int withAlpha(int color, float alpha) {
        return Color.argb(Math.round(alpha * 255f), Color.red(color), Color.green(color), Color.blue(color));
    }
}
