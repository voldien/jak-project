// Debug shader for drawing things in red. Uses the same conventions as direct_basic, see there for more details

#version 430 core
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec3 position_in;

layout (location = 0) out vec4 fragment_color;

#include"common.glsl"


void main() {
  gl_Position = vec4((position_in.x - 0.5) * 16., -(position_in.y - 0.5) * 32, position_in.z * 2 - 1., 1.0);
  // scissoring area adjust
  gl_Position.y *= SCISSOR_ADJUST;
  fragment_color = vec4(1.0, 0, 0, 0.7);
}
