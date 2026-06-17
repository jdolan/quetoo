#version 450

layout(location = 0) out vec3 frag_color;

vec2 positions[3] = vec2[](
  vec2( 0.0, -0.6),
  vec2( 0.6,  0.6),
  vec2(-0.6,  0.6)
);

vec3 colors[3] = vec3[](
  vec3(1.0, 0.2, 0.2),
  vec3(0.2, 1.0, 0.2),
  vec3(0.2, 0.2, 1.0)
);

void main() {
  gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
  frag_color = colors[gl_VertexIndex];
}
