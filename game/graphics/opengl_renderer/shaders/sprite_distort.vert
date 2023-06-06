#version 430 core
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec3 xyz;
layout (location = 1) in vec2 st;

#include"common.glsl"

uniform vec4 u_color;

layout (location = 0) out flat vec4 fragment_color;
layout (location = 1) out vec2 tex_coord;

void main() {
  fragment_color = u_color;
  tex_coord = st;
  vec4 transformed = vec4(xyz, 1.0);

  // correct xy offset
  transformed.xy -= (2048.);
  // correct z scale
  transformed.z /= (8388608);
  transformed.z -= 1;
  // correct xy scale
  transformed.x /= (256);
  transformed.y /= -(128);
  transformed.y *= HEIGHT_SCALE;

  gl_Position = transformed;
}
