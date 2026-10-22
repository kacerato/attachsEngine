#include "editor/editor_session.h"
#include "editor/editor_material_fields.h"
#include "editor/editor_widgets.h"
#include <cstdio>
#include <sstream>

// PK-9: Inspector do Material por shader (Unity 6/URP). Os toques e arrastes
// dos campos de editor_material_fields.h e a entrada numérica deles. A tela
// desenha (editor_screen.cpp); aqui só se lê o gesto e se grava o recurso.
namespace ae::editor {
namespace {
struct FieldHit {
  const MaterialFieldSpec *field=nullptr;
  MaterialFieldPart part=MaterialFieldPart::Main;
  u32 option=0;
};
FieldHit fieldOfWidget(u32 widget) {
  FieldHit hit;
  if(!isMaterialFieldWidget(widget)) return hit;
  const u32 offset=widget-widgetId(EditorWidget::MaterialFieldBase);
  const u32 index=offset/16u,part=offset%16u;
  if(index>=materialFieldSpecs.size()) return hit;
  hit.field=&materialFieldSpecs[index];
  if(part>=static_cast<u32>(MaterialFieldPart::Option) && part<static_cast<u32>(MaterialFieldPart::Track)) {
    hit.part=MaterialFieldPart::Option;hit.option=part-static_cast<u32>(MaterialFieldPart::Option);
  } else hit.part=static_cast<MaterialFieldPart>(part);
  return hit;
}
std::string numberText(float value) {
  char text[32];std::snprintf(text,sizeof(text),"%.4g",static_cast<double>(value));return text;
}
} // namespace

bool EditorSession::setMaterialField(std::string_view key,const float value[3],std::string &diagnostic) {
  diagnostic.clear();
  const auto *field=findMaterialField(key);
  const auto *entity=document_.find(state_.selection);
  const auto *current=findMaterialAsset(sharedMaterialGuid(entity?meshRenderer(*entity):nullptr));
  if(!field || !current) {diagnostic="Abra um material do projeto para editar o shader.";return false;}
  auto candidate=*current;
  if(!applyMaterialField(*field,candidate,value)) {diagnostic="Valor recusado: "+std::string(field->label);return false;}
  // Mesmo valor no ativo ainda vale na multiedição (atribuir a todos); o
  // lote descarta os alvos que não mudam.
  ++candidate.revision;
  if(!commitSharedMaterial(candidate,diagnostic,true,key)) return false;
  // A prévia do arraste já está na GPU; o recurso gravado a substitui.
  state_.materialSliderField=0;
  if(const auto *saved=findMaterialAsset(candidate.guid)) refreshMaterialAssetView(*saved);
  return true;
}

void EditorSession::previewMaterialField(std::string_view key,const float value[3]) {
  const auto *field=findMaterialField(key);
  const auto *entity=document_.find(state_.selection);
  const auto *current=findMaterialAsset(sharedMaterialGuid(entity?meshRenderer(*entity):nullptr));
  if(!field || !current) return;
  auto preview=*current;
  if(!applyMaterialField(*field,preview,value)) return;
  // Só a biblioteca publicada muda: o recurso, o arquivo e o histórico só no Up.
  std::vector<std::pair<resources::AssetGuid,EditorMapScene::SharedMaterial>> library;
  library.reserve(materials_.size());
  for(const auto &material:materials_) {
    const auto &source=material.guid==preview.guid?preview:material;
    library.push_back({source.guid,{source.values,source.textures,source.surface,source.sampling,source.channels,
                                    source.occlusionTexture,source.shading,source.extraTextures}});
  }
  mapScene_.setMaterialLibrary(std::move(library));
  cameraPreview_.invalidateTarget();
  appearanceChanged_=true;
}

bool EditorSession::handleMaterialFieldPointer(const ui::UiPointerEvent &event,const ui::UiPointerRouting &routing) {
  const u32 key=routing.widgetId;
  // Rolagem: arrastar em qualquer ponto do Inspector que não seja um slider.
  const bool card=isMaterialFieldWidget(key) || key==widgetId(EditorWidget::MaterialInspectorScroll) ||
                  (key>=widgetId(EditorWidget::MaterialSectionBase) && key<widgetId(EditorWidget::MaterialSectionBase)+16u);
  const auto hit=fieldOfWidget(key);
  const bool slider=hit.field && hit.field->kind==MaterialFieldKind::Slider && hit.part==MaterialFieldPart::Track;
  if(routing.dragging && card && !slider) {
    const float limit=std::max(0.0f,layout_.materialInspectorContent-layout_.materialInspectorWindow);
    state_.materialInspectorScroll=std::clamp(state_.materialInspectorScroll-routing.stepDelta.y,0.0f,limit);
    return true;
  }
  // Slider: segue o dedo (prévia) e grava ao soltar, como a barra da Unity.
  if(slider) {
    const u32 index=materialFieldIndex(*hit.field);
    const auto found=std::find_if(layout_.materialSliders.begin(),layout_.materialSliders.end(),
                                  [&](const auto &entry) {return entry.first==index;});
    if(found==layout_.materialSliders.end() || found->second.width<=0) return true;
    const float t=std::clamp((event.position.x-found->second.x)/found->second.width,0.0f,1.0f);
    float value[3]{hit.field->minimum+t*(hit.field->maximum-hit.field->minimum),0,0};
    if(routing.released) {
      std::string diagnostic;
      if(!setMaterialField(hit.field->key,value,diagnostic) && !diagnostic.empty()) state_.status=diagnostic;
      state_.materialSliderField=0;
      publishMaterialLibrary();
      return true;
    }
    state_.materialSliderField=index+1;state_.materialSliderValue=value[0];
    previewMaterialField(hit.field->key,value);
    return true;
  }
  if(!routing.tapped) return card;
  if(key>=widgetId(EditorWidget::MaterialSectionBase) && key<widgetId(EditorWidget::MaterialSectionBase)+16u) {
    state_.materialSectionsCollapsed^=1u<<(key-widgetId(EditorWidget::MaterialSectionBase));
    return true;
  }
  if(key==widgetId(EditorWidget::MaterialChoiceClose)) {state_.materialChoiceField=0;return true;}
  if(key>=widgetId(EditorWidget::MaterialChoiceOptionBase) && key<widgetId(EditorWidget::MaterialChoiceOptionBase)+16u &&
     state_.materialChoiceField && state_.materialChoiceField<=materialFieldSpecs.size()) {
    const auto &field=materialFieldSpecs[state_.materialChoiceField-1];
    const float value[3]{static_cast<float>(key-widgetId(EditorWidget::MaterialChoiceOptionBase)),0,0};
    std::string diagnostic;
    state_.materialChoiceField=0;
    if(!setMaterialField(field.key,value,diagnostic)) state_.status=diagnostic;
    else if(field.key=="pk9.shader") state_.status="Shader: "+std::string(field.options[static_cast<u32>(value[0])]);
    return true;
  }
  if(!hit.field) return key==widgetId(EditorWidget::MaterialInspectorScroll);
  const auto &field=*hit.field;
  const u32 index=materialFieldIndex(field);
  const auto &asset=state_.materialSlotView.asset;
  float current[3]{};
  if(field.read && state_.materialSlotView.assetValid) field.read(asset,current);
  std::string diagnostic;
  const auto apply=[&](const float value[3]) {
    if(!setMaterialField(field.key,value,diagnostic)) state_.status=diagnostic;
    return true;
  };
  switch(field.kind) {
  case MaterialFieldKind::Segment: {
    if(hit.part!=MaterialFieldPart::Option || hit.option>=field.options.size()) return true;
    const float value[3]{static_cast<float>(hit.option),0,0};
    return apply(value);
  }
  case MaterialFieldKind::Toggle: {
    const float value[3]{current[0]>.5f?0.f:1.f,0,0};
    return apply(value);
  }
  case MaterialFieldKind::Choice:
    state_.materialChoiceField=state_.materialChoiceField==index+1?0:index+1;
    return true;
  case MaterialFieldKind::Stepper: {
    if(hit.part==MaterialFieldPart::Minus || hit.part==MaterialFieldPart::Plus) {
      const float value[3]{std::clamp(current[0]+(hit.part==MaterialFieldPart::Plus?1.f:-1.f),field.minimum,field.maximum),0,0};
      return apply(value);
    }
    break;
  }
  case MaterialFieldKind::Texture:
    state_.textureBinding=field.binding;state_.texturePicker=true;state_.texturePickerScroll=0;
    state_.materialPicker=false;state_.meshPage=0;
    return true;
  default: break;
  }
  // Valor tocado: teclado numérico com o valor atual (um, dois ou três números).
  if(history_.isOpen()) return true;
  const u32 count=field.kind==MaterialFieldKind::Color?3u:field.kind==MaterialFieldKind::Pair?2u:1u;
  std::string text;
  for(u32 i=0;i<count;++i) text+=(i?" ":"")+numberText(current[i]);
  state_.numericField=materialFieldWidget(index,MaterialFieldPart::Value);
  state_.numericEntity=state_.selection;state_.numericInstance=0;state_.numericProperty=std::string(field.key);
  std::snprintf(state_.numericText,sizeof(state_.numericText),"%s",text.c_str());
  state_.numericReplace=true;state_.numericError=false;
  return true;
}

bool EditorSession::commitMaterialFieldText(u32 widget,std::string_view text) {
  const auto hit=fieldOfWidget(widget);
  if(!hit.field) return false;
  const u32 count=hit.field->kind==MaterialFieldKind::Color?3u:hit.field->kind==MaterialFieldKind::Pair?2u:1u;
  std::string normalized(text);
  // Vírgula decimal do teclado do Android quando há um número só.
  if(count==1) for(auto &c:normalized) if(c==',') c='.';
  std::istringstream input(normalized);
  float value[3]{};
  for(u32 i=0;i<count;++i)
    if(!(input>>value[i])) {
      state_.status=count==3?"Digite três valores: r g b":count==2?"Digite dois valores: x y":"Digite um número";
      return false;
    }
  std::string diagnostic;
  if(!setMaterialField(hit.field->key,value,diagnostic)) {state_.status=diagnostic;return false;}
  return true;
}
} // namespace ae::editor
