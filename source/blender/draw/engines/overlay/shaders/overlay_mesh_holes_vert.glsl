/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_outline_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_mesh_holes)

#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_index_load_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

void main()
{
  uint edge = uint(gl_VertexID) / 6u;
  uint vertex = uint(gl_VertexID) % 6u;
  float3 p0 = drw_point_object_to_world(
      gpu_attr_load_float3(pos, gpu_attr_0, gpu_index_load(edge * 2u)));
  float3 p1 = drw_point_object_to_world(
      gpu_attr_load_float3(pos, gpu_attr_0, gpu_index_load(edge * 2u + 1u)));
  float4 h0 = drw_point_world_to_homogenous(p0);
  float4 h1 = drw_point_world_to_homogenous(p1);

  /* Clip before perspective division, including lines crossing the near plane. */
  float d0 = h0.z + h0.w;
  float d1 = h1.z + h1.w;
  gl_Position = float4(NAN_FLT);
  if (d0 <= 0.0f && d1 <= 0.0f) {
    return;
  }
  if (d0 < 0.0f) {
    float t = d0 / (d0 - d1);
    h0 = mix(h0, h1, t);
    p0 = mix(p0, p1, t);
  }
  else if (d1 < 0.0f) {
    float t = d1 / (d1 - d0);
    h1 = mix(h1, h0, t);
    p1 = mix(p1, p0, t);
  }
  float2 direction = (h1.xy / h1.w - h0.xy / h0.w) * uniform_buf.size_viewport;
  if (dot(direction, direction) < 1e-12f) {
    return;
  }
  float2 normal = normalize(float2(-direction.y, direction.x));
  /* Two triangles: (start+, start-, end+), (end+, start-, end-). */
  bool at_end = vertex == 2u || vertex == 3u || vertex == 5u;
  float side = (vertex == 0u || vertex == 2u || vertex == 3u) ? 1.0f : -1.0f;
  float radius = line_width * 0.5f + ((do_smooth_lines || line_width < 1.0f) ? 0.5f : 0.0f);
  gl_Position = at_end ? h1 : h0;
  gl_Position.xy += normal * (side * radius * 2.0f) * uniform_buf.size_viewport_inv *
                    gl_Position.w;
  gl_Position.z -= 1e-6f * gl_Position.w;
  hole.distance = side * radius;
  eObjectInfoFlag flag = drw_object_infos().flag;
  hole.color = is_transform ? theme.colors.transform :
                              (flag_test(flag, OBJECT_ACTIVE) ? theme.colors.active_object :
                                                                theme.colors.object_select);
  view_clipping_distances(at_end ? p1 : p0);
}
