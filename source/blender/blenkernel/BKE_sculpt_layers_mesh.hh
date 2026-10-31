/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "BKE_multires.hh"
#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

namespace blender {
struct BlendDataReader;
struct BlendWriter;
struct Mesh;
struct Object;
struct SculptLayer;
struct SculptLayerStack;

namespace bke::sculpt_layers {

struct EditChunk {
  int grid = -1;
  bool allocated = false;
  Array<int> indices;
  Array<float3> samples;
};
struct Edit {
  int uid = 0;
  bool mesh_allocated = false;
  Vector<EditChunk> chunks;
};

SculptLayerStack *copy(const SculptLayerStack *stack);
void free(SculptLayerStack *stack);
size_t memory_size(const SculptLayerStack *stack);
void blend_write(BlendWriter *writer, const SculptLayerStack *stack);
void blend_read(BlendDataReader *reader, SculptLayerStack **stack);

/** Null means supported. Existing stacks with changed topology are retained but not edited. */
const char *support_error(const Object &object, bool recording);
bool topology_valid(const Mesh &mesh);
/** Layer 1 alone is native geometry, with no authoring buffers or per-stroke recording. */
bool native_single(const Mesh &mesh);
bool ensure(Object &object);
SculptLayer *active(Mesh &mesh);
bool add(Object &object);
bool remove(Object &object);
/** Merge the visible result into static Layer 1 and return to native single-layer sculpting. */
bool bake(Object &object);
void evaluate(Mesh &mesh);
/** Preserve individual contributions when changing the stored Multires resolution. */
bool subdivide(Object &object,
               MultiresModifierData &mmd,
               int level,
               MultiresSubdivideModeType mode);
void delete_higher(Mesh &mesh, int level);
/** Record the native Multires reshape, including detail propagated from lower sculpt levels. */
void record_multires(Object &object, Edit &edit);

/** Called serially at the end of a native position undo step, using its original local snapshots.
 */
void record_mesh(Object &object, Span<int> indices, Span<float3> before, Edit &edit);
void swap_edit(Object &object, Edit &edit);
/** Restore authoring data lost when the first stroke crosses a preceding memfile undo step. */
void restore_created_stack(Mesh &mesh, const SculptLayerStack &after, bool undo);

}  // namespace bke::sculpt_layers
}  // namespace blender
