package dev.aether.editor.shell;

import java.util.ArrayList;
import java.util.List;

/**
 * Catálogo de cenas iniciais.
 *
 * IDs antigos permanecem reconhecidos para compatibilidade. A criação mostra
 * somente entradas disponíveis; planos futuros pertencem à documentação.
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
    /** Seleção declarativa do provedor inicial; não depende do nome da cena. */
    public final boolean spectralWater;

    private SceneTemplate(String id, String name, String note, boolean ready,
                          String thumbnail, String previewExtra, boolean spectralWater) {
        this.id = id;
        this.name = name;
        this.note = note;
        this.ready = ready;
        this.thumbnail = thumbnail;
        this.previewExtra = previewExtra;
        this.spectralWater = spectralWater;
    }

    public static List<SceneTemplate> all() {
        List<SceneTemplate> list = new ArrayList<>();
        list.add(new SceneTemplate(EMPTY, "Cena vazia",
                "Cena sem objetos; câmera de edição independente.", true, null, null, false));
        list.add(new SceneTemplate("ocean", "Laboratório oceânico",
                "Oceano espectral, espuma e geometria importada.", true, "water-lab.png", "aether.ocean_preview", true));
        list.add(new SceneTemplate("forest", "Estrada na floresta",
                "Estrada de terra, LOD e sombras.", true, "forest-test.png", "aether.map_preview", false));
        list.add(new SceneTemplate("boat", "Barco na água",
                "Modelo de casco e superfície oceânica.", true, null, "aether.ocean_preview", true));
        list.add(new SceneTemplate("material", "Prévia de material",
                "Esfera PBR e mapa de ambiente.", true, null, "aether.material_preview", false));
        list.add(new SceneTemplate("backroom", "Demonstração de interior",
                "Interior com GI e reflexos.", false, "backroom-demo.png", null, false));
        list.add(new SceneTemplate("vehicle", "Área de veículos",
                "Suspensão e pneus sobre pista.", false, "vehicle-sandbox.png", null, false));
        list.add(new SceneTemplate("river", "Vale com rio",
                "Rio com fluxo e correnteza.", false, null, null, false));
        return list;
    }

    public static SceneTemplate byId(String id) {
        for (SceneTemplate template : all()) {
            if (template.id.equals(id)) return template;
        }
        return all().get(0);
    }
}
