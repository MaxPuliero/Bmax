/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_color_blend.hh"
#include "BLI_math_color_c.hh"
#include "BLI_math_vector.hh"
#include "BLI_set.hh"

#include "BLT_translation.hh"

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_mesh.hh"
#include "BKE_multires.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_report.hh"
#include "BKE_subdiv_ccg.hh"

#include "IMB_colormanagement.hh"

#include "WM_api.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "ED_paint.hh"
#include "ED_screen.hh"

#include "mesh_brush_common.hh"
#include "sculpt_automask.hh"
#include "sculpt_color.hh"
#include "sculpt_filter.hh"
#include "sculpt_intern.hh"
#include "sculpt_smooth.hh"
#include "sculpt_undo.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include <cmath>

namespace blender::ed::sculpt_paint::color {

enum class FilterType {
  Fill = 0,
  Hue,
  Saturation,
  Value,
  Brightness,
  Contrast,
  Red,
  Green,
  Blue,
  Smooth,
};

static const float fill_filter_default_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

static float3 fill_color_resolve_from_paint(const bContext *C, const bool use_secondary_color)
{
  const ToolSettings *ts = CTX_data_tool_settings(C);
  const Sculpt *sd = ts->sculpt;
  const Paint *paint = &sd->paint;
  const Brush *brush = BKE_paint_brush_for_read(paint);

  /* Use brush colors if a brush tool is active, otherwise use unified paint colors. */
  if (WM_toolsystem_active_tool_is_brush(C) && brush) {
    const float3 color = use_secondary_color ? BKE_brush_secondary_color_get(paint, brush) :
                                               BKE_brush_color_get(paint, brush);
    return color;
  }

  /* Use unified paint colors when filter tool is active. */
  const float3 color = use_secondary_color ? paint->unified_paint_settings.secondary_color :
                                             paint->unified_paint_settings.color;
  return color;
}

static float3 fill_color_resolve(const bContext *C, wmOperator *op, const bool use_secondary_color)
{
  if (RNA_struct_property_is_set(op->ptr, "fill_color")) {
    float3 fill_color;
    RNA_float_get_array(op->ptr, "fill_color", fill_color);
    return fill_color;
  }

  return fill_color_resolve_from_paint(C, use_secondary_color);
}

/* Stores the used color as the fill color to ensure the redo panel works as expected. */
static void fill_color_store_current(const bContext *C, wmOperator *op)
{
  const bool use_secondary_color = RNA_boolean_get(op->ptr, "use_secondary_color");
  const float3 fill_color = fill_color_resolve_from_paint(C, use_secondary_color);
  RNA_float_set_array(op->ptr, "fill_color", fill_color);
}

static EnumPropertyItem prop_color_filter_types[] = {
    {int(FilterType::Fill), "FILL", 0, "Fill", "Fill with a specific color"},
    {int(FilterType::Hue), "HUE", 0, "Hue", "Change hue"},
    {int(FilterType::Saturation), "SATURATION", 0, "Saturation", "Change saturation"},
    {int(FilterType::Value), "VALUE", 0, "Value", "Change value"},
    {int(FilterType::Brightness), "BRIGHTNESS", 0, "Brightness", "Change brightness"},
    {int(FilterType::Contrast), "CONTRAST", 0, "Contrast", "Change contrast"},
    {int(FilterType::Smooth), "SMOOTH", 0, "Smooth", "Smooth colors"},
    {int(FilterType::Red), "RED", 0, "Red", "Change red channel"},
    {int(FilterType::Green), "GREEN", 0, "Green", "Change green channel"},
    {int(FilterType::Blue), "BLUE", 0, "Blue", "Change blue channel"},
    {0, nullptr, 0, nullptr, nullptr},
};

struct LocalData {
  Vector<float> factors;
  Vector<float4> colors;
  Vector<int> neighbor_offsets;
  Vector<int> neighbor_data;
  Vector<float4> average_colors;
  Vector<float4> new_colors;
};

BLI_NOINLINE static void clamp_factors(const MutableSpan<float> factors,
                                       const float min,
                                       const float max)
{
  for (float &factor : factors) {
    factor = std::clamp(factor, min, max);
  }
}

/** Identical pointwise color math for mesh vertices and Multires grid samples. */
static void apply_color_filter(const FilterType mode,
                               const Span<float4> orig_colors,
                               MutableSpan<float> factors,
                               const float3 &filter_fill_color,
                               MutableSpan<float4> new_colors)
{
  for (const int i : orig_colors.index_range()) {
    new_colors[i] = orig_colors[i];
  }
  switch (mode) {
    case FilterType::Fill: {
      clamp_factors(factors, 0.0f, 1.0f);
      for (const int i : orig_colors.index_range()) {
        float fill_color_rgba[4];
        copy_v3_v3(fill_color_rgba, filter_fill_color);
        fill_color_rgba[3] = 1.0f;
        mul_v4_fl(fill_color_rgba, factors[i]);
        blend_color_mix_float(new_colors[i], orig_colors[i], fill_color_rgba);
      }
      break;
    }
    case FilterType::Hue: {
      for (const int i : orig_colors.index_range()) {
        float3 hsv_color;
        rgb_to_hsv_v(orig_colors[i], hsv_color);
        const float hue = hsv_color[0];
        hsv_color[0] = fmod((hsv_color[0] + fabs(factors[i])) - hue, 1);
        hsv_to_rgb_v(hsv_color, new_colors[i]);
      }
      break;
    }
    case FilterType::Saturation: {
      for (const int i : orig_colors.index_range()) {
        float3 hsv_color;
        rgb_to_hsv_v(orig_colors[i], hsv_color);

        if (hsv_color[1] > 0.001f) {
          hsv_color[1] = std::clamp(hsv_color[1] + factors[i] * hsv_color[1], 0.0f, 1.0f);
          hsv_to_rgb_v(hsv_color, new_colors[i]);
        }
        else {
          copy_v3_v3(new_colors[i], orig_colors[i]);
        }
      }
      break;
    }
    case FilterType::Value: {
      for (const int i : orig_colors.index_range()) {
        float3 hsv_color;
        rgb_to_hsv_v(orig_colors[i], hsv_color);
        hsv_color[2] = std::clamp(hsv_color[2] + factors[i], 0.0f, 1.0f);
        hsv_to_rgb_v(hsv_color, new_colors[i]);
      }
      break;
    }
    case FilterType::Red: {
      for (const int i : orig_colors.index_range()) {
        copy_v3_v3(new_colors[i], orig_colors[i]);
        new_colors[i][0] = std::clamp(orig_colors[i][0] + factors[i], 0.0f, 1.0f);
      }
      break;
    }
    case FilterType::Green: {
      for (const int i : orig_colors.index_range()) {
        copy_v3_v3(new_colors[i], orig_colors[i]);
        new_colors[i][1] = std::clamp(orig_colors[i][1] + factors[i], 0.0f, 1.0f);
      }
      break;
    }
    case FilterType::Blue: {
      for (const int i : orig_colors.index_range()) {
        copy_v3_v3(new_colors[i], orig_colors[i]);
        new_colors[i][2] = std::clamp(orig_colors[i][2] + factors[i], 0.0f, 1.0f);
      }
      break;
    }
    case FilterType::Brightness: {
      clamp_factors(factors, -1.0f, 1.0f);
      for (const int i : orig_colors.index_range()) {
        const float brightness = factors[i];
        const float contrast = 0;
        float delta = contrast / 2.0f;
        const float gain = 1.0f - delta * 2.0f;
        delta *= -1;
        const float offset = gain * (brightness + delta);
        for (int component = 0; component < 3; component++) {
          new_colors[i][component] = std::clamp(
              gain * orig_colors[i][component] + offset, 0.0f, 1.0f);
        }
      }
      break;
    }
    case FilterType::Contrast: {
      clamp_factors(factors, -1.0f, 1.0f);
      for (const int i : orig_colors.index_range()) {
        const float brightness = 0;
        const float contrast = factors[i];
        float delta = contrast / 2.0f;
        float gain = 1.0f - delta * 2.0f;

        float offset;
        if (contrast > 0) {
          gain = 1.0f / ((gain != 0.0f) ? gain : FLT_EPSILON);
          offset = gain * (brightness - delta);
        }
        else {
          delta *= -1;
          offset = gain * (brightness + delta);
        }
        for (int component = 0; component < 3; component++) {
          new_colors[i][component] = std::clamp(
              gain * orig_colors[i][component] + offset, 0.0f, 1.0f);
        }
      }
      break;
    }
    case FilterType::Smooth:
      BLI_assert_unreachable();
      break;
  }

  /* In particular Hue must never alter a fully masked or hidden sample. */
  for (const int i : orig_colors.index_range()) {
    if (factors[i] == 0.0f) {
      new_colors[i] = orig_colors[i];
    }
  }
}

static void color_filter_task(const Depsgraph &depsgraph,
                              Object &ob,
                              const OffsetIndices<int> faces,
                              const Span<int> corner_verts,
                              const GroupedSpan<int> vert_to_face_map,
                              const MeshAttributeData &attribute_data,
                              const FilterType mode,
                              const float filter_strength,
                              const float3 &filter_fill_color,
                              const bke::pbvh::MeshNode &node,
                              LocalData &tls,
                              bke::GSpanAttributeWriter &color_attribute)
{
  SculptSession &ss = *ob.runtime->sculpt_session;

  const Span<float4> orig_colors = orig_color_data_get_mesh(ob, node);

  const Span<int> verts = node.verts();

  Array<float, bke::pbvh::MESH_LEAF_LIMIT> factors(verts.size());
  fill_factor_from_hide_and_mask(attribute_data.hide_vert, attribute_data.mask, verts, factors);
  auto_mask::calc_vert_factors(
      depsgraph, ob, ss.filter_cache->automasking.get(), node, verts, factors);
  scale_factors(factors, filter_strength);

  Array<float4, bke::pbvh::MESH_LEAF_LIMIT> new_colors(verts.size());

  /* Copy alpha. */
  for (const int i : verts.index_range()) {
    new_colors[i][3] = orig_colors[i][3];
  }

  if (mode == FilterType::Smooth) {
    clamp_factors(factors, -1.0f, 1.0f);

    tls.colors.resize(verts.size());
    const MutableSpan<float4> colors = tls.colors;
    for (const int i : verts.index_range()) {
      colors[i] = color_vert_get(faces,
                                 corner_verts,
                                 vert_to_face_map,
                                 color_attribute.span,
                                 color_attribute.domain,
                                 verts[i]);
    }

    const GroupedSpan<int> neighbors = calc_vert_neighbors(
        faces, corner_verts, vert_to_face_map, {}, verts, tls.neighbor_offsets, tls.neighbor_data);

    tls.average_colors.resize(verts.size());
    const MutableSpan<float4> average_colors = tls.average_colors;
    smooth::neighbor_color_average(faces,
                                   corner_verts,
                                   vert_to_face_map,
                                   color_attribute.span,
                                   color_attribute.domain,
                                   neighbors,
                                   average_colors);

    for (const int i : verts.index_range()) {
      const int vert = verts[i];

      if (factors[i] < 0.0f) {
        interp_v4_v4v4(average_colors[i], average_colors[i], colors[i], 0.5f);
      }

      bool copy_alpha = colors[i][3] == average_colors[i][3];

      if (factors[i] < 0.0f) {
        float4 delta_color;

        /* Unsharp mask. */
        copy_v4_v4(delta_color, ss.filter_cache->pre_smoothed_color[vert]);
        delta_color -= average_colors[i];

        copy_v4_v4(new_colors[i], colors[i]);
        madd_v4_v4fl(new_colors[i], delta_color, factors[i]);
      }
      else {
        blend_color_interpolate_float(new_colors[i], colors[i], average_colors[i], factors[i]);
      }

      new_colors[i] = math::clamp(new_colors[i], 0.0f, 1.0f);

      /* Prevent accumulated numeric error from corrupting alpha. */
      if (copy_alpha) {
        new_colors[i][3] = average_colors[i][3];
      }
    }
  }
  else {
    apply_color_filter(mode, orig_colors, factors, filter_fill_color, new_colors);
  }

  for (const int i : verts.index_range()) {
    color_vert_set(faces,
                   corner_verts,
                   vert_to_face_map,
                   color_attribute.domain,
                   verts[i],
                   new_colors[i],
                   color_attribute.span);
  }
}

static void sculpt_color_presmooth_init(const Mesh &mesh, Object &object)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
  const IndexMask &node_mask = ss.filter_cache->node_mask;
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const bke::GAttributeReader color_attribute = active_color_attribute(mesh);
  const GVArraySpan colors = *color_attribute;

  if (ss.filter_cache->pre_smoothed_color.is_empty()) {
    ss.filter_cache->pre_smoothed_color = Array<float4>(mesh.verts_num);
  }
  const MutableSpan<float4> pre_smoothed_color = ss.filter_cache->pre_smoothed_color;

  node_mask.foreach_index(
      [&](const int i) {
        for (const int vert : nodes[i].verts()) {
          pre_smoothed_color[vert] = color_vert_get(
              faces, corner_verts, vert_to_face_map, colors, color_attribute.domain, vert);
        }
      },
      exec_mode::grain_size(1));

  struct LocalData {
    Vector<int> neighbor_offsets;
    Vector<int> neighbor_data;
    Vector<float4> averaged_colors;
  };
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  for ([[maybe_unused]] const int iteration : IndexRange(2)) {
    node_mask.foreach_index(
        [&](const int i) {
          LocalData &tls = all_tls.local();
          const Span<int> verts = nodes[i].verts();

          const GroupedSpan<int> neighbors = calc_vert_neighbors(faces,
                                                                 corner_verts,
                                                                 vert_to_face_map,
                                                                 {},
                                                                 verts,
                                                                 tls.neighbor_offsets,
                                                                 tls.neighbor_data);

          Array<float4, bke::pbvh::MESH_LEAF_LIMIT> averaged_colors(verts.size());
          smooth::neighbor_data_average_mesh(
              pre_smoothed_color.as_span(), neighbors, averaged_colors.as_mutable_span());

          for (const int i : verts.index_range()) {
            pre_smoothed_color[verts[i]] = math::interpolate(
                pre_smoothed_color[verts[i]], averaged_colors[i], 0.5f);
          }
        },
        exec_mode::grain_size(1));
  }
}

/** Per-operator buffers; no change to the shared filter cache or saved mesh layout. */
struct GridColorFilterData {
  Vector<int> grids;
  Array<int> offsets;
  Array<int> grid_to_node;
  Array<float4> previous;
  Array<float4> pre_smoothed;
  Set<int> dirty_grids;
  IndexMaskMemory undo_memory;
};

static void grid_color_filter_init(Depsgraph &depsgraph, Object &object, wmOperator &op)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const SubdivCCG &ccg = *ss.subdiv_ccg;
  auto *data = MEM_new<GridColorFilterData>(__func__);
  op.customdata = data;
  const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  data->offsets = Array<int>(ccg.grids_num, -1);
  data->grid_to_node = Array<int>(ccg.grids_num, -1);
  for (const int i : nodes.index_range()) {
    for (const int grid : nodes[i].grids()) {
      data->grid_to_node[grid] = i;
    }
  }
  ss.filter_cache->node_mask.foreach_index([&](const int i) {
    for (const int grid : nodes[i].grids()) {
      data->offsets[grid] = data->grids.size() * ccg.grid_area;
      data->grids.append(grid);
    }
  });
  /* Seam stitching can write duplicate samples owned by adjacent, otherwise locked nodes. */
  const IndexMask undo_nodes = multires_color_nodes(
      object, ss.filter_cache->node_mask, data->undo_memory);
  undo::push_nodes(depsgraph, object, undo_nodes, undo::Type::Color);
}

static void sculpt_color_filter_apply_grids(bContext *C, wmOperator *op, Object &object)
{
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  const Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  SculptSession &ss = *object.runtime->sculpt_session;
  SubdivCCG &ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(ccg);
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  GridColorFilterData &data = *static_cast<GridColorFilterData *>(op->customdata);
  const FilterType mode = FilterType(RNA_enum_get(op->ptr, "type"));
  const float strength = RNA_float_get(op->ptr, "strength");
  const float3 fill_color = fill_color_resolve(
      C, op, RNA_boolean_get(op->ptr, "use_secondary_color"));
  const IndexMask &node_mask = ss.filter_cache->node_mask;
  if (auto_mask::is_enabled(sd.paint, object, nullptr) && ss.filter_cache->automasking &&
      ss.filter_cache->automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL)
  {
    ss.filter_cache->automasking->calc_cavity_factor(depsgraph, object, node_mask);
  }
  const auto snapshot = [&](MutableSpan<float4> dst) {
    for (const int grid : data.grids) {
      dst.slice(data.offsets[grid], ccg.grid_area)
          .copy_from(ccg.colors.as_span().slice(grid * ccg.grid_area, ccg.grid_area));
    }
  };
  const auto read = [&](const Span<float4> src, const SubdivCCGCoord coord) -> float4 {
    const int offset = data.offsets[coord.grid_index];
    return offset < 0 ? ccg.colors[coord.to_index(key)] :
                        src[offset + coord.y * ccg.grid_size + coord.x];
  };
  const auto average = [&](const Span<float4> src, const SubdivCCGCoord coord) {
    SubdivCCGNeighbors neighbors;
    BKE_subdiv_ccg_neighbor_coords_get(ccg, coord, false, neighbors);
    float4 sum(0);
    for (const SubdivCCGCoord other : neighbors.unique()) {
      sum += read(src, other);
    }
    return neighbors.unique().is_empty() ? read(src, coord) : sum / neighbors.unique().size();
  };
  if (mode == FilterType::Smooth) {
    if (data.previous.is_empty()) {
      data.previous.reinitialize(data.grids.size() * ccg.grid_area);
    }
    snapshot(data.previous);
    if (strength < 0.0f && data.pre_smoothed.is_empty()) {
      data.pre_smoothed = data.previous;
      for ([[maybe_unused]] const int iteration : IndexRange(2)) {
        for (const int grid : data.grids) {
          for (int y = 0; y < ccg.grid_size; y++) {
            for (int x = 0; x < ccg.grid_size; x++) {
              const SubdivCCGCoord coord{grid, short(x), short(y)};
              data.pre_smoothed[data.offsets[grid] + y * ccg.grid_size + x] = math::interpolate(
                  read(data.previous, coord), average(data.previous, coord), 0.5f);
            }
          }
        }
        data.previous.as_mutable_span().copy_from(data.pre_smoothed);
      }
      snapshot(data.previous);
    }
  }
  Array<bool> changed_grids(ccg.grids_num, false);
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  node_mask.foreach_index(
      [&](const int node_i) {
        const bke::pbvh::GridsNode &node = nodes[node_i];
        const Span<int> grids = node.grids();
        LocalData &tls = all_tls.local();
        tls.factors.resize(grids.size() * ccg.grid_area);
        fill_factor_from_hide_and_mask(ccg, grids, tls.factors);
        auto_mask::calc_grids_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), node, grids, tls.factors);
        scale_factors(tls.factors, strength);
        const Span<float4> original = *orig_color_data_lookup_grids(node);
        tls.new_colors.resize(original.size());
        if (mode != FilterType::Smooth) {
          apply_color_filter(mode, original, tls.factors, fill_color, tls.new_colors);
        }
        else {
          clamp_factors(tls.factors, -1.0f, 1.0f);
          int local = 0;
          for (const int grid : grids) {
            for (int y = 0; y < ccg.grid_size; y++) {
              for (int x = 0; x < ccg.grid_size; x++, local++) {
                const SubdivCCGCoord coord{grid, short(x), short(y)};
                const float4 current = read(data.previous, coord);
                float4 mean = average(data.previous, coord);
                const float factor = tls.factors[local];
                if (factor == 0.0f) {
                  tls.new_colors[local] = current;
                  continue;
                }
                if (factor < 0.0f) {
                  mean = (mean + current) * 0.5f;
                  tls.new_colors[local] = current +
                                          (read(data.pre_smoothed, coord) - mean) * factor;
                }
                else {
                  tls.new_colors[local] = math::interpolate(current, mean, factor);
                }
                tls.new_colors[local] = math::clamp(tls.new_colors[local], 0.0f, 1.0f);
                if (current[3] == mean[3]) {
                  tls.new_colors[local][3] = mean[3];
                }
              }
            }
          }
        }
        int local = 0;
        for (const int grid : grids) {
          for (const int offset : IndexRange(ccg.grid_area)) {
            const int index = grid * ccg.grid_area + offset;
            const float4 value = tls.new_colors[local++];
            if (math::distance_squared(ccg.colors[index], value) > 0.0f) {
              ccg.colors[index] = value;
              changed_grids[grid] = true;
            }
          }
        }
      },
      exec_mode::grain_size(1));
  Vector<int> dirty;
  for (const int grid : data.grids) {
    if (changed_grids[grid]) {
      dirty.append(grid);
    }
  }
  const Vector<int> stitched = BKE_subdiv_ccg_colors_stitch(ccg, dirty);
  Array<bool> changed_nodes(nodes.size(), false);
  for (const int grid : stitched) {
    data.dirty_grids.add(grid);
    changed_nodes[data.grid_to_node[grid]] = true;
  }
  IndexMaskMemory memory;
  pbvh.tag_attribute_changed(IndexMask::from_bools(changed_nodes, memory), ccg.color_name);
  flush_update_step(C, UpdateType::Color);
}

static void sculpt_color_filter_apply(bContext *C, wmOperator *op, Object &ob)
{
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  const Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  SculptSession &ss = *ob.runtime->sculpt_session;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  if (pbvh.type() == bke::pbvh::Type::Grids) {
    sculpt_color_filter_apply_grids(C, op, ob);
    return;
  }
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();

  const FilterType mode = FilterType(RNA_enum_get(op->ptr, "type"));
  float filter_strength = RNA_float_get(op->ptr, "strength");
  const bool use_secondary_color = RNA_boolean_get(op->ptr, "use_secondary_color");
  const float3 fill_color = fill_color_resolve(C, op, use_secondary_color);

  Mesh &mesh = *id_cast<Mesh *>(ob.data);
  if (filter_strength < 0.0 && ss.filter_cache->pre_smoothed_color.is_empty()) {
    sculpt_color_presmooth_init(mesh, ob);
  }

  const IndexMask &node_mask = ss.filter_cache->node_mask;
  if (auto_mask::is_enabled(sd.paint, ob, nullptr) && ss.filter_cache->automasking &&
      ss.filter_cache->automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL)
  {
    ss.filter_cache->automasking->calc_cavity_factor(depsgraph, ob, node_mask);
  }

  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  bke::GSpanAttributeWriter color_attribute = active_color_attribute_for_write(mesh);
  const MeshAttributeData attribute_data(mesh);

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  node_mask.foreach_index(
      [&](const int i) {
        LocalData &tls = all_tls.local();
        color_filter_task(depsgraph,
                          ob,
                          faces,
                          corner_verts,
                          vert_to_face_map,
                          attribute_data,
                          mode,
                          filter_strength,
                          fill_color,
                          nodes[i],
                          tls,
                          color_attribute);
      },
      exec_mode::grain_size(1));
  pbvh.tag_attribute_changed(node_mask, mesh.active_color_attribute);
  color_attribute.finish();
  flush_update_step(C, UpdateType::Color);
}

static void sculpt_color_filter_end(bContext *C, wmOperator *op, Object &ob)
{
  SculptSession &ss = *ob.runtime->sculpt_session;

  if (FilterType(RNA_enum_get(op->ptr, "type")) == FilterType::Fill &&
      !RNA_struct_property_is_set(op->ptr, "fill_color"))
  {
    fill_color_store_current(C, op);
  }

  if (auto *data = static_cast<GridColorFilterData *>(op->customdata)) {
    const Vector<int> dirty(data->dirty_grids.begin(), data->dirty_grids.end());
    if (!dirty.is_empty()) {
      BKE_subdiv_ccg_colors_store(*id_cast<Mesh *>(ob.data), *ss.subdiv_ccg, dirty);
    }
    MEM_delete(data);
    op->customdata = nullptr;
  }
  ss.stroke_delta.reset();
  undo::push_end(ob);
  MEM_delete(ss.filter_cache);
  ss.filter_cache = nullptr;
  flush_update_done(C, ob, UpdateType::Color);
}

static void sculpt_color_filter_cancel(bContext *C, wmOperator *op)
{
  if (auto *data = static_cast<GridColorFilterData *>(op->customdata)) {
    Object &object = *CTX_data_active_object(C);
    undo::restore_color_grids_from_undo_step(object);
    data->dirty_grids.clear();
    sculpt_color_filter_end(C, op, object);
  }
}

static wmOperatorStatus sculpt_color_filter_modal(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
{
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.runtime->sculpt_session;

  if (op->customdata && ELEM(event->type, EVT_ESCKEY, RIGHTMOUSE) && event->val == KM_PRESS) {
    sculpt_color_filter_cancel(C, op);
    return OPERATOR_CANCELLED;
  }

  if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    /* For Fill: if this was a click (not a drag), apply once at the tool's configured strength */
    if (FilterType(RNA_enum_get(op->ptr, "type")) == FilterType::Fill &&
        !ss.filter_cache->has_dragged)
    {
      RNA_float_set(op->ptr, "strength", ss.filter_cache->start_filter_strength);
      sculpt_color_filter_apply(C, op, ob);
    }

    sculpt_color_filter_end(C, op, ob);
    return OPERATOR_FINISHED;
  }

  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  /* Use a pixel threshold to distinguish a click from a drag */
  int2 start_mouse;
  RNA_int_get_array(op->ptr, "start_mouse", &start_mouse[0]);
  const int2 mouse_2d(event->mval[0], event->mval[1]);

  const int drag_threshold = WM_event_drag_threshold(event);
  ss.filter_cache->has_dragged |= math::distance_manhattan(start_mouse, mouse_2d) > drag_threshold;

  if (!ss.filter_cache->has_dragged) {
    return OPERATOR_RUNNING_MODAL;
  }

  const float len = (start_mouse[0] - event->mval[0]) * 0.001f;
  float filter_strength = ss.filter_cache->start_filter_strength * -len;

  RNA_float_set(op->ptr, "strength", filter_strength);

  sculpt_color_filter_apply(C, op, ob);

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_color_filter_init(bContext *C, wmOperator *op)
{
  const Scene &scene = *CTX_data_scene(C);
  Object &ob = *CTX_data_active_object(C);
  Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  View3D *v3d = CTX_wm_view3d(C);

  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  int mval[2];
  RNA_int_get_array(op->ptr, "start_mouse", mval);
  float mval_fl[2] = {float(mval[0]), float(mval[1])};

  const bool use_automasking = auto_mask::is_enabled(sd.paint, ob, nullptr);
  if (use_automasking) {
    if (v3d) {
      /* Update the active face set manually as the paint cursor is not enabled when using the Mesh
       * Filter Tool. */
      cursor_geometry_info_update(C, mval_fl, false);
    }
  }

  /* Disable for dyntopo for now. */
  if (!color_supported_check(scene, ob, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  /* Ensure that we have a PBVH to be able to push changes on only visible nodes. */
  bke::object::pbvh_ensure(*CTX_data_ensure_evaluated_depsgraph(C), ob);

  undo::push_begin(scene, ob, op);
  BKE_sculpt_color_layer_create_if_needed(&ob);

  /* CTX_data_ensure_evaluated_depsgraph should be used at the end to include the potential
   * creation of color layer data. */
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  BKE_sculptsession_update_for_edit(depsgraph, &ob, true);
  if (SubdivCCG *ccg = ob.runtime->sculpt_session->subdiv_ccg) {
    const Mesh &mesh = *id_cast<Mesh *>(ob.data);
    BKE_subdiv_ccg_colors_ensure(mesh, *ccg, mesh.active_color_attribute);
  }

  filter::cache_init(C,
                     ob,
                     sd,
                     undo::Type::Color,
                     mval_fl,
                     RNA_float_get(op->ptr, "area_normal_radius"),
                     RNA_float_get(op->ptr, "strength"));
  const SculptSession &ss = *ob.runtime->sculpt_session;
  filter::Cache *filter_cache = ss.filter_cache;
  filter_cache->active_face_set = face_set_none_id;
  if (auto_mask::is_enabled(sd.paint, ob, nullptr)) {
    auto_mask::filter_cache_ensure(*depsgraph, sd.paint, ob);
  }
  if (bke::object::pbvh_get(ob)->type() == bke::pbvh::Type::Grids) {
    grid_color_filter_init(*depsgraph, ob, *op);
  }

  return OPERATOR_PASS_THROUGH;
}

static wmOperatorStatus sculpt_color_filter_exec(bContext *C, wmOperator *op)
{
  Object &ob = *CTX_data_active_object(C);

  if (sculpt_color_filter_init(C, op) == OPERATOR_CANCELLED) {
    return OPERATOR_CANCELLED;
  }

  sculpt_color_filter_apply(C, op, ob);
  sculpt_color_filter_end(C, op, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus sculpt_color_filter_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  Object &ob = *CTX_data_active_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d && v3d->shading.type == OB_SOLID) {
    v3d->shading.color_type = V3D_SHADING_VERTEX_COLOR;
  }

  RNA_int_set_array(op->ptr, "start_mouse", event->mval);

  /* Immediate execution path (used by key-bindings like `Ctrl+X`). */
  if (RNA_boolean_get(op->ptr, "use_immediate")) {
    RNA_float_set(op->ptr, "strength", 1.0f);
  }

  if (sculpt_color_filter_init(C, op) == OPERATOR_CANCELLED) {
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "use_immediate")) {
    sculpt_color_filter_apply(C, op, ob);
    sculpt_color_filter_end(C, op, ob);
    return OPERATOR_FINISHED;
  }

  ED_paint_brush_type_update_sticky_shading_color(C, &ob);

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static std::string sculpt_color_filter_get_name(wmOperatorType * /*ot*/, PointerRNA *ptr)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, "type");
  const int value = RNA_property_enum_get(ptr, prop);
  const char *ui_name = nullptr;

  RNA_property_enum_name_gettexted(nullptr, ptr, prop, value, &ui_name);
  return ui_name;
}

static void sculpt_color_filter_ui(bContext * /*C*/, wmOperator *op)
{
  ui::Layout &layout = *op->layout;

  layout.prop(op->ptr, "strength", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (FilterType(RNA_enum_get(op->ptr, "type")) == FilterType::Fill) {
    layout.prop(op->ptr, "fill_color", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

void SCULPT_OT_color_filter(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Filter Color";
  ot->idname = "SCULPT_OT_color_filter";
  ot->description = "Applies a filter to modify the active color attribute";

  /* API callbacks. */
  ot->invoke = sculpt_color_filter_invoke;
  ot->exec = sculpt_color_filter_exec;
  ot->modal = sculpt_color_filter_modal;
  ot->cancel = sculpt_color_filter_cancel;
  ot->poll = sculpt_mode_poll;
  ot->ui = sculpt_color_filter_ui;
  ot->get_name = sculpt_color_filter_get_name;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* rna */
  filter::register_operator_props(ot);

  RNA_def_enum(
      ot->srna, "type", prop_color_filter_types, int(FilterType::Fill), "Filter Type", "");

  PropertyRNA *prop = RNA_def_float_color(ot->srna,
                                          "fill_color",
                                          3,
                                          fill_filter_default_color,
                                          0.0f,
                                          FLT_MAX,
                                          "Fill Color",
                                          "",
                                          0.0f,
                                          1.0f);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MESH);
  RNA_def_property_subtype(prop, PROP_COLOR);

  prop = RNA_def_boolean(ot->srna,
                         "use_immediate",
                         false,
                         "Immediate Apply",
                         "Apply once without entering modal interaction");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna, "use_secondary_color", false, "Use Secondary Color", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

}  // namespace blender::ed::sculpt_paint::color
