#version 430 core

out vec4 color;

layout(location = 0) in flat vec4 fragment_color;
layout(location = 1) in vec3 tex_coord;
layout(location = 2) in flat uvec2 tex_info;

layout(binding = 0) uniform sampler2D tex_T0;
uniform float alpha_min;
uniform float alpha_max;

void main() {
  vec4 T0 = texture(tex_T0, tex_coord.xy);
  if (tex_info.y == 0) {
    T0.w = 1.0;
  }
  color = fragment_color * T0;

  if (color.a < alpha_min || color.a > alpha_max) {
    discard;
  }
}
