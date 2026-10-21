/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include <cfloat>
#include <limits>
#include <queue>

#include "DNA_brush_types.h"
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_report.hh"
#include "BKE_subdiv_ccg.hh"
#include "BLI_array_utils.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "../paint_intern.hh"
#include "mesh_brush_common.hh"
#include "paint_mask.hh"
#include "sculpt_hide.hh"
#include "sculpt_intern.hh"
#include "sculpt_smooth.hh"
#include "sculpt_undo.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint::mask {

enum class FilterType {
  Smooth = 0,
  Sharpen = 1,
  Grow = 2,
  Shrink = 3,
  ContrastIncrease = 5,
  ContrastDecrease = 6,
};

BLI_NOINLINE static void copy_old_hidden_mask_mesh(const Span<int> verts,
                                                   const Span<bool> hide_vert,
                                                   const Span<float> mask,
                                                   const MutableSpan<float> new_mask)
{
  BLI_assert(verts.size() == new_mask.size());
  if (hide_vert.is_empty()) {
    return;
  }

  for (const int i : verts.index_range()) {
    if (hide_vert[verts[i]]) {
      new_mask[i] = mask[verts[i]];
    }
  }
}

BLI_NOINLINE static void multiply_add(const Span<float> src,
                                      const float factor,
                                      const float offset,
                                      const MutableSpan<float> dst)
{
  for (const int i : src.index_range()) {
    dst[i] = factor * src[i] + offset;
  }
}

BLI_NOINLINE static void mask_increase_contrast(const Span<float> src,
                                                const MutableSpan<float> dst)
{
  const float contrast = 0.1f;
  const float delta = contrast * 0.5f;
  const float gain = math::rcp(1.0f - contrast);
  const float offset = gain * -delta;
  multiply_add(src, gain, offset, dst);

  mask::clamp_mask(dst);
}

BLI_NOINLINE static void mask_decrease_contrast(const Span<float> src,
                                                const MutableSpan<float> dst)
{
  const float contrast = -0.1f;
  const float delta = contrast * 0.5f;
  const float gain = 1.0f - contrast;
  const float offset = gain * -delta;
  multiply_add(src, gain, offset, dst);

  mask::clamp_mask(dst);
}

BLI_NOINLINE static void sharpen_masks(const Span<float> old_masks,
                                       const MutableSpan<float> new_mask)
{
  for (const int i : old_masks.index_range()) {
    float val = new_mask[i];
    float mask = old_masks[i];
    val -= mask;
    if (mask > 0.5f) {
      mask += 0.05f;
    }
    else {
      mask -= 0.05f;
    }
    mask += val / 2.0f;
    new_mask[i] = mask;
  }

  mask::clamp_mask(new_mask);
}

struct FilterLocalData {
  Vector<int> visible_verts;
  Vector<float> node_mask;
  Vector<float> new_mask;
  Vector<int> neighbor_offsets;
  Vector<int> neighbor_data;
};

static void apply_new_mask_mesh(const Depsgraph &depsgraph,
                                Object &object,
                                const IndexMask &node_mask,
                                const OffsetIndices<int> node_verts,
                                const Span<float> new_mask,
                                MutableSpan<float> mask)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();

  Array<bool> node_changed(node_mask.min_array_size(), false);

  node_mask.foreach_index(
      [&](const int i, const int pos) {
        const Span<int> verts = nodes[i].verts();
        const Span<float> new_node_mask = new_mask.slice(node_verts[pos]);
        if (array_utils::indexed_data_equal<float>(mask, verts, new_node_mask)) {
          return;
        }
        undo::push_node(depsgraph, object, &nodes[i], undo::Type::Mask);
        scatter_data_mesh(new_node_mask, verts, mask);
        bke::pbvh::node_update_mask_mesh(mask, nodes[i]);
        node_changed[i] = true;
      },
      exec_mode::grain_size(1));

  IndexMaskMemory memory;
  pbvh.tag_masks_changed(IndexMask::from_bools(node_changed, memory));
}

static void smooth_mask_mesh(const OffsetIndices<int> faces,
                             const Span<int> corner_verts,
                             const GroupedSpan<int> vert_to_face,
                             const Span<bool> hide_poly,
                             const Span<bool> hide_vert,
                             const Span<float> mask,
                             const bke::pbvh::MeshNode &node,
                             FilterLocalData &tls,
                             MutableSpan<float> new_mask)
{
  const Span<int> verts = node.verts();

  const GroupedSpan<int> neighbors = calc_vert_neighbors(faces,
                                                         corner_verts,
                                                         vert_to_face,
                                                         hide_poly,
                                                         verts,
                                                         tls.neighbor_offsets,
                                                         tls.neighbor_data);

  tls.node_mask.resize(verts.size());
  const MutableSpan<float> node_mask = tls.node_mask;
  gather_data_mesh(mask, verts, node_mask);

  smooth::neighbor_data_average_mesh(mask, neighbors, new_mask);
  mask::mix_new_masks(new_mask, 0.5f, node_mask);
  copy_old_hidden_mask_mesh(verts, hide_vert, mask, node_mask);
  mask::clamp_mask(node_mask);
  new_mask.copy_from(node_mask);
}

static void sharpen_mask_mesh(const OffsetIndices<int> faces,
                              const Span<int> corner_verts,
                              const GroupedSpan<int> vert_to_face,
                              const Span<bool> hide_poly,
                              const Span<bool> hide_vert,
                              const Span<float> mask,
                              const bke::pbvh::MeshNode &node,
                              FilterLocalData &tls,
                              MutableSpan<float> new_mask)
{
  const Span<int> verts = node.verts();

  tls.node_mask.resize(verts.size());
  const MutableSpan<float> node_mask = tls.node_mask;
  gather_data_mesh(mask, verts, node_mask);

  const GroupedSpan<int> neighbors = calc_vert_neighbors(faces,
                                                         corner_verts,
                                                         vert_to_face,
                                                         hide_poly,
                                                         verts,
                                                         tls.neighbor_offsets,
                                                         tls.neighbor_data);

  smooth::neighbor_data_average_mesh(mask, neighbors, new_mask);

  sharpen_masks(node_mask, new_mask);
  copy_old_hidden_mask_mesh(verts, hide_vert, mask, new_mask);
}

static void grow_mask_mesh(const OffsetIndices<int> faces,
                           const Span<int> corner_verts,
                           const GroupedSpan<int> vert_to_face,
                           const Span<bool> hide_poly,
                           const Span<bool> hide_vert,
                           const Span<float> mask,
                           const bke::pbvh::MeshNode &node,
                           FilterLocalData &tls,
                           MutableSpan<float> new_mask)
{
  const Span<int> verts = node.verts();

  const GroupedSpan<int> neighbors = calc_vert_neighbors(faces,
                                                         corner_verts,
                                                         vert_to_face,
                                                         hide_poly,
                                                         verts,
                                                         tls.neighbor_offsets,
                                                         tls.neighbor_data);

  for (const int i : verts.index_range()) {
    new_mask[i] = mask[verts[i]];
    for (const int neighbor : neighbors[i]) {
      new_mask[i] = std::max(mask[neighbor], new_mask[i]);
    }
  }
  copy_old_hidden_mask_mesh(verts, hide_vert, mask, new_mask);
}

static void shrink_mask_mesh(const OffsetIndices<int> faces,
                             const Span<int> corner_verts,
                             const GroupedSpan<int> vert_to_face,
                             const Span<bool> hide_poly,
                             const Span<bool> hide_vert,
                             const Span<float> mask,
                             const bke::pbvh::MeshNode &node,
                             FilterLocalData &tls,
                             MutableSpan<float> new_mask)
{
  const Span<int> verts = node.verts();

  const GroupedSpan<int> neighbors = calc_vert_neighbors(faces,
                                                         corner_verts,
                                                         vert_to_face,
                                                         hide_poly,
                                                         verts,
                                                         tls.neighbor_offsets,
                                                         tls.neighbor_data);

  for (const int i : verts.index_range()) {
    new_mask[i] = mask[verts[i]];
    for (const int neighbor : neighbors[i]) {
      new_mask[i] = std::min(mask[neighbor], new_mask[i]);
    }
  }
  copy_old_hidden_mask_mesh(verts, hide_vert, mask, new_mask);
}

static bool increase_contrast_mask_mesh(const Depsgraph &depsgraph,
                                        const Object &object,
                                        const Span<bool> hide_vert,
                                        bke::pbvh::MeshNode &node,
                                        FilterLocalData &tls,
                                        MutableSpan<float> mask)
{
  const Span<int> verts = hide::node_visible_verts(node, hide_vert, tls.visible_verts);

  const Span<float> node_mask = gather_data_mesh(mask.as_span(), verts, tls.node_mask);

  tls.new_mask.resize(verts.size());
  const MutableSpan<float> new_mask = tls.new_mask;
  mask_increase_contrast(node_mask, new_mask);
  copy_old_hidden_mask_mesh(verts, hide_vert, mask, new_mask);

  if (node_mask == new_mask.as_span()) {
    return false;
  }

  undo::push_node(depsgraph, object, &node, undo::Type::Mask);
  scatter_data_mesh(new_mask.as_span(), verts, mask);
  bke::pbvh::node_update_mask_mesh(mask, node);
  return true;
}

static bool decrease_contrast_mask_mesh(const Depsgraph &depsgraph,
                                        const Object &object,
                                        const Span<bool> hide_vert,
                                        bke::pbvh::MeshNode &node,
                                        FilterLocalData &tls,
                                        MutableSpan<float> mask)
{
  const Span<int> verts = hide::node_visible_verts(node, hide_vert, tls.visible_verts);

  const Span<float> node_mask = gather_data_mesh(mask.as_span(), verts, tls.node_mask);

  tls.new_mask.resize(verts.size());
  const MutableSpan<float> new_mask = tls.new_mask;
  mask_decrease_contrast(node_mask, new_mask);
  copy_old_hidden_mask_mesh(verts, hide_vert, mask, new_mask);

  if (node_mask == new_mask.as_span()) {
    return false;
  }

  undo::push_node(depsgraph, object, &node, undo::Type::Mask);
  scatter_data_mesh(new_mask.as_span(), verts, mask);
  bke::pbvh::node_update_mask_mesh(mask, node);
  return true;
}

BLI_NOINLINE static void copy_old_hidden_mask_grids(const SubdivCCG &subdiv_ccg,
                                                    const Span<int> grids,
                                                    const MutableSpan<float> new_mask)
{
  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
  if (subdiv_ccg.grid_hidden.is_empty()) {
    return;
  }
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<float> masks = subdiv_ccg.masks;
  for (const int i : grids.index_range()) {
    const Span grid_masks = masks.slice(bke::ccg::grid_range(key, grids[i]));
    MutableSpan grid_dst = new_mask.slice(bke::ccg::grid_range(key, i));
    bits::foreach_1_index(grid_hidden[grids[i]],
                          [&](const int offset) { grid_dst[offset] = grid_masks[offset]; });
  }
}

static void apply_new_mask_grids(const Depsgraph &depsgraph,
                                 Object &object,
                                 const IndexMask &node_mask,
                                 const OffsetIndices<int> node_verts,
                                 const Span<float> new_mask)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  MutableSpan<float> masks = subdiv_ccg.masks;

  Array<bool> node_changed(node_mask.min_array_size(), false);

  node_mask.foreach_index(
      [&](const int i, const int pos) {
        const Span<int> grids = nodes[i].grids();
        const Span<float> new_node_mask = new_mask.slice(node_verts[pos]);
        if (mask_equals_array_grids(masks, key, grids, new_node_mask)) {
          return;
        }
        undo::push_node(depsgraph, object, &nodes[i], undo::Type::Mask);
        scatter_data_grids(subdiv_ccg, new_node_mask, grids, masks);
        bke::pbvh::node_update_mask_grids(key, masks, nodes[i]);
        node_changed[i] = true;
      },
      exec_mode::grain_size(1));

  IndexMaskMemory memory;
  pbvh.tag_masks_changed(IndexMask::from_bools(node_changed, memory));

  /* New mask values need propagation across grid boundaries. */
  BKE_subdiv_ccg_average_grids(subdiv_ccg);
}

static void smooth_mask_grids(const SubdivCCG &subdiv_ccg,
                              const bke::pbvh::GridsNode &node,
                              FilterLocalData &tls,
                              MutableSpan<float> new_mask)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = node.grids();
  const int grid_verts_num = grids.size() * key.grid_area;

  tls.node_mask.resize(grid_verts_num);
  const MutableSpan<float> node_mask = tls.node_mask;
  gather_data_grids(subdiv_ccg, subdiv_ccg.masks.as_span(), grids, node_mask);

  smooth::average_data_grids(subdiv_ccg, subdiv_ccg.masks.as_span(), grids, new_mask);
  mask::mix_new_masks(new_mask, 0.5f, node_mask);
  copy_old_hidden_mask_grids(subdiv_ccg, grids, node_mask);
  mask::clamp_mask(node_mask);
  new_mask.copy_from(node_mask);
}

static void sharpen_mask_grids(const SubdivCCG &subdiv_ccg,
                               const bke::pbvh::GridsNode &node,
                               FilterLocalData &tls,
                               MutableSpan<float> new_mask)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = node.grids();
  const int grid_verts_num = grids.size() * key.grid_area;

  tls.node_mask.resize(grid_verts_num);
  const MutableSpan<float> node_mask = tls.node_mask;
  gather_data_grids(subdiv_ccg, subdiv_ccg.masks.as_span(), grids, node_mask);

  smooth::average_data_grids(subdiv_ccg, subdiv_ccg.masks.as_span(), grids, new_mask);

  sharpen_masks(node_mask, new_mask);

  copy_old_hidden_mask_grids(subdiv_ccg, grids, new_mask);
}

static void grow_mask_grids(const SubdivCCG &subdiv_ccg,
                            const bke::pbvh::GridsNode &node,
                            MutableSpan<float> new_mask)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<float> masks = subdiv_ccg.masks;

  const Span<int> grids = node.grids();

  for (const int i : grids.index_range()) {
    const int grid = grids[i];
    const Span grid_masks = masks.slice(bke::ccg::grid_range(key, grid));
    MutableSpan grid_dst = new_mask.slice(bke::ccg::grid_range(key, i));

    for (const short y : IndexRange(key.grid_size)) {
      for (const short x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);

        SubdivCCGNeighbors neighbors;
        SubdivCCGCoord coord{grid, x, y};
        BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, coord, false, neighbors);

        grid_dst[offset] = grid_masks[offset];
        for (const SubdivCCGCoord neighbor : neighbors.coords) {
          grid_dst[offset] = std::max(masks[neighbor.to_index(key)], grid_dst[offset]);
        }
      }
    }
  }

  copy_old_hidden_mask_grids(subdiv_ccg, grids, new_mask);
}

static void shrink_mask_grids(const SubdivCCG &subdiv_ccg,
                              const bke::pbvh::GridsNode &node,
                              MutableSpan<float> new_mask)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<float> masks = subdiv_ccg.masks;

  const Span<int> grids = node.grids();

  for (const int i : grids.index_range()) {
    const int grid = grids[i];
    const Span grid_masks = masks.slice(bke::ccg::grid_range(key, grid));
    MutableSpan grid_dst = new_mask.slice(bke::ccg::grid_range(key, i));

    for (const short y : IndexRange(key.grid_size)) {
      for (const short x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);

        SubdivCCGNeighbors neighbors;
        SubdivCCGCoord coord{grid, x, y};
        BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, coord, false, neighbors);

        grid_dst[offset] = grid_masks[offset];
        for (const SubdivCCGCoord neighbor : neighbors.coords) {
          grid_dst[offset] = std::min(masks[neighbor.to_index(key)], grid_dst[offset]);
        }
      }
    }
  }

  copy_old_hidden_mask_grids(subdiv_ccg, grids, new_mask);
}

static bool increase_contrast_mask_grids(const Depsgraph &depsgraph,
                                         const Object &object,
                                         bke::pbvh::GridsNode &node,
                                         FilterLocalData &tls)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = node.grids();
  const int grid_verts_num = grids.size() * key.grid_area;

  tls.node_mask.resize(grid_verts_num);
  const MutableSpan<float> node_mask = tls.node_mask;
  gather_data_grids(subdiv_ccg, subdiv_ccg.masks.as_span(), grids, node_mask);

  tls.new_mask.resize(grid_verts_num);
  const MutableSpan<float> new_mask = tls.new_mask;
  mask_increase_contrast(node_mask, new_mask);

  copy_old_hidden_mask_grids(subdiv_ccg, grids, new_mask);

  if (node_mask.as_span() == new_mask.as_span()) {
    return false;
  }

  undo::push_node(depsgraph, object, &node, undo::Type::Mask);
  scatter_data_grids(subdiv_ccg, new_mask.as_span(), grids, subdiv_ccg.masks.as_mutable_span());
  bke::pbvh::node_update_mask_grids(key, subdiv_ccg.masks, node);
  return true;
}

static bool decrease_contrast_mask_grids(const Depsgraph &depsgraph,
                                         const Object &object,
                                         bke::pbvh::GridsNode &node,
                                         FilterLocalData &tls)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = node.grids();
  const int grid_verts_num = grids.size() * key.grid_area;

  tls.node_mask.resize(grid_verts_num);
  const MutableSpan<float> node_mask = tls.node_mask;
  gather_data_grids(subdiv_ccg, subdiv_ccg.masks.as_span(), grids, node_mask);

  tls.new_mask.resize(grid_verts_num);
  const MutableSpan<float> new_mask = tls.new_mask;
  mask_decrease_contrast(node_mask, new_mask);

  copy_old_hidden_mask_grids(subdiv_ccg, grids, new_mask);

  if (node_mask.as_span() == new_mask.as_span()) {
    return false;
  }

  undo::push_node(depsgraph, object, &node, undo::Type::Mask);
  scatter_data_grids(subdiv_ccg, new_mask.as_span(), grids, subdiv_ccg.masks.as_mutable_span());
  bke::pbvh::node_update_mask_grids(key, subdiv_ccg.masks, node);
  return true;
}

BLI_NOINLINE static void copy_old_hidden_mask_bmesh(const int mask_offset,
                                                    const Set<BMVert *, 0> &verts,
                                                    const MutableSpan<float> new_mask)
{
  int i = 0;
  for (const BMVert *vert : verts) {
    if (BM_elem_flag_test(vert, BM_ELEM_HIDDEN)) {
      new_mask[i] = BM_ELEM_CD_GET_FLOAT(vert, mask_offset);
    }
    i++;
  }
}

static void apply_new_mask_bmesh(const Depsgraph &depsgraph,
                                 Object &object,
                                 const int mask_offset,
                                 const IndexMask &node_mask,
                                 const OffsetIndices<int> node_verts,
                                 const Span<float> new_mask)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
  BMesh &bm = *ss.bm;

  Array<bool> node_changed(node_mask.min_array_size(), false);

  node_mask.foreach_index(
      [&](const int i, const int pos) {
        const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&nodes[i]);
        const Span<float> new_node_mask = new_mask.slice(node_verts[pos]);
        if (mask_equals_array_bmesh(mask_offset, verts, new_node_mask)) {
          return;
        }
        undo::push_node(depsgraph, object, &nodes[i], undo::Type::Mask);
        scatter_mask_bmesh(new_node_mask, bm, verts);
        bke::pbvh::node_update_mask_bmesh(mask_offset, nodes[i]);
        node_changed[i] = true;
      },
      exec_mode::grain_size(1));

  IndexMaskMemory memory;
  pbvh.tag_masks_changed(IndexMask::from_bools(node_changed, memory));
}

static void smooth_mask_bmesh(const BMesh &bm,
                              const int mask_offset,
                              bke::pbvh::BMeshNode &node,
                              FilterLocalData &tls,
                              MutableSpan<float> new_mask)
{
  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  tls.node_mask.resize(verts.size());
  const MutableSpan<float> node_mask = tls.node_mask;
  gather_mask_bmesh(bm, verts, node_mask);

  average_neighbor_mask_bmesh(mask_offset, verts, new_mask);
  mask::mix_new_masks(new_mask, 0.5f, node_mask);
  copy_old_hidden_mask_bmesh(mask_offset, verts, node_mask);
  mask::clamp_mask(node_mask);
  new_mask.copy_from(node_mask);
}

static void sharpen_mask_bmesh(const BMesh &bm,
                               const int mask_offset,
                               bke::pbvh::BMeshNode &node,
                               FilterLocalData &tls,
                               MutableSpan<float> new_mask)
{
  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  tls.node_mask.resize(verts.size());
  const MutableSpan<float> node_mask = tls.node_mask;
  gather_mask_bmesh(bm, verts, node_mask);

  average_neighbor_mask_bmesh(mask_offset, verts, new_mask);

  sharpen_masks(node_mask, new_mask);

  copy_old_hidden_mask_bmesh(mask_offset, verts, new_mask);
}

static void grow_mask_bmesh(const int mask_offset,
                            bke::pbvh::BMeshNode &node,
                            MutableSpan<float> new_mask)
{
  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  BMeshNeighborVerts neighbors;
  int i = 0;
  for (BMVert *vert : verts) {
    new_mask[i] = BM_ELEM_CD_GET_FLOAT(vert, mask_offset);
    for (const BMVert *neighbor : vert_neighbors_get_bmesh(*vert, neighbors)) {
      new_mask[i] = std::max(BM_ELEM_CD_GET_FLOAT(neighbor, mask_offset), new_mask[i]);
    }
    i++;
  }

  copy_old_hidden_mask_bmesh(mask_offset, verts, new_mask);
}

static void shrink_mask_bmesh(const int mask_offset,
                              bke::pbvh::BMeshNode &node,
                              MutableSpan<float> new_mask)
{
  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  BMeshNeighborVerts neighbors;
  int i = 0;
  for (BMVert *vert : verts) {
    new_mask[i] = BM_ELEM_CD_GET_FLOAT(vert, mask_offset);
    for (const BMVert *neighbor : vert_neighbors_get_bmesh(*vert, neighbors)) {
      new_mask[i] = std::min(BM_ELEM_CD_GET_FLOAT(neighbor, mask_offset), new_mask[i]);
    }
    i++;
  }

  copy_old_hidden_mask_bmesh(mask_offset, verts, new_mask);
}

static bool increase_contrast_mask_bmesh(const Depsgraph &depsgraph,
                                         Object &object,
                                         const int mask_offset,
                                         bke::pbvh::BMeshNode &node,
                                         FilterLocalData &tls)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  BMesh &bm = *ss.bm;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  tls.node_mask.resize(verts.size());
  const MutableSpan<float> node_mask = tls.node_mask;
  gather_mask_bmesh(bm, verts, node_mask);

  tls.new_mask.resize(verts.size());
  const MutableSpan<float> new_mask = tls.new_mask;
  mask_increase_contrast(node_mask, new_mask);

  copy_old_hidden_mask_bmesh(mask_offset, verts, new_mask);

  if (node_mask.as_span() == new_mask.as_span()) {
    return false;
  }

  undo::push_node(depsgraph, object, &node, undo::Type::Mask);
  scatter_mask_bmesh(new_mask.as_span(), bm, verts);
  bke::pbvh::node_update_mask_bmesh(mask_offset, node);
  return true;
}

static bool decrease_contrast_mask_bmesh(const Depsgraph &depsgraph,
                                         Object &object,
                                         const int mask_offset,
                                         bke::pbvh::BMeshNode &node,
                                         FilterLocalData &tls)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  BMesh &bm = *ss.bm;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  tls.node_mask.resize(verts.size());
  const MutableSpan<float> node_mask = tls.node_mask;
  gather_mask_bmesh(bm, verts, node_mask);

  tls.new_mask.resize(verts.size());
  const MutableSpan<float> new_mask = tls.new_mask;
  mask_decrease_contrast(node_mask, new_mask);

  copy_old_hidden_mask_bmesh(mask_offset, verts, new_mask);

  if (node_mask.as_span() == new_mask.as_span()) {
    return false;
  }

  undo::push_node(depsgraph, object, &node, undo::Type::Mask);
  scatter_mask_bmesh(new_mask.as_span(), bm, verts);
  bke::pbvh::node_update_mask_bmesh(mask_offset, node);
  return true;
}

static wmOperatorStatus sculpt_mask_filter_exec(bContext *C, wmOperator *op)
{
  const Scene &scene = *CTX_data_scene(C);
  Object &ob = *CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const FilterType filter_type = FilterType(RNA_enum_get(op->ptr, "filter_type"));

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  MultiresModifierData *mmd = BKE_sculpt_multires_active(&scene, &ob);
  BKE_sculpt_mask_layers_ensure(CTX_data_depsgraph_pointer(C), CTX_data_main(C), &ob, mmd);

  ed::sculpt_paint::mask_overlay_check(*C, *op);

  BKE_sculpt_update_object_for_edit(depsgraph, &ob, false);

  SculptSession &ss = *ob.runtime->sculpt_session;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);

  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);
  undo::push_begin(scene, ob, op);

  int iterations = RNA_int_get(op->ptr, "iterations");

  /* Auto iteration count calculates the number of iteration based on the vertices of the mesh to
   * avoid adding an unnecessary amount of undo steps when using the operator from a shortcut.
   * One iteration per 50000 vertices in the mesh should be fine in most cases.
   * Maybe we want this to be configurable. */
  if (RNA_boolean_get(op->ptr, "auto_iteration_count")) {
    iterations = int(vertex_count_get(ob) / 50000.0f) + 1;
  }

  threading::EnumerableThreadSpecific<FilterLocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      Mesh &mesh = *id_cast<Mesh *>(ob.data);
      const OffsetIndices<int> faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
      const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
      const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
      bke::SpanAttributeWriter mask = attributes.lookup_for_write_span<float>(".sculpt_mask");

      Array<int> node_vert_offset_data;
      OffsetIndices node_offsets = create_node_vert_offsets(
          nodes, node_mask, node_vert_offset_data);
      Array<float> new_masks(node_offsets.total_size());

      for ([[maybe_unused]] const int iteration : IndexRange(iterations)) {
        switch (filter_type) {
          case FilterType::Smooth: {
            node_mask.foreach_index(
                [&](const int i, const int pos) {
                  FilterLocalData &tls = all_tls.local();
                  smooth_mask_mesh(faces,
                                   corner_verts,
                                   vert_to_face_map,
                                   hide_poly,
                                   hide_vert,
                                   mask.span,
                                   nodes[i],
                                   tls,
                                   new_masks.as_mutable_span().slice(node_offsets[pos]));
                },
                exec_mode::grain_size(1));
            apply_new_mask_mesh(*depsgraph, ob, node_mask, node_offsets, new_masks, mask.span);
            break;
          }
          case FilterType::Sharpen: {
            node_mask.foreach_index(
                [&](const int i, const int pos) {
                  FilterLocalData &tls = all_tls.local();
                  sharpen_mask_mesh(faces,
                                    corner_verts,
                                    vert_to_face_map,
                                    hide_poly,
                                    hide_vert,
                                    mask.span,
                                    nodes[i],
                                    tls,
                                    new_masks.as_mutable_span().slice(node_offsets[pos]));
                },
                exec_mode::grain_size(1));
            apply_new_mask_mesh(*depsgraph, ob, node_mask, node_offsets, new_masks, mask.span);
            break;
          }
          case FilterType::Grow: {
            node_mask.foreach_index(
                [&](const int i, const int pos) {
                  FilterLocalData &tls = all_tls.local();
                  grow_mask_mesh(faces,
                                 corner_verts,
                                 vert_to_face_map,
                                 hide_poly,
                                 hide_vert,
                                 mask.span,
                                 nodes[i],
                                 tls,
                                 new_masks.as_mutable_span().slice(node_offsets[pos]));
                },
                exec_mode::grain_size(1));
            apply_new_mask_mesh(*depsgraph, ob, node_mask, node_offsets, new_masks, mask.span);
            break;
          }
          case FilterType::Shrink: {
            node_mask.foreach_index(
                [&](const int i, const int pos) {
                  FilterLocalData &tls = all_tls.local();
                  shrink_mask_mesh(faces,
                                   corner_verts,
                                   vert_to_face_map,
                                   hide_poly,
                                   hide_vert,
                                   mask.span,
                                   nodes[i],
                                   tls,
                                   new_masks.as_mutable_span().slice(node_offsets[pos]));
                },
                exec_mode::grain_size(1));
            apply_new_mask_mesh(*depsgraph, ob, node_mask, node_offsets, new_masks, mask.span);
            break;
          }
          case FilterType::ContrastIncrease: {
            Array<bool> node_changed(node_mask.min_array_size(), false);
            node_mask.foreach_index(
                [&](const int i) {
                  FilterLocalData &tls = all_tls.local();
                  node_changed[i] = increase_contrast_mask_mesh(
                      *depsgraph, ob, hide_vert, nodes[i], tls, mask.span);
                },
                exec_mode::grain_size(1));
            IndexMaskMemory memory;
            pbvh.tag_masks_changed(IndexMask::from_bools(node_changed, memory));
            break;
          }
          case FilterType::ContrastDecrease: {
            Array<bool> node_changed(node_mask.min_array_size(), false);
            node_mask.foreach_index(
                [&](const int i) {
                  FilterLocalData &tls = all_tls.local();
                  node_changed[i] = decrease_contrast_mask_mesh(
                      *depsgraph, ob, hide_vert, nodes[i], tls, mask.span);
                },
                exec_mode::grain_size(1));
            IndexMaskMemory memory;
            pbvh.tag_masks_changed(IndexMask::from_bools(node_changed, memory));
            break;
          }
        }
      }
      mask.finish();
      break;
    }
    case bke::pbvh::Type::Grids: {
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

      Array<int> node_vert_offset_data;
      OffsetIndices node_offsets = create_node_vert_offsets(
          BKE_subdiv_ccg_key_top_level(subdiv_ccg), nodes, node_mask, node_vert_offset_data);
      Array<float> new_masks(node_offsets.total_size());

      for ([[maybe_unused]] const int iteration : IndexRange(iterations)) {
        switch (filter_type) {
          case FilterType::Smooth: {
            node_mask.foreach_index(
                [&](const int i, const int pos) {
                  FilterLocalData &tls = all_tls.local();
                  smooth_mask_grids(subdiv_ccg,
                                    nodes[i],
                                    tls,
                                    new_masks.as_mutable_span().slice(node_offsets[pos]));
                },
                exec_mode::grain_size(1));
            apply_new_mask_grids(*depsgraph, ob, node_mask, node_offsets, new_masks);
            break;
          }
          case FilterType::Sharpen: {
            node_mask.foreach_index(
                [&](const int i, const int pos) {
                  FilterLocalData &tls = all_tls.local();
                  sharpen_mask_grids(subdiv_ccg,
                                     nodes[i],
                                     tls,
                                     new_masks.as_mutable_span().slice(node_offsets[pos]));
                },
                exec_mode::grain_size(1));
            apply_new_mask_grids(*depsgraph, ob, node_mask, node_offsets, new_masks);
            break;
          }
          case FilterType::Grow: {
            node_mask.foreach_index(
                [&](const int i, const int pos) {
                  grow_mask_grids(
                      subdiv_ccg, nodes[i], new_masks.as_mutable_span().slice(node_offsets[pos]));
                },
                exec_mode::grain_size(1));
            apply_new_mask_grids(*depsgraph, ob, node_mask, node_offsets, new_masks);
            break;
          }
          case FilterType::Shrink: {
            node_mask.foreach_index(
                [&](const int i, const int pos) {
                  shrink_mask_grids(
                      subdiv_ccg, nodes[i], new_masks.as_mutable_span().slice(node_offsets[pos]));
                },
                exec_mode::grain_size(1));
            apply_new_mask_grids(*depsgraph, ob, node_mask, node_offsets, new_masks);
            break;
          }
          case FilterType::ContrastIncrease: {
            Array<bool> node_changed(node_mask.min_array_size(), false);
            node_mask.foreach_index(
                [&](const int i) {
                  FilterLocalData &tls = all_tls.local();
                  node_changed[i] = increase_contrast_mask_grids(*depsgraph, ob, nodes[i], tls);
                },
                exec_mode::grain_size(1));
            IndexMaskMemory memory;
            pbvh.tag_masks_changed(IndexMask::from_bools(node_changed, memory));
            break;
          }
          case FilterType::ContrastDecrease: {
            Array<bool> node_changed(node_mask.min_array_size(), false);
            node_mask.foreach_index(
                [&](const int i) {
                  FilterLocalData &tls = all_tls.local();
                  node_changed[i] = decrease_contrast_mask_grids(*depsgraph, ob, nodes[i], tls);
                },
                exec_mode::grain_size(1));
            IndexMaskMemory memory;
            pbvh.tag_masks_changed(IndexMask::from_bools(node_changed, memory));
            break;
          }
        }
      }
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      BMesh &bm = *ss.bm;
      vert_random_access_ensure(ob);
      const int mask_offset = CustomData_get_offset_named(
          &bm.vdata, CD_PROP_FLOAT, ".sculpt_mask");

      Array<int> node_vert_offset_data;
      OffsetIndices node_offsets = create_node_vert_offsets_bmesh(
          nodes, node_mask, node_vert_offset_data);
      Array<float> new_masks(node_offsets.total_size());

      for ([[maybe_unused]] const int iteration : IndexRange(iterations)) {
        switch (filter_type) {
          case FilterType::Smooth: {
            node_mask.foreach_index(
                [&](const int i, const int pos) {
                  FilterLocalData &tls = all_tls.local();
                  smooth_mask_bmesh(bm,
                                    mask_offset,
                                    nodes[i],
                                    tls,
                                    new_masks.as_mutable_span().slice(node_offsets[pos]));
                },
                exec_mode::grain_size(1));
            apply_new_mask_bmesh(*depsgraph, ob, mask_offset, node_mask, node_offsets, new_masks);
            break;
          }
          case FilterType::Sharpen: {
            node_mask.foreach_index(
                [&](const int i, const int pos) {
                  FilterLocalData &tls = all_tls.local();
                  sharpen_mask_bmesh(bm,
                                     mask_offset,
                                     nodes[i],
                                     tls,
                                     new_masks.as_mutable_span().slice(node_offsets[pos]));
                },
                exec_mode::grain_size(1));
            apply_new_mask_bmesh(*depsgraph, ob, mask_offset, node_mask, node_offsets, new_masks);
            break;
          }
          case FilterType::Grow: {
            node_mask.foreach_index(
                [&](const int i, const int pos) {
                  grow_mask_bmesh(
                      mask_offset, nodes[i], new_masks.as_mutable_span().slice(node_offsets[pos]));
                },
                exec_mode::grain_size(1));
            apply_new_mask_bmesh(*depsgraph, ob, mask_offset, node_mask, node_offsets, new_masks);
            break;
          }
          case FilterType::Shrink: {
            node_mask.foreach_index(
                [&](const int i, const int pos) {
                  shrink_mask_bmesh(
                      mask_offset, nodes[i], new_masks.as_mutable_span().slice(node_offsets[pos]));
                },
                exec_mode::grain_size(1));
            apply_new_mask_bmesh(*depsgraph, ob, mask_offset, node_mask, node_offsets, new_masks);
            break;
          }
          case FilterType::ContrastIncrease: {
            Array<bool> node_changed(node_mask.min_array_size(), false);
            node_mask.foreach_index(
                [&](const int i) {
                  FilterLocalData &tls = all_tls.local();
                  node_changed[i] = increase_contrast_mask_bmesh(
                      *depsgraph, ob, mask_offset, nodes[i], tls);
                },
                exec_mode::grain_size(1));
            IndexMaskMemory memory;
            pbvh.tag_masks_changed(IndexMask::from_bools(node_changed, memory));
            break;
          }
          case FilterType::ContrastDecrease: {
            Array<bool> node_changed(node_mask.min_array_size(), false);
            node_mask.foreach_index(
                [&](const int i) {
                  FilterLocalData &tls = all_tls.local();
                  node_changed[i] = decrease_contrast_mask_bmesh(
                      *depsgraph, ob, mask_offset, nodes[i], tls);
                },
                exec_mode::grain_size(1));
            IndexMaskMemory memory;
            pbvh.tag_masks_changed(IndexMask::from_bools(node_changed, memory));
            break;
          }
        }
      }
      break;
    }
  }

  undo::push_end(ob);

  flush_update_step(C, UpdateType::Mask);
  flush_update_done(C, ob, UpdateType::Mask);

  tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

static CurveMapping *bevel_curve_ensure(Sculpt &sculpt)
{
  if (!sculpt.bevel_mask_curve) {
    sculpt.bevel_mask_curve = BKE_curvemapping_add(1, 0, 0, 1, 1);
    BKE_curvemap_reset(&sculpt.bevel_mask_curve->cm[0],
                       &sculpt.bevel_mask_curve->clipr,
                       CURVE_PRESET_LINE,
                       CurveMapSlopeType::Negative);
  }
  BKE_curvemapping_init(sculpt.bevel_mask_curve);
  return sculpt.bevel_mask_curve;
}

static wmOperatorStatus bevel_mask_exec(bContext *C, wmOperator *op)
{
  Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const float radius = RNA_float_get(op->ptr, "radius");
  const float threshold = RNA_float_get(op->ptr, "threshold");
  const eBrushCurvePreset profile = eBrushCurvePreset(RNA_enum_get(op->ptr, "profile"));
  CurveMapping *curve = bevel_curve_ensure(*scene.toolsettings->sculpt);
  MultiresModifierData *mmd = BKE_sculpt_multires_active(&scene, &object);
  BKE_sculpt_mask_layers_ensure(depsgraph, CTX_data_main(C), &object, mmd);
  BKE_sculpt_update_object_for_edit(depsgraph, &object, false);
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  SculptSession &ss = *object.runtime->sculpt_session;
  Mesh &mesh = *id_cast<Mesh *>(object.data);
  const int size = vertex_count_get(object);
  Array<float3> positions(size);
  Array<float> original(size);
  Array<bool> hidden(size, false);
  Array<int> neighbor_offsets(size + 1);
  Vector<int> neighbor_indices;
  neighbor_indices.reserve(int64_t(size) * 6);
  int bm_mask_offset = -1;
  CCGKey key{};
  if (pbvh.type() == bke::pbvh::Type::Mesh) {
    positions.as_mutable_span().copy_from(bke::pbvh::vert_positions_eval(*depsgraph, object));
    const VArray<float> masks = *mesh.attributes().lookup_or_default<float>(
        ".sculpt_mask", bke::AttrDomain::Point, 0.0f);
    const VArray<bool> hide = *mesh.attributes().lookup_or_default<bool>(
        ".hide_vert", bke::AttrDomain::Point, false);
    masks.materialize(original);
    hide.materialize(hidden);
    Array<int> edge_offsets, edge_indices;
    const Span<int2> edges = mesh.edges();
    const GroupedSpan<int> vert_edges = bke::mesh::build_vert_to_edge_map(
        edges, size, edge_offsets, edge_indices);
    for (const int i : positions.index_range()) {
      neighbor_offsets[i] = neighbor_indices.size();
      for (const int edge : vert_edges[i]) {
        neighbor_indices.append(edges[edge][0] == i ? edges[edge][1] : edges[edge][0]);
      }
    }
  }
  else if (pbvh.type() == bke::pbvh::Type::Grids) {
    const SubdivCCG &ccg = *ss.subdiv_ccg;
    key = BKE_subdiv_ccg_key_top_level(ccg);
    positions.as_mutable_span().copy_from(ccg.positions);
    original.as_mutable_span().copy_from(ccg.masks);
    SubdivCCGNeighbors adjacent;
    for (const int i : positions.index_range()) {
      neighbor_offsets[i] = neighbor_indices.size();
      const SubdivCCGCoord coord = SubdivCCGCoord::from_index(key, i);
      hidden[i] = !ccg.grid_hidden.is_empty() &&
                  ccg.grid_hidden[coord.grid_index][i % key.grid_area];
      BKE_subdiv_ccg_neighbor_coords_get(ccg, coord, true, adjacent);
      for (const SubdivCCGCoord &neighbor : adjacent.coords) {
        neighbor_indices.append(neighbor.to_index(key));
      }
    }
  }
  else {
    BM_mesh_elem_index_ensure(ss.bm, BM_VERT);
    BM_mesh_elem_table_ensure(ss.bm, BM_VERT);
    bm_mask_offset = CustomData_get_offset_named(&ss.bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");
    for (const int i : positions.index_range()) {
      neighbor_offsets[i] = neighbor_indices.size();
      BMVert *vert = ss.bm->vtable[i];
      positions[i] = float3(vert->co);
      original[i] = BM_ELEM_CD_GET_FLOAT(vert, bm_mask_offset);
      hidden[i] = BM_elem_flag_test(vert, BM_ELEM_HIDDEN);
      BMIter iter;
      BMEdge *edge;
      BM_ITER_ELEM (edge, &iter, vert, BM_EDGES_OF_VERT) {
        neighbor_indices.append(BM_elem_index_get(BM_edge_other_vert(edge, vert)));
      }
    }
  }
  neighbor_offsets[size] = neighbor_indices.size();
  const OffsetIndices<int> neighbor_groups(neighbor_offsets);
  auto neighbors_of = [&](const int i) {
    return neighbor_indices.as_span().slice(neighbor_groups[i]);
  };
  /* Seed the interpolated threshold crossing of every edge, then propagate on
   * the surface graph. Disconnected components never exchange mask values. */
  Array<float> distance(size, std::numeric_limits<float>::infinity());
  using Entry = std::pair<float, int>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
  for (const int i : positions.index_range()) {
    if (hidden[i]) {
      continue;
    }
    for (const int j : neighbors_of(i)) {
      if (hidden[j] || (original[i] >= threshold) == (original[j] >= threshold)) {
        continue;
      }
      const float length = math::distance(positions[i], positions[j]);
      const float seed = length *
                         std::abs((original[i] - threshold) / (original[i] - original[j]));
      distance[i] = std::min(distance[i], seed);
    }
    if (distance[i] <= radius) {
      queue.emplace(distance[i], i);
    }
  }
  while (!queue.empty()) {
    const auto [current, i] = queue.top();
    queue.pop();
    if (current != distance[i]) {
      continue;
    }
    for (const int j : neighbors_of(i)) {
      if (hidden[j] || (original[i] >= threshold) != (original[j] >= threshold)) {
        continue;
      }
      const float next = current + math::distance(positions[i], positions[j]);
      if (next <= radius && next < distance[j]) {
        distance[j] = next;
        queue.emplace(next, j);
      }
    }
  }
  Array<float> result = original;
  for (const int i : positions.index_range()) {
    if (!hidden[i] && distance[i] <= radius) {
      const float t = 0.5f +
                      (original[i] >= threshold ? distance[i] : -distance[i]) / (2.0f * radius);
      result[i] = std::clamp(BKE_brush_curve_strength(profile, curve, 1.0f - t, 1.0f), 0.0f, 1.0f);
    }
  }
  IndexMaskMemory memory;
  const IndexMask nodes_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);
  undo::push_begin(scene, object, op);
  /* Save all affected nodes before any mask values are written. */
  nodes_mask.foreach_index([&](const int i) {
    bke::pbvh::Node *node = nullptr;
    bool changed = false;
    if (pbvh.type() == bke::pbvh::Type::Mesh) {
      auto &mesh_node = pbvh.nodes<bke::pbvh::MeshNode>()[i];
      node = &mesh_node;
      for (const int vert : mesh_node.verts()) {
        changed |= result[vert] != original[vert];
      }
    }
    else if (pbvh.type() == bke::pbvh::Type::Grids) {
      auto &grid_node = pbvh.nodes<bke::pbvh::GridsNode>()[i];
      node = &grid_node;
      for (const int grid : grid_node.grids()) {
        for (const int vert : IndexRange(grid * key.grid_area, key.grid_area)) {
          changed |= result[vert] != original[vert];
        }
      }
    }
    else {
      auto &bm_node = pbvh.nodes<bke::pbvh::BMeshNode>()[i];
      node = &bm_node;
      for (const BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(&bm_node)) {
        const int index = BM_elem_index_get(vert);
        changed |= result[index] != original[index];
      }
    }
    if (changed) {
      undo::push_node(*depsgraph, object, node, undo::Type::Mask);
    }
  });
  if (pbvh.type() == bke::pbvh::Type::Mesh) {
    bke::SpanAttributeWriter<float> masks =
        mesh.attributes_for_write().lookup_for_write_span<float>(".sculpt_mask");
    masks.span.copy_from(result);
    MutableSpan nodes = pbvh.nodes<bke::pbvh::MeshNode>();
    nodes_mask.foreach_index(
        [&](const int i) { bke::pbvh::node_update_mask_mesh(masks.span, nodes[i]); });
    masks.finish();
  }
  else if (pbvh.type() == bke::pbvh::Type::Grids) {
    ss.subdiv_ccg->masks.as_mutable_span().copy_from(result);
    MutableSpan nodes = pbvh.nodes<bke::pbvh::GridsNode>();
    nodes_mask.foreach_index([&](const int i) {
      bke::pbvh::node_update_mask_grids(key, ss.subdiv_ccg->masks, nodes[i]);
    });
  }
  else {
    for (const int i : result.index_range()) {
      BM_ELEM_CD_SET_FLOAT(ss.bm->vtable[i], bm_mask_offset, result[i]);
    }
    MutableSpan nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
    nodes_mask.foreach_index(
        [&](const int i) { bke::pbvh::node_update_mask_bmesh(bm_mask_offset, nodes[i]); });
  }
  pbvh.tag_masks_changed(nodes_mask);
  undo::push_end(object);
  ed::sculpt_paint::mask_overlay_check(*C, *op);
  flush_update_step(C, UpdateType::Mask);
  flush_update_done(C, object, UpdateType::Mask);
  tag_update_overlays(C);
  return OPERATOR_FINISHED;
}

static void bevel_mask_ui(bContext *C, wmOperator *op)
{
  ui::Layout &layout = *op->layout;
  layout.prop(op->ptr, "radius", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout.prop(op->ptr, "profile", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout.prop(op->ptr, "threshold", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (RNA_enum_get(op->ptr, "profile") == BRUSH_CURVE_CUSTOM) {
    Scene &scene = *CTX_data_scene(C);
    Sculpt &sculpt = *scene.toolsettings->sculpt;
    bevel_curve_ensure(sculpt);
    PointerRNA ptr = RNA_pointer_create_discrete(&scene.id, RNA_Sculpt, &sculpt);
    ui::template_curve_mapping(
        &layout, &ptr, "bevel_mask_curve", 'v', false, false, false, false, false);
  }
}

void SCULPT_OT_bevel_mask(wmOperatorType *ot)
{
  ot->name = "Bevel Mask";
  ot->idname = "SCULPT_OT_bevel_mask";
  ot->description = "Shape the mask transition using a surface radius and falloff profile";
  ot->poll = sculpt_mode_poll;
  ot->exec = bevel_mask_exec;
  ot->ui = bevel_mask_ui;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  RNA_def_float_distance(ot->srna,
                         "radius",
                         0.1f,
                         0.000001f,
                         FLT_MAX,
                         "Radius",
                         "Distance on either side of the mask boundary in object space",
                         0.001f,
                         10.0f);
  RNA_def_enum(ot->srna,
               "profile",
               rna_enum_brush_curve_preset_items,
               BRUSH_CURVE_SMOOTH,
               "Profile",
               "Transition profile; Custom uses the independent Bevel Mask curve");
  RNA_def_float_factor(ot->srna,
                       "threshold",
                       0.5f,
                       0.001f,
                       0.999f,
                       "Boundary",
                       "Mask value defining the center of the bevel",
                       0.01f,
                       0.99f);
}

void SCULPT_OT_mask_filter(wmOperatorType *ot)
{
  ot->name = "Mask Filter";
  ot->idname = "SCULPT_OT_mask_filter";
  ot->description = "Applies a filter to modify the current mask";

  ot->exec = sculpt_mask_filter_exec;
  ot->poll = sculpt_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  static EnumPropertyItem type_items[] = {
      {int(FilterType::Smooth), "SMOOTH", 0, "Smooth Mask", ""},
      {int(FilterType::Sharpen), "SHARPEN", 0, "Sharpen Mask", ""},
      {int(FilterType::Grow), "GROW", 0, "Grow Mask", ""},
      {int(FilterType::Shrink), "SHRINK", 0, "Shrink Mask", ""},
      {int(FilterType::ContrastIncrease), "CONTRAST_INCREASE", 0, "Increase Contrast", ""},
      {int(FilterType::ContrastDecrease), "CONTRAST_DECREASE", 0, "Decrease Contrast", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_enum(ot->srna,
               "filter_type",
               type_items,
               int(FilterType::Smooth),
               "Type",
               "Filter that is going to be applied to the mask");
  RNA_def_int(ot->srna,
              "iterations",
              1,
              1,
              100,
              "Iterations",
              "Number of times that the filter is going to be applied",
              1,
              100);
  RNA_def_boolean(
      ot->srna,
      "auto_iteration_count",
      true,
      "Auto Iteration Count",
      "Use an automatic number of iterations based on the number of vertices of the sculpt");
}

}  // namespace blender::ed::sculpt_paint::mask
