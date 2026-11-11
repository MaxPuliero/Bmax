/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "sculpt_thick.hh"

#include <cfloat>

#include "BLI_kdopbvh.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_object_types.h"

#include "BKE_attribute.hh"
#include "BKE_mesh.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "mesh_brush_common.hh"
#include "sculpt_intern.hh"

namespace blender::ed::sculpt_paint::thick {

struct Surface {
  BVHTree *tree = nullptr;
  Array<float3> positions;
  Array<float3> normals;
  const float3 *source_positions = nullptr;
  const float3 *source_normals = nullptr;
  int64_t source_size = 0;
  float4x4 object_to_world;
  float4x4 world_to_object;

  ~Surface()
  {
    BLI_bvhtree_free(tree);
  }
};

void prepare(Object &object, const Brush &brush)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  if (!(brush.flag & BRUSH_PERSISTENT) ||
      (!brush.use_sculpt_limit_up && !brush.use_sculpt_limit_down) ||
      !BKE_sculpt_has_persistent_base(object))
  {
    ss.thick_surface.reset();
    return;
  }

  const Mesh &mesh = *id_cast<const Mesh *>(object.data);
  const auto attributes = mesh.attributes();
  const VArraySpan<float3> mesh_positions = *attributes.lookup<float3>(".sculpt_persistent_co",
                                                                       bke::AttrDomain::Point);
  const VArraySpan<float3> mesh_normals = *attributes.lookup<float3>(".sculpt_persistent_no",
                                                                     bke::AttrDomain::Point);
  const Span<float3> positions = ss.subdiv_ccg ?
                                     ss.persistent.sculpt_persistent_co.as_span() :
                                     Span<float3>(mesh_positions.data(), mesh_positions.size());
  const Span<float3> normals = ss.subdiv_ccg ?
                                   ss.persistent.sculpt_persistent_no.as_span() :
                                   Span<float3>(mesh_normals.data(), mesh_normals.size());
  if (positions.is_empty() || normals.size() != positions.size()) {
    ss.thick_surface.reset();
    return;
  }
  if (ss.thick_surface && ss.thick_surface->source_positions == positions.data() &&
      ss.thick_surface->source_normals == normals.data() &&
      ss.thick_surface->source_size == positions.size() &&
      equals_m4m4(ss.thick_surface->object_to_world.ptr(), object.object_to_world().ptr()))
  {
    return;
  }

  auto surface = std::make_shared<Surface>();
  surface->source_positions = positions.data();
  surface->source_normals = normals.data();
  surface->source_size = positions.size();
  surface->object_to_world = object.object_to_world();
  surface->world_to_object = math::invert(surface->object_to_world);
  const float3x3 normal_matrix = math::transpose(float3x3(surface->world_to_object));

  /* Nearest tangent planes intentionally approximate the frozen surface. Bound the acceleration
   * structure on dense Multires meshes; the persistent base itself remains full resolution. */
  const int64_t stride = std::max<int64_t>(1, (positions.size() + 999999) / 1000000);
  const int samples = int((positions.size() + stride - 1) / stride);
  surface->positions.reinitialize(samples);
  surface->normals.reinitialize(samples);
  surface->tree = BLI_bvhtree_new(samples, 0.0f, 4, 6);
  for (const int i : IndexRange(samples)) {
    const int64_t source = int64_t(i) * stride;
    surface->positions[i] = math::transform_point(surface->object_to_world, positions[source]);
    surface->normals[i] = math::normalize(normal_matrix * normals[source]);
    BLI_bvhtree_insert(surface->tree, i, surface->positions[i], 1);
  }
  BLI_bvhtree_balance(surface->tree);
  ss.thick_surface = std::move(surface);
}

bool is_active(const SculptSession &ss)
{
  return ss.cache && ss.cache->brush && ss.thick_surface &&
         (ss.cache->brush->flag & BRUSH_PERSISTENT) &&
         (ss.cache->brush->use_sculpt_limit_up || ss.cache->brush->use_sculpt_limit_down);
}

float3 constrain(const SculptSession &ss, const float3 &before, const float3 &proposed)
{
  if (!is_active(ss) || before == proposed) {
    return proposed;
  }
  const Surface &surface = *ss.thick_surface;
  const Brush &brush = *ss.cache->brush;
  const bool invert = ss.cache->toggle_settings.invert;
  const bool up = invert ? brush.use_sculpt_limit_down : brush.use_sculpt_limit_up;
  const bool down = invert ? brush.use_sculpt_limit_up : brush.use_sculpt_limit_down;
  const float up_distance = invert ? brush.sculpt_limit_down : brush.sculpt_limit_up;
  const float down_distance = invert ? brush.sculpt_limit_up : brush.sculpt_limit_down;
  float3 result = math::transform_point(surface.object_to_world, proposed);
  const float3 previous = math::transform_point(surface.object_to_world, before);
  /* AABB lower bounds include normal distance as well as tangential distance. Unlike an
   * axis-only KD search, this remains efficient above an almost planar reference surface. */
  BVHTreeNearest nearest{};
  nearest.index = -1;
  nearest.dist_sq = FLT_MAX;
  const int index = BLI_bvhtree_find_nearest(surface.tree, result, &nearest, nullptr, nullptr);
  if (index == -1) {
    return proposed;
  }
  const float3 normal = surface.normals[index];
  const float3 origin = surface.positions[index];
  const float distance = math::dot(result - origin, normal);
  const float previous_distance = math::dot(previous - origin, normal);
  float target = distance;
  if (up) {
    target = std::min(target, std::max(up_distance, previous_distance));
  }
  if (down) {
    target = std::max(target, std::min(-down_distance, previous_distance));
  }
  if (target == distance) {
    return proposed;
  }
  result += normal * (target - distance);
  return math::transform_point(surface.world_to_object, result);
}

NodePositions capture(const Depsgraph &depsgraph, const Object &object, const IndexMask &node_mask)
{
  NodePositions result;
  const SculptSession &ss = *object.runtime->sculpt_session;
  if (!is_active(ss)) {
    return result;
  }
  result.positions.resize(node_mask.size());
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  if (pbvh.type() == bke::pbvh::Type::Mesh) {
    const Span<float3> positions = bke::pbvh::vert_positions_eval(depsgraph, object);
    const auto nodes = pbvh.nodes<bke::pbvh::MeshNode>();
    node_mask.foreach_index(
        [&](const int node, const int pos) {
          const Span<int> verts = nodes[node].verts();
          result.positions[pos].reinitialize(verts.size());
          for (const int i : verts.index_range()) {
            result.positions[pos][i] = positions[verts[i]];
          }
        },
        exec_mode::grain_size(1));
  }
  else if (pbvh.type() == bke::pbvh::Type::Grids) {
    const SubdivCCG &ccg = *ss.subdiv_ccg;
    const auto nodes = pbvh.nodes<bke::pbvh::GridsNode>();
    node_mask.foreach_index(
        [&](const int node, const int pos) {
          const Span<int> grids = nodes[node].grids();
          result.positions[pos].reinitialize(grids.size() * ccg.grid_area);
          gather_data_grids(
              ccg, ccg.positions.as_span(), grids, result.positions[pos].as_mutable_span());
        },
        exec_mode::grain_size(1));
  }
  return result;
}

void apply(const Depsgraph &depsgraph,
           const Sculpt &sd,
           Object &object,
           const IndexMask &node_mask,
           const NodePositions &before)
{
  if (before.positions.is_empty()) {
    return;
  }
  SculptSession &ss = *object.runtime->sculpt_session;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  if (pbvh.type() == bke::pbvh::Type::Mesh) {
    const PositionDeformData data(depsgraph, object);
    auto nodes = pbvh.nodes<bke::pbvh::MeshNode>();
    node_mask.foreach_index(
        [&](const int node, const int pos) {
          const Span<int> verts = nodes[node].verts();
          Array<float3> translations(verts.size());
          for (const int i : verts.index_range()) {
            const float3 current = data.eval[verts[i]];
            translations[i] = constrain(ss, before.positions[pos][i], current) - current;
          }
          clip_and_lock_translations(sd, ss, data.eval, verts, translations);
          data.deform(translations, verts);
          bke::pbvh::update_node_bounds_mesh(data.eval, nodes[node]);
        },
        exec_mode::grain_size(1));
  }
  else if (pbvh.type() == bke::pbvh::Type::Grids) {
    SubdivCCG &ccg = *ss.subdiv_ccg;
    auto nodes = pbvh.nodes<bke::pbvh::GridsNode>();
    node_mask.foreach_index(
        [&](const int node, const int pos) {
          int offset = 0;
          for (const int grid : nodes[node].grids()) {
            for (const int i : IndexRange(ccg.grid_area)) {
              float3 &current = ccg.positions[grid * ccg.grid_area + i];
              const float3 translation = constrain(ss, before.positions[pos][offset++], current) -
                                         current;
              float3 clipped = translation;
              clip_and_lock_translations(
                  sd, ss, Span<float3>(&current, 1), MutableSpan<float3>(&clipped, 1));
              current += clipped;
            }
          }
          bke::pbvh::update_node_bounds_grids(ccg.grid_area, ccg.positions, nodes[node]);
        },
        exec_mode::grain_size(1));
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

}  // namespace blender::ed::sculpt_paint::thick
