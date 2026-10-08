/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"
#  include "gpu_index_load_infos.hh"
#endif

#ifdef GLSL_CPP_STUBS
#  define CURVES_SHADER
#  define DRW_HAIR_INFO
#endif

#include "overlay_common_infos.hh"

GPU_SHADER_INTERFACE_INFO(overlay_sculpt_curves_selection_iface)
SMOOTH(float, mask_weight)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_sculpt_curves_selection)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(bool, is_point_domain)
PUSH_CONSTANT(float, selection_opacity)
SAMPLER(2, samplerBuffer, selection_tx)
VERTEX_OUT(overlay_sculpt_curves_selection_iface)
VERTEX_SOURCE("overlay_sculpt_curves_selection_vert.glsl")
FRAGMENT_SOURCE("overlay_sculpt_curves_selection_frag.glsl")
FRAGMENT_OUT(0, float4, out_color)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_curves)
ADDITIONAL_INFO(draw_curves_infos)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_sculpt_curves_selection_clipped,
                    overlay_sculpt_curves_selection,
                    drw_clipped)

GPU_SHADER_INTERFACE_INFO(overlay_sculpt_curves_cage_iface)
NO_PERSPECTIVE(float, line_distance)
NO_PERSPECTIVE(float4, final_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_sculpt_curves_cage)
DO_STATIC_COMPILATION()
STORAGE_BUF_FREQ(0, read, float, pos[], GEOMETRY)
STORAGE_BUF_FREQ(1, read, float, selection[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(int2, gpu_attr_1)
ADDITIONAL_INFO(gpu_index_buffer_load)
VERTEX_OUT(overlay_sculpt_curves_cage_iface)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
PUSH_CONSTANT(float, opacity)
PUSH_CONSTANT(float, depth_bias)
PUSH_CONSTANT(float, line_width)
PUSH_CONSTANT(float4, unselected_color)
PUSH_CONSTANT(float4, selected_color)
VERTEX_SOURCE("overlay_sculpt_curves_cage_vert.glsl")
FRAGMENT_SOURCE("overlay_sculpt_curves_cage_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_sculpt_curves_cage_clipped, overlay_sculpt_curves_cage, drw_clipped)
