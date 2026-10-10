// ABI 12 implementation inside AnimationAuthoringScope::Impl's constructor.
api.propertyCatalog=[](void *c,u64 target,int index,AnimationAuthorProperty *out,u8 *label,int capacity)->int {
  auto &s=*static_cast<Impl*>(c);if(!s.ready()||target>std::numeric_limits<EditorEntityId>::max())return -1;
  const auto entries=s.session.animationPropertyCatalog(static_cast<EditorEntityId>(target),s.error);
  if(!s.error.empty())return -1;
  if(index==-1)return static_cast<int>(entries.size());
  if(index<0||static_cast<usize>(index)>=entries.size()||!out||!authorProperty(entries[index].address,*out)) {s.reject("Índice ou endereço de propriedade inválido");return -1;}
  return copyText(entries[index].label,label,capacity);
};
api.addProperty=[](void *c,u64 token,u64 owner,u64 target,const AnimationAuthorProperty *wire,u64 *out)->int {
  auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);if(!draft)return 0;
  resources::AnimationPropertyAddress address;
  if(!wire||!out||owner>std::numeric_limits<EditorEntityId>::max()||target>std::numeric_limits<EditorEntityId>::max()||!authorProperty(*wire,address))return s.reject("Endereço tipado inválido");
  if(!s.propertyContext(*draft,owner))return 0;
  auto candidate=draft->asset;u64 id=0;
  if(!s.session.prepareAnimationProperty(candidate,static_cast<EditorEntityId>(owner),static_cast<EditorEntityId>(target),address,id,s.error)||!s.budget(candidate,token))return 0;
  draft->asset=std::move(candidate);draft->propertyOwner=owner;draft->propertySceneRevision=s.session.document().revision();draft->changed();*out=id;return 1;
};
api.putProperty=[](void *c,u64 token,u64 owner,u64 track,float time,const AnimationAuthorPropertyValue *wire)->int {
  auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);if(!draft)return 0;
  resources::AnimationPropertyValue value;
  if(!wire||owner>std::numeric_limits<EditorEntityId>::max()||!authorPropertyValue(*wire,value))return s.reject("Valor tipado inválido");
  if(!s.propertyContext(*draft,owner))return 0;
  auto candidate=draft->asset;
  if(!candidate.putPropertyValue(track,time,value,s.error)||!s.budget(candidate,token)||
     !s.session.validateAnimationPropertyClip(candidate,static_cast<EditorEntityId>(owner),time,s.error))return 0;
  draft->asset=std::move(candidate);draft->propertyOwner=owner;draft->propertySceneRevision=s.session.document().revision();draft->changed();return 1;
};
api.sampleProperty=[](void *c,u64 token,u64 track,float time,AnimationAuthorPropertyValue *out)->int {
  auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);if(!draft||!out)return 0;
  const auto *property=draft->asset.propertyTrack(track);if(!property||!std::isfinite(time)||time<0||time>draft->asset.duration)return s.reject("Canal ou tempo tipado inválido");
  resources::CompiledAnimationProperty compiled;resources::AnimationPropertyValue value;
  if(!resources::compileAnimationProperty(property->data,draft->asset.duration,compiled,s.error)||!compiled.sample(time,value))return 0;
  *out=authorPropertyValue(value);return 1;
};
api.remapProperty=[](void *c,u64 token,u64 owner,u64 track,u64 target,const AnimationAuthorProperty *wire)->int {
  auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);if(!draft)return 0;
  resources::AnimationPropertyAddress address;
  if(!wire||owner>std::numeric_limits<EditorEntityId>::max()||target>std::numeric_limits<EditorEntityId>::max()||!authorProperty(*wire,address))return s.reject("Destino tipado inválido");
  if(!s.propertyContext(*draft,owner))return 0;
  auto candidate=draft->asset;
  if(!s.session.remapAnimationProperty(candidate,static_cast<EditorEntityId>(owner),static_cast<EditorEntityId>(target),track,address,s.error)||!s.budget(candidate,token))return 0;
  draft->asset=std::move(candidate);draft->propertyOwner=owner;draft->propertySceneRevision=s.session.document().revision();draft->changed();return 1;
};
api.configureDrivers=[](void *c,u64 token,u64 owner,const u8 *bytes,int length)->int {
  auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);if(!draft)return 0;
  if(owner>std::numeric_limits<EditorEntityId>::max()||!textArgument(bytes,length,4*1024*1024))return s.reject("Contrato de drivers fora do orçamento");
  std::istringstream input(textValue(bytes,length));resources::AnimationClipDrivers drivers;
  if(!resources::readAnimationClipDrivers(input,drivers,s.error))return 0;
  input>>std::ws;if(!input.eof())return s.reject("Dados adicionais no contrato de drivers");
  if(!s.propertyContext(*draft,owner))return 0;
  auto candidate=draft->asset;
  if(!s.session.configureAnimationDrivers(candidate,static_cast<EditorEntityId>(owner),drivers,s.error)||!s.budget(candidate,token))return 0;
  draft->asset=std::move(candidate);draft->propertyOwner=owner;draft->propertySceneRevision=s.session.document().revision();draft->changed();return 1;
};
api.bindPropertyObject=[](void *c,u64 token,u64 owner,u64 target,u64 *out)->int {
  auto &s=*static_cast<Impl*>(c);auto *draft=s.find(token);if(!draft)return 0;
  if(!out||owner>std::numeric_limits<EditorEntityId>::max()||target>std::numeric_limits<EditorEntityId>::max())return s.reject("Objeto de binding inválido");
  if(!s.propertyContext(*draft,owner))return 0;
  auto candidate=draft->asset;u64 id=0;
  if(!s.session.animationPropertyBinding(candidate,static_cast<EditorEntityId>(owner),static_cast<EditorEntityId>(target),id,s.error)||!candidate.valid(&s.error))return 0;
  draft->asset=std::move(candidate);draft->propertyOwner=owner;draft->propertySceneRevision=s.session.document().revision();draft->changed();*out=id;return 1;
};
api.driverParameters=[](void *c,u64 target,int index,AnimationAuthorDriverParameter *out,u8 *label,int capacity)->int {
  auto &s=*static_cast<Impl*>(c);if(!s.ready()||target>std::numeric_limits<EditorEntityId>::max())return -1;
  const auto entries=s.session.animationDriverParameters(static_cast<EditorEntityId>(target),s.error);
  if(!s.error.empty())return -1;
  if(index==-1)return static_cast<int>(entries.size());
  if(index<0||static_cast<usize>(index)>=entries.size()||!out||!authorProperty(entries[index].input.address,out->property)) {s.reject("Parâmetro de driver inválido");return -1;}
  out->parameter=entries[index].input.parameter;out->controller=entries[index].input.controller;
  return copyText(entries[index].label,label,capacity);
};
