#pragma once
#include <string>
namespace ae::test {
inline constexpr auto FbxTriangle=R"FBX(; FBX 7.4.0 project file
FBXHeaderExtension: {
 FBXHeaderVersion: 1003
 FBXVersion: 7400
}
GlobalSettings: {
 Version: 1000
 Properties70: {
  P: "UpAxis", "int", "Integer", "",1
  P: "UpAxisSign", "int", "Integer", "",1
  P: "FrontAxis", "int", "Integer", "",2
  P: "FrontAxisSign", "int", "Integer", "",1
  P: "CoordAxis", "int", "Integer", "",0
  P: "CoordAxisSign", "int", "Integer", "",1
  P: "UnitScaleFactor", "double", "Number", "",100
 }
}
Objects: {
 Geometry: 100, "Geometry::Triangle", "Mesh" {
  Vertices: *9 { a: 0,0,0,1,0,0,0,1,0 }
  PolygonVertexIndex: *3 { a: 0,1,-3 }
  LayerElementNormal: 0 {
   Version: 101
   Name: ""
   MappingInformationType: "ByPolygonVertex"
   ReferenceInformationType: "Direct"
   Normals: *9 { a: 0,0,1,0,0,1,0,0,1 }
  }
  LayerElementUV: 0 {
   Version: 101
   Name: "UVMap"
   MappingInformationType: "ByPolygonVertex"
   ReferenceInformationType: "Direct"
   UV: *6 { a: 0,0,1,0,0,1 }
  }
  Layer: 0 {
   Version: 100
   LayerElement: { Type: "LayerElementNormal" TypedIndex: 0 }
   LayerElement: { Type: "LayerElementUV" TypedIndex: 0 }
  }
 }
 Model: 200, "Model::Triangle", "Mesh" {
  Version: 232
  Properties70: {
   P: "Lcl Translation", "Lcl Translation", "", "A",2,3,4
   P: "Lcl Rotation", "Lcl Rotation", "", "A",0,0,0
   P: "Lcl Scaling", "Lcl Scaling", "", "A",1,1,1
  }
  Shading: T
  Culling: "CullingOff"
 }
}
Connections: {
 C: "OO",100,200
 C: "OO",200,0
}
)FBX";
// Owned ASCII FBX with one real linear skin, one independent shape and two
// animated source properties. The meshless variant keeps the same rig/take.
inline std::string fbxDeformingCharacter(bool meshless=false) {
  const std::string triangle=FbxTriangle;
  auto result=triangle.substr(0,triangle.find("Objects: {"))+"Objects: {\n";
  if(!meshless)result+=triangle.substr(triangle.find(" Geometry:"),triangle.find(" Model:")-triangle.find(" Geometry:"));
  result+=R"FBX(
 Model: 200, "Model::Rig", "Null" { Version: 232 }
 Model: 300, "Model::Joint", "LimbNode" { Version: 232 }
 AnimationStack: 600, "AnimStack::Move", "" {
  Properties70: {
   P: "LocalStart", "KTime", "Time", "",0
   P: "LocalStop", "KTime", "Time", "",46186158000
   P: "ReferenceStart", "KTime", "Time", "",0
   P: "ReferenceStop", "KTime", "Time", "",46186158000
  }
 }
 AnimationLayer: 601, "AnimLayer::Base", "" { }
 AnimationCurveNode: 500, "AnimCurveNode::T", "" {
  Properties70: { P: "d|X", "Number", "", "A",0 }
 }
 AnimationCurve: 501, "AnimCurve::X", "" {
  Default: 0 KeyVer: 4008
  KeyTime: *2 { a: 0,46186158000 }
  KeyValueFloat: *2 { a: 0,2 }
  KeyAttrFlags: *1 { a: 4 }
  KeyAttrDataFloat: *4 { a: 0,0,0,0 }
  KeyAttrRefCount: *1 { a: 2 }
 }
)FBX";
  if(!meshless)result+=R"FBX(
 Model: 201, "Model::Body", "Mesh" { Version: 232 }
 Deformer: 400, "Deformer::Skin", "Skin" { Version: 101 SkinningType: "Linear" }
 Deformer: 401, "SubDeformer::Joint", "Cluster" {
  Version: 100
  Indexes: *3 { a: 0,1,2 }
  Weights: *3 { a: 1,1,1 }
  Transform: *16 { a: 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 }
  TransformLink: *16 { a: 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 }
 }
 Geometry: 700, "Geometry::Raise", "Shape" {
  Version: 100
  Indexes: *3 { a: 0,1,2 }
  Vertices: *9 { a: 0,0,1,0,0,1,0,0,1 }
 }
 Deformer: 701, "Deformer::Shapes", "BlendShape" { Version: 100 }
 Deformer: 702, "SubDeformer::Raise", "BlendShapeChannel" {
  Version: 100 DeformPercent: 0 FullWeights: *1 { a: 100 }
 }
 AnimationCurveNode: 704, "AnimCurveNode::Raise", "" {
  Properties70: { P: "d|DeformPercent", "Number", "", "A",0 }
 }
 AnimationCurve: 703, "AnimCurve::Raise", "" {
  Default: 0 KeyVer: 4008
  KeyTime: *2 { a: 0,46186158000 }
  KeyValueFloat: *2 { a: 0,100 }
  KeyAttrFlags: *1 { a: 4 }
  KeyAttrDataFloat: *4 { a: 0,0,0,0 }
  KeyAttrRefCount: *1 { a: 2 }
 }
)FBX";
  result+=R"FBX(}
Connections: {
 C: "OO",200,0
 C: "OO",300,200
 C: "OO",601,600
 C: "OO",500,601
 C: "OP",500,300,"Lcl Translation"
 C: "OP",501,500,"d|X"
)FBX";
  if(!meshless)result+=R"FBX(
 C: "OO",100,201
 C: "OO",201,200
 C: "OO",400,100
 C: "OO",401,400
 C: "OO",300,401
 C: "OO",701,100
 C: "OO",702,701
 C: "OO",700,702
 C: "OO",704,601
 C: "OP",704,702,"DeformPercent"
 C: "OP",703,704,"d|DeformPercent"
)FBX";
  return result+"}\n";
}
}
