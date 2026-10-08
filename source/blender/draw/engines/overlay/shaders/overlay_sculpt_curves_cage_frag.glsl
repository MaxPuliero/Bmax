/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_sculpt_curves_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_sculpt_curves_cage)

void main()
{
  frag_color = final_color;
  frag_color.a *= clamp(line_width * 0.5f + 0.5f - abs(line_distance), 0.0f, 1.0f) * (254.0f / 255.0f);
  /* Coverage is already antialiased; keep the line resolve from widening it. */
  line_output = float4(0.0f);
}
