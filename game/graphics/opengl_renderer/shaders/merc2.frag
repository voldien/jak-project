#version 430 core

layout(location = 0) out vec4 color;

layout(location = 0) in vec4 vtx_color;
layout(location = 1) in vec2 vtx_st;
layout(location = 2) in float fog;

layout(binding = 0) uniform sampler2D tex_T0;

uniform vec4 fog_color;
uniform int ignore_alpha;

uniform int decal_enable;

uniform int gfx_hack_no_tex;

void main() {
  if (gfx_hack_no_tex == 0) {
    vec4 T0 = texture(tex_T0, vtx_st);
    // all merc is tcc=rgba and modulate
    if (decal_enable == 0) {
      color = vtx_color * T0 * 2;
    } else {
      color = T0;
    }
    color.a *= 2;
    // color.a = T0.a * 4;
  } else {
    color.rgb = vtx_color.rgb;
    color.a = 1;
  }

  if (ignore_alpha == 0 && color.w < 0.128) {
    discard;
  }

  color.xyz = mix(color.xyz, fog_color.rgb, clamp(fog_color.a * fog, 0, 1));
}
