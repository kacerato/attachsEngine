#version 450
layout(location=0) out vec2 uv;
void main() {
  // Two coarse triangles: there is no vertex inside the narrow painted stripe.
  const vec2 vertices[6]=vec2[6](vec2(0,0),vec2(1,0),vec2(0,1),vec2(0,1),vec2(1,0),vec2(1,1));
  uv=vertices[gl_VertexIndex];
  gl_Position=vec4(uv*2.0-1.0,0,1);
}
