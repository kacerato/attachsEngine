#include "editor/editor_widgets.h"

namespace ae::editor {
namespace detail {
inline constexpr WidgetRange widgetRanges[]{
  {EditorWidget::ColliderGeometryOpen,32},
  {EditorWidget::GuiHierarchyRowBase,0x0010'0000u},
  {EditorWidget::GuiHierarchyCollapseBase,0x0010'0000u},
  {EditorWidget::TweenRestart,0x0001'0000u},
  {EditorWidget::TimerControlBase,0x0001'0000u},
  {EditorWidget::PathPointSelectBase,kRange},
  {EditorWidget::HierarchyRowBase,kWideRange},{EditorWidget::HierarchyEyeBase,kWideRange},
  {EditorWidget::TransformFieldBase,kWideRange},{EditorWidget::GizmoAxisBase,kWideRange},
  {EditorWidget::NumericKeyBase,kRange},{EditorWidget::ConsoleRowBase,kRange},{EditorWidget::ImportLinkRevertBase,kRange},
  {EditorWidget::MaterialChoiceBase,kRange},{EditorWidget::MaterialNumberBase,kRange},{EditorWidget::MaterialTextureBase,kRange},
  {EditorWidget::TextureChoiceBase,kRange},{EditorWidget::TextureViewBase,kRange},{EditorWidget::TextureUvStepBase,kRange},
  {EditorWidget::TextureFilterBase,kRange},{EditorWidget::TextureManagerRowBase,kRange},{EditorWidget::TextureUserBase,kRange},
  {EditorWidget::TextureInspectorBase,kRange},
  {EditorWidget::ComponentGroupBase,kRange},{EditorWidget::ComponentVisualBase,kRange},{EditorWidget::ComponentVisualsToggle,kRange},
  {EditorWidget::ColorHueBase,kRange},{EditorWidget::AssetRowBase,kRange},{EditorWidget::CodeBody,kRange},
  {EditorWidget::RoutePointBase,kRange},{EditorWidget::CreationCategoryBase,kRange},{EditorWidget::CreationRowBase,kRange},
  {EditorWidget::FilesUp,kRange},{EditorWidget::FileRowBase,kRange},{EditorWidget::ComponentAddBase,kRange},
  {EditorWidget::ComponentFoldBase,kRange},{EditorWidget::ComponentRemoveBase,kRange},{EditorWidget::ComponentBooleanBase,kRange},
  {EditorWidget::ComponentCopyBase,kRange},{EditorWidget::ComponentPasteBase,kRange},{EditorWidget::ComponentResetBase,kRange},
  {EditorWidget::ComponentMenuBase,kRange},{EditorWidget::ComponentEnumBase,kRange},{EditorWidget::NameKeyBase,kRange},
  {EditorWidget::ScriptAddBase,kRange},{EditorWidget::ScriptFoldBase,kRange},{EditorWidget::ScriptMenuBase,kRange},
  {EditorWidget::ScriptRemoveBase,kRange},{EditorWidget::ScriptEnabledBase,kRange},{EditorWidget::ScriptSourceBase,kRange},
  {EditorWidget::ScriptFieldBase,kRange},{EditorWidget::ComponentNumberBase,kRange},{EditorWidget::MeshChoiceBase,kRange},
  {EditorWidget::ComponentReferenceBase,kRange},{EditorWidget::ReferenceChoiceBase,kRange},{EditorWidget::CodeTemplateBase,kRange},
  {EditorWidget::ComponentTripleBase,kRange},{EditorWidget::ComponentColorBase,kRange},{EditorWidget::ComponentResourceBase,kRange},
  {EditorWidget::HierarchyCollapseBase,kWideRange},{EditorWidget::ComponentPreviewComposition,kRange},
  {EditorWidget::ComponentPropertySearch,kRange},
  {EditorWidget::PrefabOverrideApplyBase,kRange},
  {EditorWidget::ComponentFieldResetBase,kWideRange},
  {EditorWidget::ImpactOpenBase,kRange},{EditorWidget::ImpactRowBase,kRange},{EditorWidget::ImpactClose,kRange},
  {EditorWidget::SceneViewRowBase,kRange},{EditorWidget::ImportMeshRowBase,kRange},
  {EditorWidget::SceneTemplateRowBase,kRange},{EditorWidget::ComponentSlotNumberBase,kRange},
  {EditorWidget::ComponentClipMoveUpBase,kRange},{EditorWidget::ComponentClipMoveDownBase,kRange},
  {EditorWidget::ComponentClipRemoveBase,kRange},
  {EditorWidget::InputActionRowBase,64},{EditorWidget::InputBindingRowBase,16},
  {EditorWidget::CreationRecipeBase,0x0010'0000u},{EditorWidget::ComponentFamilyBase,kRange},
  {EditorWidget::ComponentMoveUpBase,kRange},{EditorWidget::ComponentMoveDownBase,kRange},
  {EditorWidget::ComponentPasteNewBase,kRange},{EditorWidget::ComponentHelpBase,kRange},
  {EditorWidget::ComponentEnableBase,kRange},{EditorWidget::ComponentEnumOptionBase,kRange},
  {EditorWidget::ComponentEnumPickerClose,kRange},{EditorWidget::ScriptArraySizeBase,kRange},
  {EditorWidget::ScriptArrayElementBase,kRange},{EditorWidget::ScriptArrayHandleBase,kRange},
  {EditorWidget::ScriptArrayAddBase,kRange},{EditorWidget::ScriptArrayRemoveBase,kRange},{EditorWidget::GradientBase,kRange},
  {EditorWidget::CurveBase,kRange},{EditorWidget::LodBar,kRange},{EditorWidget::ReferenceModeToggle,kRange},
  {EditorWidget::InspectorLock,kRange},{EditorWidget::UndoHistoryClose,kRange},{EditorWidget::SceneLayersOpen,kRange},{EditorWidget::GlobalSearchOpen,kRange},{EditorWidget::LayoutsOpen,kRange},{EditorWidget::StatusConsole,kRange},{EditorWidget::MaterialInspectorClose,kRange},{EditorWidget::HierarchyMultiToggle,kRange},{EditorWidget::HierarchyPickBase,kRange},{EditorWidget::FilesMultiToggle,kRange},
  {EditorWidget::PropertyTweenPick,kRange},{EditorWidget::PropertyTweenChoiceBase,kRange},{EditorWidget::PropertyTweenPickerClose,kRange},
  {EditorWidget::PhysicsMaterialInspectorClose,kRange},{EditorWidget::PhysicsMaterialStepBase,kRange},{EditorWidget::AnimatorBase,kRange},{EditorWidget::NavigationBase,kRange}};
inline constexpr bool widgetRangesDisjoint() {
  for(const auto &a:widgetRanges) for(const auto &b:widgetRanges) {
    if(&a==&b) continue;
    const u64 a0=widgetId(a.base),b0=widgetId(b.base);
    if(a0<b0+b.span && b0<a0+a.span) return false;
  }
  return true;
}
}
static_assert(detail::widgetRangesDisjoint(),"duas faixas de widget se cruzam");
// Códigos dentro de EditorWidget::AnimatorBase; os "+ índice" somam a posição.
} // namespace ae::editor
