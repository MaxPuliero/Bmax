/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 * Fixed-topology, mask-aware symmetry of mesh and Multires geometry.
 */

#include <algorithm>
#include <utility>

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_types.hh"
#include "BKE_modifier.hh"
#include "BKE_multires.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_sculpt_layers_mesh.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_ccg.hh"
#include "BKE_subdiv_eval.hh"
#include "BLI_array.hh"
#include "BLI_bitmap.hh"
#include "BLI_color.hh"
#include "BLI_kdtree.hh"
#include "BLI_listbase.hh"
#include "BLI_math_vector.hh"
#include "DEG_depsgraph.hh"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "ED_mesh.hh"
#include "ED_sculpt.hh"
#include "ED_undo.hh"
#include "RNA_access.hh"
#include "RNA_define.hh"
#include "WM_api.hh"
#include "WM_types.hh"
#include "sculpt_intern.hh"
#include "sculpt_undo.hh"

namespace blender::ed::sculpt_paint {

static bool mirrored_corners(const Mesh &mesh, const Span<int> vertices, MutableSpan<int> corners)
{
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face = mesh.vert_to_face_map();
  corners.fill(-1);
  for (const int face_i : faces.index_range()) {
    const IndexRange face = faces[face_i];
    bool mapped = true;
    for (const int corner : face) {
      const int vertex = corner_verts[corner];
      const int other = vertices[vertex];
      if (other < 0 || other >= vertices.size() || vertices[other] != vertex) {
        mapped = false;
        break;
      }
    }
    if (!mapped) {
      continue;
    }
    const int v = vertices[corner_verts[face.start()]];
    int match = -1;
    int rotation = -1;
    bool ambiguous = false;
    for (const int candidate : vert_to_face[v]) {
      const IndexRange other = faces[candidate];
      if (other.size() != face.size()) {
        continue;
      }
      for (const int k : other.index_range()) {
        if (corner_verts[other[k]] != v) {
          continue;
        }
        bool equal = true;
        for (const int j : face.index_range()) {
          if (vertices[corner_verts[face[j]]] !=
              corner_verts[other[(k - j + face.size()) % face.size()]])
          {
            equal = false;
            break;
          }
        }
        if (equal) {
          if (match != -1) {
            ambiguous = true; /* Skip duplicate faces without rejecting unrelated regions. */
          }
          match = candidate;
          rotation = k;
        }
      }
    }
    if (match == -1 || ambiguous) {
      continue;
    }
    for (const int j : face.index_range()) {
      corners[face[j]] = faces[match][(rotation - j + face.size()) % face.size()];
    }
  }
  bool any = false;
  for (const int c : corners.index_range()) {
    if (corners[c] >= 0 && corners[corners[c]] != c) {
      corners[c] = -1;
    }
    any |= corners[c] >= 0;
  }
  return any;
}

/** Resolve coincident vertices from their connectivity, never from KD-tree tie ordering. */
static void spatial_symmetry_vertices(const Mesh &mesh,
                                      const int axis,
                                      const float tolerance,
                                      MutableSpan<int> vertices)
{
  const Span<float3> positions = mesh.vert_positions();
  Array<Vector<int>> candidates(mesh.verts_num);
  KDTree<float3> *tree = kdtree_new<float3>(mesh.verts_num);
  for (const int i : positions.index_range()) {
    kdtree_insert(tree, i, positions[i]);
  }
  kdtree_balance(tree);
  bool ambiguous = false;
  for (const int i : positions.index_range()) {
    float3 reflected = positions[i];
    reflected[axis] *= -1;
    KDTreeNearest<float3> nearest[2];
    const int count = kdtree_find_nearest_n(tree, reflected, nearest, 2);
    if (count == 0 || nearest[0].dist > tolerance) {
      continue;
    }
    if (count > 1 && nearest[1].dist <= tolerance) {
      ambiguous = true;
      /* An excessively broad tolerance must not create a quadratic candidate matrix.
       * Leave such cases to the native topology fallback instead of truncating a match. */
      bool overflow = false;
      kdtree_range_search_cb(
          tree, reflected, tolerance, [&](int candidate, const float3 &, float) {
            if (candidates[i].size() == 64) {
              overflow = true;
              return false;
            }
            candidates[i].append(candidate);
            return true;
          });
      if (overflow) {
        candidates[i].clear();
      }
    }
    else {
      candidates[i].append(nearest[0].index);
    }
  }
  kdtree_free(tree);
  if (ambiguous) {
    Array<Vector<int>> neighbors(mesh.verts_num);
    for (const int2 edge : mesh.edges()) {
      neighbors[edge[0]].append(edge[1]);
      neighbors[edge[1]].append(edge[0]);
    }
    const GroupedSpan<int> vert_to_face = mesh.vert_to_face_map();
    /* Arc consistency propagates the unique spatial matches through the ambiguous groups.
     * For example Suzanne's coincident eyelid/head vertices connect to different neighbors. */
    bool changed;
    do {
      changed = false;
      for (const int v : positions.index_range()) {
        Vector<int> &choices = candidates[v];
        if (choices.size() <= 1) {
          continue;
        }
        for (int j = choices.size() - 1; j >= 0; j--) {
          const int other = choices[j];
          bool compatible = neighbors[v].size() == neighbors[other].size() &&
                            vert_to_face[v].size() == vert_to_face[other].size() &&
                            candidates[other].contains(v);
          if (compatible) {
            for (const int neighbor : neighbors[v]) {
              if (candidates[neighbor].is_empty()) {
                continue;
              }
              bool has_match = false;
              for (const int candidate : candidates[neighbor]) {
                if (neighbors[other].contains(candidate)) {
                  has_match = true;
                  break;
                }
              }
              if (!has_match) {
                compatible = false;
                break;
              }
            }
          }
          if (!compatible) {
            choices.remove_and_reorder(j);
            changed = true;
          }
        }
      }
    } while (changed);
  }
  for (const int v : positions.index_range()) {
    vertices[v] = candidates[v].size() == 1 ? candidates[v][0] : -1;
  }
}

/** Extend reliable seeds through uniquely matching reversed faces. Unlike proximity alone,
 * this can recover sculpted vertices on regular topology that native topology hashing cannot
 * distinguish. Never choose between multiple compatible faces. */
static void propagate_symmetry_vertices(const Mesh &mesh, MutableSpan<int> vertices)
{
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face = mesh.vert_to_face_map();
  bool changed;
  do {
    changed = false;
    for (const int face_i : faces.index_range()) {
      const IndexRange face = faces[face_i];
      bool complete = true;
      for (const int c : face) {
        complete &= vertices[corner_verts[c]] >= 0;
      }
      if (complete) {
        continue;
      }
      for (const int anchor : face.index_range()) {
        const int a = corner_verts[face[anchor]];
        const int b = corner_verts[face[(anchor + 1) % face.size()]];
        if (vertices[a] < 0 || vertices[b] < 0) {
          continue;
        }
        int match = -1, rotation = -1;
        bool ambiguous = false;
        for (const int candidate : vert_to_face[vertices[a]]) {
          const IndexRange other = faces[candidate];
          if (other.size() != face.size()) {
            continue;
          }
          for (const int k : other.index_range()) {
            if (corner_verts[other[k]] != vertices[a] ||
                corner_verts[other[(k - 1 + face.size()) % face.size()]] != vertices[b])
            {
              continue;
            }
            bool valid = true;
            for (const int j : face.index_range()) {
              const int v = corner_verts[face[(anchor + j) % face.size()]];
              const int w = corner_verts[other[(k - j + face.size()) % face.size()]];
              if ((vertices[v] >= 0 && vertices[v] != w) || (vertices[w] >= 0 && vertices[w] != v))
              {
                valid = false;
                break;
              }
              /* Proposed pairs must themselves be reciprocal if the faces overlap. */
              for (const int l : face.index_range()) {
                if (corner_verts[face[(anchor + l) % face.size()]] == w &&
                    corner_verts[other[(k - l + face.size()) % face.size()]] != v)
                {
                  valid = false;
                  break;
                }
              }
            }
            if (valid) {
              ambiguous |= match >= 0;
              match = candidate;
              rotation = k;
            }
          }
        }
        if (match < 0 || ambiguous) {
          continue;
        }
        for (const int j : face.index_range()) {
          const int v = corner_verts[face[(anchor + j) % face.size()]];
          const int w = corner_verts[faces[match][(rotation - j + face.size()) % face.size()]];
          vertices[v] = w;
          vertices[w] = v;
        }
        changed = true;
        break;
      }
    }
  } while (changed);
}

static bool symmetry_map(Object &object,
                         const int axis,
                         const float tolerance,
                         MutableSpan<int> corners)
{
  const Mesh &mesh = *id_cast<Mesh *>(object.data);
  const Span<float3> positions = mesh.vert_positions();
  Array<int> vertices(mesh.verts_num, -1);
  spatial_symmetry_vertices(mesh, axis, tolerance, vertices);
  for (const int v : vertices.index_range()) {
    if (vertices[v] >= 0 && vertices[vertices[v]] != v) {
      vertices[v] = -1;
    }
  }
  bool complete = true;
  for (const int v : vertices.index_range()) {
    complete &= vertices[v] >= 0 && vertices[vertices[v]] == v;
  }
  if (!complete) {
    /* Fill only unresolved pairs. Never replace a spatial/connectivity match. */
    Array<int> topology(mesh.verts_num, -1);
    ED_mesh_mirror_topo_table_begin(&object, nullptr);
    for (const int i : positions.index_range()) {
      topology[i] = mesh_get_x_mirror_vert(&object, nullptr, i, true);
    }
    ED_mesh_mirror_topo_table_end(&object);
    for (const int i : positions.index_range()) {
      const int other = topology[i];
      if (vertices[i] < 0 && other >= 0 && other < vertices.size() && topology[other] == i &&
          vertices[other] < 0 &&
          (other == i || positions[i][axis] * positions[other][axis] <= tolerance * tolerance))
      {
        vertices[i] = other;
        vertices[other] = i;
      }
    }
  }
  propagate_symmetry_vertices(mesh, vertices);
  return mirrored_corners(mesh, vertices, corners);
}

static bool symmetrize_blend_poll(bContext *C)
{
  if (!sculpt_mode_poll_view3d(C)) {
    return false;
  }
  Object *object = CTX_data_active_object(C);
  return ID_IS_EDITABLE(object->data) && !ID_IS_OVERRIDE_LIBRARY(object->data);
}

/** Sparse updates in scene-linear RGBA; the original color storage type/domain is retained. */
struct SymmetryColors {
  std::string name;
  Vector<int> indices;
  Vector<ColorGeometry4f> values;

  void apply(Mesh &mesh) const
  {
    bke::GSpanAttributeWriter attribute = mesh.attributes_for_write().lookup_for_write_span(name);
    if (attribute.span.type().is<ColorGeometry4f>()) {
      MutableSpan<ColorGeometry4f> dst = attribute.span.typed<ColorGeometry4f>();
      for (const int i : indices.index_range()) {
        dst[indices[i]] = values[i];
      }
    }
    else {
      MutableSpan<ColorGeometry4b> dst = attribute.span.typed<ColorGeometry4b>();
      for (const int i : indices.index_range()) {
        dst[indices[i]] = blender::color::encode(values[i]);
      }
    }
    attribute.finish();
  }
};

template<typename Mobility>
static SymmetryColors mesh_symmetry_colors(const Mesh &mesh,
                                           const Span<int> vertices,
                                           const Span<int> corners,
                                           const float strength,
                                           const Mobility &mobility)
{
  SymmetryColors result;
  if (!mesh.active_color_attribute) {
    return result;
  }
  const bke::AttributeAccessor attributes = mesh.attributes();
  const auto meta = attributes.lookup_meta_data(mesh.active_color_attribute);
  if (!meta || !ELEM(meta->data_type, bke::AttrType::ColorFloat, bke::AttrType::ColorByte) ||
      !ELEM(meta->domain, bke::AttrDomain::Point, bke::AttrDomain::Corner))
  {
    return result;
  }
  result.name = mesh.active_color_attribute;
  const auto colors = attributes.lookup<ColorGeometry4f>(result.name);
  const bool point = meta->domain == bke::AttrDomain::Point;
  const Span<int> mirror = point ? vertices : corners;
  const Span<int> corner_verts = mesh.corner_verts();
  auto append = [&](const int index, const float4 &value) {
    const ColorGeometry4f original = colors.varray[index];
    const ColorGeometry4f updated(value);
    const bool changed = meta->data_type == bke::AttrType::ColorByte ?
                             blender::color::encode(original) != blender::color::encode(updated) :
                             math::distance_squared(float4(original), value) > 0.0f;
    if (changed) {
      result.indices.append(index);
      result.values.append(updated);
    }
  };
  for (const int a : mirror.index_range()) {
    const int b = mirror[a];
    if (b < a || mirror[b] != a) {
      continue;
    }
    const float am = mobility(point ? a : corner_verts[a]);
    const float bm = mobility(point ? b : corner_verts[b]);
    if (am + bm == 0.0f) {
      continue;
    }
    const float4 ca(colors.varray[a]), cb(colors.varray[b]);
    const float4 target = (ca * bm + cb * am) / (am + bm);
    append(a, ca + (target - ca) * (strength * am));
    if (a != b) {
      append(b, cb + (target - cb) * (strength * bm));
    }
  }
  return result;
}

static wmOperatorStatus symmetrize_mesh_exec(bContext *C, wmOperator *op)
{
  Object &object = *CTX_data_active_object(C);
  Mesh &mesh = *id_cast<Mesh *>(object.data);
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  BKE_sculptsession_update_for_edit(&depsgraph, &object, false);
  bke::pbvh::Tree &tree = bke::object::pbvh_ensure(depsgraph, object);
  if (tree.type() != bke::pbvh::Type::Mesh) {
    BKE_report(op->reports, RPT_INFO, "Symmetrize Blend requires fixed mesh topology");
    return OPERATOR_FINISHED;
  }
  if (!shape_key_check(object, op->reports)) {
    return OPERATOR_CANCELLED;
  }
  if (mesh.sculpt_layer_stack && !bke::sculpt_layers::native_single(mesh)) {
    BKE_report(op->reports, RPT_INFO, "Enable Multires before editing multiple Sculpt Layers");
    return OPERATOR_FINISHED;
  }
  const int axis = RNA_enum_get(op->ptr, "axis");
  const float strength = RNA_float_get(op->ptr, "strength");
  Array<int> corners(mesh.corners_num, -1);
  if (!symmetry_map(object, axis, RNA_float_get(op->ptr, "tolerance"), corners)) {
    BKE_report(op->reports, RPT_INFO, "No mirrored regions found; geometry was left unchanged");
    return OPERATOR_FINISHED;
  }
  Array<int> vertices(mesh.verts_num, -1);
  Array<bool> locked(mesh.verts_num, false);
  bool partial = false;
  const Span<int> corner_verts = mesh.corner_verts();
  for (const int c : corners.index_range()) {
    if (corners[c] < 0) {
      locked[corner_verts[c]] = true; /* Preserve every face in an unmatched region. */
      partial = true;
    }
    else {
      vertices[corner_verts[c]] = corner_verts[corners[c]];
    }
  }
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArray<float> masks = *attributes.lookup_or_default<float>(
      ".sculpt_mask", bke::AttrDomain::Point, 0.0f);
  const VArray<bool> hidden = *attributes.lookup_or_default<bool>(
      ".hide_vert", bke::AttrDomain::Point, false);
  const PositionDeformData positions(depsgraph, object);
  Array<bool> changed(mesh.verts_num, false);
  Vector<int> indices;
  Vector<float3> translations;
  auto mobility = [&](const int v) {
    return locked[v] || hidden[v] ? 0.0f : 1.0f - std::clamp(masks[v], 0.0f, 1.0f);
  };
  auto append = [&](const int v, const float3 &result) {
    const float3 delta = result - positions.eval[v];
    if (math::length_squared(delta) > 0.0f) {
      indices.append(v);
      translations.append(delta);
      changed[v] = true;
    }
  };
  for (const int v : vertices.index_range()) {
    const int w = vertices[v];
    if (w < v || vertices[w] != v) {
      continue;
    }
    const float a_move = mobility(v), b_move = mobility(w);
    if (a_move + b_move == 0.0f) {
      continue;
    }
    const float3 a = positions.eval[v];
    float3 b = positions.eval[w];
    b[axis] *= -1;
    const float3 target = (a * b_move + b * a_move) / (a_move + b_move);
    append(v, a + (target - a) * (strength * a_move));
    if (v != w) {
      float3 result = b + (target - b) * (strength * b_move);
      result[axis] *= -1;
      append(w, result);
    }
  }
  SymmetryColors colors;
  if (RNA_boolean_get(op->ptr, "use_vertex_paint")) {
    colors = mesh_symmetry_colors(mesh, vertices, corners, strength, mobility);
  }
  if (indices.is_empty() && colors.indices.is_empty()) {
    return OPERATOR_FINISHED;
  }
  MutableSpan<bke::pbvh::MeshNode> nodes = tree.nodes<bke::pbvh::MeshNode>();
  IndexMaskMemory memory;
  const IndexMask node_mask = IndexMask::from_predicate(
      nodes.index_range(), memory, [&](const int i) {
        for (const int v : nodes[i].all_verts()) {
          if (changed[v]) {
            return true;
          }
        }
        return false;
      });
  /* Native position/color undo have separate payloads. Group them into one user undo step,
   * retaining native shape-key/deform-modifier handling instead of replacing the mesh. */
  ED_undo_group_begin(C);
  if (!indices.is_empty()) {
    undo::push_begin(*CTX_data_scene(C), object, op);
    undo::push_nodes(depsgraph, object, node_mask, undo::Type::Position);
    positions.deform(translations, indices);
    node_mask.foreach_index(
        [&](const int i) { bke::pbvh::update_node_bounds_mesh(positions.eval, nodes[i]); });
    tree.tag_positions_changed(node_mask);
    tree.flush_bounds_to_parents();
    flush_update_step(C, UpdateType::Position);
    undo::push_end(object);
  }
  if (!colors.indices.is_empty()) {
    Array<bool> color_verts(mesh.verts_num, false);
    const bool point = mesh.attributes().lookup_meta_data(colors.name)->domain ==
                       bke::AttrDomain::Point;
    for (const int i : colors.indices) {
      color_verts[point ? i : corner_verts[i]] = true;
    }
    const IndexMask color_nodes = IndexMask::from_predicate(
        nodes.index_range(), memory, [&](const int i) {
          for (const int v : nodes[i].all_verts()) {
            if (color_verts[v]) {
              return true;
            }
          }
          return false;
        });
    undo::push_begin(*CTX_data_scene(C), object, op);
    undo::push_nodes(depsgraph, object, color_nodes, undo::Type::Color);
    colors.apply(mesh);
    tree.tag_attribute_changed(color_nodes, colors.name);
    flush_update_step(C, UpdateType::Color);
    undo::push_end(object);
  }
  ED_undo_group_end(C);
  object.runtime->sculpt_session->stroke_delta.reset();
  if (!indices.is_empty()) {
    flush_update_done(C, object, UpdateType::Position);
  }
  if (!colors.indices.is_empty()) {
    flush_update_done(C, object, UpdateType::Color);
  }
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, &object);
  if (partial) {
    BKE_report(
        op->reports, RPT_INFO, "Matching regions symmetrized; unmatched regions left unchanged");
  }
  return OPERATOR_FINISHED;
}

static wmOperatorStatus symmetrize_blend_exec(bContext *C, wmOperator *op)
{
  Object &object = *CTX_data_active_object(C);
  Mesh &mesh = *id_cast<Mesh *>(object.data);
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  MultiresModifierData *mmd = BKE_sculpt_multires_active(CTX_data_scene(C), &object);
  const float strength = RNA_float_get(op->ptr, "strength");
  if (strength == 0.0f) {
    return OPERATOR_FINISHED;
  }
  if (object.runtime->sculpt_session->cache) {
    return OPERATOR_CANCELLED;
  }
  if (!mmd || mmd->totlvl < 1 || (mmd->flags & eMultiresModifierFlag_UseSculptBaseMesh)) {
    return symmetrize_mesh_exec(C, op);
  }
  if (mesh.key || ID_REAL_USERS(&mesh.id) > 1 ||
      CustomData_external_test(&mesh.corner_data, CD_MDISPS))
  {
    BKE_report(
        op->reports, RPT_ERROR, "Use a single-user, packed Multires mesh without shape keys");
    return OPERATOR_CANCELLED;
  }
  for (const ModifierData &md : object.modifiers) {
    if ((md.mode & eModifierMode_Realtime) && &md != &mmd->modifier) {
      BKE_report(
          op->reports, RPT_ERROR, "Disable other viewport modifiers before Symmetrize Blend");
      return OPERATOR_CANCELLED;
    }
  }
  if (mesh.sculpt_layer_stack && !bke::sculpt_layers::native_single(mesh)) {
    if (const char *error = bke::sculpt_layers::support_error(object, true)) {
      BKE_report(op->reports, RPT_ERROR, error);
      return OPERATOR_CANCELLED;
    }
  }
  const int axis = RNA_enum_get(op->ptr, "axis");
  Array<int> mirror(mesh.corners_num, -1);
  if (mesh.faces_num == 0 ||
      !symmetry_map(object, axis, RNA_float_get(op->ptr, "tolerance"), mirror))
  {
    BKE_report(op->reports, RPT_INFO, "No mirrored regions found; geometry was left unchanged");
    return OPERATOR_FINISHED;
  }
  BKE_sculptsession_update_for_edit(&depsgraph, &object, false);
  const SubdivCCG *visible_ccg = object.runtime->sculpt_session->subdiv_ccg;
  if (!visible_ccg || visible_ccg->grids_num != mesh.corners_num) {
    BKE_report(
        op->reports, RPT_ERROR, "Use a subdivided Sculpt level, with Sculpt Base Mesh disabled");
    return OPERATOR_CANCELLED;
  }
  int mapped_grids = 0;
  for (const int other : mirror) {
    mapped_grids += other >= 0;
  }
  const bool partial = mapped_grids != mirror.size();
  multires_flush_sculpt_updates(&object);
  Mesh *work = BKE_mesh_copy_for_eval(mesh);
  bke::subdiv::Settings settings;
  BKE_multires_subdiv_settings_init(&settings, mmd);
  bke::subdiv::Subdiv *subdiv = bke::subdiv::new_from_mesh(&settings, work);
  if (!subdiv) {
    BKE_id_free(nullptr, work);
    BKE_report(op->reports, RPT_ERROR, "Cannot evaluate the Multires base topology");
    return OPERATOR_CANCELLED;
  }
  bke::subdiv::displacement_attach_from_multires(subdiv, work, mmd);
  if (!bke::subdiv::eval_begin_from_mesh(subdiv, work, bke::subdiv::SUBDIV_EVALUATOR_TYPE_CPU)) {
    bke::subdiv::free(subdiv);
    BKE_id_free(nullptr, work);
    return OPERATOR_CANCELLED;
  }
  SubdivToCCGSettings ccg_settings{};
  ccg_settings.resolution = (1 << mmd->totlvl) + 1;
  ccg_settings.need_mask = false;
  ccg_settings.need_normal = false;
  /* Initialize colors explicitly only when requested; masks use the visible Sculpt level. */
  char *color_name = work->active_color_attribute;
  work->active_color_attribute = nullptr;
  std::unique_ptr<SubdivCCG> ccg = BKE_subdiv_to_ccg(*subdiv, ccg_settings, *work);
  work->active_color_attribute = color_name;
  if (!ccg) {
    BKE_id_free(nullptr, work);
    return OPERATOR_CANCELLED;
  }
  if (RNA_boolean_get(op->ptr, "use_vertex_paint") && color_name) {
    const auto meta = work->attributes().lookup_meta_data(color_name);
    if (meta && ELEM(meta->data_type, bke::AttrType::ColorFloat, bke::AttrType::ColorByte) &&
        ELEM(meta->domain, bke::AttrDomain::Point, bke::AttrDomain::Corner))
    {
      BKE_subdiv_ccg_colors_ensure(*work, *ccg, color_name);
    }
  }
  const auto *disps = static_cast<const MDisps *>(
      CustomData_get_layer(&work->corner_data, CD_MDISPS));
  bool changed = false;
  Array<bool> changed_grids(mesh.corners_num, false);
  Array<bool> changed_color_grids(mesh.corners_num, false);
  bool colors_changed = false;
  const int size = ccg->grid_size;
  const int area = ccg->grid_area;
  auto mobility = [&](const int grid, const int offset) {
    const int x = offset % size, y = offset / size;
    if (partial && (x == 0 || y == 0 || x == size - 1 || y == size - 1)) {
      SubdivCCGNeighbors neighbors;
      BKE_subdiv_ccg_neighbor_coords_get(*ccg, {grid, short(x), short(y)}, true, neighbors);
      for (const SubdivCCGCoord &duplicate : neighbors.duplicates()) {
        if (mirror[duplicate.grid_index] < 0) {
          return 0.0f; /* Do not open a crack at the boundary of an unmatched region. */
        }
      }
    }
    if (disps && disps[grid].hidden && BLI_BITMAP_TEST(disps[grid].hidden, offset)) {
      return 0.0f;
    }
    if (visible_ccg->masks.is_empty()) {
      return 1.0f;
    }
    const int mask_size = visible_ccg->grid_size;
    const float u = float(offset % size) * (mask_size - 1) / (size - 1);
    const float v = float(offset / size) * (mask_size - 1) / (size - 1);
    const int x0 = int(u), y0 = int(v);
    const int x1 = std::min(x0 + 1, mask_size - 1);
    const int y1 = std::min(y0 + 1, mask_size - 1);
    const float tx = u - x0, ty = v - y0;
    const Span<float> masks = visible_ccg->masks.as_span().slice(grid * visible_ccg->grid_area,
                                                                 visible_ccg->grid_area);
    const float lo = masks[y0 * mask_size + x0] * (1 - tx) + masks[y0 * mask_size + x1] * tx;
    const float hi = masks[y1 * mask_size + x0] * (1 - tx) + masks[y1 * mask_size + x1] * tx;
    return 1.0f - std::clamp(lo * (1 - ty) + hi * ty, 0.0f, 1.0f);
  };
  for (const int grid : mirror.index_range()) {
    if (mirror[grid] < 0) {
      continue;
    }
    for (int y = 0; y < size; y++) {
      for (int x = 0; x < size; x++) {
        const int offset = y * size + x;
        const int other_offset = x * size + y;
        const int a_i = grid * area + offset;
        const int b_i = mirror[grid] * area + other_offset;
        if (a_i > b_i) {
          continue;
        }
        const float a_move = mobility(grid, offset);
        const float b_move = mobility(mirror[grid], other_offset);
        if (a_move + b_move == 0.0f) {
          continue;
        }
        const float3 a = ccg->positions[a_i];
        float3 b = ccg->positions[b_i];
        b[axis] *= -1;
        const float3 target = (a * b_move + b * a_move) / (a_move + b_move);
        const float3 result_a = a + (target - a) * (strength * a_move);
        float3 result_b = b + (target - b) * (strength * b_move);
        result_b[axis] *= -1;
        const bool changed_a = math::distance_squared(a, result_a) > 0.0f;
        changed |= changed_a;
        changed_grids[grid] = changed_grids[grid] || changed_a;
        ccg->positions[a_i] = result_a;
        if (a_i != b_i) {
          const bool changed_b = math::distance_squared(ccg->positions[b_i], result_b) > 0.0f;
          changed |= changed_b;
          changed_grids[mirror[grid]] = changed_grids[mirror[grid]] || changed_b;
          ccg->positions[b_i] = result_b;
        }
        if (!ccg->colors.is_empty()) {
          const float4 ca = ccg->colors[a_i], cb = ccg->colors[b_i];
          const float4 color_target = (ca * b_move + cb * a_move) / (a_move + b_move);
          const float4 color_a = ca + (color_target - ca) * (strength * a_move);
          const float4 color_b = cb + (color_target - cb) * (strength * b_move);
          const bool color_changed_a = math::distance_squared(ca, color_a) > 0.0f;
          const bool color_changed_b = a_i != b_i && math::distance_squared(cb, color_b) > 0.0f;
          colors_changed |= color_changed_a || color_changed_b;
          changed_color_grids[grid] = changed_color_grids[grid] || color_changed_a;
          changed_color_grids[mirror[grid]] = changed_color_grids[mirror[grid]] || color_changed_b;
          ccg->colors[a_i] = color_a;
          if (a_i != b_i) {
            ccg->colors[b_i] = color_b;
          }
        }
      }
    }
  }
  if (!changed && !colors_changed) {
    ccg.reset();
    BKE_id_free(nullptr, work);
    return OPERATOR_FINISHED;
  }
  if (changed && !multiresModifier_reshapeFromCCG(mmd->totlvl, work, ccg.get())) {
    ccg.reset();
    BKE_id_free(nullptr, work);
    BKE_report(op->reports, RPT_ERROR, "Cannot reshape the Multires displacement");
    return OPERATOR_CANCELLED;
  }
  auto *source = static_cast<MDisps *>(
      CustomData_get_layer_for_write(&work->corner_data, CD_MDISPS, work->corners_num));
  const auto *current = static_cast<const MDisps *>(
      CustomData_get_layer(&mesh.corner_data, CD_MDISPS));
  for (const int grid : mirror.index_range()) {
    if (!current || source[grid].totdisp != current[grid].totdisp) {
      ccg.reset();
      BKE_id_free(nullptr, work);
      BKE_report(op->reports, RPT_ERROR, "Multires displacement storage is inconsistent");
      return OPERATOR_CANCELLED;
    }
  }
  undo::geometry_begin(*CTX_data_scene(C), object, op);
  if (colors_changed) {
    Vector<int> color_grids;
    for (const int grid : mirror.index_range()) {
      if (changed_color_grids[grid]) {
        color_grids.append(grid);
      }
    }
    BKE_subdiv_ccg_colors_store(mesh, *ccg, color_grids);
    if (partial) {
      /* Syncing coarse colors normally averages every grid meeting a base vertex. Retain the
       * exact original storage on unmatched faces, including Byte Color rounding and seams. */
      const bke::GAttributeReader original = work->attributes().lookup(ccg->color_name);
      bke::GSpanAttributeWriter base = mesh.attributes_for_write().lookup_for_write_span(
          ccg->color_name);
      Array<bool> excluded(base.span.size(), false);
      for (const int corner : mirror.index_range()) {
        if (mirror[corner] < 0) {
          excluded[base.domain == bke::AttrDomain::Point ? mesh.corner_verts()[corner] : corner] =
              true;
        }
      }
      for (const int i : excluded.index_range()) {
        if (excluded[i]) {
          original.varray.get(i, base.span[i]);
        }
      }
      base.finish();
    }
  }
  ccg.reset();
  auto *destination = static_cast<MDisps *>(
      CustomData_get_layer_for_write(&mesh.corner_data, CD_MDISPS, mesh.corners_num));
  for (const int grid : mirror.index_range()) {
    if (changed_grids[grid]) {
      std::swap(destination[grid].disps, source[grid].disps);
    }
  }
  BKE_id_free(nullptr, work);
  SculptSession &ss = *object.runtime->sculpt_session;
  ss.stroke_delta.reset();
  if (changed && mesh.sculpt_layer_stack && !bke::sculpt_layers::native_single(mesh)) {
    bke::sculpt_layers::Edit edit;
    bke::sculpt_layers::record_multires(object, edit);
  }
  undo::geometry_end(object);
  BKE_sculptsession_free_pbvh(object);
  DEG_id_tag_update(&mesh.id, ID_RECALC_GEOMETRY);
  DEG_id_tag_update(&object.id, ID_RECALC_GEOMETRY);
  BKE_scene_graph_evaluated_ensure(&depsgraph, CTX_data_main(C));
  BKE_sculptsession_update_for_edit(&depsgraph, &object, false);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, &object);
  if (partial) {
    BKE_report(
        op->reports, RPT_INFO, "Matching regions symmetrized; unmatched regions left unchanged");
  }
  return OPERATOR_FINISHED;
}

void SCULPT_OT_symmetrize_blend(wmOperatorType *ot)
{
  static const EnumPropertyItem axes[] = {{0, "X", 0, "X", ""},
                                          {1, "Y", 0, "Y", ""},
                                          {2, "Z", 0, "Z", ""},
                                          {0, nullptr, 0, nullptr, nullptr}};
  ot->name = "Symmetrize Blend";
  ot->idname = "SCULPT_OT_symmetrize_blend";
  ot->description =
      "Blend mirrored sculpt geometry, keeping masked points fixed and Multires levels intact";
  ot->poll = symmetrize_blend_poll;
  ot->exec = symmetrize_blend_exec;
  ot->invoke = WM_operator_props_popup_confirm;
  ot->flag = OPTYPE_REGISTER;
  RNA_def_enum(ot->srna, "axis", axes, 0, "Axis", "Object-space symmetry axis");
  RNA_def_boolean(ot->srna,
                  "use_vertex_paint",
                  true,
                  "Vertex Paint",
                  "Also blend the active color attribute, respecting masks and Strength");
  RNA_def_float_factor(ot->srna,
                       "strength",
                       1.0f,
                       0.0f,
                       1.0f,
                       "Strength",
                       "Amount of the mask-weighted symmetry correction",
                       0.0f,
                       1.0f);
  RNA_def_float_distance(ot->srna,
                         "tolerance",
                         0.0001f,
                         0.000001f,
                         1.0f,
                         "Match Tolerance",
                         "Object-space tolerance for matching mirrored base vertices; every "
                         "mirrored face is validated",
                         0.000001f,
                         0.01f);
}

}  // namespace blender::ed::sculpt_paint
