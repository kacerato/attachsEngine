#include "editor/editor_session.h"
#include "editor/editor_collider_fit.h"
#include "scene/dynamic_body_motor.h"
#include "scene/component_schema.h"
#include "scene/prefab_link.h"
#include "editor/editor_import_transaction.h"
#include "runtime/physics_requirements.h"
#include "runtime/transform_math.h"

namespace ae::editor {
bool EditorSession::prepareDynamicMotor(EditorEntityId id,MotorCollisionPolicy policy,EditorEntity &candidate,std::string &summary,std::string &error) const {
  const auto fail=[&](std::string reason){error=std::move(reason);return false;};
  error.clear();summary.clear();
  const auto *source=document_.find(id);
  if(isPlaying()||history_.isOpen())return fail("Configure em Edit Mode, após finalizar a edição atual.");
  if(!source||id==document_.root())return fail("Selecione o objeto que deve possuir o corpo e a locomoção.");
  if(static_cast<u32>(policy)>3)return fail("Política de colisão inválida.");
  for(auto object=source;object;object=document_.find(object->parent)) {
    if(scene::prefabLink(object->components))return fail("Desvincule o prefab antes de alterar sua estrutura física.");
    if(runtime::characterComponent(*object)||object->components.find("astra.physics2d.body"))return fail("Separe as autoridades Character/2D antes de configurar o motor 3D.");
  }
  candidate=*source;
  const bool newBody=!runtime::physicsBody(*source);
  const auto plan=scene::planComponentAddition(candidate.components,scene::DynamicBodyMotor::descriptor.id);
  if(!candidate.components.find(scene::DynamicBodyMotor::descriptor)) {
    if(!plan.ready)return fail(plan.error?plan.error:"Motor indisponível.");
    candidate.components=plan.candidate;
  }
  auto *body=runtime::editPhysicsBody(candidate);
  if(!body)return fail("Motor exige Body no mesmo objeto.");
  body->motion=scene::BodyMotion::Dynamic;body->sensor=false;
  if(newBody)for(auto &axis:body->freezeRotation)axis=true;
  const auto &motor=static_cast<const scene::DynamicBodyMotor&>(*candidate.components.find(scene::DynamicBodyMotor::descriptor));
  if(body->freezePosition[0]||body->freezePosition[2]||(motor.jumpSpeed>0&&body->freezePosition[1]))return fail("Libere os eixos de posição usados pelo motor; os bloqueios existentes serão preservados.");
  std::vector<EditorEntityId> ids;document_.collectSubtree(id,ids);u32 parts=0,owned=0;
  for(auto member:ids) {
    const auto &object=*document_.find(member);
    for(usize i=0;i<object.components.size();++i)if(&object.components.at(i)->type()==&scene::Collider::descriptor) {
      const auto &c=static_cast<const scene::Collider&>(*object.components.at(i));
      if((c.owner?c.owner:member)==id){++owned;if(c.enabled)++parts;}
    }
  }
  if(policy==MotorCollisionPolicy::Preserve) {
    if(!parts)return fail("Sem colisores ativos vinculados. Escolha Ajustar ou Convexo, ou vincule os colisores dos filhos a este Body.");
  } else {
    if(owned>1||(owned&&(!runtime::colliderComponent(*source)||(runtime::colliderComponent(*source)->owner&&runtime::colliderComponent(*source)->owner!=id))))
      return fail("O corpo possui uma colisão composta. Preserve suas partes ou ajuste cada colisor no Inspector; não serão descartadas.");
    auto *collider=runtime::editCollider(candidate);
    if(!collider)collider=static_cast<scene::Collider*>(candidate.components.add(scene::Collider::descriptor));
    if(!collider)return fail("Não foi possível adicionar a colisão.");
    if(collider->owner&&collider->owner!=id)return fail("Este colisor pertence a outro Body. Preserve ou corrija seu proprietário primeiro.");
    collider->enabled=true;
    if(policy==MotorCollisionPolicy::FitPrimitive) {
      if(!fitEditorCollider(mapScene_,document_,id,*collider))return fail("Geometria de todos os slots é necessária para ajustar a colisão.");
      collider->collisionMesh={};
    } else {
      if(policy==MotorCollisionPolicy::ConvexMesh&&!runtime::meshRenderer(*source))return fail("Convexo requer uma malha visual carregada neste objeto.");
      collider->shape=scene::ColliderShape::Mesh;collider->convex=true;collider->collisionMesh={};
    }
  }
  summary="Malha, materiais, identidade e filhos preservados. Body dinâmico sólido; ";
  summary+=policy==MotorCollisionPolicy::Preserve?std::to_string(parts)+" parte(s) de colisão preservadas.":policy==MotorCollisionPolicy::FitPrimitive?"primitiva medida em todos os slots da malha.":"casco convexo calculado dos slots visuais; reentrâncias são preenchidas.";
  summary+=newBody?" Novo Body com rotação bloqueada; ajustável no Inspector.":" Massa, amortecimento, rotação e velocidade existentes preservados.";
  error.clear();return true;
}
bool EditorSession::previewDynamicMotor(EditorEntityId id,MotorCollisionPolicy policy,EditorEntity &candidate,std::string &summary,std::string &error) const {
  if(!prepareDynamicMotor(id,policy,candidate,summary,error))return false;
  EditorDocument graph=document_;
  if(!graph.applyEntityValues(id,candidate)){error="A configuração viola o contrato do objeto.";return false;}
  // Validate dormant authoring too, without changing its saved active state.
  for(auto object=graph.find(id);object;object=graph.find(object->parent))if(!object->active){auto enabled=*object;enabled.active=true;graph.applyEntityValues(enabled.id,enabled);}
  return EditorPlayScene::validatePhysics(graph,mapScene_,error);
}
bool EditorSession::configureDynamicMotor(EditorEntityId id,MotorCollisionPolicy policy,std::string &error) {
  if(policy==MotorCollisionPolicy::Decompose){if(id!=motorBakeObject_){error="Prévia pertence a outro objeto";return false;}return applyMotorDecomposition(error);}
  EditorEntity candidate;std::string summary;
  if(!previewDynamicMotor(id,policy,candidate,summary,error))return false;
  if(!history_.begin("Configurar locomoção")){error="Não foi possível abrir a edição";return false;}
  if(!history_.applyValues(document_,id,candidate)){history_.cancel(document_);error="Configuração cancelada sem alterar a cena";return false;}
  history_.end();state_.motorSetupTarget=0;state_.inspectorMenu=false;state_.entityMenu=false;
  setSelection(id);state_.componentSelection=id;
  state_.expandedNative=document_.find(id)->components.find(scene::DynamicBodyMotor::descriptor)->instanceId();
  state_.expandedComponent.clear();state_.componentGroup.clear();state_.propertyPage=0;
  state_.inspectorSurface=EditorInspectorSurface::Inspection;state_.compactPanel=EditorScreenState::CompactPanel::Inspector;
  state_.status="Locomoção configurada no objeto; visual preservado. Undo restaura a configuração anterior.";
  error.clear();return true;
}

namespace {
std::string motorSourceError(const EditorDocument &document,EditorEntityId root,const EditorSession::MotorBakeSource &source) {
  const auto *object=document.find(source.object);const auto *render=object?runtime::meshRenderer(*object):nullptr;
  if(!render||source.slot>=render->slotCount())return "Objeto ou slot de malha ausente";
  for(auto member=object;member;member=document.find(member->parent)) {
    if(member->components.find(scene::SkinnedMesh::descriptor))return "Skin deformada exige bake de pose; não será usada a pose base silenciosamente";
    if(member->id==root)return {};
    if(runtime::physicsBody(*member)||runtime::characterComponent(*member)||member->components.find("astra.physics2d.body"))
      return "Autoridade física própria em "+std::string(member->name)+"; não será absorvida pelo Body principal";
  }
  return "A fonte deve pertencer à hierarquia do Body selecionado";
}
}
std::vector<EditorSession::MotorBakeSourceOption> EditorSession::motorDecompositionSources(EditorEntityId id) const {
  std::vector<MotorBakeSourceOption> options;std::vector<EditorEntityId> objects;
  if(!document_.find(id))return options;
  document_.collectSubtree(id,objects);
  for(auto member:objects) {
    const auto *object=document_.find(member);const auto *render=runtime::meshRenderer(*object);if(!render)continue;
    std::string path=object->name;
    for(auto parent=document_.find(object->parent);parent&&member!=id&&parent->id!=id;parent=document_.find(parent->parent))path=std::string(parent->name)+" / "+path;
    for(u32 slot=0;slot<render->slotCount();++slot) {
      if(options.size()==256){options.push_back({{},"Mais fontes na hierarquia","Lista limitada a 256 slots; selecione uma sub-hierarquia ou use a API tipada"});return options;}
      MotorBakeSource source{member,slot};auto error=motorSourceError(document_,id,source);
      if(error.empty()) {
        const auto asset=render->slotAsset(slot);const auto mesh=asset.valid()?mapScene_.assetSlot(asset):render->slotMesh(slot);
        std::span<const EditorPickMesh::Triangle> triangles;float matrix[16];
        if(!mesh||!mapScene_.localGeometry(mesh,triangles,matrix)||triangles.empty())error="Geometria ausente; resolva a importação antes de gerar";
      }
      options.push_back({source,path+" · slot "+std::to_string(slot+1),std::move(error)});
    }
  }
  return options;
}
void EditorSession::refreshMotorBakeSources(EditorEntityId id) {
  const auto options=motorDecompositionSources(id);
  const auto *object=document_.find(id);const auto *recipe=object?scene::collisionRecipe(object->components):nullptr;
  state_.motorBakeRecipeAvailable=recipe&&!recipe->parts.empty();
  if(motorBakeSourcesObject_!=id||motorBakeSourcesVersion_.epoch!=sceneVersion().epoch) {
    motorBakeSourcesObject_=id;motorBakeSources_.clear();state_.motorBakeSourcePage=0;motorBakeSourcesExplicit_=false;
    if(recipe&&!recipe->parts.empty()) {
      for(const auto &source:recipe->sources)motorBakeSources_.push_back({static_cast<EditorEntityId>(source.object),source.slot});
      motorBakeSourcesExplicit_=true;
    }
  }
  if(!motorBakeSourcesExplicit_) {
    motorBakeSources_.clear();
    // Default preserves the old all-slots contract, including unresolved slots:
    // generation must diagnose missing geometry instead of silently dropping it.
    for(const auto &option:options)if(option.source.object==id)motorBakeSources_.push_back(option.source);
    if(motorBakeSources_.size()>128)motorBakeSources_.clear();
  }
  state_.motorBakeSourceRows.clear();
  for(const auto &option:options)state_.motorBakeSourceRows.push_back({option.label,option.error,std::find(motorBakeSources_.begin(),motorBakeSources_.end(),option.source)!=motorBakeSources_.end()});
  state_.motorBakeSelectedSources=static_cast<u32>(motorBakeSources_.size());motorBakeSourcesVersion_=sceneVersion();
  if(!options.empty())state_.motorBakeSourcePage=std::min<u32>(state_.motorBakeSourcePage,static_cast<u32>((options.size()-1)/state_.motorBakePartsPerPage()));
}
bool EditorSession::selectMotorDecompositionSources(EditorEntityId id,std::span<const MotorBakeSource> sources,std::string &error) {
  if(isPlaying()||history_.isOpen()||!document_.find(id)||id==document_.root()){error="Selecione o Body em Edit Mode, fora de uma edição aberta";return false;}
  if(sources.size()>128){error="Limite de 128 fontes selecionadas; escolha um conjunto menor";return false;}
  std::vector<MotorBakeSource> next;
  for(const auto &source:sources) {
    if(auto reason=motorSourceError(document_,id,source);!reason.empty()){error=std::move(reason);return false;}
    if(std::find(next.begin(),next.end(),source)!=next.end()){error="Fonte/slot duplicado na seleção";return false;}
    next.push_back(source);
  }
  if(motorBakeSourcesObject_!=id||next!=motorBakeSources_)cancelMotorDecomposition();
  motorBakeSourcesObject_=id;motorBakeSources_=std::move(next);motorBakeSourcesExplicit_=true;motorBakeSourcesVersion_=sceneVersion();refreshMotorBakeSources(id);error.clear();return true;
}
bool EditorSession::collectMotorGeometry(EditorEntityId id,std::vector<float> &points,std::vector<std::string> &sources,std::vector<resources::ConvexBakeOrigin> &origins,std::string &error) const {
  if(motorBakeSourcesObject_!=id||motorBakeSources_.empty()) {
    const auto *object=document_.find(id);const auto *render=object?runtime::meshRenderer(*object):nullptr;
    error=render&&render->slotCount()>128?"Este objeto excede 128 slots; escolha explicitamente um conjunto menor":"Escolha pelo menos uma fonte de malha para este Body";return false;
  }
  std::vector<std::pair<EditorEntityId,u32>> used;
  for(const auto &source:motorBakeSources_) {
    if(auto reason=motorSourceError(document_,id,source);!reason.empty()){error=std::move(reason);return false;}
    const auto *object=document_.find(source.object);const auto *render=runtime::meshRenderer(*object);const auto slot=source.slot;
    const auto asset=render->slotAsset(slot);const auto mesh=asset.valid()?mapScene_.assetSlot(asset):render->slotMesh(slot);
    std::span<const EditorPickMesh::Triangle> triangles;float m[16];
    if(!mesh||!mapScene_.localGeometry(mesh,triangles,m)||triangles.empty()){error=std::string(object->name)+": geometria ausente no slot "+std::to_string(slot+1);return false;}
    // Compose authored local matrices up to the body; no world inverse/TRS
    // decomposition can discard shear or collapse distinct mesh instances.
    float relative[16]{};relative[0]=relative[5]=relative[10]=relative[15]=1;
    for(auto member=object;member&&member->id!=id;member=document_.find(member->parent)) {
      float local[16],composed[16];runtime::transformMatrix(member->transform,local);runtime::multiplyMatrix(local,relative,composed);std::copy_n(composed,16,relative);
    }
    float composed[16];runtime::multiplyMatrix(relative,m,composed);std::copy_n(composed,16,m);
    const float determinant=m[0]*(m[5]*m[10]-m[9]*m[6])-m[4]*(m[1]*m[10]-m[9]*m[2])+m[8]*(m[1]*m[6]-m[5]*m[2]);
    if(!std::isfinite(determinant)||std::abs(determinant)<1e-12f){error=std::string(object->name)+": transformação singular ou não finita";return false;}
    const auto guid=asset.valid()?asset:mapScene_.assetGuid(mesh-1);
    if(guid.valid()&&std::find(sources.begin(),sources.end(),guid.text())==sources.end())sources.push_back(guid.text());
    resources::ConvexBakeOrigin origin;origin.object=source.object;origin.slot=slot;origin.meshGuid=guid.valid()?guid.text():"";std::copy_n(m,16,origin.relative.data());
    origin.firstTriangle=static_cast<u32>(points.size()/9);
    const auto key=std::pair{source.object,mesh};
    if(std::find(used.begin(),used.end(),key)!=used.end()){origins.push_back(std::move(origin));continue;}used.push_back(key);
    origin.triangleCount=static_cast<u32>(triangles.size());origins.push_back(std::move(origin));
    if(points.size()+triangles.size()*9>900000){error="Decomposição limitada a 100 mil triângulos; escolha uma malha de colisão simplificada.";return false;}
    for(const auto &t:triangles)for(u32 v=0;v<3;++v)for(u32 k=0;k<3;++k) {
      const auto vertex=determinant<0&&v?3-v:v;
      const float p=m[12+k]+m[k]*t[vertex*3]+m[4+k]*t[vertex*3+1]+m[8+k]*t[vertex*3+2];
      if(!std::isfinite(p)){error="Geometria não finita";return false;}points.push_back(p);
    }
  }
  if(points.empty()){error="Geometria vazia";return false;}return true;
}
resources::ConvexBakeProgress EditorSession::motorDecompositionProgress() const {
  return motorBake_?motorBake_->progress():resources::ConvexBakeProgress{};
}
void EditorSession::cancelMotorDecomposition() {
  if(motorBake_)motorBake_->cancel();
  motorBakeObject_=0;motorBakePreviewed_=false;
  state_.motorBakeReady=false;state_.motorBakePreview.clear();state_.motorBakeEnabled.clear();
  motorBakeRegenerating_=false;motorBakePartMapping_.clear();state_.motorBakeRegenerating=false;
  state_.motorBakePartMapping.clear();state_.motorBakePreviousParts.clear();state_.motorBakePartNotes.clear();state_.motorBakeMappingSummary.clear();state_.motorBakeMappingConfirmed=true;
}
bool EditorSession::beginMotorDecomposition(EditorEntityId id,resources::ConvexBakeSettings settings,std::string &error) {
  if(motorBake_&&motorBake_->progress().status==resources::ConvexBakeStatus::Running){error="Aguarde o worker concluir ou cancelar a geração anterior";return false;}
  if(files_.rootPath().empty()||!publishGeometry_){error="Abra um projeto com publicação de malhas antes de gerar";return false;}
  if(!resources::validConvexBakeSettings(settings)){error="Orçamento de decomposição inválido";return false;}
  EditorEntity candidate;std::string summary;
  const auto *object=document_.find(id);const auto *recipe=object?scene::collisionRecipe(object->components):nullptr;
  const bool regenerating=recipe&&!recipe->parts.empty();
  if(regenerating) {
    if(isPlaying()||history_.isOpen()||id==document_.root()||!recipe->valid()||!runtime::physicsBody(*object)){error="Receita exige Body válido em Edit Mode, fora de um gesto";return false;}
    candidate=*object;
    for(const auto &p:recipe->parts)candidate.components.removeInstance(p.collider);
  } else if(!prepareDynamicMotor(id,MotorCollisionPolicy::Decompose,candidate,summary,error))return false;
  const auto extra=regenerating?0u:1u;
  if(candidate.components.size()+settings.maximumParts-(regenerating?0u:1u)+extra>scene::Components::MaximumCount){error="O orçamento de partes/receita excede os slots livres";return false;}
  refreshMotorBakeSources(id);
  std::vector<float> points;std::vector<std::string> sources;std::vector<resources::ConvexBakeOrigin> origins;
  if(!collectMotorGeometry(id,points,sources,origins,error))return false;
  const auto hash=Sha256::hex({reinterpret_cast<const u8*>(points.data()),points.size()*sizeof(float)});
  setSelection(id);
  motorBake_=std::make_unique<resources::ConvexBakeJob>();
  if(!motorBake_->start(std::move(points),settings,hash,std::move(sources),error,std::move(origins)))return false;
  motorBakeObject_=id;motorBakeVersion_=sceneVersion();motorBakeRoot_=files_.rootPath();motorBakeSourceHash_=hash;motorBakePreviewed_=false;
  motorBakeSettings_=settings;motorBakeRegenerating_=regenerating;state_.motorBakeRegenerating=regenerating;state_.motorBakePartMapping.clear();state_.motorBakeMappingConfirmed=true;
  state_.motorSetupTarget=id;state_.motorSetupPolicy=3;state_.motorBakeReady=false;state_.motorBakeRunning=true;state_.motorBakePage=0;
  state_.motorBakeSourcesOpen=false;
  state_.motorBakePreview.clear();state_.motorBakeEnabled.clear();state_.motorSetupError.clear();
  state_.motorSetupSummary="Gerando em segundo plano; a cena ainda não foi alterada.";return true;
}
void EditorSession::refreshMotorDecomposition() {
  const auto current=sceneVersion();
  if(state_.motorSetupTarget==state_.selection&&state_.motorSetupPolicy==3&&
      (motorBakeSourcesObject_!=state_.selection||motorBakeSourcesVersion_.epoch!=current.epoch||motorBakeSourcesVersion_.revision!=current.revision))refreshMotorBakeSources(state_.selection);
  if(!motorBake_)return;
  const auto progress=motorBake_->progress();state_.motorBakeRunning=progress.status==resources::ConvexBakeStatus::Running;
  state_.motorBakeProgress=progress.fraction;state_.motorBakeStage=progress.stage;
  const auto version=sceneVersion();
  if(motorBakeObject_&&(isPlaying()||version.epoch!=motorBakeVersion_.epoch||version.revision!=motorBakeVersion_.revision||
      files_.rootPath()!=motorBakeRoot_||state_.selection!=motorBakeObject_||state_.motorSetupTarget!=motorBakeObject_)) {
    cancelMotorDecomposition();state_.motorSetupError="Cena, seleção ou projeto mudou; gere uma nova prévia.";return;
  }
  if(!motorBakeObject_)return;
  if(progress.status==resources::ConvexBakeStatus::Cancelled){state_.motorSetupError="Geração cancelada; cena e recursos preservados.";motorBakeObject_=0;return;}
  if(progress.status==resources::ConvexBakeStatus::Failed){state_.motorSetupError=progress.error;motorBakeObject_=0;return;}
  const auto *result=motorBake_->result();if(!result)return;
  if(motorBakePreviewed_){if(!state_.motorBakeEnabled.empty())state_.motorBakePage=std::min<u32>(state_.motorBakePage,static_cast<u32>((state_.motorBakeEnabled.size()-1)/state_.motorBakePartsPerPage()));return;}
  motorBakePreviewed_=true;state_.motorBakeReady=true;state_.motorBakeEnabled.assign(result->parts.size(),true);
  for(const auto &part:result->parts){EditorScreenState::MotorBakePreviewPart preview;
    for(usize i=0;i<part.indices.size();i+=3){std::array<float,9> triangle;for(u32 v=0;v<3;++v)std::copy_n(part.vertices[part.indices[i+v]].data(),3,triangle.data()+v*3);preview.triangles.push_back(triangle);}
    state_.motorBakePreview.push_back(std::move(preview));}
  initializeMotorBakeMapping();refreshMotorBakeCandidate();
  state_.motorSetupSummary=std::to_string(result->parts.size())+" cascos. "+(motorBakeRegenerating_?"Revise os vínculos com os Colisores anteriores. Base e edições locais permanecem distintas.":"Confira no viewport; partes desativadas ficam cinza. Receita e fontes serão salvas junto do objeto.");
}
bool EditorSession::applyMotorDecomposition(std::string &error) {
  const auto fail=[&](std::string message){error=std::move(message);return false;};
  const auto *result=motorBake_?motorBake_->result():nullptr;const auto version=sceneVersion();
  if(!result||!motorBakeObject_||!state_.motorBakeReady)return fail("Gere e confira a prévia antes de aplicar");
  if(isPlaying()||history_.isOpen()||version.epoch!=motorBakeVersion_.epoch||version.revision!=motorBakeVersion_.revision||files_.rootPath()!=motorBakeRoot_||state_.selection!=motorBakeObject_)return fail("Prévia desatualizada; gere novamente");
  if(motorBakeRegenerating_&&!state_.motorBakeMappingConfirmed)return fail("Revise e confirme as correspondências antes de aplicar");
  if(state_.motorBakeEnabled.size()!=result->parts.size()||std::none_of(state_.motorBakeEnabled.begin(),state_.motorBakeEnabled.end(),[](bool v){return v;}))return fail("Ative pelo menos uma parte para o motor possuir suporte físico");
  std::vector<float> points;std::vector<std::string> sources;std::vector<resources::ConvexBakeOrigin> origins;
  if(!collectMotorGeometry(motorBakeObject_,points,sources,origins,error))return false;
  if(Sha256::hex({reinterpret_cast<const u8*>(points.data()),points.size()*sizeof(float)})!=motorBakeSourceHash_)return fail("Geometria fonte mudou; gere novamente");
  if(origins!=result->origins)return fail("Identidade, seleção ou pose das fontes mudou; gere novamente");
  EditorEntity candidate;
  resources::GltfImport model;
  if(!resources::importGlb(result->glb,importLimits_,{},model))return fail(model.diagnostic);
  const auto contentHash=Sha256::hex(result->glb);const std::string path="Collision/bake-"+contentHash+".glb";
  std::string expectedHash;
  if(const auto *existing=assets_.findByPath(path)){if(existing->contentHash!=contentHash)return fail("Recurso de bake foi alterado; não será sobrescrito");expectedHash=contentHash;}
  auto candidateSources=importedSources_;auto candidateAssets=assets_;ModelImportReport report;StagedSource staged;
  if(!stageSource(model,contentHash,path,resources::ImportAmbiguityPolicy::Refuse,candidateSources,candidateAssets,report,staged,{}))return fail(report.diagnostic);
  const auto source=std::find_if(candidateSources.begin(),candidateSources.end(),[&](const auto &s){return s.guid==report.source;});
  if(source==candidateSources.end()||source->identities.size()!=result->parts.size())return fail("Identidades dos cascos não correspondem à prévia");
  if(!buildMotorBakeCandidate(source->identities,report.source,candidate,error))return false;
  const auto *firstCollider=runtime::colliderComponent(candidate);
  if(!firstCollider)return fail("A revisão não possui Colisor ativo");
  const auto firstInstance=firstCollider->instanceId();
  class Geometry final : public runtime::CollisionGeometrySource {
  public:
    Geometry(const EditorMapScene &m,const resources::ConvexBakeResult &r,const std::vector<resources::AssetGuid> &ids):map(m),result(r),guids(ids){}
    bool meshTriangles(u32 slot,std::vector<float> &out) const override {std::span<const EditorPickMesh::Triangle> tris;float m[16];if(!map.localGeometry(slot,tris,m))return false;
      for(const auto &t:tris)for(u32 v=0;v<3;++v)for(u32 k=0;k<3;++k)out.push_back(m[12+k]+m[k]*t[v*3]+m[4+k]*t[v*3+1]+m[8+k]*t[v*3+2]);
      return true;}
    bool meshTriangles(const resources::AssetGuid &id,std::vector<float> &out) const override {
      const auto found=std::find(guids.begin(),guids.end(),id);if(found==guids.end())return meshTriangles(map.assetSlot(id),out);
      const auto &part=result.parts[static_cast<usize>(found-guids.begin())];for(auto index:part.indices)out.insert(out.end(),part.vertices[index].begin(),part.vertices[index].end());return true;
    }
  private:const EditorMapScene &map;const resources::ConvexBakeResult &result;const std::vector<resources::AssetGuid> &guids;
  } geometry(mapScene_,*result,source->identities);
  auto graph=document_;if(!graph.applyEntityValues(motorBakeObject_,candidate))return fail("Configuração candidata inválida");
  // Disabled local overrides must not conceal an invalid dynamic mesh during
  // publication. Validate every surviving recipe part on an isolated copy.
  auto checked=*graph.find(motorBakeObject_);
  for(const auto &p:scene::collisionRecipe(checked.components)->parts) {
    auto *component=checked.components.editInstance(p.collider);
    if(component&&&component->type()==&scene::Collider::descriptor)static_cast<scene::Collider*>(component)->enabled=true;
  }
  if(!graph.applyEntityValues(motorBakeObject_,checked))return fail("Geometria candidata inválida");
  for(auto object=graph.find(motorBakeObject_);object;object=graph.find(object->parent))if(!object->active){auto enabled=*object;enabled.active=true;graph.applyEntityValues(enabled.id,enabled);}
  runtime::GameWorld world;runtime::ScenePhysics physics;
  if(!world.load(graph)||!physics.start(world,&geometry))return fail(physics.error().empty()?"Cena candidata inválida":physics.error());
  std::vector<resources::AssetGuid> dependencies;
  for(const auto &guid:sources)for(const auto &s:importedSources_)if(std::any_of(s.identities.begin(),s.identities.end(),[&](const auto &id){return id.text()==guid;}))
    if(std::find(dependencies.begin(),dependencies.end(),s.guid)==dependencies.end())dependencies.push_back(s.guid);
  if(!commitModelImport(result->glb,model,path,expectedHash,report,resources::ImportAmbiguityPolicy::Refuse,{},{},{},dependencies))return fail(report.diagnostic);
  const auto id=motorBakeObject_;
  // The new immutable resource remains registered on Undo so Redo and other
  // users remain resolvable. Only the authored object change enters history.
  if(!history_.begin(motorBakeRegenerating_?"Regenerar colisão preservando edições":"Aplicar colisão decomposta e motor"))return fail("Histórico ocupado; recurso disponível, objeto não alterado");
  if(!history_.applyValues(document_,id,candidate)){history_.cancel(document_);return fail("Objeto recusou a configuração; recurso permanece disponível");}
  history_.end();cancelMotorDecomposition();state_.motorSetupTarget=0;state_.inspectorMenu=false;state_.entityMenu=false;
  setSelection(id);state_.componentSelection=id;state_.expandedNative=firstInstance;state_.expandedComponent.clear();state_.componentGroup.clear();state_.propertyPage=0;
  state_.inspectorSurface=EditorInspectorSurface::Inspection;state_.compactPanel=EditorScreenState::CompactPanel::Inspector;
  state_.status="Partes de colisão aplicadas; visual preservado. Edite cada Colisor no Inspector. 1 Undo restaura o objeto.";error.clear();return true;
}
}
