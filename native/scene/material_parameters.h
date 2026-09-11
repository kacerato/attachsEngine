#pragma once
namespace ae::scene {
// Authored scalar overrides. Resource textures and pipeline selection remain
// with the referenced material; this value has no renderer dependency.
struct MaterialParameters {
  bool enabled=false;
  float baseColor[3]{1,1,1};
  float roughness=.5f,metallic=0,normalScale=1,specular=1;
  float emission[3]{};
  float emissionStrength=1;
  // Comparação por valor, não por bytes: a struct tem padding depois do bool, e
  // um memcmp acusaria diferença onde não existe nenhuma.
  friend bool operator==(const MaterialParameters &a,const MaterialParameters &b) {
    if(a.enabled!=b.enabled || a.roughness!=b.roughness || a.metallic!=b.metallic ||
       a.normalScale!=b.normalScale || a.specular!=b.specular || a.emissionStrength!=b.emissionStrength) return false;
    for(int i=0;i<3;++i) if(a.baseColor[i]!=b.baseColor[i] || a.emission[i]!=b.emission[i]) return false;
    return true;
  }
  friend bool operator!=(const MaterialParameters &a,const MaterialParameters &b) {return !(a==b);}
};
}
