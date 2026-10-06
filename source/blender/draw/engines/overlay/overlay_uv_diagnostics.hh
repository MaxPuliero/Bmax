/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <algorithm>
#include <cfloat>

#include "BKE_attribute.hh"
#include "BKE_editmesh.hh"
#include "BKE_mesh_types.hh"
#include "BLI_array.hh"
#include "BLI_math_geom.h"
#include "BLI_math_vector.hh"
#include "ED_uvedit.hh"
#include "bmesh.hh"
#include "overlay_base.hh"

namespace blender::draw::overlay {

/** Optional UV shell boundaries and diagnostics, accumulated across visible UV objects. */
class UVDiagnostics {
 private:
  bool enabled_ = false;
  bool show_outline_ = false;
  bool show_overlap_ = false;
  bool show_flipped_ = false;
  int outline_pass_ = 0;
  StorageVectorBuffer<UVDiagnosticVertex> vertices_ = {"uv_diagnostic_vertices"};
  StorageVectorBuffer<UVDiagnosticNode> nodes_ = {"uv_boundary_nodes"};
  StorageVectorBuffer<float4> edges_ = {"uv_boundary_edges"};
  TextureFromPool coverage_tx_ = {"uv_diagnostic_coverage"};
  Framebuffer coverage_fb_ = {"uv_diagnostic_coverage_fb"};
  PassSimple coverage_ps_ = {"UV diagnostic coverage"};
  PassSimple resolve_ps_ = {"UV diagnostic resolve"};

  int build_boundary_tree(MutableSpan<float4> edges)
  {
    const int root = nodes_.size();
    float2 lo(FLT_MAX), hi(-FLT_MAX);
    for (const float4 &edge : edges) {
      lo = math::min(lo, math::min(edge.xy(), edge.zw()));
      hi = math::max(hi, math::max(edge.xy(), edge.zw()));
    }
    nodes_.append({float4(lo.x, lo.y, hi.x, hi.y), int4(0)});
    if (edges.size() <= 8) {
      const int start = edges_.size();
      edges_.extend(edges);
      nodes_[root].data = int4(root + 1, start, edges.size(), 0);
    }
    else {
      const int axis = (hi.x - lo.x > hi.y - lo.y) ? 0 : 1;
      const int middle = edges.size() / 2;
      std::nth_element(edges.begin(),
                       edges.begin() + middle,
                       edges.end(),
                       [axis](const float4 &a, const float4 &b) {
                         return a[axis] + a[axis + 2] < b[axis] + b[axis + 2];
                       });
      build_boundary_tree(edges.take_front(middle));
      build_boundary_tree(edges.drop_front(middle));
      nodes_[root].data.x = nodes_.size();
    }
    return root;
  }

 public:
  void begin_sync(Resources &res, const State &state)
  {
    vertices_.clear_and_trim();
    nodes_.clear_and_trim();
    edges_.clear_and_trim();
    enabled_ = state.is_space_image() && !state.hide_overlays &&
               (state.object_mode == OB_MODE_OBJECT || (state.object_mode & OB_MODE_EDIT));
    if (!enabled_) {
      return;
    }
    const SpaceImage &sima = *reinterpret_cast<const SpaceImage *>(state.space_data);
    show_outline_ = sima.overlay.flag & SI_OVERLAY_UV_SHELL_OUTLINE;
    show_overlap_ = sima.overlay.flag & SI_OVERLAY_UV_OVERLAP;
    show_flipped_ = sima.overlay.flag & SI_OVERLAY_UV_FLIPPED;
    enabled_ = sima.mode == SI_MODE_UV && (show_outline_ || show_overlap_ || show_flipped_);
    if (!enabled_) {
      return;
    }
    coverage_ps_.init();
    coverage_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL);
    coverage_ps_.shader_set(res.shaders->uv_diagnostics.get());
    coverage_ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    coverage_ps_.push_constant("show_outline", show_outline_);
    coverage_ps_.bind_ssbo("vertices", &vertices_);
    coverage_ps_.bind_ssbo("boundary_nodes", &nodes_);
    coverage_ps_.bind_ssbo("boundary_edges", &edges_);

    resolve_ps_.init();
    resolve_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA);
    resolve_ps_.shader_set(res.shaders->uv_diagnostics_resolve.get());
    resolve_ps_.bind_texture("coverage_tx", &coverage_tx_);
    resolve_ps_.push_constant("show_overlap", show_overlap_);
    resolve_ps_.push_constant("show_flipped", show_flipped_);
    resolve_ps_.push_constant("outline_pass", &outline_pass_);
    resolve_ps_.push_constant("overlap_opacity", sima.overlay.uv_overlap_opacity);
    resolve_ps_.push_constant("flipped_opacity", sima.overlay.uv_flipped_opacity);
    resolve_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }

  void object_sync(const Mesh &mesh, StringRef uv_map, const State &state)
  {
    if (!enabled_ || !mesh.runtime->edit_mesh) {
      return;
    }
    BMEditMesh &em = *mesh.runtime->edit_mesh;
    BMesh &bm = *em.bm;
    const int offset = CustomData_get_offset_named(&bm.ldata, CD_PROP_FLOAT2, uv_map);
    if (offset < 0) {
      return;
    }
    BM_mesh_elem_index_ensure(&bm, BM_FACE);
    Array<bool> visible(bm.totface, false);
    Array<int> roots(bm.totface, -1);
    BMIter iter;
    BMFace *face;
    BM_ITER_MESH (face, &iter, &bm, BM_FACES_OF_MESH) {
      visible[BM_elem_index_get(face)] = uvedit_face_visible_test_ex(state.scene->toolsettings,
                                                                     face);
    }
    if (show_outline_) {
      Array<bool> visited(bm.totface, false);
      BM_ITER_MESH (face, &iter, &bm, BM_FACES_OF_MESH) {
        const int face_index = BM_elem_index_get(face);
        if (!visible[face_index] || visited[face_index]) {
          continue;
        }
        Vector<BMFace *> shell;
        Vector<float4> boundary;
        shell.append(face);
        visited[face_index] = true;
        for (int i = 0; i < shell.size(); i++) {
          BMLoop *first = BM_FACE_FIRST_LOOP(shell[i]);
          BMLoop *loop = first;
          do {
            bool shared = false;
            BMLoop *radial = loop->radial_next;
            while (radial != loop) {
              const int other_index = BM_elem_index_get(radial->f);
              if (visible[other_index] && BM_loop_uv_share_edge_check(loop, radial, offset)) {
                shared = true;
                if (!visited[other_index]) {
                  visited[other_index] = true;
                  shell.append(radial->f);
                }
              }
              radial = radial->radial_next;
            }
            if (!shared) {
              const float2 a = BM_ELEM_CD_GET_FLOAT2_P(loop, offset);
              const float2 b = BM_ELEM_CD_GET_FLOAT2_P(loop->next, offset);
              boundary.append(float4(a.x, a.y, b.x, b.y));
            }
            loop = loop->next;
          } while (loop != first);
        }
        const int root = boundary.is_empty() ? -1 :
                                               build_boundary_tree(boundary.as_mutable_span());
        for (BMFace *shell_face : shell) {
          roots[BM_elem_index_get(shell_face)] = root;
        }
      }
    }
    /* Original edit meshes may not have the evaluated draw cache tessellation. */
    Array<std::array<BMLoop *, 3>> triangles(poly_to_tri_count(bm.totface, bm.totloop));
    BM_mesh_calc_tessellation(&bm, triangles);
    for (const std::array<BMLoop *, 3> &tri : triangles) {
      const int face_index = BM_elem_index_get(tri[0]->f);
      if (!visible[face_index]) {
        continue;
      }
      const float2 a = BM_ELEM_CD_GET_FLOAT2_P(tri[0], offset);
      const float2 b = BM_ELEM_CD_GET_FLOAT2_P(tri[1], offset);
      const float2 c = BM_ELEM_CD_GET_FLOAT2_P(tri[2], offset);
      const float2 ab = b - a, ac = c - a;
      const float signed_area = ab.x * ac.y - ab.y * ac.x;
      if (signed_area == 0.0f) {
        continue;
      }
      const uint flipped = signed_area < 0.0f;
      const int root = roots[face_index];
      vertices_.append({a, root, flipped});
      vertices_.append({b, root, flipped});
      vertices_.append({c, root, flipped});
    }
  }

  /** Read original object-mode UVs directly, avoiding an evaluated modifier mesh or BMesh copy. */
  void object_sync_object(const Mesh &mesh, StringRef uv_map)
  {
    if (!enabled_) {
      return;
    }
    const bke::AttributeAccessor attributes = mesh.attributes();
    const auto uv_attribute = attributes.lookup<float2>(uv_map, bke::AttrDomain::Corner);
    if (!uv_attribute) {
      return;
    }
    const VArraySpan<float2> uv = *uv_attribute;
    const VArray<bool> hidden = *attributes.lookup_or_default<bool>(
        ".hide_poly", bke::AttrDomain::Face, false);
    const OffsetIndices faces = mesh.faces();
    const Span<int> corner_faces = mesh.corner_to_face_map();
    const Span<int> corner_vertices = mesh.corner_verts();
    const Span<int> corner_edges = mesh.corner_edges();
    Array<int> roots(mesh.faces_num, -1);
    auto next_corner = [&](const int corner) {
      const IndexRange face = faces[corner_faces[corner]];
      return corner == face.last() ? face.start() : corner + 1;
    };
    if (show_outline_) {
      /* A compact radial list handles seams and non-manifold edges without copying geometry. */
      Array<int> first_corner(mesh.edges_num, -1);
      Array<int> radial_next(mesh.corners_num, -1);
      for (const int corner : corner_edges.index_range()) {
        const int edge = corner_edges[corner];
        radial_next[corner] = first_corner[edge];
        first_corner[edge] = corner;
      }
      Array<bool> visited(mesh.faces_num, false);
      for (const int face : faces.index_range()) {
        if (hidden[face] || visited[face]) {
          continue;
        }
        Vector<int> shell;
        Vector<float4> boundary;
        shell.append(face);
        visited[face] = true;
        for (int i = 0; i < shell.size(); i++) {
          for (const int corner : faces[shell[i]]) {
            const int next = next_corner(corner);
            bool shared = false;
            for (int other = first_corner[corner_edges[corner]]; other != -1;
                 other = radial_next[other])
            {
              const int other_face = corner_faces[other];
              if (other_face == shell[i] || hidden[other_face]) {
                continue;
              }
              const int other_next = next_corner(other);
              const bool same_direction = corner_vertices[corner] == corner_vertices[other];
              const int other_a = same_direction ? other : other_next;
              const int other_b = same_direction ? other_next : other;
              if (uv[corner] == uv[other_a] && uv[next] == uv[other_b]) {
                shared = true;
                if (!visited[other_face]) {
                  visited[other_face] = true;
                  shell.append(other_face);
                }
              }
            }
            if (!shared) {
              boundary.append(float4(uv[corner].x, uv[corner].y, uv[next].x, uv[next].y));
            }
          }
        }
        const int root = boundary.is_empty() ? -1 :
                                               build_boundary_tree(boundary.as_mutable_span());
        for (const int shell_face : shell) {
          roots[shell_face] = root;
        }
      }
    }
    for (const int3 &triangle : mesh.corner_tris()) {
      const int face = corner_faces[triangle.x];
      if (hidden[face]) {
        continue;
      }
      const float2 a = uv[triangle.x], b = uv[triangle.y], c = uv[triangle.z];
      const float2 ab = b - a, ac = c - a;
      const float area = ab.x * ac.y - ab.y * ac.x;
      if (area == 0.0f) {
        continue;
      }
      const uint flipped = area < 0.0f;
      vertices_.append({a, roots[face], flipped});
      vertices_.append({b, roots[face], flipped});
      vertices_.append({c, roots[face], flipped});
    }
  }

  void end_sync()
  {
    if (!enabled_ || vertices_.is_empty()) {
      return;
    }
    vertices_.push_update();
    nodes_.push_update();
    edges_.push_update();
    coverage_ps_.draw_procedural(GPU_PRIM_TRIS, 1, vertices_.size());
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    if (!enabled_ || vertices_.is_empty()) {
      return;
    }
    int viewport[4];
    GPU_framebuffer_viewport_get(framebuffer, viewport);
    const int2 size(viewport[2], viewport[3]);
    const eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    coverage_tx_.acquire_2d(size, gpu::TextureFormat::SFLOAT_16_16_16_16, usage);
    coverage_fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(coverage_tx_));
    GPU_framebuffer_bind(coverage_fb_);
    GPU_framebuffer_clear_color(coverage_fb_, double4(0.0));
    manager.submit(coverage_ps_, view);
    GPU_framebuffer_bind(framebuffer);
    outline_pass_ = false;
    manager.submit(resolve_ps_);
  }

  void draw_outline(Framebuffer &framebuffer, Manager &manager)
  {
    if (!enabled_ || vertices_.is_empty()) {
      return;
    }
    if (show_outline_) {
      GPU_framebuffer_bind(framebuffer);
      outline_pass_ = true;
      manager.submit(resolve_ps_);
    }
    coverage_tx_.release();
  }
};

}  // namespace blender::draw::overlay
