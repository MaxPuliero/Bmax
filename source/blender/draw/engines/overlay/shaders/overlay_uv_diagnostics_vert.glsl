/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"
VERTEX_SHADER_CREATE_INFO(overlay_uv_diagnostics)
#include "draw_view_lib.glsl"

void main()
{
  UVDiagnosticVertex vertex = vertices[gl_VertexID];
  gl_Position = drw_point_world_to_homogenous(float3(vertex.uv, 0.0f));
  shell_root = vertex.shell_root;
  flipped = vertex.flipped;
}
