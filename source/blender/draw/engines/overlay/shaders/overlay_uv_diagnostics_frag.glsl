/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"
FRAGMENT_SHADER_CREATE_INFO(overlay_uv_diagnostics)
#include "draw_view_lib.glsl"

float2 uv_to_pixel(float2 uv)
{
  float4 p = drw_point_world_to_homogenous(float3(uv, 0.0f));
  return (p.xy / p.w * 0.5f + 0.5f) * uniform_buf.size_viewport;
}

bool near_shell_boundary()
{
  if (shell_root < 0) {
    return false;
  }
  float2 pixel = gl_FragCoord.xy;
  int end = boundary_nodes[shell_root].data.x;
  int index = shell_root;
  while (index < end) {
    UVDiagnosticNode node = boundary_nodes[index];
    float2 a = uv_to_pixel(node.bounds.xy);
    float2 b = uv_to_pixel(node.bounds.zw);
    float2 lo = min(a, b) - float2(2.0f);
    float2 hi = max(a, b) + float2(2.0f);
    if (any(lessThan(pixel, lo)) || any(greaterThan(pixel, hi))) {
      index = node.data.x;
      continue;
    }
    for (int i = 0; i < node.data.z; i++) {
      float4 edge = boundary_edges[node.data.y + i];
      float2 start = uv_to_pixel(edge.xy);
      float2 delta = uv_to_pixel(edge.zw) - start;
      float t = clamp(dot(pixel - start, delta) / max(dot(delta, delta), 1e-20f), 0.0f, 1.0f);
      float2 distance = pixel - (start + t * delta);
      if (dot(distance, distance) < 4.0f) {
        return true;
      }
    }
    index++;
  }
  return false;
}

void main()
{
  /* Raster coverage clips the outline to its own shell, including at corners and holes.
   * Counting fragments highlights intersections, rather than entire overlapping faces.
   * Shared triangulation edges use the GPU top-left rule and are counted only once. */
  frag_color = float4(1.0f, float(flipped), float(show_outline && near_shell_boundary()), 0.0f);
}
