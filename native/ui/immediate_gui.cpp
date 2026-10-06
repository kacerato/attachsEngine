#include "ui/immediate_gui.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>

namespace ae::ui {
namespace {
UiColor argb(ImU32 c) { return (c & 0xFF00FF00u) | ((c & 0xFFu)<<16) | ((c>>16)&0xFFu); }
ImVec4 color(UiColor c) { return {float((c>>16)&255)/255,float((c>>8)&255)/255,float(c&255)/255,float(c>>24)/255}; }
void nativeMarker(const ImDrawList *,const ImDrawCmd *) {}
}
ImmediateGui::ImmediateGui() {
  IMGUI_CHECKVERSION(); context_=ImGui::CreateContext(); activate();
  auto &io=ImGui::GetIO(); io.IniFilename=nullptr; io.LogFilename=nullptr;
  io.BackendPlatformName="attachsEngine"; io.BackendRendererName="attachsEngine instanced UI";
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
  ImFontConfig config; config.SizePixels=17;
  io.Fonts->AddFontDefault(&config);
  unsigned char *pixels=nullptr; int w=0,h=0;
  io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
  atlas_.assign(pixels,pixels+static_cast<usize>(w)*h*4); atlasWidth_=static_cast<u32>(w); atlasHeight_=static_cast<u32>(h);
  io.Fonts->SetTexID(static_cast<ImTextureID>(1));
  ImGui::StyleColorsDark(); auto &s=ImGui::GetStyle(); const UiPalette p;
  s.WindowRounding=0; s.ChildRounding=0; s.FrameRounding=3; s.PopupRounding=3;
  s.FramePadding={8,7}; s.ItemSpacing={8,7}; s.WindowPadding={12,12};
  s.Colors[ImGuiCol_WindowBg]=color(p.canvas); s.Colors[ImGuiCol_ChildBg]=color(p.surface);
  s.Colors[ImGuiCol_Text]=color(p.text); s.Colors[ImGuiCol_TextDisabled]=color(p.textMuted);
  s.Colors[ImGuiCol_FrameBg]=color(p.raised); s.Colors[ImGuiCol_Border]=color(p.lineSoft);
  s.Colors[ImGuiCol_Button]=color(p.raised); s.Colors[ImGuiCol_ButtonHovered]=color(p.track);
  s.Colors[ImGuiCol_ButtonActive]=color(p.accent); s.Colors[ImGuiCol_CheckMark]=color(p.accent);
  s.Colors[ImGuiCol_SliderGrab]=color(p.accent); s.Colors[ImGuiCol_Header]=color(p.raised);
  s.Colors[ImGuiCol_HeaderHovered]=color(p.track); s.Colors[ImGuiCol_HeaderActive]=color(p.accent);
}
ImmediateGui::~ImmediateGui() { ImGui::DestroyContext(context_); }
void ImmediateGui::activate() const noexcept { ImGui::SetCurrentContext(context_); }
bool ImmediateGui::setFont(std::span<const u8> ttf,float pixels) {
  if(hasBegun_ || ttf.size()<12 || ttf.size()>2u*1024u*1024u || !std::isfinite(pixels) || pixels<6 || pixels>64 ||
     ttf[0]!=0 || ttf[1]!=1 || ttf[2]!=0 || ttf[3]!=0) return false;
  activate();fontData_.assign(ttf.begin(),ttf.end());auto &io=ImGui::GetIO();
  io.Fonts->Clear();io.FontDefault=nullptr;
  ImFontConfig config;config.FontDataOwnedByAtlas=false;
  if(!io.Fonts->AddFontFromMemoryTTF(fontData_.data(),static_cast<int>(fontData_.size()),pixels,&config)) return false;
  unsigned char *rgba=nullptr;int w=0,h=0;io.Fonts->GetTexDataAsRGBA32(&rgba,&w,&h);
  atlas_.assign(rgba,rgba+static_cast<usize>(w)*h*4);atlasWidth_=static_cast<u32>(w);atlasHeight_=static_cast<u32>(h);
  io.Fonts->SetTexID(static_cast<ImTextureID>(1));return true;
}
void ImmediateGui::begin(float w,float h,float dt) { activate();hasBegun_=true;auto &io=ImGui::GetIO(); io.DisplaySize={w,h}; io.DeltaTime=std::clamp(dt,0.001f,0.1f); ImGui::NewFrame(); }
void ImmediateGui::end(UiDrawList &list) {
  activate(); ImGui::Render(); rejectedCommands_=0;
  const auto *data=ImGui::GetDrawData(); if (!data) return;
  for (const auto *mesh : data->CmdLists) for (const auto &command : mesh->CmdBuffer) {
    if (command.UserCallback) {
      if (command.UserCallback==nativeMarker) {
        if (!list.append(*static_cast<const UiDrawList *>(command.UserCallbackData))) ++rejectedCommands_;
        continue;
      }
      // Reset is unnecessary: this backend has no mutable per-command GPU state.
      if (command.UserCallback!=ImDrawCallback_ResetRenderState) ++rejectedCommands_;
      continue;
    }
    // This backend exposes its font atlas only. Unknown user textures are an error,
    // never silently drawn using a different image.
    if (command.GetTexID()!=static_cast<ImTextureID>(1)) { ++rejectedCommands_; continue; }
    UiRect clip{command.ClipRect.x-data->DisplayPos.x,command.ClipRect.y-data->DisplayPos.y,
                command.ClipRect.z-command.ClipRect.x,command.ClipRect.w-command.ClipRect.y};
    if (!list.pushClip(clip)) { ++rejectedCommands_; continue; }
    for (unsigned int i=0; i+2<command.ElemCount; i+=3) {
      UiMeshVertex v[3]; bool valid=true;
      for (unsigned int k=0;k<3;++k) {
        const auto offset=command.IdxOffset+i+k;
        if (offset>=static_cast<unsigned int>(mesh->IdxBuffer.Size)) { valid=false; break; }
        const auto index=command.VtxOffset+mesh->IdxBuffer[static_cast<int>(offset)];
        if (index>=static_cast<unsigned int>(mesh->VtxBuffer.Size)) { valid=false; break; }
        const auto &vertex=mesh->VtxBuffer[static_cast<int>(index)];
        v[k]={{vertex.pos.x-data->DisplayPos.x,vertex.pos.y-data->DisplayPos.y},{vertex.uv.x,vertex.uv.y},argb(vertex.col)};
      }
      if (!valid || !list.addTriangle(v[0],v[1],v[2])) ++rejectedCommands_;
    }
    list.popClip();
  }
}
void ImmediateGui::insert(const UiDrawList &list) { activate(); ImGui::GetWindowDrawList()->AddCallback(nativeMarker,const_cast<UiDrawList *>(&list)); }
void ImmediateGui::pointer(const UiPointerEvent &e) {
  activate(); auto &io=ImGui::GetIO();
  if (e.phase==UiPointerPhase::Down) { if (pointerActive_) return; pointerActive_=true; pointerId_=e.pointerId; }
  if (pointerActive_ && e.pointerId!=pointerId_) return;
  io.AddMouseSourceEvent(e.device==UiPointerDevice::Touch ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
  io.AddMousePosEvent(e.position.x,e.position.y);
  if (e.phase==UiPointerPhase::Down) io.AddMouseButtonEvent(0,true);
  if (e.phase==UiPointerPhase::Up || e.phase==UiPointerPhase::Cancel) { io.AddMouseButtonEvent(0,false); pointerActive_=false; }
}
void ImmediateGui::text(std::string_view text) { activate(); std::string copy(text); ImGui::GetIO().AddInputCharactersUTF8(copy.c_str()); }
void ImmediateGui::key(int key,bool down) {
  activate();
  if ((key>=ImGuiKey_NamedKey_BEGIN && key<ImGuiKey_NamedKey_END) || key==ImGuiMod_Ctrl || key==ImGuiMod_Shift || key==ImGuiMod_Alt || key==ImGuiMod_Super)
    ImGui::GetIO().AddKeyEvent(static_cast<ImGuiKey>(key),down);
}
void ImmediateGui::wheel(float x,float y) { activate(); ImGui::GetIO().AddMouseWheelEvent(x,y); }
bool ImmediateGui::wantsKeyboard() const noexcept { activate(); return ImGui::GetIO().WantTextInput; }
u32 ImmediateGui::inputId() const noexcept {
  activate(); const auto *g=ImGui::GetCurrentContext();
  return g->ActiveId==g->InputTextState.ID && ImGui::GetIO().WantTextInput ? g->ActiveId : 0;
}
std::string ImmediateGui::inputText() const {
  activate();const auto &state=ImGui::GetCurrentContext()->InputTextState;
  return inputId() && state.TextA.Data ? std::string(state.TextA.Data,static_cast<usize>(state.TextLen)) : std::string{};
}
bool ImmediateGui::replaceInput(u32 id,std::string_view text) {
  if(!id || id!=inputId() || text.size()>4096 || text.find('\0')!=std::string_view::npos) return false;
  auto &state=ImGui::GetCurrentContext()->InputTextState;
  if(state.Flags & ImGuiInputTextFlags_ReadOnly || text.size()>=static_cast<usize>(state.BufCapacity)) return false;
  if(!(state.Flags & ImGuiInputTextFlags_CallbackAlways)) {
    // ImGui's built-in numeric editors do not install our callback. Use its
    // real text-input path so filtering and the internal undo state stay valid.
    state.SelectAll();
    if(text.empty()) {key(ImGuiKey_Backspace,true);key(ImGuiKey_Backspace,false);}
    else ImGui::GetIO().AddInputCharactersUTF8(std::string(text).c_str());
    return true;
  }
  replacementId_=id;replacement_=text;return true;
}
void ImmediateGui::finishInput(bool accept) {
  const int k=accept?ImGuiKey_Enter:ImGuiKey_Escape;key(k,true);key(k,false);
}
void ImmediateGui::cancelInput() {
  activate();replacementId_=0;replacement_.clear();
  ImGui::ClearActiveID();ImGui::GetIO().InputQueueCharacters.resize(0);
  auto &events=ImGui::GetCurrentContext()->InputEventsQueue;
  for(int i=events.Size-1;i>=0;--i)if(events[i].Type==ImGuiInputEventType_Text)events.erase(events.Data+i);
}
bool ImmediateGui::overlayAt(UiPoint point,const char *mainWindow) const noexcept {
  activate();const auto *g=ImGui::GetCurrentContext();
  for(int i=g->Windows.Size-1;i>=0;--i) {
    const auto *w=g->Windows[i];
    if(!w->WasActive || (w->Flags & ImGuiWindowFlags_NoMouseInputs) || !w->OuterRectClipped.Contains({point.x,point.y})) continue;
    if((w->Flags & ImGuiWindowFlags_Popup) || std::string_view(w->RootWindow->Name)!=mainWindow) return true;
    return false;
  }
  return false;
}
int ImmediateGui::inputCallback(ImGuiInputTextCallbackData *data) {
  auto &self=*static_cast<ImmediateGui *>(data->UserData);
  if(data->EventFlag!=ImGuiInputTextFlags_CallbackAlways || self.replacementId_!=ImGui::GetCurrentContext()->InputTextState.ID) return 0;
  if(self.replacement_.size()<static_cast<usize>(data->BufSize)) {
    data->DeleteChars(0,data->BufTextLen);data->InsertChars(0,self.replacement_.c_str());
    data->CursorPos=data->BufTextLen;data->SelectionStart=data->SelectionEnd=data->CursorPos;
  }
  self.replacementId_=0;self.replacement_.clear();return 0;
}
} // namespace ae::ui
