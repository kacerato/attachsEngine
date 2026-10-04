#include "ui/gui_workbench.h"
#include "ui/gui_images.h"
#include "imgui.h"
#include "ui/ui_icon_id.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace ae::ui {
GuiWorkbench::GuiWorkbench() {
  addTool("Diagnostico UI",[](GuiWorkbench &w) {
    ImGui::Text("Documento: %u elementos / revisao %llu",static_cast<u32>(w.document_.nodes().size()),static_cast<unsigned long long>(w.document_.revision()));
    ImGui::Text("Canvas: %.0f x %.0f",w.canvas_.width,w.canvas_.height);
    ImGui::Text("ImGui: %s",IMGUI_VERSION);
    ImGui::Text("Atlas: %u x %u",w.immediate_.atlasWidth(),w.immediate_.atlasHeight());
    ImGui::Text("Comandos recusados: %u",w.immediate_.rejectedCommands());
    ImGui::Text("Eventos descartados: %u",w.preview_.droppedEvents());
    if(const auto *p=w.authorView_.placement(w.selection_)) ImGui::Text("Selecao %u: %.0f,%.0f / %.0fx%.0f",p->node,p->bounds.x,p->bounds.y,p->bounds.width,p->bounds.height);
  });
}
bool GuiWorkbench::addTool(std::string name,Tool draw) {
  if(drawingTool_ || name.empty() || name.size()>128 || !draw || tools_.size()>=32) return false;
  for(const auto &tool:tools_) if(tool.name==name) return false;
  tools_.push_back({std::move(name),std::move(draw)});return true;
}
bool GuiWorkbench::save() {
  if (!storage_ || previewing_) return false;
  history_.commit(document_);
  if(!storage_(document_,resource_,true,diagnostic_)) return false;
  diagnostic_="Interface salva";return true;
}
bool GuiWorkbench::setResource(std::string_view path) {
  if(path.empty() || path.size()>=sizeof(resource_) || path.find('\0')!=std::string_view::npos) return false;
  std::copy(path.begin(),path.end(),resource_);resource_[path.size()]=0;return true;
}
namespace {
void insertPropertyIcon(ImmediateGui &gui,UiDrawList &drawing,UiRect area,UiIcon icon,ImVec2 at,float size) {
  drawing.begin(area,{});
  const auto low=ImGui::GetWindowDrawList()->GetClipRectMin(),high=ImGui::GetWindowDrawList()->GetClipRectMax();
  if(drawing.pushClip({low.x,low.y,high.x-low.x,high.y-low.y})) {
    drawing.addImage({at.x,at.y,size,size},static_cast<UiImageId>(icon));drawing.popClip();
  }
  gui.insert(drawing);
}
void keepActiveFieldVisible() {
  if(!ImGui::IsItemActive()) return;
  const auto window=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
  if(ImGui::GetItemRectMax().y>window.y+size.y || ImGui::GetItemRectMin().y<window.y)
    ImGui::SetScrollHereY(.5f);
}
bool editString(const char *label,std::string &value,usize capacity,ImmediateGui &immediate) {
  std::vector<char> buffer(capacity,0);
  std::copy_n(value.data(),std::min(value.size(),capacity-1),buffer.data());
  if(label[0]!='#') {ImGui::TextUnformatted(label);ImGui::SetNextItemWidth(-1);}
  const std::string id=std::string("##")+label;
  const bool changed=ImGui::InputText(id.c_str(),buffer.data(),buffer.size(),ImGuiInputTextFlags_CallbackAlways,ImmediateGui::inputCallback,&immediate);
  // The Android IME reduces the child height after focus. Keep the actual
  // edited field inside that child instead of hiding it behind the keyboard.
  keepActiveFieldVisible();
  if(!changed) return false;
  value=buffer.data(); return true;
}
bool editColor(const char *label,UiColor &packed) {
  float c[]{float((packed>>16)&255)/255,float((packed>>8)&255)/255,float(packed&255)/255,float(packed>>24)/255};
  if (!ImGui::ColorEdit4(label,c,ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_DisplayHex)) return false;
  auto channel=[](float v){return static_cast<u32>(std::clamp(v,0.0f,1.0f)*255+0.5f);};
  packed=(channel(c[3])<<24)|(channel(c[0])<<16)|(channel(c[1])<<8)|channel(c[2]); return true;
}
}
void GuiWorkbench::setPreview(bool enabled) {
  history_.commit(document_); previewing_=enabled;
  if (enabled) { preview_.load(document_);preview_.setImages(images_); preview_.layout(canvas_); lastEvent_.clear(); }
}
void GuiWorkbench::tree() {
  ImGui::TextUnformatted("ELEMENTOS");
  ImGui::Separator();
  if (ImGui::Button("+ Criar",{-1,36})) ImGui::OpenPopup("create");
  if (ImGui::BeginPopup("create")) {
    for (u32 i=0;i<kGuiKindCount;++i) if (ImGui::MenuItem(guiKindName(static_cast<GuiKind>(i)))) {
      history_.begin(document_);
      const auto *parent=document_.find(selection_);
      selection_=document_.create(static_cast<GuiKind>(i),parent && (parent->kind==GuiKind::Panel || guiContainer(parent->kind)) ? parent->id : 0);
      history_.commit(document_);
    }
    ImGui::EndPopup();
  }
  if (document_.nodes().empty()) ImGui::TextWrapped("Crie um elemento. Selecione um Panel para criar filhos dentro dele.");
  // Parent order is validated; indentation represents the actual ownership tree.
  for (const auto &node : document_.nodes()) {
    u32 depth=0; const auto *parent=document_.find(node.parent);
    while (parent && depth<GuiDocument::kMaximumNodes) { ++depth; parent=document_.find(parent->parent); }
    ImGui::PushID(static_cast<int>(node.id));
    if (depth) ImGui::Indent(static_cast<float>(depth)*12);
    if (ImGui::Selectable(node.name.c_str(),selection_==node.id)) {selection_=node.id;canvasSettings_=false;}
    if (ImGui::IsItemHovered() && ImGui::GetIO().MouseSource!=ImGuiMouseSource_TouchScreen)
      ImGui::SetTooltip("%s | ID %u",guiKindName(node.kind),node.id);
    if (depth) ImGui::Unindent(static_cast<float>(depth)*12);
    ImGui::PopID();
  }
  if (selection_ && document_.find(selection_)) {
    ImGui::Separator();
    if(ImGui::Button("Subir")){history_.begin(document_);document_.reorder(selection_,-1);history_.commit(document_);}ImGui::SameLine();
    if(ImGui::Button("Descer")){history_.begin(document_);document_.reorder(selection_,1);history_.commit(document_);}
    if (ImGui::Button("Duplicar")) { history_.begin(document_); selection_=document_.duplicate(selection_); history_.commit(document_); }
    ImGui::SameLine();
    if (ImGui::Button("Excluir")) { history_.begin(document_); document_.remove(selection_); selection_=0; history_.commit(document_); }
  }
}
void GuiWorkbench::properties() {
  if(propertiesSelection_!=selection_) {propertiesSelection_=selection_;actionSelection_=stateSelection_=0;ImGui::SetScrollY(0);}
  ImGui::TextUnformatted("PROPRIEDADES"); ImGui::Separator();
  if(canvasSettings_){canvasProperties();return;}
  const auto *stored=document_.find(selection_);
  if (!stored) { ImGui::TextWrapped("Selecione na lista ou toque em um elemento no canvas. Arraste para posicionar."); return; }
  GuiNode node=*stored;
  ImGui::PushID(static_cast<int>(node.id));
  const auto iconAt=ImGui::GetCursorScreenPos();
  const auto icon=guiContainer(node.kind)?UiIcon::UiAutoLayout:node.kind==GuiKind::Image?UiIcon::AssetsTexture:UiIcon::UiInterfaceCanvas;
  insertPropertyIcon(immediate_,propertyIconDrawing_,area_,icon,iconAt,20);ImGui::Dummy({20,20});ImGui::SameLine();
  ImGui::Text("%s / ID %u",guiKindName(node.kind),node.id);
  ImGui::SetNextItemWidth(-1);
  bool changed=editString("Nome",node.name,257,immediate_);bool previewMotion=false;
  if(!guiContainer(node.kind) && node.kind!=GuiKind::Image)changed=editString("Texto",node.text,4097,immediate_)||changed;
  auto editPair=[&](const char *label,UiPoint &v){float values[]{v.x,v.y};ImGui::TextUnformatted(label);ImGui::SetNextItemWidth(-1);const bool edit=ImGui::DragFloat2((std::string("##")+label).c_str(),values,1,0,100000);keepActiveFieldVisible();v={values[0],values[1]};return edit;};
  if(node.kind==GuiKind::Image && ImGui::CollapsingHeader("Imagem",ImGuiTreeNodeFlags_DefaultOpen)) {
    changed=editString("Recurso do projeto",node.image,1025,immediate_)||changed;
    if(ImGui::Button("Escolher imagem")){imagePaths_=imageChoices_?imageChoices_():std::vector<std::string>{};ImGui::OpenPopup("images");}
    // Follow the available editor surface when Android's IME changes its height.
    ImGui::SetNextWindowPos({area_.x+area_.width*.5f,area_.y+16},ImGuiCond_Always,{.5f,0});
    ImGui::SetNextWindowSize({std::max(120.f,std::min(580.f,area_.width-32)),std::max(100.f,std::min(440.f,area_.height-32))},ImGuiCond_Always);
    if(ImGui::BeginPopup("images")) {
      ImGui::SetNextItemWidth(-1);
      ImGui::InputTextWithHint("##image_search","Buscar imagem",imageQuery_,sizeof(imageQuery_),
          ImGuiInputTextFlags_CallbackAlways,ImmediateGui::inputCallback,&immediate_);
      keepActiveFieldVisible();
      std::vector<usize> matches;
      const std::string_view query(imageQuery_);
      for(usize i=0;i<imagePaths_.size();++i) {
        const auto &path=imagePaths_[i];
        if(query.empty() || std::search(path.begin(),path.end(),query.begin(),query.end(),
            [](unsigned char a,unsigned char b){return std::tolower(a)==std::tolower(b);})!=path.end())matches.push_back(i);
      }
      ImGui::Text("%zu / %zu imagens",matches.size(),imagePaths_.size());
      if(imagePaths_.empty())ImGui::TextWrapped("Importe PNG/JPEG/KTX2 pelo navegador de arquivos do projeto.");
      else if(matches.empty())ImGui::TextUnformatted("Nenhuma imagem encontrada.");
      ImGui::BeginChild("image_results",{0,0});
      ImGuiListClipper clipper;clipper.Begin(static_cast<int>(matches.size()));
      while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
        const auto &path=imagePaths_[matches[static_cast<usize>(i)]];
        if(ImGui::Selectable(path.c_str(),path==node.image)) {node.image=path;changed=true;ImGui::CloseCurrentPopup();}
      }
      ImGui::EndChild();ImGui::EndPopup();
    }
    int fit=static_cast<int>(node.imageFit);if(ImGui::Combo("Ajuste",&fit,"Esticar\0Conter\0Cobrir\0")){node.imageFit=static_cast<GuiImageFit>(fit);changed=true;}
    changed=editColor("Tinta",node.imageTint)||changed;
    if(const auto *image=images_?images_->find(node.image):nullptr){if(!image->error.empty())ImGui::TextWrapped("%s",image->error.c_str());else ImGui::Text("%u x %u pixels",image->width,image->height);}
  }
  if(ImGui::CollapsingHeader("Interacao",ImGuiTreeNodeFlags_DefaultOpen)) {
    const bool builtIn=node.kind==GuiKind::Button || node.kind==GuiKind::Toggle || node.kind==GuiKind::Slider;
    if(builtIn)ImGui::TextUnformatted("Controle com entrada propria");
    if(node.kind!=GuiKind::Button)changed=ImGui::Checkbox(builtIn?"Emitir clique adicional":"Clicavel",&node.interaction.clickable)||changed;
    if(node.interaction.clickable || node.kind==GuiKind::Button) {
      ImGui::TextUnformatted("Ao clicar");
      int action=static_cast<int>(node.interaction.action);ImGui::SetNextItemWidth(-1);
      if(ImGui::Combo("##click_action",&action,"Evento para script\0Alternar visibilidade\0Alternar habilitado\0Definir valor\0Iniciar animacao\0Parar e restaurar animacao\0")){node.interaction.action=static_cast<GuiClickAction>(action);changed=true;}
      if(action) {
        ImGui::TextUnformatted("Alvo");
        const auto *target=document_.find(node.interaction.target);
        const std::string label=node.interaction.target?(target?target->name:"Alvo removido / ID "+std::to_string(node.interaction.target)):"Este elemento";
        ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##click_target",label.c_str())) {
          if(ImGui::Selectable("Este elemento",!node.interaction.target)){node.interaction.target=0;changed=true;}
          for(const auto &candidate:document_.nodes())if(ImGui::Selectable(candidate.name.c_str(),candidate.id==node.interaction.target)){node.interaction.target=candidate.id;changed=true;}
          ImGui::EndCombo();
        }
        if(action==3){changed=ImGui::DragFloat("Valor da acao",&node.interaction.value,.05f)||changed;keepActiveFieldVisible();}
        const auto *actual=node.interaction.target?target:&node;
        if(!actual)ImGui::TextWrapped("Alvo ausente. Escolha outro elemento; o runtime reporta este erro.");
        else if(action==3 && actual->kind!=GuiKind::Toggle && actual->kind!=GuiKind::Slider && actual->kind!=GuiKind::Progress)ImGui::TextWrapped("Definir valor requer Toggle, Slider ou Progress.");
        else if(action>=4 && !actual->motion.enabled)ImGui::TextWrapped("Ative Animacao no alvo antes de executar esta acao.");
      }
    }
  }
  const auto conceptHeader=[&](UiIcon icon,const char *label) {
    // ImGui callbacks consume these lists at frame end; each marker needs stable storage.
    auto &drawing=behaviorIconDrawing_[icon==UiIcon::UiActionSequence?0:1];
    auto at=ImGui::GetCursorScreenPos();at.y+=3;
    insertPropertyIcon(immediate_,drawing,area_,icon,at,20);
    ImGui::Dummy({24,24});ImGui::SameLine();return ImGui::CollapsingHeader(label);
  };
  if(conceptHeader(UiIcon::UiActionSequence,"Sequencia de acoes")) {
    ImGui::TextWrapped("A acao principal de clique roda primeiro. As demais rodam em ordem por evento.");
    if(ImGui::Button("Adicionar acao") && node.actions.size()<GuiDocument::kMaximumActions){node.actions.push_back({});actionSelection_=static_cast<int>(node.actions.size()-1);changed=true;}
    ImGui::Text("%zu / 16 acoes adicionais",node.actions.size());
    static constexpr const char *names[]{"Evento para script","Alternar visibilidade","Alternar habilitado","Definir valor","Iniciar animacao","Parar animacao"};
    for(usize i=0;i<node.actions.size();++i) {
      const auto &a=node.actions[i];ImGui::PushID(static_cast<int>(i));
      const std::string label=std::to_string(i+1)+". "+(a.event==GuiEventKind::Click?"Clique: ":"Valor: ")+names[static_cast<u32>(a.action)];
      if(ImGui::Selectable(label.c_str(),actionSelection_==static_cast<int>(i)))actionSelection_=static_cast<int>(i);
      ImGui::PopID();
    }
    if(!node.actions.empty()) {
      actionSelection_=std::clamp(actionSelection_,0,static_cast<int>(node.actions.size()-1));
      auto &a=node.actions[static_cast<usize>(actionSelection_)];
      int event=static_cast<int>(a.event),action=static_cast<int>(a.action);
      if(ImGui::Combo("Evento##binding",&event,"Clique\0Valor alterado\0")){a.event=static_cast<GuiEventKind>(event);changed=true;}
      ImGui::SetNextItemWidth(-1);
      if(ImGui::Combo("##binding_action",&action,"Evento para script\0Alternar visibilidade\0Alternar habilitado\0Definir valor\0Iniciar animacao\0Parar animacao\0")){a.action=static_cast<GuiClickAction>(action);changed=true;}
      if(action) {
        const auto *target=document_.find(a.target);const std::string label=a.target?(target?target->name:"Alvo removido / ID "+std::to_string(a.target)):"Este elemento";
        ImGui::TextUnformatted("Alvo");ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##binding_target",label.c_str())){if(ImGui::Selectable("Este elemento",!a.target)){a.target=0;changed=true;}for(const auto &n:document_.nodes())if(ImGui::Selectable(n.name.c_str(),n.id==a.target)){a.target=n.id;changed=true;}ImGui::EndCombo();}
        if(action==3){changed=ImGui::DragFloat("Valor##binding",&a.value,.05f)||changed;keepActiveFieldVisible();}
        const auto *actual=a.target?target:&node;
        if(!actual)ImGui::TextWrapped("Alvo ausente: esta acao reportara um erro.");
        else if(action==3 && actual->kind!=GuiKind::Toggle && actual->kind!=GuiKind::Slider && actual->kind!=GuiKind::Progress)ImGui::TextWrapped("Definir valor requer Toggle, Slider ou Progress.");
        else if(action>=4 && !actual->motion.enabled)ImGui::TextWrapped("Ative Animacao no alvo.");
      }
      if(ImGui::Button("Subir") && actionSelection_>0){std::swap(node.actions[actionSelection_],node.actions[actionSelection_-1]);--actionSelection_;changed=true;}
      ImGui::SameLine();if(ImGui::Button("Descer") && actionSelection_+1<static_cast<int>(node.actions.size())){std::swap(node.actions[actionSelection_],node.actions[actionSelection_+1]);++actionSelection_;changed=true;}
      if(ImGui::Button("Remover acao")){node.actions.erase(node.actions.begin()+actionSelection_);changed=true;}
      if(event==0 && !node.interaction.clickable && node.kind!=GuiKind::Button)ImGui::TextWrapped("Ative Clicavel em Interacao para emitir clique.");
      if(event==1 && node.kind!=GuiKind::Toggle && node.kind!=GuiKind::Slider && node.kind!=GuiKind::Progress)ImGui::TextWrapped("Este tipo nao emite alteracao de valor.");
    }
  }
  if(conceptHeader(UiIcon::UiVisualStates,"Estados visuais")) {
    auto &t=node.transitions;changed=ImGui::Checkbox("Transicoes por estado",&t.enabled)||changed;
    if(t.enabled) {
      ImGui::TextWrapped("Sem fundo obrigatorio. Tinta multiplica as cores; pose compoe com Animacao.");
      ImGui::Combo("Editar estado",&stateSelection_,"Normal\0Pressionado\0Desabilitado\0");
      auto &v=stateSelection_==2?t.disabled:stateSelection_==1?t.pressed:t.normal;
      float xy[]{v.pose.x,v.pose.y};ImGui::TextUnformatted("Deslocamento XY");ImGui::SetNextItemWidth(-1);
      changed=ImGui::DragFloat2("##state_xy",xy,1,-100000,100000)||changed;keepActiveFieldVisible();v.pose.x=xy[0];v.pose.y=xy[1];
      changed=ImGui::DragFloat("Escala##state",&v.pose.scale,.01f,.01f,100,"%.2f")||changed;keepActiveFieldVisible();
      changed=ImGui::SliderFloat("Opacidade##state",&v.pose.opacity,0,1,"%.2f")||changed;keepActiveFieldVisible();
      changed=editColor("Tinta##estado",v.tint)||changed;
      changed=ImGui::DragFloat("Transicao (s)",&t.duration,.01f,0,60,"%.2f")||changed;keepActiveFieldVisible();
      int easing=static_cast<int>(t.easing);if(ImGui::Combo("Curva##state",&easing,"Linear\0Suave\0Acelerar\0Desacelerar\0")){t.easing=static_cast<GuiEasing>(easing);changed=true;}
      ImGui::TextWrapped("Interagir testa toque, soltar e cancelar. Habilitado no alvo ou ancestral determina o estado desabilitado.");
    }
  }
  if(ImGui::CollapsingHeader("Animacao",ImGuiTreeNodeFlags_DefaultOpen)) {
    auto &m=node.motion;changed=ImGui::Checkbox("Animar elemento",&m.enabled)||changed;
    if(m.enabled) {
      ImGui::TextWrapped("Posicao, escala e opacidade se propagam aos filhos. O layout dos irmaos permanece estavel.");
      if(ImGui::Button("Pulsar")){m.from={};m.to={0,0,1.15f,1};m.duration=.5f;m.pingPong=m.loop=true;changed=true;}
      ImGui::SameLine();if(ImGui::Button("Aparecer")){m.from={0,0,1,0};m.to={};m.duration=.3f;m.pingPong=m.loop=false;changed=true;}
      const auto editPose=[&](const char *name,GuiPose &p){
        ImGui::TextUnformatted(name);float xy[]{p.x,p.y};ImGui::SetNextItemWidth(-1);
        bool edit=ImGui::DragFloat2((std::string("##pose_")+name).c_str(),xy,1,-100000,100000);keepActiveFieldVisible();p.x=xy[0];p.y=xy[1];
        ImGui::PushID(name);edit=ImGui::DragFloat("Escala",&p.scale,.01f,.01f,100,"%.2f")||edit;keepActiveFieldVisible();
        edit=ImGui::SliderFloat("Opacidade",&p.opacity,0,1,"%.2f")||edit;keepActiveFieldVisible();ImGui::PopID();return edit;
      };
      changed=editPose("Inicio / deslocamento XY",m.from)||changed;changed=editPose("Fim / deslocamento XY",m.to)||changed;
      changed=ImGui::DragFloat("Duracao (s)",&m.duration,.01f,.01f,3600,"%.2f")||changed;keepActiveFieldVisible();
      changed=ImGui::DragFloat("Atraso (s)",&m.delay,.01f,0,3600,"%.2f")||changed;keepActiveFieldVisible();
      int easing=static_cast<int>(m.easing);if(ImGui::Combo("Curva",&easing,"Linear\0Suave\0Acelerar\0Desacelerar\0")){m.easing=static_cast<GuiEasing>(easing);changed=true;}
      changed=ImGui::Checkbox("Iniciar automaticamente",&m.autoPlay)||changed;
      changed=ImGui::Checkbox("Repetir",&m.loop)||changed;changed=ImGui::Checkbox("Ir e voltar",&m.pingPong)||changed;
      if(ImGui::Button("Pre-visualizar animacao",{-1,0}))previewMotion=true;
      ImGui::TextWrapped("Parar restaura a pose de autoria. Use Interagir para testar as acoes.");
    }
  }
  const auto *parent=document_.find(node.parent);const bool managed=parent&&guiContainer(parent->kind)&&!node.sizing.ignore;
  if(guiContainer(node.kind) || (parent&&guiContainer(parent->kind))) {
    if(ImGui::CollapsingHeader("Composicao automatica",ImGuiTreeNodeFlags_DefaultOpen)) {
      if(parent && guiContainer(parent->kind)) {
      changed=editPair("Tamanho minimo",node.sizing.minimum)||changed;
      changed=editPair("Preferido (0 = medir)",node.sizing.preferred)||changed;
      if(parent->kind!=GuiKind::Grid)changed=editPair("Peso de expansao",node.sizing.flexible)||changed;
      changed=ImGui::Checkbox("Fora do fluxo",&node.sizing.ignore)||changed;
      }
      if(guiContainer(node.kind)) {
        float padding[]{node.sizing.padding.left,node.sizing.padding.top,node.sizing.padding.right,node.sizing.padding.bottom};
        ImGui::TextUnformatted("Padding: esquerda / topo / direita / base");ImGui::SetNextItemWidth(-1);
        changed=ImGui::DragFloat4("##Padding",padding,1,0,100000)||changed;keepActiveFieldVisible();node.sizing.padding={padding[0],padding[1],padding[2],padding[3]};
        if(node.kind==GuiKind::Grid)changed=editPair("Espacamento XY",node.sizing.spacing)||changed;
        else {float &gap=node.kind==GuiKind::HBox?node.sizing.spacing.x:node.sizing.spacing.y;changed=ImGui::DragFloat("Espacamento",&gap,1,0,100000)||changed;keepActiveFieldVisible();}
        int align=static_cast<int>(node.sizing.alignment);if(ImGui::Combo("Alinhamento",&align,"Inicio\0Centro\0Fim\0Esticar\0")){node.sizing.alignment=static_cast<GuiAlignment>(align);changed=true;}
        if(node.kind==GuiKind::Grid){int columns=static_cast<int>(node.sizing.columns);if(ImGui::SliderInt("Colunas",&columns,1,64)){node.sizing.columns=static_cast<u32>(columns);changed=true;}}
      }
      if(managed)ImGui::TextWrapped("O container controla este retangulo. Use tamanho, peso e ordem; Fora do fluxo libera ancoras e arraste.");
    }
  }
  if (ImGui::CollapsingHeader("Estado",ImGuiTreeNodeFlags_DefaultOpen)) {
    changed=ImGui::Checkbox("Visivel",&node.visible)||changed;
    changed=ImGui::Checkbox("Habilitado",&node.enabled)||changed;
    if (node.kind==GuiKind::Slider || node.kind==GuiKind::Progress) {
      changed=ImGui::DragFloat("Minimo",&node.minimum,0.05f)||changed;
      keepActiveFieldVisible();
      changed=ImGui::DragFloat("Maximo",&node.maximum,0.05f)||changed;
      keepActiveFieldVisible();
      if (node.maximum>node.minimum) {
        ImGui::SetNextItemWidth(-1);
        node.value=std::clamp(node.value,node.minimum,node.maximum);
        changed=ImGui::SliderFloat("Valor",&node.value,node.minimum,node.maximum)||changed;
        keepActiveFieldVisible();
      }
    } else if (node.kind==GuiKind::Toggle) {
      bool on=node.value==node.maximum;
      if (ImGui::Checkbox("Marcado",&on)) { node.value=on?node.maximum:node.minimum;changed=true; }
    }
  }
  if (ImGui::CollapsingHeader("Layout",ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::BeginDisabled(managed);
    float minimum[]{node.anchorMin.x,node.anchorMin.y}, maximum[]{node.anchorMax.x,node.anchorMax.y};
    ImGui::TextUnformatted("Ancora inicial");ImGui::SetNextItemWidth(-1);
    changed=ImGui::DragFloat2("##Ancora inicio",minimum,0.01f,0,1,"%.2f")||changed;
    keepActiveFieldVisible();
    ImGui::TextUnformatted("Ancora final");ImGui::SetNextItemWidth(-1);
    changed=ImGui::DragFloat2("##Ancora fim",maximum,0.01f,0,1,"%.2f")||changed;
    keepActiveFieldVisible();
    node.anchorMin={minimum[0],minimum[1]}; node.anchorMax={maximum[0],maximum[1]};
    float offsets[]{node.offsets.x,node.offsets.y,node.offsets.width,node.offsets.height};
    ImGui::TextUnformatted("Bordas: esquerda / topo");ImGui::SetNextItemWidth(-1);
    changed=ImGui::DragFloat2("##Esquerda / Topo",offsets,1,-10000,10000,"%.0f")||changed;
    keepActiveFieldVisible();
    ImGui::TextUnformatted("Bordas: direita / base");ImGui::SetNextItemWidth(-1);
    changed=ImGui::DragFloat2("##Direita / Base",offsets+2,1,-10000,10000,"%.0f")||changed;
    keepActiveFieldVisible();
    node.offsets={offsets[0],offsets[1],offsets[2],offsets[3]};
    if (ImGui::Button("Esticar")) { node.anchorMin={0,0};node.anchorMax={1,1};node.offsets={16,16,-16,-16};changed=true; }
    ImGui::SameLine();
    if (ImGui::Button("Centralizar")) {
      const auto *placement=authorView_.placement(node.id);
      const float w=placement?placement->bounds.width:200,h=placement?placement->bounds.height:48;
      node.anchorMin=node.anchorMax={0.5f,0.5f};node.offsets={-w/2,-h/2,w/2,h/2};changed=true;
    }
    ImGui::TextWrapped("Offsets medem as bordas relativas as ancoras. Esticar acompanha o tamanho do pai.");
    ImGui::EndDisabled();
    changed=ImGui::Checkbox("Recortar filhos",&node.clipChildren)||changed;
  }
  if (ImGui::CollapsingHeader("Aparencia",ImGuiTreeNodeFlags_DefaultOpen)) {
    bool backgroundVisible=(node.background>>24)!=0;
    if(ImGui::Checkbox("Desenhar fundo",&backgroundVisible)) {
      node.background=(node.background&0x00FFFFFF)|(backgroundVisible?0xFF000000:0);
      changed=true;
    }
    changed=editColor("Fundo",node.background)||changed;
    if(node.kind!=GuiKind::Image && !guiContainer(node.kind)) {
      changed=editColor("Texto / Controle",node.foreground)||changed;
      changed=ImGui::DragFloat("Fonte",&node.fontSize,0.5f,6,128,"%.1f")||changed;
      keepActiveFieldVisible();
    }
    if(node.kind==GuiKind::Button || node.kind==GuiKind::Toggle || node.kind==GuiKind::Slider || node.kind==GuiKind::Progress)
      changed=editColor("Destaque",node.accent)||changed;
    changed=ImGui::DragFloat("Raio",&node.radius,0.5f,0,512,"%.1f")||changed;
    keepActiveFieldVisible();
  }
  if (changed) { history_.begin(document_); document_.update(node,diagnostic_); }
  if(previewMotion){history_.commit(document_);setPreview(true);preview_.playAnimation(node.id);}
  // Keep a drag/text edit together. Commit once no item owns the interaction.
  if (!ImGui::IsAnyItemActive()) history_.commit(document_);
  ImGui::PopID();
}
void GuiWorkbench::canvasProperties() {
  auto canvas=document_.canvas();bool changed=false;
  const auto iconAt=ImGui::GetCursorScreenPos();
  insertPropertyIcon(immediate_,propertyIconDrawing_,area_,canvas.mode==GuiCanvasMode::World?UiIcon::UiWorldCanvas:UiIcon::UiInterfaceCanvas,iconAt,24);ImGui::Dummy({24,24});ImGui::SameLine();
  ImGui::TextUnformatted("Canvas / apresentacao");
  int mode=static_cast<int>(canvas.mode);if(ImGui::Combo("Destino",&mode,"Tela\0Mundo 3D\0")){canvas.mode=static_cast<GuiCanvasMode>(mode);changed=true;}
  if(canvas.mode==GuiCanvasMode::World) {
    float resolution[]{canvas.resolution.x,canvas.resolution.y};ImGui::TextUnformatted("Resolucao de autoria");ImGui::SetNextItemWidth(-1);
    changed=ImGui::DragFloat2("##Resolucao",resolution,1,32,8192)||changed;keepActiveFieldVisible();canvas.resolution={resolution[0],resolution[1]};
    ImGui::TextUnformatted("Posicao no mundo");ImGui::SetNextItemWidth(-1);changed=ImGui::DragFloat3("##Posicao",canvas.position,.05f,-1000000,1000000)||changed;keepActiveFieldVisible();
    ImGui::TextUnformatted("Rotacao XYZ em graus");ImGui::SetNextItemWidth(-1);changed=ImGui::DragFloat3("##Rotacao",canvas.rotation,1,-36000,36000)||changed;keepActiveFieldVisible();
    changed=ImGui::DragFloat("Unidades / pixel",&canvas.unitsPerPixel,.0001f,.00001f,10,"%.5f")||changed;keepActiveFieldVisible();
    changed=ImGui::Checkbox("Oclusao pela cena",&canvas.occlusion)||changed;
    ImGui::TextWrapped("Componha em 2D; entre em Play para ver e interagir no plano 3D. Posicao marca o centro do plano.");
  }
  if(changed){history_.begin(document_);document_.setCanvas(canvas,diagnostic_);}if(!ImGui::IsAnyItemActive())history_.commit(document_);
}
void GuiWorkbench::draw(const UiRect &area,const UiRect &surface,UiDrawList &list,float dt) {
  area_=area; if (area.isEmpty()) return;
  immediate_.begin(surface.right(),surface.bottom(),dt);
  ImGui::SetNextWindowPos({area.x,area.y}); ImGui::SetNextWindowSize({area.width,area.height});
  ImGui::Begin("Interface##attachs",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
  const auto mark=ImGui::GetCursorScreenPos();
  toolbarDrawing_.begin(area,list.fontMetrics());
  toolbarDrawing_.addImage({mark.x,mark.y,24,24},static_cast<UiImageId>(UiIcon::UiInterfaceCanvas));
  immediate_.insert(toolbarDrawing_);ImGui::Dummy({24,24});ImGui::SameLine();
  ImGui::TextUnformatted("INTERFACE"); ImGui::SameLine();
  if(ImGui::Button("Canvas")){canvasSettings_=true;compactRoute_=2;}ImGui::SameLine();
  bool preview=previewing_;
  if (ImGui::Checkbox("Interagir",&preview)) setPreview(preview);
  ImGui::SameLine();
  ImGui::BeginDisabled(previewing_ || !history_.canUndo());
  if (ImGui::Button("Desfazer")) history_.undo(document_);
  ImGui::EndDisabled(); ImGui::SameLine();
  ImGui::BeginDisabled(previewing_ || !history_.canRedo());
  if (ImGui::Button("Refazer")) history_.redo(document_);
  ImGui::EndDisabled();
  if(!tools_.empty()) {
    ImGui::SameLine();
    if(ImGui::Button("Ferramentas")) ImGui::OpenPopup("tools");
    if(ImGui::BeginPopup("tools")) {
      for(u32 i=0;i<tools_.size();++i) if(ImGui::MenuItem(tools_[i].name.c_str())) activeTool_=i+1;
      ImGui::EndPopup();
    }
  }
  if(area.width<950) ImGui::NewLine(); else ImGui::SameLine();
  ImGui::SetNextItemWidth(std::max(100.0f,std::min(220.0f,area.width*0.4f)));
  ImGui::InputText("##resource",resource_,sizeof(resource_),ImGuiInputTextFlags_CallbackAlways,ImmediateGui::inputCallback,&immediate_);
  ImGui::SameLine();
  ImGui::BeginDisabled(!storage_ || previewing_);
  if (ImGui::Button("Salvar")) save();
  ImGui::SameLine();
  if (ImGui::Button("Abrir")) { history_.begin(document_); if (storage_(document_,resource_,false,diagnostic_)) selection_=0; history_.commit(document_); }
  ImGui::EndDisabled();
  ImGui::Separator();
  const bool compact=area.width<950;
  if (compact && !previewing_) {
    if(ImGui::RadioButton("Canvas",compactRoute_==0)) compactRoute_=0;
    ImGui::SameLine(); if(ImGui::RadioButton("Elementos",compactRoute_==1)) compactRoute_=1;
    ImGui::SameLine(); if(ImGui::RadioButton("Propriedades",compactRoute_==2)) compactRoute_=2;
  }
  ImVec2 available=ImGui::GetContentRegionAvail(); available.y=std::max(32.0f,available.y-28);
  const float left=(!compact && !previewing_)?190.0f:0, right=(!compact && !previewing_)?300.0f:0;
  if (left>0) { ImGui::BeginChild("tree",{left,available.y}); tree(); ImGui::EndChild(); ImGui::SameLine(); }
  if (compact && compactRoute_ && !previewing_) {
    ImGui::BeginChild("context",{available.x,available.y});
    if(compactRoute_==1) tree();else properties();
    ImGui::EndChild(); canvas_={};
  } else {
    const float width=std::max(32.0f,available.x-left-right-(left?16:0));
    ImGui::BeginChild("canvas",{width,available.y},ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    const auto origin=ImGui::GetCursorScreenPos(), space=ImGui::GetContentRegionAvail();
    canvas_={origin.x,origin.y,space.x,space.y};
    ImGui::InvisibleButton("canvasInput",space);
    if(authorView_.document().revision()!=document_.revision()) authorView_.load(document_,false);
    if(previewing_)preview_.advance(dt);
    authorView_.layout(canvas_); preview_.layout(canvas_);
    canvasDrawing_.begin(canvas_,list.fontMetrics());
    const UiPalette palette;
    canvasDrawing_.addRect(canvas_,palette.silhouette);
    if (previewing_) preview_.draw(canvasDrawing_); else {
      authorView_.draw(canvasDrawing_);
      if(const auto *p=authorView_.placement(selection_)) {
        canvasDrawing_.pushClip(p->clip);
        canvasDrawing_.addBorder(p->bounds,palette.accent,2,0);
        canvasDrawing_.popClip();
      }
      if(document_.nodes().empty()) {
        UiTypeStyle style{};style.size=16;
        canvasDrawing_.addText(canvas_,"Crie sua interface em + Criar",palette.textMuted,style,UiAlign::Center);
      }
    }
    immediate_.insert(canvasDrawing_);
    ImGui::EndChild();
  }
  if(right>0) { ImGui::SameLine();ImGui::BeginChild("properties",{right,available.y}); properties();ImGui::EndChild(); }
  GuiEvent event;
  while(preview_.poll(event)) {
    const auto *n=preview_.document().find(event.node);
    lastEvent_=(n?n->name:"element")+std::string(event.kind==GuiEventKind::Click?" : clique":" : valor = ");
    if(event.kind==GuiEventKind::ValueChanged)lastEvent_+=std::to_string(event.value);
  }
  if(previewing_ && !preview_.diagnostic().empty())ImGui::TextWrapped("%s",preview_.diagnostic().c_str());
  else if (!diagnostic_.empty()) ImGui::TextUnformatted(diagnostic_.c_str());
  else if (!lastEvent_.empty()) ImGui::TextUnformatted(lastEvent_.c_str());
  else ImGui::Text("%u elementos | %s | .aeui v4",static_cast<u32>(document_.nodes().size()),previewing_?"Preview isolado":"Edicao");
  ImGui::End();
  if(activeTool_ && activeTool_<=tools_.size()) {
    bool open=true;
    ImGui::SetNextWindowSize({std::min(580.0f,area.width-24),std::min(350.0f,area.height-24)},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({area.x+12,area.y+70},ImGuiCond_FirstUseEver);
    if(ImGui::Begin(tools_[activeTool_-1].name.c_str(),&open,ImGuiWindowFlags_NoSavedSettings)) {
      drawingTool_=true;tools_[activeTool_-1].draw(*this);drawingTool_=false;
    }
    ImGui::End();
    if(!open) activeTool_=0;
  }
  immediate_.end(list);
}
bool GuiWorkbench::pointer(const UiPointerEvent &e) {
  if(!captured_ && e.phase==UiPointerPhase::Move && area_.contains(e.position)) { immediate_.pointer(e);return true; }
  if(e.phase==UiPointerPhase::Down) {
    if(captured_ || !area_.contains(e.position)) return false;
    captured_=true;pointerId_=e.pointerId;
    canvasPointer_=canvas_.contains(e.position) && !immediate_.overlayAt(e.position,"Interface##attachs");
    if(canvasPointer_ && !previewing_) {
      selection_=authorView_.hit(e.position);canvasSettings_=false;dragEditable_=false;dragStart_=e.position;
      if(const auto *node=document_.find(selection_)) {const auto *parent=document_.find(node->parent);dragEditable_=!parent || !guiContainer(parent->kind) || node->sizing.ignore; dragNode_=*node;dragBefore_=document_;history_.begin(document_); }
    }
  }
  if(!captured_ || pointerId_!=e.pointerId) return false;
  if(canvasPointer_) {
    if(previewing_) preview_.pointer(e);
    else if(selection_ && dragEditable_ && e.phase==UiPointerPhase::Move) {
      auto node=dragNode_; const float x=e.position.x-dragStart_.x,y=e.position.y-dragStart_.y;
      node.offsets.x+=x;node.offsets.width+=x;node.offsets.y+=y;node.offsets.height+=y;
      document_.update(node,diagnostic_);
    }
  } else immediate_.pointer(e);
  if(e.phase==UiPointerPhase::Up || e.phase==UiPointerPhase::Cancel) {
    if(canvasPointer_ && !previewing_ && selection_) {
      if(e.phase==UiPointerPhase::Cancel) document_=dragBefore_;
      history_.commit(document_);
    }
    captured_=false;canvasPointer_=false;
  }
  return true;
}
void GuiWorkbench::cancelPointers() {
  if(captured_) pointer({pointerId_,UiPointerPhase::Cancel,dragStart_});
  preview_.cancelPointers();
}
} // namespace ae::ui
