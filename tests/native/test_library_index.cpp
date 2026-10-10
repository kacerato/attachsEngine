#include "harness.h"
#include "resources/library_index.h"
using namespace ae;
using namespace ae::resources;
namespace {
LibraryPackage samplePackage() {
  LibraryPackage package;package.id=assetGuidFromSeed("package:example");package.name="Pacote exemplo";
  package.version="1.0";package.origin="Sources/example.unitypackage";package.sourceHash=std::string(64,'a');
  package.licenseNotice="Uso local autorizado; licença original preservada.";
  LibraryItem texture;texture.id=assetGuidFromSeed("package:example:guid-texture");texture.kind=LibraryItemKind::Texture;
  texture.path="Assets/Color.png";texture.sourceIdentity="unity:11111111111111111111111111111111:2800000";
  texture.sourceHash=std::string(64,'b');texture.conversion=LibraryConversionState::Converted;
  texture.nativeGuid=assetGuidFromSeed("native:texture");texture.converter="texture-native";texture.converterVersion=1;texture.recipe="{\"interpretation\":0}\n";
  LibraryItem material;material.id=assetGuidFromSeed("package:example:guid-material");material.kind=LibraryItemKind::Material;
  material.path="Assets/Paint.mat";material.sourceIdentity="unity:22222222222222222222222222222222:2100000";
  material.sourceHash=std::string(64,'c');material.conversion=LibraryConversionState::Converted;
  material.nativeGuid=assetGuidFromSeed("native:material");material.converter="unity-standard-metallic";material.converterVersion=1;
  material.recipe="source=c native=d";material.dependencies={texture.id};material.knownLosses={"A iluminação da cena Unity não foi convertida."};
  package.items={texture,material};return package;
}
AssetRecord record(const LibraryItem &item,const char *path,AssetType type) {
  AssetRecord value;value.guid=item.nativeGuid;value.path=path;value.type=type;value.contentHash=item.sourceHash;return value;
}
}
AE_TEST(library_index_round_trip_tracks_native_guids_after_rename_and_dependency_removal) {
  auto source=samplePackage();const auto texture=source.items[0],material=source.items[1];LibraryIndex index;
  AE_EXPECT_TRUE(index.add(source),"typed package with provenance recipe and known loss");
  LibraryIndex reopened;std::string diagnostic;
  AE_EXPECT_TRUE(LibraryIndex::deserialize(index.serialize(),reopened,diagnostic),diagnostic.c_str());
  AE_EXPECT_TRUE(reopened.item(material.id)&&reopened.item(material.id)->knownLosses==material.knownLosses,"losses survive reload without source payload");
  AE_EXPECT_TRUE(reopened.item(texture.id)&&reopened.item(texture.id)->recipe==texture.recipe,"opaque native importer recipe preserves its newline");
  AssetRegistry registry;
  AE_EXPECT_TRUE(registry.add(record(texture,"Textures/Color.png",AssetType::Texture)),"native texture identity");
  auto nativeMaterial=record(material,"Materials/Paint.material",AssetType::Material);nativeMaterial.dependencies={texture.nativeGuid};
  AE_EXPECT_TRUE(registry.add(nativeMaterial),"native material depends on real registry identity");
  AE_EXPECT_EQ(reopened.resolve(material.id,registry).state,LibraryResolutionState::Registered,"conversion resolves outputs without claiming runtime image");
  AE_EXPECT_TRUE(registry.setPath(material.nativeGuid,"Materials/Renamed.material"),"native rename preserves GUID");
  const auto resolved=reopened.resolve(material.id,registry);
  AE_EXPECT_TRUE(resolved.output&&resolved.output->path=="Materials/Renamed.material","library follows current registry path");
  AE_EXPECT_TRUE(!registry.remove(texture.nativeGuid),"registry retains ownership of runtime dependency");
  AE_EXPECT_TRUE(registry.remove(material.nativeGuid),"remove output through existing registry");
  AE_EXPECT_EQ(reopened.resolve(material.id,registry).state,LibraryResolutionState::MissingOutput,"removed output cannot remain advertised registered");
  nativeMaterial.dependencies.clear();AE_EXPECT_TRUE(registry.remove(texture.nativeGuid)&&registry.add(nativeMaterial),"changed native graph fixture");
  const auto missing=reopened.resolve(material.id,registry);
  AE_EXPECT_TRUE(missing.state==LibraryResolutionState::MissingDependency&&missing.missing==texture.id,"provenance dependency remains visible after removal");
  source.items[1].conversion=LibraryConversionState::Unsupported;source.items[1].diagnostic="Shader personalizado sem conversor.";
  AE_EXPECT_TRUE(reopened.replace(source)&&reopened.resolve(material.id,registry).state==LibraryResolutionState::Unsupported,"explicit converter result overrides a stale output");
}
AE_TEST(library_index_rejects_traversal_duplicates_partial_recipes_and_malformed_replacement_atomically) {
  LibraryIndex index;const auto source=samplePackage();AE_EXPECT_TRUE(index.add(source),"baseline package");const auto original=index.serialize();
  auto candidate=source;candidate.items[0].path="../external.png";
  AE_EXPECT_TRUE(!index.replace(candidate)&&index.serialize()==original,"source paths cannot escape package");
  candidate=source;candidate.items[1].recipe.clear();AE_EXPECT_TRUE(!index.replace(candidate),"converted entry requires reproducible recipe");
  candidate=source;candidate.id=assetGuidFromSeed("package:second");AE_EXPECT_TRUE(!index.add(candidate),"package-qualified item identity must remain unique");
  std::string diagnostic;
  AE_EXPECT_TRUE(!LibraryIndex::deserialize(original+"PAYLOAD",index,diagnostic)&&index.serialize()==original,"malformed load preserves previous complete index");
  AE_EXPECT_TRUE(!LibraryIndex::deserialize("ASTRA_LIBRARY_INDEX_1\nPACKAGES 4294967295\n",index,diagnostic)&&index.serialize()==original,"bounded counts before allocation");
  candidate=source;candidate.items[0].dependencies={candidate.items[1].id};
  AE_EXPECT_TRUE(index.replace(candidate),"cyclic source graph preserved as provenance");AssetRegistry registry;
  AE_EXPECT_TRUE(registry.add(record(candidate.items[0],"Color.png",AssetType::Texture))&&registry.add(record(candidate.items[1],"Paint.material",AssetType::Material)),"registered outputs");
  AE_EXPECT_EQ(index.resolve(candidate.items[1].id,registry).state,LibraryResolutionState::Registered,"cycle traversal visits each identity once");
}
