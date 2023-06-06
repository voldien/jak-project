#version 430 core

layout(location = 0) out vec4 color;

layout(location = 0) in vec4 fragment_color;
layout(location = 1) in vec3 tex_coord;
layout(location = 2) in float fogginess;

layout(location = 0) uniform sampler2D tex_T0;

uniform float alpha_min;
uniform float alpha_max;
uniform vec4 fog_color;

uniform int gfx_hack_no_tex;

void main() {
  if (gfx_hack_no_tex == 0) {
    // vec4 T0 = texture(tex_T0, tex_coord);
    vec4 T0 = texture(tex_T0, tex_coord.xy);
    color = fragment_color * T0;
  } else {
    color = fragment_color / 2;
  }

  if (color.a < alpha_min || color.a > alpha_max) {
    discard;
  }

  color.rgb = mix(color.rgb, fog_color.rgb, clamp(fogginess * fog_color.a, 0, 1));
}
