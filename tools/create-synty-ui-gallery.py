"""Generate a real, editable AEUI 2 project from all baked Synty icons.

The native preview tool creates the project/scene. Every page stays below the
current 64-image atlas limit. Original Unity resources remain in the library;
PNG and GLB outputs are copied to project-relative resource paths.
"""
import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path
from PIL import Image

PAGE_SIZE = 24


def node(identifier, parent, kind, name, text="", *, background=0, anchors=(0, 0, 0, 0),
         offsets=(0, 0, 200, 48), minimum=(0, 0), preferred=(0, 0), flexible=(0, 0),
         padding=(0, 0, 0, 0), spacing=(8, 8), columns=6, image="", visible=True,
         enabled=True, font=14):
    # Exact field order of GuiDocument::write; no custom renderer or UI format.
    fields = [identifier, parent, kind, json.dumps(name), json.dumps(text), *anchors,
              *offsets, background, 0xFFE5E7EB, 0xFF619CA8, font, 0, 0, 0, 1,
              int(visible), int(enabled), 1, *minimum, *preferred, *flexible,
              *padding, *spacing, 3, columns, 0, json.dumps(image), 1, 0xFFFFFFFF]
    return " ".join(map(str, fields))


def document(names, page):
    total = (len(names) + PAGE_SIZE - 1) // PAGE_SIZE
    rows = [node(1, 0, 8, "library", background=0xFF172027,
                 anchors=(0, 0, 1, 1), offsets=(12, 12, -12, -12), padding=(12, 8, 12, 8)),
            node(2, 1, 1, "page_title", f"POLYGON | {len(names)} icones | pagina {page+1}/{total}",
                 preferred=(0, 36), minimum=(0, 28), font=18),
            node(3, 1, 9, "icon_grid", preferred=(600, 400), flexible=(1, 1))]
    for slot in range(PAGE_SIZE):
        index = page * PAGE_SIZE + slot
        valid = index < len(names)
        name = names[index] if valid else ""
        identifier = 4 + slot * 3
        rows += [node(identifier, 3, 0, f"cell_{slot}", preferred=(100, 100), visible=valid),
                 node(identifier+1, identifier, 6, f"icon_{slot}", image=f"images/synty/{name}.png" if valid else "",
                      anchors=(0, 0, 1, 1), offsets=(4, 2, -4, -24)),
                 node(identifier+2, identifier, 1, f"label_{slot}", name.removeprefix("SM_Icon_"),
                      anchors=(0, 1, 1, 1), offsets=(0, -24, 0, 0), font=11)]
    rows += [node(76, 1, 7, "navigation", preferred=(0, 44), minimum=(0, 44)),
             node(77, 76, 2, "previous", "Anterior", preferred=(116, 44), enabled=page > 0),
             node(78, 76, 1, "page_range", f"{page*PAGE_SIZE+1}-{min((page+1)*PAGE_SIZE,len(names))} / {len(names)}",
                  preferred=(100, 44), flexible=(1, 0)),
             node(79, 76, 2, "next", "Proxima", preferred=(116, 44), enabled=page+1 < total)]
    return f"AEUI 2 {len(rows)} 80\n0 1200 800 0 0 0 0 0 0 0.006 1\n" + "\n".join(rows) + "\n"


def behavior(names):
    values = ",\n        ".join(json.dumps(name) for name in names)
    return '''using Astra;

// Uses the existing native document, image atlas and typed Play API.
[ComponentId("example.gui.menu")]
public sealed class GuiMenu : Behavior
{
    private const int PageSize = 24;
    private static readonly string[] Names = [
        ''' + values + '''
    ];
    private readonly GuiElement[] cells = new GuiElement[PageSize];
    private readonly GuiElement[] images = new GuiElement[PageSize];
    private readonly GuiElement[] labels = new GuiElement[PageSize];
    private GuiElement previous, next, title, range;
    private int page;
    public override void Start()
    {
        previous=Gui.Find("previous"); next=Gui.Find("next");
        title=Gui.Find("page_title"); range=Gui.Find("page_range");
        for(int i=0;i<PageSize;i++) {
            cells[i]=Gui.Find($"cell_{i}"); images[i]=Gui.Find($"icon_{i}");
            labels[i]=Gui.Find($"label_{i}");
        }
        ShowPage(0);
    }
    private void ShowPage(int value)
    {
        int pages=(Names.Length+PageSize-1)/PageSize;
        page=System.Math.Clamp(value,0,pages-1);
        for(int i=0;i<PageSize;i++) {
            int index=page*PageSize+i;
            bool valid=index<Names.Length;
            cells[i].Visible=valid;
            images[i].Image=valid?$"images/synty/{Names[index]}.png":"";
            labels[i].Text=valid?Names[index][8..]:"";
        }
        previous.Enabled=page>0; next.Enabled=page+1<pages;
        title.Text=$"POLYGON | {Names.Length} icones | pagina {page+1}/{pages}";
        range.Text=$"{page*PageSize+1}-{System.Math.Min((page+1)*PageSize,Names.Length)} / {Names.Length}";
    }
    public override void Update(float elapsed)
    {
        while(Gui.Poll(out var message)) {
            if(message.Kind!=GuiEventKind.Click)continue;
            if(message.Element.Id==previous.Id)ShowPage(page-1);
            if(message.Element.Id==next.Id)ShowPage(page+1);
        }
    }
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("library", type=Path)
    parser.add_argument("project", type=Path)
    parser.add_argument("--preview-tool", type=Path, required=True)
    args = parser.parse_args()
    library, project = args.library.resolve(), args.project.resolve()
    if project.exists():
        raise ValueError("Use a new project directory; existing authoring is never overwritten")
    source = json.loads((library / "catalog.json").read_text(encoding="utf8"))
    baked = json.loads((library / "baked-catalog.json").read_text(encoding="utf8"))
    names = [icon["name"] for icon in source["icons"]]
    if len(baked) != len(names) or {icon["name"] for icon in baked} != set(names):
        raise ValueError("Bake is incomplete")
    for icon in baked:
        with Image.open(library / icon["sprite"]) as image:
            if image.mode != "RGBA" or image.getchannel("A").getextrema() != (0, 255):
                raise ValueError("Missing real transparency: " + icon["name"])
        if icon["vertices"] <= 0:
            raise ValueError("Empty geometry: " + icon["name"])
    subprocess.run([str(args.preview_tool.resolve()), "write-project", str(project), "expansion"], check=True)
    for folder in ("images/synty", "images/synty-palettes", "models/synty"):
        (project / folder).mkdir(parents=True)
    resources = []
    for icon in baked:
        row = dict(icon)
        for field, folder in (("sprite", "images/synty"), ("model", "models/synty")):
            target = project / folder / Path(icon[field]).name
            shutil.copyfile(library / icon[field], target)
            row[field] = target.relative_to(project).as_posix()
            row[field + "Sha256"] = hashlib.sha256(target.read_bytes()).hexdigest()
        resources.append(row)
    palettes = []
    for asset in json.loads((library / "baked-palettes.json").read_text(encoding="utf8")):
        target = project / "images/synty-palettes" / Path(asset["output"]).name
        if target.exists():
            raise ValueError("Duplicate palette filename: " + asset["output"])
        shutil.copyfile(library / asset["output"], target)
        palettes.append({**asset, "path": target.relative_to(project).as_posix()})
    pages = (len(names) + PAGE_SIZE - 1) // PAGE_SIZE
    for page in range(pages):
        (project / "UI" / f"page-{page+1:02}.aeui").write_text(document(names, page), encoding="utf8")
    (project / "UI/main.aeui").write_text(document(names, 0), encoding="utf8")
    (project / "Scripts/GuiMenu.cs").write_text(behavior(names), encoding="utf8")
    (project / "synty-resources.json").write_text(json.dumps({"packageSha256": source["packageSha256"],
        "iconCount": len(names), "pageSize": PAGE_SIZE, "pageCount": pages, "resources": resources,
        "palettes": palettes}, indent=2), encoding="utf8")
    (project / "README.md").write_text("""# Biblioteca POLYGON na attachsEngine

Abra este diretório como projeto. UI/main.aeui é a primeira página editável.
Em Play, Anterior/Proxima percorrem os 520 ícones pela API C# da engine.
UI/page-01.aeui ... page-22.aeui permitem editar cada página sem executar scripts.
Os controles de navegação e as imagens têm fundo transparente.

Para reutilizar um ícone, copie images/synty/<nome>.png para seu projeto e atribua
esse caminho relativo a um controle Image. Para objeto 3D, importe o arquivo
models/synty/<nome>.glb pelo importador real de modelos. Isso não converte o
prefab Unity em um prefab nativo. A geometria, UV e textura foram convertidas;
shader Unity/lighting/cena continuam como fontes na biblioteca original.
As 16 texturas têm versões de 1024² em images/synty-palettes; os originais de
4096² permanecem intactos na biblioteca, junto dos 17 materiais Unity.

Cada imagem é um render RGBA da geometria real, não uma prévia com fundo removido.
synty-resources.json registra GUID, hash, material e saídas. O atlas carrega apenas
os recursos da página atual: 24 no máximo, dentro do limite atual de 64.
O pacote é local e foi excluído do Git público. A licença original continua válida.
""", encoding="utf8")
    print(json.dumps({"project": str(project), "icons": len(names), "pages": pages,
                      "nativeDocuments": pages + 1, "transparentImages": len(baked), "models": len(baked), "palettes": len(palettes)}))


if __name__ == "__main__":
    main()
