#include "editor/editor_session.h"
#include "runtime/scene_lines.h"

namespace ae::editor {

namespace {
// Cache por thread de sceneCameraEntities (editor_scene_camera.h). Fica num .cpp:
// variável thread_local inline em cabeçalho duplica a rotina de TLS no MinGW.
struct SceneCameraCandidates {
  u64 identity = 0, structure = 0;
  bool valid = false;
  std::vector<EditorEntityId> ids, scratch;
};
thread_local SceneCameraCandidates sceneCameraCandidates;
} // namespace

const std::vector<EditorEntityId> &sceneCameraEntities(const runtime::SceneGraph &document) {
  auto &cache=sceneCameraCandidates;
  if(!cache.valid||cache.identity!=document.identity()||cache.structure!=document.structureRevision()) {
    cache.identity=document.identity();cache.structure=document.structureRevision();cache.valid=true;
    cache.ids.clear();document.collectSubtree(document.root(),cache.scratch);
    for(auto id:cache.scratch) if(const auto *entity=document.find(id); entity&&cameraComponent(*entity)) cache.ids.push_back(id);
  }
  return cache.ids;
}

runtime::GameView EditorSession::gameView() const {
  runtime::GameView view;
  const auto rect=layout_.viewport;
  if(rect.isEmpty()) return view;
  view.width=rect.width*pixelsPerUnit_;view.height=rect.height*pixelsPerUnit_;
  view.dpi=dpi_;view.platform=platform_;
  view.frustum=view_.frustum;
  const auto pose=sceneCameraPose();
  float cameraRect[4];
  if(pose.entity && pose.clippedViewport(cameraRect)) {
    // Camera.pixelRect: lente e proporção do retângulo da câmera, não da vista.
    auto projection=projection_;
    const float aspect=pose.buildProjection(projection,rect.width*cameraRect[2],rect.height*cameraRect[3]);
    view.frustum=renderer::buildPerspectiveFrustum(pose.position,pose.yaw,pose.pitch,aspect,projection);
    view.camera=pose.entity;
    view.pixelX=cameraRect[0]*view.width;view.pixelY=cameraRect[1]*view.height;
    view.pixelWidth=cameraRect[2]*view.width;view.pixelHeight=cameraRect[3]*view.height;
  }
  view.valid=view.frustum.valid&&view.width>0&&view.height>0;
  return view;
}

bool EditorSession::playStackView(renderer::RenderViewSnapshot &out) {
  out={};
  const auto stack=sceneCameraStack();
  if(stack.size()<2 || layout_.viewport.isEmpty()) return false;
  // Até duas câmeras visíveis: a principal (a de baixo) e a de cima. As do meio
  // ficam para um compositor de N vistas; o console avisa uma vez por Play.
  if(stack.size()>2 && !playStackWarned_) {
    playStackWarned_=true;
    reportProblem(EditorConsoleSeverity::Warning,"Mais de duas câmeras na tela: só a de baixo e a de cima aparecem.");
  }
  const auto &pose=stack.back();
  const auto rect=pose.screenRect(layout_.viewport);
  if(rect.isEmpty()) return false;
  out.width=std::clamp(static_cast<u32>(std::lround(rect.width*pixelsPerUnit_)),1u,4096u);
  out.height=std::clamp(static_cast<u32>(std::lround(rect.height*pixelsPerUnit_)),1u,4096u);
  auto settings=projection_;
  const float aspect=pose.buildProjection(settings,rect.width,rect.height);
  out.frustum=renderer::buildPerspectiveFrustum(pose.position,pose.yaw,pose.pitch,aspect,settings);
  if(!out.frustum.valid) {out={};return false;}
  const auto version=sceneVersion();
  out.sceneEpoch=version.epoch;out.sceneRevision=version.revision;
  out.cameraEntity=pose.entity;out.environmentLayerMask=pose.environmentMask;
  applySceneCameraOutput(pose,out,aspect);
  out.continuous=true;out.requestId=++playStackSerial_;
  return true;
}

runtime::SceneView EditorSession::currentSceneView(std::string_view name) const {
  runtime::SceneView view;
  view.name = std::string(name);
  std::copy(camera_.target, camera_.target + 3, view.target);
  view.distance = camera_.distance;
  view.yaw = camera_.yaw;
  view.pitch = camera_.pitch;
  view.verticalFov = projection_.verticalFieldOfViewRadians;
  return view;
}

bool EditorSession::applySceneView(u32 index) {
  const auto *view = document_.views().at(index);
  if (!view) return false;
  finishCameraGesture(false);
  std::copy(view->target, view->target + 3, camera_.target);
  camera_.distance = view->distance;
  camera_.yaw = view->yaw;
  camera_.pitch = view->pitch;
  // Lente zero é "a do editor": uma vista antiga não impõe um campo de visão
  // que ela nunca guardou.
  if (view->verticalFov > 0) projection_.verticalFieldOfViewRadians = view->verticalFov;
  state_.viewSelected = index;
  state_.status = "Vista \"" + view->name + "\"";
  return true;
}

bool EditorSession::loadAssets(std::string_view text) {
  if(!resources::AssetRegistry::deserialize(text, assets_)) return false;
  environmentMaps_.clear();
  loadMaterialAssets();
  loadEnvironmentProfiles();
  loadPhysicsMaterials();
  loadAnimatorControllers();
  loadNavMeshes();
  loadProbeAssets();
  loadLightmapAssets();
  loadAnimationClipAssets();
  refreshLibraryRows();
  return true;
}

void EditorSession::setParticleBudget(u32 total) {
  particleBudget_ = total ? total : 1;
  previewParticles_.setBudget(particleBudget_);
  playScene_.particles().setBudget(particleBudget_);
}

void EditorSession::updateParticles(float deltaSeconds, const float (&camera)[3], renderer::particles::ParticleFrame &out) {
  // PK-10a: Material do projeto com shader Partícula -> textura publicada e cor
  // base. A textura de todo Material do projeto já sobe com a biblioteca
  // (collectUsedTextures), então aqui só se resolve o índice.
  const runtime::ParticleMaterialResolver materials = [this](const resources::AssetGuid &guid, runtime::ParticleMaterialLook &look) {
    const auto *material = findMaterialAsset(guid);
    if (!material || !scene::materialShaderIsParticle(material->shading.shader)) return false;
    const u32 texture = mapScene_.resolveTexture(material->textures[0], 0, EditorMapScene::samplerFlags(material->sampling[0]));
    look.texture = texture == scene::MaterialTextureKeep || texture == renderer::InvalidMapTexture ? ~0u : texture;
    std::copy(material->values.baseColor, material->values.baseColor + 3, look.color);
    look.additive = material->shading.blend == scene::MaterialBlendAdditive;
    return true;
  };
  // G6a: malhas das partículas. A Malha do projeto vem da geometria de CPU
  // que o editor já guarda para seleção; o conjunto é refeito quando o pacote
  // publicado muda.
  if (!particleMeshSet_ || particleMeshGeometry_ != mapScene_.geometryIdentity()) {
    particleMeshSet_ = runtime::builtinParticleMeshes();
    particleMeshRanges_.clear();
    particleMeshGeometry_ = mapScene_.geometryIdentity();
  }
  const runtime::ParticleMeshes meshes{particleMeshSet_, [this](const resources::AssetGuid &mesh, u32 &range) {
    return particleMeshRange(mesh, range);
  }};
  // G6a: forma Malha — a mesma resolução das malhas desenhadas, com o conjunto
  // atual (o Play avança antes deste quadro desenhar).
  const runtime::SceneParticles::ShapeMeshResolver shapeMeshes =
      [this](const resources::AssetGuid &mesh, std::shared_ptr<const renderer::particles::ParticleMeshSet> &set, u32 &range) {
        if (!particleMeshRange(mesh, range)) return false;
        set = particleMeshSet_;
        return true;
      };
  previewParticles_.setShapeMeshResolver(shapeMeshes);
  playScene_.particles().setShapeMeshResolver(shapeMeshes);
  if (isPlaying() && playScene_.active()) {
    playScene_.particles().setBudget(particleBudget_);
    playScene_.particles().build(playScene_.document(), camera, out, &materials, &meshes);
    runtime::appendLineRenderers(playScene_.document(), camera, out, &materials);
    out.meshes = particleMeshSet_;
    return;
  }
  previewParticles_.setPreviewLoop(true);
  previewParticles_.setBudget(particleBudget_);
  previewParticles_.advance(evaluatedEditorScene(), deltaSeconds);
  previewParticles_.build(evaluatedEditorScene(), camera, out, &materials, &meshes);
  runtime::appendLineRenderers(evaluatedEditorScene(), camera, out, &materials);
  out.meshes = particleMeshSet_;
}

// Intervalo da Malha do projeto no conjunto das partículas. Uma malha nova
// cria um conjunto novo (cópia + revisão), porque o anterior pode estar com o
// renderer; ausente ou grande demais fica recusada até o pacote mudar.
bool EditorSession::particleMeshRange(const resources::AssetGuid &mesh, u32 &range) {
  if (const auto found = particleMeshRanges_.find(mesh); found != particleMeshRanges_.end()) {
    range = found->second;
    return range != ~0u;
  }
  std::vector<float> positions, uvs;
  std::vector<u32> indices;
  auto next = std::make_shared<renderer::particles::ParticleMeshSet>(*particleMeshSet_);
  const u32 slot = mapScene_.assetSlot(mesh);
  if (!slot || !mapScene_.assetGeometry(slot, positions, uvs, indices) ||
      !renderer::particles::appendParticleMesh(*next, positions, uvs, indices, range)) {
    particleMeshRanges_[mesh] = ~0u;
    return false;
  }
  // Revisão única no processo: um conjunto refeito nunca repete a de outro.
  static u64 revisions = 1;
  next->revision = ++revisions;
  particleMeshSet_ = std::move(next);
  particleMeshRanges_[mesh] = range;
  return true;
}

bool EditorSession::extractMap(std::vector<renderer::MapDrawState> &out) const {
  const auto &graph=evaluatedEditorScene();
  if(!mapScene_.extract(graph, out)) return false;
  applyLodGroups(graph, out);
  if(state_.clipOpen&&state_.clipPoseSelectionIsolated&&!isPlaying())for(auto &draw:out)
    if(!animationPoseSelectionVisible(draw.objectId))draw.visible=false;
  // Camadas e objetos escondidos na vista da cena (só o editor; o Play
  // extrai do mundo).
  if(state_.hiddenLayers || !state_.sceneHidden.empty()) for(auto &draw:out)
    if(const auto *entity=graph.find(static_cast<EditorEntityId>(draw.objectId));
       entity && ((entity->layer<32 && (state_.hiddenLayers&(1u<<entity->layer))) || state_.sceneHiddenHas(entity->id))) draw.visible=false;
  return true;
}

runtime::LodView EditorSession::lodView() const {
  runtime::LodView lod;
  if(isPlaying() && playScene_.active()) {
    const auto camera=resolveSceneCamera(playScene_.document());
    if(camera.entity) {
      std::copy(camera.position,camera.position+3,lod.position);
      float r[4];camera.clippedViewport(r);
      const auto &area=layout_.viewport;
      lod.verticalFov=camera.lens(area.height*r[3]>0?area.width*r[2]/(area.height*r[3]):1.f).verticalFovRadians;
      lod.orthographicHalfHeight=camera.projection==scene::CameraProjection::Orthographic?camera.orthographicHalfHeight:0;
      return lod;
    }
  }
  const auto &frustum=view_.frustum;
  std::copy(frustum.cameraPosition,frustum.cameraPosition+3,lod.position);
  lod.verticalFov=2*std::atan(frustum.tangentHalfVertical);
  lod.orthographicHalfHeight=renderer::isOrthographic(frustum)?frustum.orthographicHalfHeight:0;
  return lod;
}

bool EditorSession::lodSelectionChanged() const {
  const auto &graph=isPlaying()&&playScene_.active()?playScene_.document():evaluatedEditorScene();
  // Uma troca animada muda a cobertura a cada quadro até terminar.
  if(runtime::lodCrossFadeRunning(lodClock_,lastWallSeconds_)) return true;
  // A própria consulta inicia a troca quando a câmera cruza o limite. Esse
  // primeiro quadro ainda tem fator zero e pode ser visualmente idêntico ao
  // anterior; por isso o relógio real precisa reter a transição e o estado
  // "correndo" também conta como mudança. Copiar o relógio aqui descartava
  // o início, então a cena não era republicada nos quadros seguintes.
  const auto next=runtime::lodObjectStates(graph,lodView(),&lodClock_,lastWallSeconds_);
  return runtime::lodCrossFadeRunning(lodClock_,lastWallSeconds_)||next!=lodStates_;
}

bool EditorSession::extractPlayMap(std::vector<renderer::MapDrawState> &out,
                    const runtime::InputDeviceState &platformInput,bool platformFocus) {
  if(!isPlaying()) return false;
  if(!playScene_.active()) {
    if(EditorPlayScene::unresolvedEntity(document_)!=kInvalidEntity) {
      state_.status="Play indisponível: há componentes de tipo ausente";
      reportProblem(EditorConsoleSeverity::Error,state_.status);return false;
    }
    runtimeCodeGeneration_=code_.publishedGeneration();
    playScene_.configureGui(gui_.document());
    playScene_.setActiveScene(activeSceneName());
    playScene_.animatorGraphs().setControllers(animatorControllers_);
    playScene_.navigation().setMeshes(navMeshes_);
    if(!playScene_.start(document_,mapScene_)) {
      state_.status=!playScene_.scriptDiagnostics().empty()?playScene_.scriptDiagnostics():playScene_.physicsError().empty()?"Falha ao preparar a cena para Play":playScene_.physicsError();
      reportProblem(EditorConsoleSeverity::Error,state_.status);
      return false;
    }
    reportedPhysicsConnectionDiagnostic_.clear();
    reportedEventConnectionDiagnostic_.clear();
    reportProblem(EditorConsoleSeverity::Info,"Play iniciado · código publicado "+std::to_string(runtimeCodeGeneration_));
    playLastSeconds_=sceneTime_;
  }
  const double elapsed=std::max(0.0,static_cast<double>(sceneTime_)-playLastSeconds_);
  playLastSeconds_=sceneTime_;
  playScene_.pause(state_.playPaused);
  if(state_.playPaused) playTouches_.cancel();
  // O toque vira ESTADO DE DISPOSITIVO; quem decide o que ele significa é o
  // mapa de ações do projeto. Pausado, o gameplay perde o foco: as ações leem
  // zero e nenhum botão fica preso ao retomar.
  const auto actions=playTouches_.consumeInput();
  {const auto &stick=playTouches_.joystickState();state_.playStickActive=stick.active&&!state_.playPaused;
   state_.playStickOrigin[0]=stick.originX;state_.playStickOrigin[1]=stick.originY;state_.playStickKnob[0]=stick.knobX;state_.playStickKnob[1]=stick.knobY;state_.playStickRadius=stick.radiusPixels;}
  if(!gameplayInputFocused()||!platformFocus)cancelPlayButtons();
  runtime::InputDeviceState device;
  device.moveX=actions.moveRight;device.moveY=actions.moveForward;
  device.lookX=actions.lookScreenX;device.lookY=actions.lookScreenY;
  device.touchButtons=(jumpPressed_?1u:0u)|(secondaryPressed_?2u:0u);
  for(const auto &[pointer,mask]:playButtonPointers_)device.touchButtons|=mask;
  device.canceledDeviceGroups=platformInput.canceledDeviceGroups|playButtonCanceled_;
  device.canceledSources=platformInput.canceledSources;
  playButtonCanceled_=0;
  device.keys=platformInput.keys;
  device.gamepadButtons=platformInput.gamepadButtons;
  device.gamepadAxes=platformInput.gamepadAxes;
  device.mouseButtons=platformInput.mouseButtons;device.mouseAxes=platformInput.mouseAxes;
  jumpPressed_=false;
  secondaryPressed_=false;
  playScene_.setInputFocus(gameplayInputFocused()&&platformFocus);
  playScene_.submitInput(device,elapsed);
  const auto &input=playScene_.input();
  const auto &map=input.map();
  float look[2]{0,0};
  input.axis2(map.lookAction(),look);
  auto view=resolveSceneCamera(playScene_.document());
  const auto *viewEntity=playScene_.document().find(view.entity);
  if(viewEntity&&cameraLook(*viewEntity)) {
    if(!applyCameraLook(*playScene_.executionGraph(),view.entity,look[0],look[1])) return false;
    view=resolveSceneCamera(playScene_.document());
    viewEntity=playScene_.document().find(view.entity);
  }
  const auto motorTarget=playScene_.motorReceiver(view.entity,state_.selection);
  if(!playScene_.routeDefaultMotorControl(motorTarget,view.entity?view.yaw:0))return false;
  if(state_.playStepRequested) {
    state_.playStepRequested=false;
    if(!playScene_.step()) return false;
  }
  playScene_.setGameView(gameView());
  if(!playScene_.advance(elapsed)) {state_.status=playScene_.frameError();reportProblem(EditorConsoleSeverity::Error,state_.status);return false;}
  // SceneManager.LoadScene (única): o mundo inteiro é trocado aqui, fora de
  // qualquer callback de script. O documento autoral continua intacto e volta
  // ao parar o Play, como as cenas carregadas em Play Mode na Unity.
  runtime::SceneGraph next;std::string nextName;
  if(playScene_.takeSceneRequest(next,nextName)) {
    playScene_.stop();
    playScene_.configureGui(gui_.document());
    playScene_.setActiveScene(nextName);
    playScene_.animatorGraphs().setControllers(animatorControllers_);
    playScene_.navigation().setMeshes(navMeshes_);
    if(!playScene_.start(next,mapScene_)) {
      state_.status="Troca de cena falhou: "+nextName+(playScene_.scriptDiagnostics().empty()?"":" · "+playScene_.scriptDiagnostics());
      reportProblem(EditorConsoleSeverity::Error,state_.status);
      return false;
    }
    state_.selection=kInvalidEntity;state_.componentSelection=kInvalidEntity;state_.expandedNative=0;
    reportedPhysicsConnectionDiagnostic_.clear();reportedEventConnectionDiagnostic_.clear();
  }
  const auto &eventDiagnostic=playScene_.eventConnections().diagnostic();
  if(eventDiagnostic!=reportedEventConnectionDiagnostic_) {
    reportedEventConnectionDiagnostic_=eventDiagnostic;
    if(!eventDiagnostic.empty())reportProblem(EditorConsoleSeverity::Warning,eventDiagnostic);
  }
  const auto &connectionDiagnostic=playScene_.physicsConnections().diagnostic();
  if(connectionDiagnostic!=reportedPhysicsConnectionDiagnostic_) {
    reportedPhysicsConnectionDiagnostic_=connectionDiagnostic;
    if(!connectionDiagnostic.empty())reportProblem(EditorConsoleSeverity::Warning,connectionDiagnostic);
  }
  state_.playTimeScale=playScene_.world().clock().scale();
  if(!playScene_.scriptDiagnostics().empty()) state_.status=playScene_.scriptDiagnostics();
  if(!playScene_.extract(mapScene_,out)) {reportProblem(EditorConsoleSeverity::Error,"Play: recurso da malha não pôde ser extraído");return false;}
  applyLodGroups(playScene_.document(),out);
  return true;
}

void EditorSession::setRenderingSettings(const renderer::ProjectRenderingSettings &settings) {
  renderingSettings_ = settings;
  state_.qualityDraft = settings;
  state_.qualityApplied = settings;
  state_.qualityDirty = false;
  renderingSettingsRequested_ = false;
  renderingSettingsRequestInFlight_ = false;
  qualityRevision_ = 0;
  renderingSettingsRequestRevision_ = 0;
}

void EditorSession::completeRenderingSettingsRequest(bool success) {
  if (!renderingSettingsRequestInFlight_) return;
  renderingSettingsRequestInFlight_ = false;
  if (success) {
    renderingSettings_ = requestedRenderingSettings_;
    state_.qualityApplied = renderingSettings_;
    state_.qualityDirty = qualityRevision_ != renderingSettingsRequestRevision_;
    state_.status = state_.qualityDirty ? "Qualidade aplicada; há novas alterações" : "Qualidade aplicada";
  } else {
    state_.qualityDirty = true;
    state_.status = "Não foi possível salvar a qualidade; revise o armazenamento e tente novamente";
  }
}

void EditorSession::applyLodGroups(const runtime::SceneGraph &graph,std::vector<renderer::MapDrawState> &draws) const {
  lodStates_=runtime::lodObjectStates(graph,lodView(),&lodClock_,lastWallSeconds_);
  if(lodStates_.empty()) return;
  for(auto &draw:draws) {
    const auto found=std::lower_bound(lodStates_.begin(),lodStates_.end(),static_cast<runtime::ObjectId>(draw.objectId),
        [](const runtime::LodObjectState &state,runtime::ObjectId id){return state.object<id;});
    if(found==lodStates_.end()||found->object!=draw.objectId) continue;
    if(found->hidden) {draw.visible=false;continue;}
    // Cobertura do cross-fade no MESMO campo que o LOD do pacote usa
    // (GpuMeshInstance::normalColumns[7]); o shader faz o dither complementar.
    draw.pose.instance.normalColumns[7]=found->dither;
    // Durante a troca, só o nível que sai projeta sombra: as duas sombras
    // somadas escureceriam o chão no meio do fade.
    if(found->dither<0) draw.castShadows=0;
  }
}

} // namespace ae::editor
