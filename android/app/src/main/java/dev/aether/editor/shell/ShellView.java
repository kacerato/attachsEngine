package dev.aether.editor.shell;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Shader;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Desenha o shell inteiro no espaço de projeto 1672x941 e escala para a tela.
 *
 * Tudo é Canvas em vez de uma árvore de Views porque as posições vieram medidas
 * dos masters: um layout que recalcula margens em dp reintroduziria justamente
 * a variação que a medição eliminou.
 */
public final class ShellView extends View {

    public interface Listener {
        void onProjectPicked(Project project);
        void onNewProjectRequested();
        void onNavUnavailable(String label);
        void onLoadFinished(Project project);
    }

    public enum Screen { SPLASH, PROJECTS, LOADING }

    private static final String[] NAV = { "Projetos", "Novo", "Importar", "Configurações" };

    private final ProjectStore store;
    private Listener listener;

    private Screen screen = Screen.SPLASH;
    private int navIndex = 0;
    private Project loadingProject;

    private float overflowX;
    private float overflowY;

    private long progressStart;
    private long progressDuration;
    private boolean progressRunning;

    private final Matrix stage = new Matrix();
    private final Matrix inverse = new Matrix();
    private final float[] touch = new float[2];
    private final List<Hit> hits = new ArrayList<>();

    private final Paint canvasPaint = Design.fill(Design.CANVAS);
    private final Paint surfacePaint = Design.fill(Design.SURFACE);
    private final Paint raisedPaint = Design.fill(Design.RAISED);
    private final Paint linePaint = Design.stroke(Design.LINE, 1f);
    private final Paint lineSoftPaint = Design.fill(Design.LINE_SOFT);
    private final Paint trackPaint = Design.fill(Design.TRACK);
    private final Paint accentPaint = Design.fill(Design.ACCENT);
    private final Paint washPaint = Design.fill(Design.WASH);
    private final Paint bracketPaint = Design.stroke(0xFF3A3A3A, 1f);
    private final Paint silhouettePaint = Design.tint(Design.SILHOUETTE);
    private final Paint imagePaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);

    private final Paint tagText = Design.text(13f, Design.TAG, false, .22f);
    private final Paint captionText = Design.text(13f, Design.DIM, false, .34f);
    private final Paint stampText = Design.text(13f, Design.FAINT, false, .18f);
    private final Paint labelText = Design.text(15f, Design.MUTED, false, .28f);
    private final Paint fieldText = Design.text(13f, Design.MUTED, false, .28f);
    private final Paint navText = Design.text(21f, Design.DIM, false, 0f);
    private final Paint navTextOn = Design.text(21f, Design.TEXT, false, 0f);
    private final Paint cardText = Design.text(20f, Design.TEXT, false, 0f);
    private final Paint kickerText = Design.text(13f, 0xEBFFFFFF, false, .26f);
    private final Paint titleText = Design.text(44f, Design.TEXT, true, -.01f);
    private final Paint pathText = Design.text(19f, Design.DIM, false, 0f);
    private final Paint statText = Design.text(22f, Design.TEXT, false, 0f);
    private final Paint buttonText = Design.text(22f, Design.ACCENT_INK, false, 0f);
    private final Paint soonText = Design.text(11f, Design.ACCENT_INK, true, .18f);

    private final Paint plusPaint = Design.stroke(Design.ACCENT_INK, 1.9f);
    private final Paint slotPaint = Design.stroke(0xFF2E2E2E, 2f);
    private final Paint dashedPaint = Design.stroke(0xFF232323, 1f);
    private final Paint openGlyphPaint = Design.stroke(0xFF7E7E7E, 1.6f);
    private final Paint navGlyphPaint = Design.stroke(Design.DIM, 1.9f);
    private final Paint toothPaint = Design.stroke(Design.DIM, 3.4f);
    private final Paint coverPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Matrix coverMatrix = new Matrix();
    private final Map<Bitmap, BitmapShader> shaders = new HashMap<>();

    private final RectF box = new RectF();
    private final Path path = new Path();

    private static final class Hit {
        final RectF area = new RectF();
        final Runnable action;
        Hit(float left, float top, float right, float bottom, Runnable action) {
            area.set(left, top, right, bottom);
            this.action = action;
        }
    }

    public ShellView(Context context, ProjectStore store) {
        super(context);
        this.store = store;
        dashedPaint.setPathEffect(new android.graphics.DashPathEffect(new float[] { 9f, 8f }, 0f));
        setBackgroundColor(Design.VOID);
    }

    public void setListener(Listener listener) { this.listener = listener; }

    public Screen screen() { return screen; }

    public void showSplash(long millis) {
        screen = Screen.SPLASH;
        startProgress(millis);
    }

    public void showProjects() {
        screen = Screen.PROJECTS;
        progressRunning = false;
        invalidate();
    }

    public void showLoading(Project project, long millis) {
        screen = Screen.LOADING;
        loadingProject = project;
        startProgress(millis);
    }

    private void startProgress(long millis) {
        progressStart = System.nanoTime();
        progressDuration = Math.max(1L, millis) * 1_000_000L;
        progressRunning = true;
        invalidate();
    }

    private float progress() {
        if (!progressRunning) return 0f;
        float linear = (System.nanoTime() - progressStart) / (float) progressDuration;
        if (linear >= 1f) return 1f;
        // O fim de um carregamento sempre custa mais que o começo; uma rampa
        // linear mentiria sobre isso e pareceria travar no final.
        return 1f - (float) Math.pow(1f - linear, 2.1);
    }

    // ------------------------------------------------------------ desenho

    @Override protected void onDraw(Canvas target) {
        float scale = Math.min(getWidth() / Design.STAGE_W, getHeight() / Design.STAGE_H);
        stage.setScale(scale, scale);
        stage.postTranslate((getWidth() - Design.STAGE_W * scale) * .5f,
                            (getHeight() - Design.STAGE_H * scale) * .5f);
        stage.invert(inverse);

        // Aparelhos mais largos que 16:9 sobram nas laterais. O conteúdo fica no
        // palco medido, mas o fundo e a silhueta transbordam até a borda: uma
        // tarja preta ao lado do splash leria como erro de renderização.
        overflowX = Math.max(0f, (getWidth() / scale - Design.STAGE_W) * .5f);
        overflowY = Math.max(0f, (getHeight() / scale - Design.STAGE_H) * .5f);

        hits.clear();
        target.save();
        target.concat(stage);
        target.clipRect(-overflowX, -overflowY,
                Design.STAGE_W + overflowX, Design.STAGE_H + overflowY);
        target.drawRect(-overflowX, -overflowY,
                Design.STAGE_W + overflowX, Design.STAGE_H + overflowY, canvasPaint);
        drawSilhouette(target);

        switch (screen) {
            case SPLASH:   drawSplash(target);   break;
            case PROJECTS: drawProjects(target); break;
            case LOADING:  drawLoading(target);  break;
        }
        target.restore();

        if (progressRunning) {
            if (progress() >= 1f) {
                progressRunning = false;
                Screen finished = screen;
                Project project = loadingProject;
                post(() -> {
                    if (listener == null) return;
                    if (finished == Screen.SPLASH) listener.onLoadFinished(null);
                    else if (finished == Screen.LOADING) listener.onLoadFinished(project);
                });
            } else {
                postInvalidateOnAnimation();
            }
        }
    }

    /**
     * O planeta do glifo tem centro (1470,168) e raio 92 nos masters; essas duas
     * medidas fixam escala e origem da silhueta sem nenhum ajuste a olho.
     */
    private void drawSilhouette(Canvas target) {
        Bitmap glyph = Design.asset(getContext(), "brand/glyph.png");
        if (glyph == null) return;
        // Ancorada à direita da tela real: em telas largas ela continua saindo
        // pela borda, como nos masters, em vez de flutuar no meio do palco.
        float right = Design.STAGE_W + overflowX + 66f;
        box.set(right - 1012f, -12f, right, -12f + 784f);
        target.drawBitmap(glyph, null, box, silhouettePaint);
    }

    private void drawChrome(Canvas target, boolean leftTag, boolean bottomStamp) {
        float left = -overflowX, right = Design.STAGE_W + overflowX;
        float top = -overflowY, bottom = Design.STAGE_H + overflowY;
        if (leftTag) {
            drawBracket(target, left + 35f, top + 36f, 30f, 44f, true);
            drawCapText(target, "CRIE", left + 68f, top + 60f, tagText);
            drawCapText(target, "SEM LIMITES", left + 68f, top + 80f, tagText);
            drawSquares(target, right - 100f, top + 37f, 11.5f);
        }
        drawBracket(target, right - 82f, bottom - 84f, 43f, 44f, false);
        if (bottomStamp) {
            drawCapText(target, "ASTRA ENGINE", left + 51f, bottom - 89f, stampText);
            drawCapText(target, "v1.0", left + 51f, bottom - 69f, stampText);
        }
    }

    private void drawSquares(Canvas target, float x, float y, float gap) {
        for (int i = 0; i < 3; ++i) {
            float left = x + i * (13f + gap);
            target.drawRect(left, y, left + 13f, y + 13f, i == 0 ? accentPaint : trackPaint);
        }
    }

    private void drawBracket(Canvas target, float x, float y, float w, float h, boolean topLeft) {
        path.reset();
        if (topLeft) {
            path.moveTo(x, y + h);
            path.lineTo(x, y);
            path.lineTo(x + w, y);
        } else {
            path.moveTo(x + w, y);
            path.lineTo(x + w, y + h);
            path.lineTo(x, y + h);
        }
        target.drawPath(path, bracketPaint);
    }

    private void drawCapText(Canvas target, String value, float x, float capTop, Paint paint) {
        target.drawText(value, x, Design.baselineForCapTop(paint, capTop), paint);
    }

    private void drawCenteredCapText(Canvas target, String value, float capTop, Paint paint) {
        float width = paint.measureText(value);
        drawCapText(target, value, (Design.STAGE_W - width) * .5f, capTop, paint);
    }

    private void drawBar(Canvas target, float x, float y, float w, float h) {
        box.set(x, y, x + w, y + h);
        target.drawRoundRect(box, h * .5f, h * .5f, trackPaint);
        float filled = Math.max(h, w * progress());
        box.set(x, y, x + filled, y + h);
        target.drawRoundRect(box, h * .5f, h * .5f, accentPaint);
    }

    private void drawLockup(Canvas target, float x, float y, float w, float h) {
        Bitmap lockup = Design.asset(getContext(), "brand/lockup.png");
        if (lockup == null) return;
        box.set(x, y, x + w, y + h);
        target.drawBitmap(lockup, null, box, imagePaint);
    }

    // ------------------------------------------------------------ telas

    private void drawSplash(Canvas target) {
        drawChrome(target, true, true);
        drawLockup(target, 385f, 376f, 892f, 156f);
        drawBar(target, 571f, 777f, 529f, 9f);
        drawCenteredCapText(target, "INICIANDO ÁREA DE TRABALHO", 814f, captionText);
    }

    private void drawLoading(Canvas target) {
        drawChrome(target, true, true);
        drawLockup(target, 634f, 184f, 385f, 63f);

        box.set(389f, 314f, 389f + 892f, 314f + 351f);
        target.drawRoundRect(box, 16f, 16f, surfacePaint);
        target.drawRoundRect(box, 16f, 16f, linePaint);

        Project project = loadingProject;
        float thumbLeft = 407f, thumbTop = 332f, thumbW = 440f, thumbH = 315f;
        box.set(thumbLeft, thumbTop, thumbLeft + thumbW, thumbTop + thumbH);
        target.drawRoundRect(box, 8f, 8f, raisedPaint);
        Bitmap cover = wideCover(project);
        if (cover != null) drawRoundedBitmap(target, cover, box, 8f);

        float column = 890f;
        drawCapText(target, "PROJETO", column, 386f, fieldText);
        drawCapText(target, project != null ? project.name : "—", column, 415f, titleText);

        // O mark identifica o produto e nunca um comando, então a linha do
        // caminho usa o mesmo desenho de pasta da navegação.
        drawFolderGlyph(target, column, 484f, Design.DIM);
        drawCapText(target, project != null ? shorten(project.path, pathText, 340f) : "\u2014",
                column + 40f, 478f, pathText);

        target.drawRect(column, 520f, 1263f, 521f, lineSoftPaint);

        drawCubeGlyph(target, column, 560f);
        drawCapText(target, "CENA", column + 40f, 545f, fieldText);
        drawCapText(target, project != null ? String.valueOf(project.scenes) : "0",
                column + 40f, 570f, statText);
        target.drawRect(column + 158f, 538f, column + 159f, 580f, lineSoftPaint);
        drawSheetGlyph(target, column + 196f, 560f);
        drawCapText(target, "RECURSOS", column + 236f, 545f, fieldText);
        drawCapText(target, project != null ? String.valueOf(project.assets) : "0",
                column + 236f, 570f, statText);

        drawBar(target, 534f, 731f, 603f, 10f);
        drawCenteredCapText(target, "CARREGANDO RECURSOS DO PROJETO", 768f, captionText);
    }

    private void drawProjects(Canvas target) {
        float left = -overflowX, right = Design.STAGE_W + overflowX;
        float top = -overflowY, bottom = Design.STAGE_H + overflowY;

        drawLockup(target, left + 69f, top + 45f, 372f, 63f);
        drawCapText(target, "CRIE", right - 349f, top + 48f, tagText);
        drawCapText(target, "SEM LIMITES", right - 349f, top + 68f, tagText);
        drawSquares(target, right - 110f, top + 45f, 13f);
        drawBracket(target, right - 87f, bottom - 94f, 44f, 51f, false);
        drawRightCapText(target, "PRONTO PARA", right - 95f, bottom - 93f, tagText);
        drawRightCapText(target, "WHAT'S NEXT", right - 95f, bottom - 73f, tagText);

        // A barra lateral encosta na borda física; deixá-la no palco abriria uma
        // faixa preta à esquerda em telas mais largas que 16:9.
        float railX = left + 14f, railEnd = left + 263f;
        target.drawRect(railEnd, top, railEnd + 1f, bottom, lineSoftPaint);
        for (int i = 0; i < NAV.length; ++i) {
            float itemTop = 216f + i * 80f;
            boolean active = i == navIndex;
            if (active) {
                target.drawRect(railX, itemTop, railEnd, itemTop + 75f, washPaint);
                target.drawRect(railX, itemTop, railX + 5f, itemTop + 75f, accentPaint);
            }
            drawNavIcon(target, i, left + 52f, itemTop + 37.5f, active);
            drawCapText(target, NAV[i], left + 114f, itemTop + 22f, active ? navTextOn : navText);
            final int index = i;
            hits.add(new Hit(railX, itemTop, railEnd, itemTop + 75f, () -> onNav(index)));
        }
        drawCapText(target, "ASTRA ENGINE", left + 51f, bottom - 78f, stampText);
        drawCapText(target, "v1.0.0", left + 51f, bottom - 58f, stampText);

        drawCapText(target, "PROJETOS", railEnd + 33f, 310f, labelText);

        float buttonRight = right - 46f, buttonLeft = buttonRight - 264f;
        box.set(buttonLeft, 234f, buttonRight, 300f);
        target.drawRoundRect(box, 14f, 14f, accentPaint);
        target.drawLine(buttonLeft + 52f, 254f, buttonLeft + 52f, 280f, plusPaint);
        target.drawLine(buttonLeft + 39f, 267f, buttonLeft + 65f, 267f, plusPaint);
        drawCapText(target, "Novo projeto", buttonLeft + 85f, 256f, buttonText);
        hits.add(new Hit(buttonLeft, 234f, buttonRight, 300f, this::requestNewProject));

        // As quatro capas preenchem a faixa entre a barra lateral e a borda
        // direita mantendo o vão de 22 dos masters. Em telas mais largas que
        // 16:9 elas crescem em vez de se afastarem: espalhar os cards deixaria
        // buracos onde o master tem uma prateleira contínua.
        List<Project> list = store.projects();
        float gridLeft = railEnd + 32f;
        float span = (right - 46f) - gridLeft;
        float cardWidth = Math.max(317f, (span - 3f * 22f) / 4f);
        for (int i = 0; i < 4; ++i) {
            float cardLeft = gridLeft + i * (cardWidth + 22f);
            if (i < list.size()) drawProjectCard(target, list.get(i), cardLeft, cardWidth);
            else { drawEmptySlot(target, cardLeft, cardWidth); break; }
        }
    }

    private void drawProjectCard(Canvas target, Project project, float left, float w) {
        float top = 344f, h = 353f, coverH = 284f;
        box.set(left, top, left + w, top + h);
        target.drawRoundRect(box, 12f, 12f, surfacePaint);

        box.set(left, top, left + w, top + coverH);
        target.drawRoundRect(box, 12f, 12f, raisedPaint);
        Bitmap cover = project.thumbnail != null
                ? Design.asset(getContext(), "thumbs/" + project.thumbnail) : null;
        if (cover != null) drawRoundedBitmap(target, cover, box, 12f);
        // O canto de baixo da capa encosta no rodapé, então ele não é redondo.
        target.drawRect(left, top + coverH - 12f, left + w, top + coverH,
                cover != null ? coverEdgePaint(cover, box) : raisedPaint);

        drawCapText(target, "ASTRA", left + 21f, top + 22f, kickerText);
        drawCapText(target, "PROJETO", left + 21f, top + 41f, kickerText);
        if (!SceneTemplate.byId(project.templateId).ready) drawSoonFlag(target, left + w - 21f, top + 20f);
        drawCapText(target, project.name, left + 22f, top + coverH + 23f, cardText);
        drawOpenGlyph(target, left + w - 41f, top + coverH + 25f);

        box.set(left, top, left + w, top + h);
        target.drawRoundRect(box, 12f, 12f, linePaint);
        hits.add(new Hit(left, top, left + w, top + h, () -> {
            if (listener != null) listener.onProjectPicked(project);
        }));
    }

    /**
     * Prepara o pincel de capa reaproveitando shader, matriz e Paint.
     *
     * A tela de carregamento anima a 60 Hz; criar um BitmapShader por quadro
     * colocaria o coletor de lixo exatamente onde o usuário está esperando.
     */
    private void drawFolderGlyph(Canvas target, float x, float centerY, int color) {
        navGlyphPaint.setColor(color);
        float y = centerY - 13f;
        path.reset();
        path.moveTo(x + 2f, y + 9f);
        path.lineTo(x + 2f, y + 23f);
        path.lineTo(x + 26f, y + 23f);
        path.lineTo(x + 26f, y + 12f);
        path.lineTo(x + 12f, y + 12f);
        path.lineTo(x + 9f, y + 8f);
        path.lineTo(x + 2f, y + 8f);
        path.close();
        target.drawPath(path, navGlyphPaint);
    }

    /** Cubo isométrico: a cena. */
    private void drawCubeGlyph(Canvas target, float x, float centerY) {
        navGlyphPaint.setColor(Design.DIM);
        float cx = x + 13f, cy = centerY, half = 12f, rise = 6.5f;
        path.reset();
        path.moveTo(cx, cy - half);
        path.lineTo(cx + half, cy - rise);
        path.lineTo(cx + half, cy + rise);
        path.lineTo(cx, cy + half);
        path.lineTo(cx - half, cy + rise);
        path.lineTo(cx - half, cy - rise);
        path.close();
        target.drawPath(path, navGlyphPaint);
        target.drawLine(cx - half, cy - rise, cx, cy, navGlyphPaint);
        target.drawLine(cx + half, cy - rise, cx, cy, navGlyphPaint);
        target.drawLine(cx, cy, cx, cy + half, navGlyphPaint);
    }

    /** Folha com canto dobrado: os assets. */
    private void drawSheetGlyph(Canvas target, float x, float centerY) {
        navGlyphPaint.setColor(Design.DIM);
        float y = centerY - 13f;
        path.reset();
        path.moveTo(x + 4f, y + 1f);
        path.lineTo(x + 16f, y + 1f);
        path.lineTo(x + 23f, y + 8f);
        path.lineTo(x + 23f, y + 25f);
        path.lineTo(x + 4f, y + 25f);
        path.close();
        target.drawPath(path, navGlyphPaint);
        path.reset();
        path.moveTo(x + 16f, y + 1f);
        path.lineTo(x + 16f, y + 8f);
        path.lineTo(x + 23f, y + 8f);
        target.drawPath(path, navGlyphPaint);
    }

    /**
     * Capa da tela de carregamento.
     *
     * Prefere a variante larga quando ela existe: a moldura aqui é 440x315 e a
     * capa do card, recortada para 317 de largura, entregaria menos pixels do
     * que o espaço pede.
     */
    private Bitmap wideCover(Project project) {
        if (project == null || project.thumbnail == null) return null;
        String name = project.thumbnail;
        int dot = name.lastIndexOf('.');
        if (dot > 0) {
            Bitmap wide = Design.asset(getContext(),
                    "thumbs/" + name.substring(0, dot) + "-wide" + name.substring(dot));
            if (wide != null) return wide;
        }
        return Design.asset(getContext(), "thumbs/" + name);
    }

    /** Selo do que ainda não existe na engine, no canto da capa. */
    private void drawSoonFlag(Canvas target, float right, float top) {
        float width = soonText.measureText("EM BREVE") + 16f;
        box.set(right - width, top, right, top + 22f);
        target.drawRoundRect(box, 3f, 3f, accentPaint);
        drawCapText(target, "EM BREVE", box.left + 8f, top + 7f, soonText);
    }

    private Paint coverEdgePaint(Bitmap cover, RectF area) {
        BitmapShader shader = shaders.get(cover);
        if (shader == null) {
            shader = new BitmapShader(cover, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP);
            shaders.put(cover, shader);
        }
        // Cobertura com corte centrado, não esticamento: as capas vêm em
        // proporções diferentes das molduras e distorcer a foto se veria.
        float scale = Math.max(area.width() / cover.getWidth(), area.height() / cover.getHeight());
        coverMatrix.setScale(scale, scale);
        coverMatrix.postTranslate(area.centerX() - cover.getWidth() * scale * .5f,
                                  area.centerY() - cover.getHeight() * scale * .5f);
        shader.setLocalMatrix(coverMatrix);
        coverPaint.setShader(shader);
        return coverPaint;
    }

    private void drawRoundedBitmap(Canvas target, Bitmap bitmap, RectF area, float radius) {
        target.drawRoundRect(area, radius, radius, coverEdgePaint(bitmap, area));
    }

    private void drawEmptySlot(Canvas target, float left, float w) {
        float top = 344f, h = 353f;
        box.set(left, top, left + w, top + h);
        target.drawRoundRect(box, 12f, 12f, dashedPaint);
        float cx = left + w * .5f, cy = top + h * .5f - 18f;
        target.drawLine(cx, cy - 18f, cx, cy + 18f, slotPaint);
        target.drawLine(cx - 18f, cy, cx + 18f, cy, slotPaint);
        float width = labelText.measureText("NOVO PROJETO");
        drawCapText(target, "NOVO PROJETO", cx - width * .5f, cy + 44f, labelText);
        hits.add(new Hit(left, top, left + w, top + h, this::requestNewProject));
    }

    private void drawOpenGlyph(Canvas target, float x, float y) {
        Paint glyph = openGlyphPaint;
        target.drawLine(x + 3f, y + 16f, x + 17f, y + 2f, glyph);
        target.drawLine(x + 8f, y + 2f, x + 17f, y + 2f, glyph);
        target.drawLine(x + 17f, y + 2f, x + 17f, y + 11f, glyph);
        path.reset();
        path.moveTo(x + 12f, y + 14f);
        path.lineTo(x + 12f, y + 18f);
        path.lineTo(x - 1f, y + 18f);
        path.lineTo(x - 1f, y + 5f);
        path.lineTo(x + 3f, y + 5f);
        target.drawPath(path, glyph);
    }

    /** Ícones da navegação: contorno, como nos masters, tingidos pelo estado. */
    private void drawNavIcon(Canvas target, int index, float left, float centerY, boolean active) {
        Paint glyph = navGlyphPaint;
        glyph.setColor(active ? Design.ACCENT : Design.DIM);
        float x = left, y = centerY - 15f;
        switch (index) {
            case 0:
                path.reset();
                path.moveTo(x + 2f, y + 9f);
                path.lineTo(x + 2f, y + 23f);
                path.lineTo(x + 28f, y + 23f);
                path.lineTo(x + 28f, y + 12f);
                path.lineTo(x + 13f, y + 12f);
                path.lineTo(x + 10f, y + 8f);
                path.lineTo(x + 2f, y + 8f);
                path.close();
                target.drawPath(path, glyph);
                break;
            case 1:
                target.drawLine(x + 15f, y + 3f, x + 15f, y + 27f, glyph);
                target.drawLine(x + 3f, y + 15f, x + 27f, y + 15f, glyph);
                break;
            case 2:
                target.drawLine(x + 15f, y + 4f, x + 15f, y + 19f, glyph);
                target.drawLine(x + 8f, y + 12f, x + 15f, y + 19f, glyph);
                target.drawLine(x + 22f, y + 12f, x + 15f, y + 19f, glyph);
                target.drawLine(x + 4f, y + 26f, x + 26f, y + 26f, glyph);
                break;
            default:
                target.drawCircle(x + 15f, y + 15f, 4.6f, glyph);
                target.drawCircle(x + 15f, y + 15f, 10.4f, glyph);
                // Dentes curtos e grossos sobre o anel; traços finos e longos
                // dariam um sol, que foi o erro da primeira versão.
                toothPaint.setColor(glyph.getColor());
                for (int i = 0; i < 8; ++i) {
                    double angle = Math.PI * i / 4.0;
                    float cos = (float) Math.cos(angle), sin = (float) Math.sin(angle);
                    target.drawLine(x + 15f + cos * 9.6f, y + 15f + sin * 9.6f,
                                    x + 15f + cos * 13.4f, y + 15f + sin * 13.4f, toothPaint);
                }
                break;
        }
    }

    private void drawRightCapText(Canvas target, String value, float right, float capTop, Paint paint) {
        drawCapText(target, value, right - paint.measureText(value), capTop, paint);
    }

    private String shorten(String value, Paint paint, float maxWidth) {
        if (paint.measureText(value) <= maxWidth) return value;
        // Corta pela frente: o fim do caminho é o que identifica o projeto.
        String tail = value;
        while (tail.length() > 4 && paint.measureText("\u2026" + tail) > maxWidth) {
            tail = tail.substring(1);
        }
        return "\u2026" + tail;
    }

    // ------------------------------------------------------------ interação

    private void onNav(int index) {
        if (index == 0) { navIndex = 0; invalidate(); return; }
        if (index == 1) { requestNewProject(); return; }
        if (listener != null) listener.onNavUnavailable(NAV[index]);
    }

    private void requestNewProject() {
        if (listener != null) listener.onNewProjectRequested();
    }

    @Override public boolean onTouchEvent(MotionEvent event) {
        if (event.getActionMasked() != MotionEvent.ACTION_UP) {
            return screen == Screen.PROJECTS;
        }
        if (screen != Screen.PROJECTS) return false;
        touch[0] = event.getX();
        touch[1] = event.getY();
        inverse.mapPoints(touch);
        for (int i = hits.size() - 1; i >= 0; --i) {
            Hit hit = hits.get(i);
            if (hit.area.contains(touch[0], touch[1])) {
                performClick();
                hit.action.run();
                return true;
            }
        }
        return true;
    }

    @Override public boolean performClick() { return super.performClick(); }
}
