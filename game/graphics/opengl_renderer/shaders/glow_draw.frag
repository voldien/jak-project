#version 430 core

layout (location = 0) out vec4 color;

layout (location = 0) in vec4 fragment_color;
layout (location = 1) in vec2 uv_texture;
layout (location = 2) in float discard_flag;

layout (binding = 0) uniform sampler2D tex;
uniform float glow_boost;


void main() {
  vec4 texture_color = texture(tex, uv_texture);
  color.xyz = texture_color.xyz * fragment_color.xyz * 2.f * discard_flag / 128.f * glow_boost;
  color.w = fragment_color.w * texture_color.w;
}
