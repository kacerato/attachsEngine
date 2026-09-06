package dev.aether.editor.shell;

import org.json.JSONException;
import org.json.JSONObject;

/** Um projeto do usuário como o shell precisa conhecê-lo. */
public final class Project {
    public final String name;
    public final String path;
    public final String templateId;
    public final String thumbnail;   // caminho relativo em assets/astra/thumbs, ou null
    public int scenes;
    public int assets;

    public Project(String name, String path, String templateId, String thumbnail, int scenes, int assets) {
        this.name = name;
        this.path = path;
        this.templateId = templateId;
        this.thumbnail = thumbnail;
        this.scenes = scenes;
        this.assets = assets;
    }

    public JSONObject toJson() throws JSONException {
        JSONObject json = new JSONObject();
        json.put("name", name);
        json.put("path", path);
        json.put("template", templateId);
        if (thumbnail != null) json.put("thumbnail", thumbnail);
        json.put("scenes", scenes);
        json.put("assets", assets);
        return json;
    }

    public static Project fromJson(JSONObject json) {
        return new Project(
                json.optString("name", "Sem nome"),
                json.optString("path", ""),
                json.optString("template", SceneTemplate.EMPTY),
                json.has("thumbnail") ? json.optString("thumbnail") : null,
                json.optInt("scenes", 1),
                json.optInt("assets", 0));
    }
}
