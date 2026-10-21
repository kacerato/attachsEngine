// Included inside ae::editor; only the recipe owns persistent FX authoring.
bool EditorSession::commitTerrainFx(std::shared_ptr<const resources::TerrainFxAsset> before,
    std::shared_ptr<const resources::TerrainFxAsset> after,std::string_view label) {
  const auto root=state_.terrainTarget;const auto project=files_.rootPath();
  const u64 bytes=TerrainFxEditorController::retainedBytes(*before)+TerrainFxEditorController::retainedBytes(*after);
  if(!history_.recordResource(label,[this,root,project,before,after](bool forward){
    if(files_.rootPath()!=project || state_.terrainTarget!=root || !state_.terrainOpen || state_.terrainBusy)return false;
    const auto &expected=forward?*before:*after;std::string current,key,error;
    if(!resources::serializeTerrainFxAsset(state_.terrainDraft.fx,current,error) ||
       !resources::serializeTerrainFxAsset(expected,key,error) || current!=key)return false;
    terrainFxController_.cancel();state_.terrainFx.bitmap.reset();++terrainFxEpoch_;
    state_.terrainDraft.fx=forward?*after:*before;clearTerrainPreview(true);queueTerrainPreview();
    state_.terrainMessage="Máscara FX restaurada · aplicar para publicar";return true;
  },bytes)){state_.terrainMessage="Histórico FX excede orçamento · alteração recusada";return false;}
  state_.terrainDraft.fx=*after;++terrainFxEpoch_;return true;
}
bool EditorSession::handleTerrainFxInput(const ui::UiPointerEvent &event,const ui::UiPointerRouting &routing) {
  auto &fxUi=state_.terrainFx;const auto id=routing.widgetId;std::string error;
  const auto cancel=[&]{const bool active=terrainFxController_.active();terrainFxController_.cancel();fxUi.bitmap=terrainFxController_.bitmap();fxUi.cursorVisible=false;
    if(active){clearTerrainPreview(true);queueTerrainPreview();}
  };
  if((terrainFxView(state_.terrainMaterialView) || terrainFxController_.active()) &&
      (event.phase==ui::UiPointerPhase::Down || routing.tapped) &&
      (id==widgetId(EditorWidget::NavigationOrbit) || id==widgetId(EditorWidget::NavigationPan) || id==widgetId(EditorWidget::NavigationZoom) || id==widgetId(EditorWidget::TerrainNavigate))){
    cancel();fxUi.mode=0;fxUi.cursorVisible=false;state_.terrainBrushArmed=false;++terrainFxEpoch_;clearTerrainPreview(true);return false;
  }
  const bool same=terrainFxToken_.root==state_.terrainTarget && terrainFxToken_.epoch==terrainFxEpoch_ && terrainFxToken_.revision==document_.revision();
  if(terrainFxController_.active() && (!same || isPlaying() || state_.terrainBusy || event.phase==ui::UiPointerPhase::Cancel ||
      (event.phase==ui::UiPointerPhase::Down && id!=terrainFxActionBase+90 && routing.target!=ui::UiPointerTarget::Viewport))) {
    cancel();state_.terrainMessage="Gesto FX cancelado · autoria preservada";
  }
  if(routing.tapped && id>=terrainFxActionBase && (id<terrainFxActionBase+90 || (id>=terrainFxActionBase+100 && id<=terrainFxActionBase+175))) {
    const auto action=id-terrainFxActionBase;if(state_.terrainBusy || isPlaying())return true;
    cancel();state_.terrainBrushArmed=false;terrainGestureActive_=false;terrainGestureStrokes_.clear();clearTerrainPreview();++terrainFxEpoch_;
    if(action==0){if(!loadTerrainCatalog())return true;initializeTerrainSurface(state_.terrainDraft);state_.terrainTab=2;state_.terrainMaterialView=300;fxUi.mode=0;++terrainFxEpoch_;fxUi.bitmap.reset();
      if(state_.terrainDraft.fx.enabled && terrainFxController_.refresh(state_.terrainDraft.fx,{},error,terrainFxLimits(fxUi)))fxUi.bitmap=terrainFxController_.bitmap();
    }
    else if(action==24 || action==25){state_.terrainMaterialView=action==24?306:307;}
    else if(action>=120 && action<=175){
      const auto view=state_.terrainMaterialView;auto recipe=state_.terrainDraft;auto &flow=recipe.surface.flow;bool changed=false;
      if(action==160 || action==161){if(!loadTerrainCatalog())return true;initializeTerrainSurface(state_.terrainDraft);state_.terrainTab=2;state_.terrainMaterialView=action==160?350:351;fxUi.mode=0;}
      else if(action==162){if(!flow.rainDrops && (!flow.puddles || !flow.rainDropTexture.texture.valid())){state_.terrainMessage="Ondulações requerem poças ligadas e fonte Rain real";return true;}flow.rainDrops=!flow.rainDrops;changed=true;}
      else if(action==163){flow.specularFade=!flow.specularFade;changed=true;}
      else if(action==164)state_.terrainMaterialView=362;
      else if(action==165)state_.terrainMaterialView=382;
      else if(action==166){if(!state_.terrainCatalog)return true;const auto *preset=terrainRainPreset(*state_.terrainCatalog);if(!preset){state_.terrainMessage="Fonte Rain original indisponível no catálogo";return true;}flow.rainDropTexture.texture=preset->identity;flow.rainDropTexture.channel=resources::TerrainSurfaceChannel::RGB;flow.rainDropTexture.linear=true;changed=true;}
      else if(action==167)state_.terrainMaterialView=view==382?362:350;
      else if(action==168 && view==362){flow.rainDropTexture.invert=!flow.rainDropTexture.invert;changed=true;}
      else if(action>=170 && action<=175 && view==382){const auto source=terrainCatalogResolve(state_,action-170,false);if(!state_.terrainCatalog || source>=state_.terrainCatalog->textures.size()){state_.terrainMessage="Resultado mudou · selecione novamente";return true;}flow.rainDropTexture.texture=state_.terrainCatalog->textures[source].identity;flow.rainDropTexture.channel=resources::TerrainSurfaceChannel::RGB;flow.rainDropTexture.linear=true;changed=true;}
      else if(action==120 || action==121){if(!loadTerrainCatalog())return true;initializeTerrainSurface(state_.terrainDraft);state_.terrainTab=2;state_.terrainMaterialView=action==120?330:340;fxUi.mode=0;}
      else if(action>=122 && action<=126){state_.terrainMaterialView=330+action-122;fxUi.mode=0;}
      else if(action>=127 && action<=131){constexpr u32 views[]{340,341,343,344,345};state_.terrainMaterialView=views[action-127];fxUi.mode=0;}
      else if(action==132)state_.terrainMaterialView=342;
      else if(action==133 || action==134)state_.terrainMaterialView=action==133?360:361;
      else if(action==135 && terrainFlowMapView(view))state_.terrainMaterialView=terrainFlowLava(view)?381:380;
      else if(action==136 && terrainFlowPicker(view))state_.terrainMaterialView=terrainFlowLava(view)?361:360;
      else if(action==137 || action==138){
        auto &enabled=action==137?flow.streams:flow.lava;const auto &map=action==137?flow.streamNormal:flow.lavaDiffuse;
        if(!enabled && (!recipe.fx.enabled || !map.texture.valid())){state_.terrainMessage="Máscara FX e fonte packed real são necessárias";return true;}
        enabled=!enabled;changed=true;
      } else if(action==139){flow.heightDampStreams=!flow.heightDampStreams;changed=true;}
      else if(action==140){flow.heightDampLava=!flow.heightDampLava;changed=true;}
      else if(action==141){flow.perTexFoam=!flow.perTexFoam;changed=true;}
      else if(action==142){flow.lavaStochastic=!flow.lavaStochastic;changed=true;}
      else if(action==143 || action==144){
        if(!state_.terrainCatalog)return true;const auto *preset=terrainFlowPreset(*state_.terrainCatalog,terrainFlowLava(view),action-143);
        if(!preset){state_.terrainMessage="Fonte original não está disponível no catálogo";return true;}
        auto &map=terrainFlowMap(recipe.surface,view);map.texture=preset->identity;map.channel=terrainFlowLava(view)?resources::TerrainSurfaceChannel::RGB:resources::TerrainSurfaceChannel::NormalRG;map.linear=true;changed=true;
      } else if(action==145){state_.terrainMaterialView=terrainFlowLava(view)?340:332;}
      else if(action==146 && terrainFlowMapView(view)){auto &map=terrainFlowMap(recipe.surface,view);map.invert=!map.invert;changed=true;}
      else if(action==147 && terrainFlowMapView(view))state_.terrainMaterialView=terrainFlowLava(view)?360:361;
      else if(action>=150 && action<=155 && terrainFlowPicker(view)){
        const auto source=terrainCatalogResolve(state_,action-150,false);
        if(!state_.terrainCatalog || source>=state_.terrainCatalog->textures.size()){state_.terrainMessage="Resultado mudou · selecione novamente";return true;}
        auto &map=terrainFlowMap(recipe.surface,view);map.texture=state_.terrainCatalog->textures[source].identity;map.channel=terrainFlowLava(view)?resources::TerrainSurfaceChannel::RGB:resources::TerrainSurfaceChannel::NormalRG;map.linear=true;changed=true;
      }
      if(changed){if(!resources::validTerrainRecipe(recipe,error)){state_.terrainMessage=error;return true;}state_.terrainDraft=std::move(recipe);queueTerrainPreview();}
    }
    else if(action>=100 && action<=104){constexpr u32 views[]{310,320,311,312,313};state_.terrainMaterialView=views[action-100];fxUi.mode=0;}
    else if(action>=110 && action<=115){
      auto recipe=state_.terrainDraft;auto &flow=recipe.surface.flow;
      if((action==110 && !flow.wetness) || (action==111 && !flow.puddles) || (action==115 && !flow.wetnessMaskSnow)) {
        if(!recipe.fx.enabled){state_.terrainMessage="Crie ou importe uma máscara FX antes de ligar o material";return true;}
      }
      if(action==111 && flow.puddles && flow.rainDrops){state_.terrainMessage="Desligue as ondulações antes de desligar poças";return true;}
      if(action==110)flow.wetness=!flow.wetness;else if(action==111)flow.puddles=!flow.puddles;
      else if(action==112)flow.heightWetness=!flow.heightWetness;else if(action==113)flow.distanceWetness=!flow.distanceWetness;
      else if(action==114)flow.perTexPorosity=!flow.perTexPorosity;else flow.wetnessMaskSnow=!flow.wetnessMaskSnow;
      if(!resources::validTerrainRecipe(recipe,error)){state_.terrainMessage=error;return true;}
      state_.terrainDraft=std::move(recipe);queueTerrainPreview();
    }
    else if(action>=1 && action<=3){fxUi.mode=action-1;if(state_.terrainMaterialView==306 || action==2)state_.terrainMaterialView=300;
      if(action==2 && state_.terrainDraft.fx.enabled){
        if(terrainFxController_.refresh(state_.terrainDraft.fx,{},error,terrainFxLimits(fxUi)))fxUi.bitmap=terrainFxController_.bitmap();
        else state_.terrainMessage=error;
      }
    }
    else if(action>=4 && action<=7){fxUi.brush.channel=resources::TerrainFxChannel(action-4);if(state_.terrainMaterialView==307)state_.terrainMaterialView=300;}
    else if(action==8)fxUi.erase=!fxUi.erase;
    else if(action>=20 && action<=23){state_.terrainMaterialView=300+action-20;state_.terrainScroll=0;}
    else if(action>=30 && action<=32){constexpr u32 budgets[]{96,192,256};fxUi.budgetMiB=budgets[action-30];}
    else if(action>=40 && action<=43){constexpr u32 sizes[]{256,512,1024,2048};fxUi.resolution=sizes[action-40];}
    else if(action==10){
      const auto &old=state_.terrainDraft.fx;
      if(old.enabled || !old.seedPixels.empty() || !old.strokes.empty()){state_.terrainMessage="Mapa existente preservado · rebase/substituição ainda indisponível";return true;}
      if(state_.terrainDraft.mode!=resources::TerrainMode::Heightfield){state_.terrainMessage="Máscara FX requer heightfield";return true;}
      resources::TerrainFxAsset next;std::shared_ptr<const TerrainFxEditorBitmap> bitmap;const double size=state_.terrainDraft.size;
      if(!TerrainFxEditorController::prepareBlank(fxUi.resolution,fxUi.resolution,{-size*.5,-size*.5,size,size},next,bitmap,error,terrainFxLimits(fxUi))){state_.terrainMessage=error;return true;}
      if(commitTerrainFx(std::make_shared<const resources::TerrainFxAsset>(old),std::make_shared<const resources::TerrainFxAsset>(std::move(next)),"Criar máscara FX")){fxUi.bitmap=std::move(bitmap);state_.terrainMaterialView=300;}
    } else if(action==50)fxUi.brush.slopeFilter=!fxUi.brush.slopeFilter;
    else if(action==51)fxUi.brush.shape=fxUi.brush.shape==resources::TerrainFxBrushShape::Circle?resources::TerrainFxBrushShape::Box:resources::TerrainFxBrushShape::Circle;
    state_.terrainScroll=0;return true;
  }
  if(!terrainFxView(state_.terrainMaterialView) || fxUi.mode!=1)return false;
  const bool canvas=id==terrainFxActionBase+90;
  if(!canvas && routing.target!=ui::UiPointerTarget::Viewport && !terrainFxController_.active())return false;
  if(terrainFxController_.active() && event.pointerId!=terrainFxPointer_)return true;
  // A vertical panel swipe that leaves the auxiliary bitmap is scrolling, not
  // a completed stroke. Keep all movement inside the bitmap available to paint.
  if(canvas && routing.dragging && !fxUi.canvas.contains(event.position) &&
      std::abs(routing.totalDelta.y)>std::abs(routing.totalDelta.x)) {
    cancel();
    if(event.phase==ui::UiPointerPhase::Move)
      state_.terrainScroll=std::clamp(state_.terrainScroll-routing.stepDelta.y,0.f,
          terrainScrollLimit(state_,terrainPanelRect(state_)));
    return true;
  }

  if(state_.terrainBusy || isPlaying())return true;
  const auto &fx=state_.terrainDraft.fx;
  double x=0,z=0;bool hit=false;float world[16]{};resources::TerrainFxSlopeFrame frame;
  const bool transformed=state_.terrainTarget && editorWorldMatrix(document_,state_.terrainTarget,world);
  bool validFrame=true;
  if(transformed)for(u32 axis=0;axis<3;++axis){double length=0;for(u32 k=0;k<3;++k)length+=world[axis*4+k]*world[axis*4+k];
    if(length<1e-12){validFrame=false;break;}for(u32 k=0;k<3;++k)frame.normalToWorld[axis*3+k]=world[axis*4+k]/length;
    for(u32 b=axis+1;b<3;++b){double dot=0;for(u32 k=0;k<3;++k)dot+=world[axis*4+k]*world[b*4+k];if(std::abs(dot)>1e-4)validFrame=false;}}
  if(canvas && fxUi.canvas.contains(event.position)){
    const auto u=(event.position.x-fxUi.canvas.x)/fxUi.canvas.width,v=(event.position.y-fxUi.canvas.y)/fxUi.canvas.height;
    x=fx.domain.originX+u*fx.domain.sizeX;z=fx.domain.originZ+v*fx.domain.sizeZ;hit=true;
  } else if(!canvas && view_.rect.contains(event.position) && router_.hitTest(event.position).target==ui::UiPointerTarget::Viewport && transformed && validFrame){
    const auto ray=screenPointToRay(view_,event.position);buildPickCandidates(nullptr,false,state_.terrainTarget);
    const auto *root=document_.find(state_.terrainTarget);const auto *link=root?scene::importLink(root->components):nullptr;
    std::erase_if(candidates_,[&](const auto &candidate){const auto *object=document_.find(candidate.id);const auto *child=object?scene::importLink(object->components):nullptr;return !link || !child || child->root || child->source!=link->source;});
    const auto picked=pickNearest(candidates_,ray,0,true);
    if(picked.hit){double local[3]{};for(u32 axis=0;axis<3;++axis)for(u32 k=0;k<3;++k)local[axis]+=(ray.origin[k]+ray.direction[k]*picked.distance-world[12+k])*frame.normalToWorld[axis*3+k];x=local[0];z=local[2];hit=true;}
  }
  if(event.phase==ui::UiPointerPhase::Down){
    if(!hit)return true;
    std::vector<float> slope;
    if(fxUi.brush.slopeFilter){
      const auto build=state_.terrainPreviewBuild;
      if(!validFrame || !build || resources::terrainRecipeNonFxKey(build->recipe)!=resources::terrainRecipeNonFxKey(state_.terrainDraft) ||
         !resources::makeTerrainFxSlopeField(build->field,fx,frame,slope,error)){state_.terrainMessage=error.empty()?"Gere prévia correspondente para filtrar inclinação":error;return true;}
    }
    terrainFxToken_={state_.terrainTarget,terrainFxEpoch_,document_.revision()};
    fxUi.bitmap.reset();
    if(!terrainFxController_.begin(fx,fxUi.brush,fxUi.erase,terrainFxToken_,slope,error,terrainFxLimits(fxUi))){state_.terrainMessage=error;return true;}
    terrainFxPointer_=event.pointerId;terrainFxLastTime_=event.timeSeconds;terrainFxLastPreviewTime_=-1e9;
  }
  if(!terrainFxController_.active())return true;
  if(hit){
    const double seconds=std::clamp(event.timeSeconds-terrainFxLastTime_,0.,.25);
    if(!terrainFxController_.sample(terrainFxToken_,x,z,seconds,1,error,terrainFxLimits(fxUi))){cancel();state_.terrainMessage=error;return true;}
    fxUi.bitmap=terrainFxController_.bitmap();fxUi.cursorVisible=true;fxUi.cursorU=float((x-fx.domain.originX)/fx.domain.sizeX);fxUi.cursorV=float((z-fx.domain.originZ)/fx.domain.sizeZ);
  }
  terrainFxLastTime_=event.timeSeconds;
  if(hit && terrainFxController_.bitmap() && (event.phase==ui::UiPointerPhase::Up || event.timeSeconds-terrainFxLastPreviewTime_>=.05)) {
    const auto build=state_.terrainPreviewBuild;
    if(build && !build->mesh.parts.empty() && resources::terrainRecipeNonFxKey(build->recipe)==resources::terrainRecipeNonFxKey(state_.terrainDraft)) {
      if(publishTerrainPreview(*build,error,&terrainFxController_.bitmap()->field))terrainFxLastPreviewTime_=event.timeSeconds;
      else state_.terrainMessage="Prévia FX: "+error;
    } else {queueTerrainPreview();state_.terrainMessage="Prévia do terreno em preparação · bitmap FX preservado";}
  }
  if(event.phase==ui::UiPointerPhase::Up){
    TerrainFxPreparedCommit commit;
    if(!terrainFxController_.hasSamples()){cancel();return true;}
    if(!terrainFxController_.prepareCommit(terrainFxToken_,commit,error,terrainFxLimits(fxUi))){cancel();state_.terrainMessage=error;return true;}
    if(!commitTerrainFx(commit.before,commit.after,"Pintar máscara FX")){cancel();return true;}
    terrainFxController_.accept(commit);fxUi.bitmap=terrainFxController_.bitmap();fxUi.cursorVisible=false;
    queueTerrainPreview();state_.terrainMessage="Máscara FX editada · material estático em prévia";
  }
  return true;
}
