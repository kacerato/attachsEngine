#version 450
// Identical fullscreen triangle to hzb_reduce_first.vert. Kept as a separate
// file because tools/generate-embedded-shaders.ps1 compiles one shader name's
// .vert/.frag pair at a time; duplicating six lines is cheaper than inventing
// a cross-shader include for this pipeline.
void main() {
  vec2 positions[3] = vec2[3](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));
  gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
}
