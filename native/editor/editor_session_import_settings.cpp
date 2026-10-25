#include "editor/editor_session.h"

namespace ae::editor {

resources::ImportProfile EditorSession::importProfileDraft() const {
  resources::ImportProfile draft;
  draft.scale=state_.importScale;draft.maximumTextureDimension=state_.importTextureDimension;
  draft.normals=state_.importNormals;draft.normalWeighting=state_.importNormalWeighting;
  draft.smoothingAngle=state_.importSmoothingAngle;draft.tangents=state_.importTangents;
  draft.importCameras=state_.importCameras;draft.importLights=state_.importLights;
  draft.textureCompression=state_.importTextureCompression;
  draft.textureStreaming=state_.importTextureStreaming;
  draft.generateLods=state_.importGenerateLods;draft.maximumLodLevels=state_.importLodLevels;
  draft.optimizePolygonOrder=state_.importOptimizeOrder;
  draft.fbxDeformation=state_.importFbxDeformation;draft.fbxReduceWeights=state_.importFbxReduceWeights;
  draft.fbxApproximateSteps=state_.importFbxApproximateSteps;draft.fbxBakeRate=state_.importFbxBakeRate;
  draft.textureStreamingPriority=state_.importTextureStreamingPriority;
  draft.generateLightmapUvs=state_.importLightmapUvs;draft.lightmapHardAngle=state_.importLightmapHardAngle;
  draft.lightmapPackMargin=state_.importLightmapPackMargin;
  draft.excludedNodes=state_.importExcludedNodes;
  return draft;
}

resources::ImportProfile EditorSession::importProfilePrepared() const {
  resources::ImportProfile prepared;
  prepared.scale=state_.importPreparedScale;prepared.maximumTextureDimension=state_.importPreparedTextureDimension;
  prepared.normals=state_.importPreparedNormals;prepared.normalWeighting=state_.importPreparedNormalWeighting;
  prepared.smoothingAngle=state_.importPreparedSmoothingAngle;prepared.tangents=state_.importPreparedTangents;
  prepared.importCameras=state_.importPreparedCameras;prepared.importLights=state_.importPreparedLights;
  prepared.textureCompression=state_.importPreparedTextureCompression;
  prepared.generateLods=state_.importPreparedGenerateLods;prepared.maximumLodLevels=state_.importPreparedLodLevels;
  prepared.optimizePolygonOrder=state_.importPreparedOptimizeOrder;
  prepared.fbxDeformation=state_.importPreparedFbxDeformation;prepared.fbxReduceWeights=state_.importPreparedFbxReduceWeights;
  prepared.fbxApproximateSteps=state_.importPreparedFbxApproximateSteps;prepared.fbxBakeRate=state_.importPreparedFbxBakeRate;
  prepared.generateLightmapUvs=state_.importPreparedLightmapUvs;prepared.lightmapHardAngle=state_.importPreparedLightmapHardAngle;
  prepared.lightmapPackMargin=state_.importPreparedLightmapPackMargin;
  return prepared;
}

} // namespace ae::editor
