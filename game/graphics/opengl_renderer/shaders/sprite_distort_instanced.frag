#version 430 core

layout(location = 0) out vec4 out_color;

layout(location = 0) in flat vec4 fragment_color;
layout(location = 1) in vec2 tex_coord;

#include "common.glsl"

layout(binding = 0) uniform sampler2D framebuffer_tex;

void main() {
  vec4 color = fragment_color;

  // correct color
  color *= 2;

  // correct texture coordinates
  vec2 texture_coords =
      vec2(tex_coord.x, (1.0f - tex_coord.y) - (1 - (SCISSOR_HEIGHT / 512.0)) / 2);

  // sample framebuffer texture
  out_color = color * texture(framebuffer_tex, texture_coords);
}
