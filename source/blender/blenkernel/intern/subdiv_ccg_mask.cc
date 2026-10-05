/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cmath>

#include "BKE_subdiv_ccg.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_subdiv.hh"
#include "BLI_color.hh"
#include "BLI_math_vector.hh"

#include "MEM_guardedalloc.h"

namespace blender {

using namespace blender::bke::subdiv;

struct PolyCornerIndex {
  int face_index;
  int corner;
};

struct GridPaintMaskData {
  // int grid_size;
  OffsetIndices<int> faces;
  const GridPaintMask *grid_paint_mask;
  /* Indexed by ptex face index, contains face/corner which corresponds
   * to it.
   *
   * NOTE: For quad face this is an index of first corner only, since
   * there we only have one ptex.
   */
  PolyCornerIndex *ptex_face_corner;
};

static int mask_get_grid_and_coord(SubdivCCGMaskEvaluator *mask_evaluator,
                                   const int ptex_face_index,
                                   const float u,
                                   const float v,
                                   const GridPaintMask **r_mask_grid,
                                   float *grid_u,
                                   float *grid_v)
{
  GridPaintMaskData *data = static_cast<GridPaintMaskData *>(mask_evaluator->user_data);
  const PolyCornerIndex *poly_corner = &data->ptex_face_corner[ptex_face_index];
  const IndexRange face = data->faces[poly_corner->face_index];
  const int start_grid_index = face.start() + poly_corner->corner;
  int corner = 0;
  if (face.size() == 4) {
    float corner_u, corner_v;
    corner = rotate_quad_to_corner(u, v, &corner_u, &corner_v);
    *r_mask_grid = &data->grid_paint_mask[start_grid_index + corner];
    ptex_face_uv_to_grid_uv(corner_u, corner_v, grid_u, grid_v);
  }
  else {
    *r_mask_grid = &data->grid_paint_mask[start_grid_index];
    ptex_face_uv_to_grid_uv(u, v, grid_u, grid_v);
  }
  return corner;
}

BLI_INLINE float read_mask_grid(const GridPaintMask *mask_grid,
                                const float grid_u,
                                const float grid_v)
{
  if (mask_grid->data == nullptr) {
    return 0;
  }
  const int grid_size = grid_size_from_level(mask_grid->level);
  const int x = roundf(grid_u * (grid_size - 1));
  const int y = roundf(grid_v * (grid_size - 1));
  return mask_grid->data[y * grid_size + x];
}

static float eval_mask(SubdivCCGMaskEvaluator *mask_evaluator,
                       const int ptex_face_index,
                       const float u,
                       const float v)
{
  const GridPaintMask *mask_grid;
  float grid_u, grid_v;
  mask_get_grid_and_coord(mask_evaluator, ptex_face_index, u, v, &mask_grid, &grid_u, &grid_v);
  return read_mask_grid(mask_grid, grid_u, grid_v);
}

static void free_mask_data(SubdivCCGMaskEvaluator *mask_evaluator)
{
  GridPaintMaskData *data = static_cast<GridPaintMaskData *>(mask_evaluator->user_data);
  MEM_delete(data->ptex_face_corner);
  MEM_delete(data);
}

/* TODO(sergey): This seems to be generally used information, which almost
 * worth adding to a subdiv itself, with possible cache of the value.
 */
static int count_num_ptex_faces(const Mesh *mesh)
{
  int num_ptex_faces = 0;
  const OffsetIndices faces = mesh->faces();
  for (const int face_index : faces.index_range()) {
    num_ptex_faces += (faces[face_index].size() == 4) ? 1 : faces[face_index].size();
  }
  return num_ptex_faces;
}

static void mask_data_init_mapping(SubdivCCGMaskEvaluator *mask_evaluator, const Mesh *mesh)
{
  GridPaintMaskData *data = static_cast<GridPaintMaskData *>(mask_evaluator->user_data);
  const OffsetIndices faces = mesh->faces();
  const int num_ptex_faces = count_num_ptex_faces(mesh);
  /* Allocate memory. */
  data->ptex_face_corner = MEM_new_array_uninitialized<PolyCornerIndex>(size_t(num_ptex_faces),
                                                                        __func__);
  /* Fill in offsets. */
  int ptex_face_index = 0;
  PolyCornerIndex *ptex_face_corner = data->ptex_face_corner;
  for (const int face_index : faces.index_range()) {
    const IndexRange face = faces[face_index];
    if (face.size() == 4) {
      ptex_face_corner[ptex_face_index].face_index = face_index;
      ptex_face_corner[ptex_face_index].corner = 0;
      ptex_face_index++;
    }
    else {
      for (int corner = 0; corner < face.size(); corner++) {
        ptex_face_corner[ptex_face_index].face_index = face_index;
        ptex_face_corner[ptex_face_index].corner = corner;
        ptex_face_index++;
      }
    }
  }
}

static void mask_init_data(SubdivCCGMaskEvaluator *mask_evaluator, const Mesh *mesh)
{
  GridPaintMaskData *data = static_cast<GridPaintMaskData *>(mask_evaluator->user_data);
  data->faces = mesh->faces();
  data->grid_paint_mask = static_cast<const GridPaintMask *>(
      CustomData_get_layer(&mesh->corner_data, CD_GRID_PAINT_MASK));
  mask_data_init_mapping(mask_evaluator, mesh);
}

static void mask_init_functions(SubdivCCGMaskEvaluator *mask_evaluator)
{
  mask_evaluator->eval_mask = eval_mask;
  mask_evaluator->free = free_mask_data;
}

bool BKE_subdiv_ccg_mask_init_from_paint(SubdivCCGMaskEvaluator *mask_evaluator, const Mesh *mesh)
{
  if (!CustomData_get_layer(&mesh->corner_data, CD_GRID_PAINT_MASK)) {
    return false;
  }
  /* Allocate all required memory. */
  mask_evaluator->user_data = MEM_new<GridPaintMaskData>("mask from grid data");
  mask_init_data(mask_evaluator, mesh);
  mask_init_functions(mask_evaluator);
  return true;
}

}  // namespace blender

namespace blender {

static float4 color_grid_sample(const float *data, const int size, float u, float v)
{
  const float x = math::clamp(u, 0.0f, 1.0f) * (size - 1);
  const float y = math::clamp(v, 0.0f, 1.0f) * (size - 1);
  const int x0 = int(x), y0 = int(y);
  const int x1 = std::min(x0 + 1, size - 1), y1 = std::min(y0 + 1, size - 1);
  const auto *colors = reinterpret_cast<const float4 *>(data);
  return math::interpolate(
      math::interpolate(colors[y0 * size + x0], colors[y0 * size + x1], x - x0),
      math::interpolate(colors[y1 * size + x0], colors[y1 * size + x1], x - x0),
      y - y0);
}

float4 BKE_subdiv_ccg_color_sample(
    const Mesh &mesh, const StringRef name, const int grid, const float u, const float v)
{
  const auto *grids = static_cast<const GridPaintColor *>(
      CustomData_get_layer_named(&mesh.corner_data, CD_GRID_PAINT_COLOR, name));
  if (grids && grids[grid].data && grids[grid].level > 0) {
    return color_grid_sample(grids[grid].data, grid_size_from_level(grids[grid].level), u, v);
  }
  const auto attr = mesh.attributes().lookup<ColorGeometry4f>(name);
  if (!attr) {
    return float4(1.0f);
  }
  const IndexRange face = mesh.faces()[mesh.corner_to_face_map()[grid]];
  const int local = grid - face.start();
  const auto get = [&](const int corner) -> float4 {
    return float4(
        attr.varray[attr.domain == bke::AttrDomain::Point ? mesh.corner_verts()[corner] : corner]);
  };
  float4 center(0);
  for (const int corner : face) {
    center += get(corner);
  }
  center /= face.size();
  const float4 vertex = get(grid);
  const float4 next = (vertex + get(face.start() + (local + 1) % face.size())) * 0.5f;
  const float4 prev = (vertex + get(face.start() + (local + face.size() - 1) % face.size())) *
                      0.5f;
  /* Grid (0,0) is the face center, (1,1) the base vertex. */
  return math::interpolate(
      math::interpolate(center, next, u), math::interpolate(prev, vertex, u), v);
}

float4 BKE_subdiv_ccg_color_sample_ptex(const Mesh &mesh,
                                        const StringRef name,
                                        const int face_index,
                                        int corner,
                                        const float u,
                                        const float v)
{
  float gu, gv;
  if (mesh.faces()[face_index].size() == 4) {
    float cu, cv;
    corner = bke::subdiv::rotate_quad_to_corner(u, v, &cu, &cv);
    bke::subdiv::ptex_face_uv_to_grid_uv(cu, cv, &gu, &gv);
  }
  else {
    bke::subdiv::ptex_face_uv_to_grid_uv(u, v, &gu, &gv);
  }
  return BKE_subdiv_ccg_color_sample(
      mesh, name, mesh.faces()[face_index].start() + corner, gu, gv);
}

void BKE_subdiv_ccg_colors_ensure(const Mesh &mesh, SubdivCCG &ccg, const StringRef name)
{
  if (ccg.color_name == name && ccg.colors.size() == ccg.positions.size()) {
    return;
  }
  ccg.color_name = name;
  ccg.colors.reinitialize(ccg.positions.size());
  for (const int grid : IndexRange(ccg.grids_num)) {
    for (int y = 0; y < ccg.grid_size; y++) {
      for (int x = 0; x < ccg.grid_size; x++) {
        ccg.colors[grid * ccg.grid_area + y * ccg.grid_size + x] = BKE_subdiv_ccg_color_sample(
            mesh, name, grid, float(x) / (ccg.grid_size - 1), float(y) / (ccg.grid_size - 1));
      }
    }
  }
}

void BKE_subdiv_ccg_colors_storage_ensure(Mesh &mesh,
                                          const SubdivCCG &ccg,
                                          const Span<int> indices)
{
  auto *grids = static_cast<GridPaintColor *>(CustomData_get_layer_named_for_write(
      &mesh.corner_data, CD_GRID_PAINT_COLOR, ccg.color_name, mesh.corners_num));
  if (!grids) {
    grids = static_cast<GridPaintColor *>(CustomData_add_layer_named(
        &mesh.corner_data, CD_GRID_PAINT_COLOR, CD_CONSTRUCT, mesh.corners_num, ccg.color_name));
  }
  for (const int grid : indices) {
    if (!grids[grid].data) {
      grids[grid].data = MEM_new_array_uninitialized<float>(ccg.grid_area * 4, __func__);
      grids[grid].level = ccg.level;
      std::copy_n(ccg.colors.data() + grid * ccg.grid_area,
                  ccg.grid_area,
                  reinterpret_cast<float4 *>(grids[grid].data));
    }
  }
}

void BKE_subdiv_ccg_colors_store(Mesh &mesh, const SubdivCCG &ccg, const Span<int> indices)
{
  if (ccg.colors.is_empty() || ccg.grids_num != mesh.corners_num ||
      !mesh.attributes().contains(ccg.color_name))
  {
    return;
  }
  auto *grids = static_cast<GridPaintColor *>(CustomData_get_layer_named_for_write(
      &mesh.corner_data, CD_GRID_PAINT_COLOR, ccg.color_name, mesh.corners_num));
  if (!grids) {
    grids = static_cast<GridPaintColor *>(CustomData_add_layer_named(
        &mesh.corner_data, CD_GRID_PAINT_COLOR, CD_CONSTRUCT, mesh.corners_num, ccg.color_name));
  }
  for (const int grid : indices) {
    GridPaintColor &dst = grids[grid];
    const int level = std::max(int(dst.level), ccg.level);
    const int size = grid_size_from_level(level);
    const Span<float4> current = ccg.colors.as_span().slice(grid * ccg.grid_area, ccg.grid_area);
    if (dst.data && int(dst.level) == ccg.level) {
      std::copy_n(current.data(), current.size(), reinterpret_cast<float4 *>(dst.data));
      continue;
    }
    float *data = MEM_new_array_uninitialized<float>(size * size * 4, __func__);
    auto *colors = reinterpret_cast<float4 *>(data);

    if (dst.data && int(dst.level) > ccg.level) {
      Array<float4> old_low(ccg.grid_area);
      const int old_size = grid_size_from_level(dst.level);
      for (int y = 0; y < ccg.grid_size; y++) {
        for (int x = 0; x < ccg.grid_size; x++) {
          old_low[y * ccg.grid_size + x] = color_grid_sample(
              dst.data, old_size, float(x) / (ccg.grid_size - 1), float(y) / (ccg.grid_size - 1));
        }
      }
      for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
          const float u = float(x) / (size - 1), v = float(y) / (size - 1);
          colors[y * size + x] = math::clamp(
              color_grid_sample(dst.data, old_size, u, v) +
                  color_grid_sample(
                      reinterpret_cast<const float *>(current.data()), ccg.grid_size, u, v) -
                  color_grid_sample(
                      reinterpret_cast<const float *>(old_low.data()), ccg.grid_size, u, v),
              0.0f,
              1.0f);
        }
      }
    }
    else {
      for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
          colors[y * size + x] = color_grid_sample(reinterpret_cast<const float *>(current.data()),
                                                   ccg.grid_size,
                                                   float(x) / (size - 1),
                                                   float(y) / (size - 1));
        }
      }
    }
    MEM_SAFE_DELETE(dst.data);
    dst.data = data;
    dst.level = level;
  }
  BKE_subdiv_ccg_colors_sync_base(mesh, ccg.color_name);
}

Vector<int> BKE_subdiv_ccg_colors_stitch(SubdivCCG &ccg, const Span<int> grids)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(ccg);
  Set<int> visited;
  Set<int> changed;
  const auto stitch = [&](const SubdivCCGCoord coord) {
    SubdivCCGNeighbors neighbors;
    BKE_subdiv_ccg_neighbor_coords_get(ccg, coord, true, neighbors);
    const int index = coord.to_index(key);
    int canonical = index;
    for (const SubdivCCGCoord &duplicate : neighbors.duplicates()) {
      canonical = std::min(canonical, duplicate.to_index(key));
    }
    /* Duplicate groups are disjoint. Averaging each group once needs no surface snapshot. */
    if (!visited.add(canonical)) {
      return;
    }
    float4 sum = ccg.colors[index];
    for (const SubdivCCGCoord &duplicate : neighbors.duplicates()) {
      sum += ccg.colors[duplicate.to_index(key)];
    }
    const float4 value = sum / (neighbors.num_duplicates + 1);
    ccg.colors[index] = value;
    for (const SubdivCCGCoord &duplicate : neighbors.duplicates()) {
      ccg.colors[duplicate.to_index(key)] = value;
      changed.add(duplicate.grid_index);
    }
  };
  for (const int grid : grids) {
    changed.add(grid);
    for (int i = 0; i < key.grid_size; i++) {
      stitch({grid, short(i), 0});
      stitch({grid, short(i), short(key.grid_size - 1)});
    }
    for (int i = 1; i < key.grid_size - 1; i++) {
      stitch({grid, 0, short(i)});
      stitch({grid, short(key.grid_size - 1), short(i)});
    }
  }
  Vector<int> result;
  for (const int grid : changed) {
    result.append(grid);
  }
  return result;
}

}  // namespace blender

namespace blender {

void BKE_subdiv_ccg_colors_sync_base(Mesh &mesh, const StringRef name)
{
  bke::GSpanAttributeWriter attribute = mesh.attributes_for_write().lookup_for_write_span(name);
  if (!attribute) {
    return;
  }
  const bool is_float = attribute.span.type().is<ColorGeometry4f>();
  if (!is_float && !attribute.span.type().is<ColorGeometry4b>()) {
    attribute.finish();
    return;
  }
  const bool point = attribute.domain == bke::AttrDomain::Point;
  Array<float4> sums(point ? mesh.verts_num : mesh.corners_num, float4(0));
  Array<int> counts(sums.size(), 0);
  for (const int corner : IndexRange(mesh.corners_num)) {
    const int index = point ? mesh.corner_verts()[corner] : corner;
    sums[index] += BKE_subdiv_ccg_color_sample(mesh, name, corner, 1.0f, 1.0f);
    counts[index]++;
  }
  for (const int i : sums.index_range()) {
    if (counts[i] == 0) {
      continue;
    }
    const ColorGeometry4f value(sums[i] / counts[i]);
    if (is_float) {
      attribute.span.typed<ColorGeometry4f>()[i] = value;
    }
    else {
      attribute.span.typed<ColorGeometry4b>()[i] = color::encode(value);
    }
  }
  attribute.finish();
}

void BKE_subdiv_ccg_colors_downsample(Mesh &mesh, const int level)
{
  if (level <= 0) {
    return;
  }
  Vector<std::string> names;
  for (const CustomDataLayer &layer : Span(mesh.corner_data.layers, mesh.corner_data.totlayer)) {
    if (layer.type == CD_GRID_PAINT_COLOR) {
      names.append(layer.name);
    }
  }
  for (const std::string &name : names) {
    auto *grids = static_cast<GridPaintColor *>(CustomData_get_layer_named_for_write(
        &mesh.corner_data, CD_GRID_PAINT_COLOR, name, mesh.corners_num));
    const int size = grid_size_from_level(level);
    for (GridPaintColor &grid : MutableSpan(grids, mesh.corners_num)) {
      if (!grid.data || int(grid.level) <= level) {
        continue;
      }
      const int old_size = grid_size_from_level(grid.level);
      float *data = MEM_new_array_uninitialized<float>(size * size * 4, __func__);
      auto *colors = reinterpret_cast<float4 *>(data);
      for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
          colors[y * size + x] = color_grid_sample(
              grid.data, old_size, float(x) / (size - 1), float(y) / (size - 1));
        }
      }
      MEM_SAFE_DELETE(grid.data);
      grid.data = data;
      grid.level = level;
    }
  }
}

}  // namespace blender
