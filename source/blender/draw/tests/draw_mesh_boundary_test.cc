/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "GPU_shader.hh"

#include "draw_subdivision.hh"
#include "draw_testing.hh"
#include "mesh_extractors/extract_mesh.hh"

namespace blender::draw {

static void test_mesh_boundary_edges()
{
  /* Two quads sharing edge 1. Ignore winding and do not include the loose edge 7. */
  const Array<int> offsets = {0, 4, 8};
  const Array<int> corner_edges = {0, 1, 2, 3, 4, 5, 6, 1};
  MeshRenderData mr{};
  mr.extract_type = MeshExtractType::Mesh;
  mr.edges_num = 8;
  mr.corners_num = 8;
  mr.faces = OffsetIndices<int>(offsets.as_span());
  mr.corner_edges = corner_edges;
  EXPECT_EQ(extract_lines_boundary(mr)->index_len_get(), 12u);

  /* A hidden face must not turn its shared edge into a false hole. */
  const Array<bool> hide_faces = {false, true};
  mr.hide_poly = VArraySpan<bool>(VArray<bool>::from_span(hide_faces));
  EXPECT_EQ(extract_lines_boundary(mr)->index_len_get(), 6u);
  const Array<bool> hide_edges = {true, false, false, false, false, false, false, false};
  mr.hide_edge = VArraySpan<bool>(VArray<bool>::from_span(hide_edges));
  EXPECT_EQ(extract_lines_boundary(mr)->index_len_get(), 4u);

  /* A closed tetrahedron has no boundary. Every edge has exactly two face users. */
  const Array<int> closed_offsets = {0, 3, 6, 9, 12};
  const Array<int> closed_edges = {0, 1, 2, 0, 3, 4, 1, 5, 3, 2, 4, 5};
  mr.edges_num = 6;
  mr.corners_num = 12;
  mr.faces = OffsetIndices<int>(closed_offsets.as_span());
  mr.corner_edges = closed_edges;
  mr.hide_poly = {};
  mr.hide_edge = {};
  EXPECT_EQ(extract_lines_boundary(mr)->index_len_get(), 0u);

  /* Non-manifold edges with three face users also aren't open boundaries. */
  const Array<int> nonmanifold_offsets = {0, 3, 6, 9};
  const Array<int> nonmanifold_edges = {0, 1, 2, 0, 3, 4, 0, 5, 6};
  mr.edges_num = 7;
  mr.corners_num = 9;
  mr.faces = OffsetIndices<int>(nonmanifold_offsets.as_span());
  mr.corner_edges = nonmanifold_edges;
  EXPECT_EQ(extract_lines_boundary(mr)->index_len_get(), 12u);

  /* Empty and loose-only meshes produce no drawable indices. */
  const Array<int> empty_offsets = {0};
  mr.faces = OffsetIndices<int>(empty_offsets.as_span());
  mr.corner_edges = {};
  mr.corners_num = 0;
  EXPECT_EQ(extract_lines_boundary(mr)->index_len_get(), 0u);
}
DRAW_TEST(mesh_boundary_edges)

static void test_mesh_boundary_edges_subdiv()
{
  Array<int> corner_edges = {0, 1, 2, 3, 4, 5, 6, 1};
  Array<int> face_indices = {0, 0, 0, 0, 1, 1, 1, 1};
  DRWSubdivCache cache{};
  cache.num_subdiv_quads = 2;
  cache.num_subdiv_loops = 8;
  cache.num_subdiv_edges = 7;
  cache.subdiv_loop_subdiv_edge_index = corner_edges.data();
  cache.subdiv_loop_face_index = face_indices.data();
  MeshRenderData mr{};
  EXPECT_EQ(extract_lines_boundary_subdiv(mr, cache)->index_len_get(), 12u);
  const Array<bool> hide_faces = {false, true};
  mr.hide_poly = VArraySpan<bool>(VArray<bool>::from_span(hide_faces));
  EXPECT_EQ(extract_lines_boundary_subdiv(mr, cache)->index_len_get(), 6u);
  /* The mapping arrays above belong to this test, not to the cache. */
  cache.subdiv_loop_subdiv_edge_index = nullptr;
  cache.subdiv_loop_face_index = nullptr;
}
DRAW_TEST(mesh_boundary_edges_subdiv)

static void test_mesh_holes_shader()
{
  for (const char *name : {"overlay_mesh_holes", "overlay_mesh_holes_clipped"}) {
    gpu::Shader *shader = GPU_shader_create_from_info_name(name);
    ASSERT_NE(shader, nullptr);
    GPU_shader_free(shader);
  }
}
DRAW_TEST(mesh_holes_shader)

}  // namespace blender::draw
