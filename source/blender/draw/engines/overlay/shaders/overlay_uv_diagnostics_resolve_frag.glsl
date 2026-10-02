/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"
FRAGMENT_SHADER_CREATE_INFO(overlay_uv_diagnostics_resolve)

void main()
{
  float3 coverage = texelFetch(coverage_tx, int2(gl_FragCoord.xy), 0).rgb;
  frag_color = float4(0.0f);
  if (outline_pass) {
    if (coverage.b > 0.0f) {
      frag_color = float4(1.0f);
    }
  }
  else if (show_overlap && coverage.r > 1.0f && overlap_opacity > 0.0f) {
    frag_color = float4(1.0f, 0.0f, 0.0f, overlap_opacity);
  }
  else if (show_flipped && coverage.g > 0.0f) {
    frag_color = float4(1.0f, 0.0f, 1.0f, 1.0f);
  }
}
