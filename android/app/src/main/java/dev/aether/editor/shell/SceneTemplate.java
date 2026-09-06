package dev.aether.editor.shell;

import java.util.ArrayList;
import java.util.List;

/**
 * Catálogo de cenas iniciais.
 *
 * O que a engine ainda não sabe montar continua listado e marcado EM BREVE em
 * vez de sumir da tela: esconder o roteiro faria o shell parecer completo e
 * transformaria cada ausência em surpresa na hora de criar o projeto.
 */
public final class SceneTemplate {
    public static final String EMPTY = "empty";

    public final String id;
    public final String name;
    public final String note;
    public final boolean ready;
    public final String thumbnail;
    /** Extra que AetherActivity entende, ou null quando a cena é vazia. */
    public final String previewExtra;

    private SceneTemplate(String id, String name, String note, boolean ready,
                          String thumbnail, String previewExtra) {
        this.id = id;
        this.name = name;
        this.note = note;
        this.ready = ready;
        this.thumbnail = thumbnail;
        this.previewExtra = previewExtra;
    }

    public static List<SceneTemplate> all() {
        List<SceneTemplate> list = new ArrayList<>();
        list.add(new SceneTemplate(EMPTY, "Empty Scene",
                "Câmera, luz direcional e nada mais.", true, null, null));
        list.add(new SceneTemplate("ocean", "Ocean Lab",
                "Cascatas FFT, espuma e corpos Jolt.", true, "water-lab.png", "aether.ocean_preview"));
        list.add(new SceneTemplate("forest", "Forest Road",
                "Estrada de terra, LOD e sombras.", true, "forest-test.png", "aether.map_preview"));
        list.add(new SceneTemplate("boat", "Boat On Water",
                "Casco flutuante sobre o oceano.", true, null, "aether.ocean_preview"));
        list.add(new SceneTemplate("material", "Material Preview",
                "Esfera PBR e mapa de ambiente.", true, null, "aether.material_preview"));
        list.add(new SceneTemplate("backroom", "Backroom Demo",
                "Interior com GI e reflexos.", false, "backroom-demo.png", null));
        list.add(new SceneTemplate("vehicle", "Vehicle Sandbox",
                "Suspensão e pneus sobre pista.", false, "vehicle-sandbox.png", null));
        list.add(new SceneTemplate("river", "River Valley",
                "Rio com fluxo e correnteza.", false, null, null));
        return list;
    }

    public static SceneTemplate byId(String id) {
        for (SceneTemplate template : all()) {
            if (template.id.equals(id)) return template;
        }
        return all().get(0);
    }
}
