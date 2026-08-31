#version 450
// Fullscreen triangle, no attributes/uniforms -- output pixel position comes
// entirely from gl_FragCoord in the paired fragment shader. Self-contained on
// purpose: this pipeline is a small standalone Hi-Z reduction pass, not part
// of the main dirt-road pipeline layout/descriptor sets.
void main() {
  vec2 positions[3] = vec2[3](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));
  gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
}
