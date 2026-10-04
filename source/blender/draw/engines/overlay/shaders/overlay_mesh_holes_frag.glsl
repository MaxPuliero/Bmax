/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_outline_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_mesh_holes)

void main()
{
  if (use_occlusion) {
    float depth = texelFetch(scene_depth_tx, int2(gl_FragCoord.xy), 0).r;
    if (gl_FragCoord.z > depth + 3.0f / 8388608.0f) {
      gpu_discard_fragment();
      return;
    }
  }
  float coverage = (do_smooth_lines || line_width < 1.0f) ?
                       clamp(line_width * 0.5f + 0.5f - abs(hole.distance),
                             0.0f,
                             min(line_width, 1.0f)) :
                       1.0f;
  frag_color = hole.color;
  frag_color.a *= coverage * (254.0f / 255.0f);
  frag_color.rgb *= frag_color.a;
  /* Coverage already antialiases the half-width line; do not expand it in the line resolve. */
  line_output = float4(0.0f);
}
