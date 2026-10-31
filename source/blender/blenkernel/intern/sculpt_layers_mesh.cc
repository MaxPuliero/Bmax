/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_sculpt_layers_mesh.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"

#include "BKE_customdata.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_modifier.hh"
#include "BKE_multires.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLO_read_write.hh"
#include "MEM_guardedalloc.h"

#include "multires_reshape.hh"

#include <algorithm>
#include <string>

namespace blender::bke::sculpt_layers {

static uint64_t topology_hash(const Mesh &mesh)
{
  uint64_t hash = 14695981039346656037ULL;
  auto add = [&](const int value) { hash = (hash ^ uint32_t(value)) * 1099511628211ULL; };
  add(mesh.verts_num);
  add(mesh.edges_num);
  add(mesh.faces_num);
  add(mesh.corners_num);
  for (const int2 edge : mesh.edges()) {
    add(edge.x);
    add(edge.y);
  }
  for (const int vert : mesh.corner_verts()) {
    add(vert);
  }
  for (const int offset : mesh.face_offsets()) {
    add(offset);
  }
  return hash;
}

static MultiresModifierData *multires(const Object &object)
{
  for (const ModifierData &md : object.modifiers) {
    if (md.type == eModifierType_Multires && (md.mode & eModifierMode_Realtime)) {
      return const_cast<MultiresModifierData *>(
          reinterpret_cast<const MultiresModifierData *>(&md));
    }
  }
  return nullptr;
}

bool native_single(const Mesh &mesh)
{
  const SculptLayerStack *stack = mesh.sculpt_layer_stack;
  return stack && stack->layers.first && stack->layers.first == stack->layers.last &&
         static_cast<const SculptLayer *>(stack->layers.first)->uid == 0;
}

bool topology_valid(const Mesh &mesh)
{
  const SculptLayerStack *stack = mesh.sculpt_layer_stack;
  if (native_single(mesh)) {
    return true;
  }
  if (!(stack && stack->verts_num == mesh.verts_num &&
        (!stack->level || stack->grids_num == mesh.corners_num) &&
        stack->topology_hash == topology_hash(mesh)))
  {
    return false;
  }
  if (stack->level) {
    const auto *mdisps = static_cast<const MDisps *>(
        CustomData_get_layer(&mesh.corner_data, CD_MDISPS));
    const int size = bke::subdiv::grid_size_from_level(stack->level);
    if (!mdisps || !stack->base_grids) {
      return false;
    }
    for (int i = 0; i < stack->grids_num; i++) {
      if (mdisps[i].level != stack->level || mdisps[i].totdisp != size * size) {
        return false;
      }
    }
  }
  return true;
}

const char *support_error(const Object &object, const bool recording)
{
  if (object.type != OB_MESH || !object.data) {
    return "Sculpt layers require a mesh";
  }
  const Mesh &mesh = *id_cast<Mesh *>(object.data);
  if (mesh.key) {
    return "Sculpt layers currently require a mesh without shape keys";
  }
  if (ID_REAL_USERS(&mesh.id) > 1) {
    return "Make the mesh single-user before editing sculpt layers";
  }
  if (mesh.flag & ME_SCULPT_DYNAMIC_TOPOLOGY) {
    return "Sculpt layers require fixed topology";
  }
  const MultiresModifierData *mmd = multires(object);
  if (!mmd) {
    return "Sculpt layers require a Multires modifier";
  }
  if (mmd && (mmd->flags & eMultiresModifierFlag_UseSculptBaseMesh)) {
    return "Disable Sculpt Base Mesh before recording sculpt layers";
  }
  if (mmd && CustomData_external_test(const_cast<CustomData *>(&mesh.corner_data), CD_MDISPS)) {
    return "Pack external Multires displacement before using sculpt layers";
  }
  for (const ModifierData &md : object.modifiers) {
    if ((md.mode & eModifierMode_Realtime) && &md != reinterpret_cast<const ModifierData *>(mmd)) {
      return "Disable other modifiers before editing sculpt layers";
    }
  }
  if (mesh.sculpt_layer_stack && !native_single(mesh)) {
    if (!topology_valid(mesh)) {
      return "The mesh topology changed; sculpt layers cannot be edited";
    }
    if (mesh.sculpt_layer_stack->level != (mmd ? mmd->totlvl : 0)) {
      return "The Multires configuration changed; sculpt layers cannot be edited";
    }
  }
  if (recording && mesh.sculpt_layer_stack) {
    const SculptLayer *layer = static_cast<const SculptLayer *>(
        BLI_findlink(&mesh.sculpt_layer_stack->layers, mesh.sculpt_layer_stack->active_index));
    if (layer && layer->influence <= 0.0f) {
      return "Raise the active sculpt layer Influence above zero before sculpting";
    }
  }
  return nullptr;
}

static void free_grids(SculptLayerGrid *grids, const int count)
{
  if (!grids) {
    return;
  }
  for (int i = 0; i < count; i++) {
    MEM_SAFE_DELETE(grids[i].data);
  }
  MEM_delete(grids);
}

void free(SculptLayerStack *stack)
{
  if (!stack) {
    return;
  }
  for (SculptLayer &layer : stack->layers) {
    MEM_SAFE_DELETE(layer.positions);
    free_grids(layer.grids, stack->grids_num);
  }
  BLI_freelistN(&stack->layers);
  MEM_SAFE_DELETE(stack->base_positions);
  free_grids(stack->base_grids, stack->grids_num);
  MEM_delete(stack);
}

static SculptLayerGrid *copy_grids(const SculptLayerGrid *src, const int count)
{
  if (!src) {
    return nullptr;
  }
  auto *dst = MEM_dupalloc(src);
  for (int i = 0; i < count; i++) {
    dst[i].data = MEM_dupalloc(src[i].data);
  }
  return dst;
}

size_t memory_size(const SculptLayerStack *stack)
{
  if (!stack) {
    return 0;
  }
  size_t size = sizeof(*stack);
  auto count_grids = [&](const SculptLayerGrid *grids) {
    if (!grids) {
      return;
    }
    size += sizeof(SculptLayerGrid) * stack->grids_num;
    for (int i = 0; i < stack->grids_num; i++) {
      if (grids[i].data) {
        size += sizeof(float3) * grids[i].samples_num;
      }
    }
  };
  if (stack->base_positions) {
    size += sizeof(float3) * stack->verts_num;
  }
  count_grids(stack->base_grids);
  for (const SculptLayer &layer : stack->layers) {
    size += sizeof(layer);
    if (layer.positions) {
      size += sizeof(float3) * stack->verts_num;
    }
    count_grids(layer.grids);
  }
  return size;
}

SculptLayerStack *copy(const SculptLayerStack *src)
{
  if (!src) {
    return nullptr;
  }
  auto *dst = MEM_dupalloc(src);
  BLI_listbase_clear(&dst->layers);
  dst->base_positions = MEM_dupalloc(src->base_positions);
  dst->base_grids = copy_grids(src->base_grids, src->grids_num);
  for (const SculptLayer &layer : src->layers) {
    auto *item = MEM_dupalloc(&layer);
    item->positions = MEM_dupalloc(layer.positions);
    item->grids = copy_grids(layer.grids, src->grids_num);
    BLI_addtail(&dst->layers, item);
  }
  return dst;
}

static void release_base(SculptLayerStack &stack)
{
  MEM_SAFE_DELETE(stack.base_positions);
  free_grids(stack.base_grids, stack.grids_num);
  stack.base_grids = nullptr;
}

static void migrate_static_base(SculptLayerStack &stack)
{
  auto *first = static_cast<SculptLayer *>(stack.layers.first);
  if (!first || first->uid == 0) {
    return;
  }
  if (stack.base_positions && first->positions) {
    for (int i = 0; i < stack.verts_num; i++) {
      const float3 value = float3(stack.base_positions[i]) +
                           float3(first->positions[i]) * first->influence;
      copy_v3_v3(stack.base_positions[i], value);
    }
  }
  if (stack.base_grids && first->grids) {
    for (int grid = 0; grid < stack.grids_num; grid++) {
      const SculptLayerGrid &src = first->grids[grid];
      if (!src.data || first->influence == 0.0f) {
        continue;
      }
      SculptLayerGrid &dst = stack.base_grids[grid];
      if (!dst.data) {
        dst.samples_num = src.samples_num;
        dst.data = reinterpret_cast<float (*)[3]>(
            MEM_new_array_zeroed<float3>(src.samples_num, __func__));
      }
      if (dst.samples_num == src.samples_num) {
        for (int i = 0; i < src.samples_num; i++) {
          const float3 value = float3(dst.data[i]) + float3(src.data[i]) * first->influence;
          copy_v3_v3(dst.data[i], value);
        }
      }
    }
  }
  MEM_SAFE_DELETE(first->positions);
  free_grids(first->grids, stack.grids_num);
  first->grids = nullptr;
  first->uid = 0;
  first->influence = 1.0f;
  if (stack.layers.first == stack.layers.last) {
    release_base(stack);
  }
}

static void write_grids(BlendWriter *writer, const SculptLayerGrid *grids, const int count)
{
  if (!grids) {
    return;
  }
  writer->write_struct_array(count, grids);
  for (int i = 0; i < count; i++) {
    if (grids[i].data) {
      writer->write_float3_array(grids[i].samples_num, &grids[i].data[0][0]);
    }
  }
}

void blend_write(BlendWriter *writer, const SculptLayerStack *stack)
{
  if (!stack) {
    return;
  }
  writer->write_struct(stack);
  if (stack->base_positions) {
    writer->write_float3_array(stack->verts_num, &stack->base_positions[0][0]);
  }
  write_grids(writer, stack->base_grids, stack->grids_num);
  for (const SculptLayer &layer : stack->layers) {
    writer->write_struct(&layer);
    if (layer.positions) {
      writer->write_float3_array(stack->verts_num, &layer.positions[0][0]);
    }
    write_grids(writer, layer.grids, stack->grids_num);
  }
}

static void read_grids(BlendDataReader *reader, SculptLayerGrid **grids, const int count)
{
  if (BLO_read_array(reader, grids, count)) {
    for (int i = 0; i < count; i++) {
      (void)BLO_read_array(reader, &(*grids)[i].data, (*grids)[i].samples_num);
    }
  }
}

void blend_read(BlendDataReader *reader, SculptLayerStack **stack_ptr)
{
  BLO_read_struct(reader, SculptLayerStack, stack_ptr);
  SculptLayerStack *stack = *stack_ptr;
  if (!stack) {
    return;
  }
  (void)BLO_read_array(reader, &stack->base_positions, stack->verts_num);
  read_grids(reader, &stack->base_grids, stack->grids_num);
  BLO_read_struct_list(reader, SculptLayer, &stack->layers);
  for (SculptLayer &layer : stack->layers) {
    (void)BLO_read_array(reader, &layer.positions, stack->verts_num);
    read_grids(reader, &layer.grids, stack->grids_num);
  }
  stack->active_index = std::clamp(
      stack->active_index, 0, std::max(0, BLI_listbase_count(&stack->layers) - 1));
  migrate_static_base(*stack);
}

static SculptLayer *append(Mesh &mesh)
{
  SculptLayerStack &stack = *mesh.sculpt_layer_stack;
  auto *layer = MEM_new<SculptLayer>(__func__);
  layer->uid = stack.next_uid++;
  const std::string name = "Layer " + std::to_string(BLI_listbase_count(&stack.layers) + 1);
  STRNCPY_UTF8(layer->name, name.c_str());
  if (stack.grids_num && stack.layers.first) {
    layer->grids = MEM_new_array<SculptLayerGrid>(stack.grids_num, __func__);
  }
  BLI_addtail(&stack.layers, layer);
  stack.active_index = BLI_listbase_count(&stack.layers) - 1;
  return layer;
}

bool ensure(Object &object)
{
  if (support_error(object, false)) {
    return false;
  }
  Mesh &mesh = *id_cast<Mesh *>(object.data);
  if (mesh.sculpt_layer_stack) {
    migrate_static_base(*mesh.sculpt_layer_stack);
    return true;
  }
  auto *stack = MEM_new<SculptLayerStack>(__func__);
  stack->verts_num = mesh.verts_num;
  stack->grids_num = mesh.corners_num;
  stack->level = multires(object)->totlvl;
  mesh.sculpt_layer_stack = stack;
  SculptLayer *layer = append(mesh);
  layer->uid = 0;
  free_grids(layer->grids, stack->grids_num);
  layer->grids = nullptr;
  return true;
}

static void capture_base(Object &object)
{
  multires_flush_sculpt_updates(&object);
  Mesh &mesh = *id_cast<Mesh *>(object.data);
  SculptLayerStack &stack = *mesh.sculpt_layer_stack;
  stack.verts_num = mesh.verts_num;
  stack.grids_num = mesh.corners_num;
  stack.level = multires(object)->totlvl;
  stack.topology_hash = topology_hash(mesh);
  stack.base_positions = reinterpret_cast<float (*)[3]>(
      MEM_new_array<float3>(mesh.verts_num, __func__));
  MutableSpan(reinterpret_cast<float3 *>(stack.base_positions), mesh.verts_num)
      .copy_from(mesh.vert_positions());
  stack.base_grids = MEM_new_array<SculptLayerGrid>(stack.grids_num, __func__);
  if (stack.level) {
    const auto *mdisps = static_cast<const MDisps *>(
        CustomData_get_layer(&mesh.corner_data, CD_MDISPS));
    for (int grid = 0; mdisps && grid < stack.grids_num; grid++) {
      stack.base_grids[grid].samples_num = mdisps[grid].totdisp;
      stack.base_grids[grid].data = MEM_dupalloc(mdisps[grid].disps);
    }
  }
}

SculptLayer *active(Mesh &mesh)
{
  auto *stack = mesh.sculpt_layer_stack;
  return stack ? static_cast<SculptLayer *>(BLI_findlink(&stack->layers, stack->active_index)) :
                 nullptr;
}

bool add(Object &object)
{
  if (!ensure(object) || support_error(object, false)) {
    return false;
  }
  Mesh &mesh = *id_cast<Mesh *>(object.data);
  if (native_single(mesh)) {
    capture_base(object);
  }
  append(mesh);
  return true;
}

static void compose_grid(Mesh &mesh, const int index)
{
  const SculptLayerStack &stack = *mesh.sculpt_layer_stack;
  auto *mdisps = static_cast<MDisps *>(
      CustomData_get_layer_for_write(&mesh.corner_data, CD_MDISPS, mesh.corners_num));
  if (!mdisps || !mdisps[index].disps) {
    return;
  }
  MutableSpan dst(reinterpret_cast<float3 *>(mdisps[index].disps), mdisps[index].totdisp);
  dst.fill(float3(0));
  if (stack.base_grids[index].data && stack.base_grids[index].samples_num == dst.size()) {
    dst.copy_from(
        Span(reinterpret_cast<const float3 *>(stack.base_grids[index].data), dst.size()));
  }
  for (const SculptLayer &layer : stack.layers) {
    if (!layer.grids) {
      continue;
    }
    const SculptLayerGrid &grid = layer.grids[index];
    if (!grid.data || grid.samples_num != dst.size() || layer.influence == 0.0f) {
      continue;
    }
    for (const int i : dst.index_range()) {
      dst[i] += float3(grid.data[i]) * layer.influence;
    }
  }
}

void evaluate(Mesh &mesh)
{
  if (native_single(mesh) || !topology_valid(mesh)) {
    return;
  }
  const SculptLayerStack &stack = *mesh.sculpt_layer_stack;
  if (stack.level) {
    for (int i = 0; i < stack.grids_num; i++) {
      compose_grid(mesh, i);
    }
  }
  if (stack.base_positions) {
    MutableSpan<float3> dst = mesh.vert_positions_for_write();
    dst.copy_from(Span(reinterpret_cast<const float3 *>(stack.base_positions), stack.verts_num));
    for (const SculptLayer &layer : stack.layers) {
      if (!layer.positions || layer.influence == 0.0f) {
        continue;
      }
      for (const int i : dst.index_range()) {
        dst[i] += float3(layer.positions[i]) * layer.influence;
      }
    }
    mesh.tag_positions_changed();
  }
}

bool remove(Object &object)
{
  const bool can_evaluate = support_error(object, false) == nullptr;
  Mesh &mesh = *id_cast<Mesh *>(object.data);
  SculptLayer *layer = active(mesh);
  if (!layer) {
    return false;
  }
  auto &stack = *mesh.sculpt_layer_stack;
  if (layer == stack.layers.first) {
    return false;
  }
  BLI_remlink(&stack.layers, layer);
  MEM_SAFE_DELETE(layer->positions);
  free_grids(layer->grids, stack.grids_num);
  MEM_delete(layer);
  stack.active_index = std::max(
      0, std::min(stack.active_index, BLI_listbase_count(&stack.layers) - 1));
  if (can_evaluate) {
    evaluate(mesh);
  }
  if (native_single(mesh)) {
    release_base(stack);
  }
  return true;
}

bool bake(Object &object)
{
  if (!ensure(object) || support_error(object, false)) {
    return false;
  }
  multires_flush_sculpt_updates(&object);
  SculptLayerStack &stack = *id_cast<Mesh *>(object.data)->sculpt_layer_stack;
  auto *first = static_cast<SculptLayer *>(stack.layers.first);
  for (SculptLayer *layer = first->next; layer;) {
    SculptLayer *next = layer->next;
    BLI_remlink(&stack.layers, layer);
    MEM_SAFE_DELETE(layer->positions);
    free_grids(layer->grids, stack.grids_num);
    MEM_delete(layer);
    layer = next;
  }
  release_base(stack);
  first->influence = 1.0f;
  stack.active_index = 0;
  return true;
}

void record_multires(Object &object, Edit &edit)
{
  Mesh &mesh = *id_cast<Mesh *>(object.data);
  SculptLayerStack &stack = *mesh.sculpt_layer_stack;
  SculptLayer *layer = active(mesh);
  if (!layer || native_single(mesh) || layer->influence <= 0.0f || !stack.level) {
    return;
  }
  /* Native reshape preserves and transports high-resolution detail when a lower level is edited.
   * Compare its final displacement to the pre-stroke composition; only changed grids need undo. */
  multires_flush_sculpt_updates(&object);
  const auto *mdisps = static_cast<const MDisps *>(
      CustomData_get_layer(&mesh.corner_data, CD_MDISPS));
  if (!mdisps) {
    return;
  }
  Array<float3> expected(mdisps[0].totdisp);
  for (int grid = 0; grid < stack.grids_num; grid++) {
    const int count = mdisps[grid].totdisp;
    expected.fill(float3(0));
    if (stack.base_grids[grid].data) {
      expected.as_mutable_span().copy_from(
          Span(reinterpret_cast<const float3 *>(stack.base_grids[grid].data), count));
    }
    for (const SculptLayer &item : stack.layers) {
      if (item.grids && item.grids[grid].data && item.influence != 0.0f) {
        for (int i = 0; i < count; i++) {
          expected[i] += float3(item.grids[grid].data[i]) * item.influence;
        }
      }
    }
    bool changed = false;
    for (int i = 0; i < count; i++) {
      changed |= expected[i] != float3(mdisps[grid].disps[i]);
    }
    if (!changed) {
      continue;
    }
    SculptLayerGrid &saved = layer->uid == 0 ? stack.base_grids[grid] : layer->grids[grid];
    EditChunk chunk;
    chunk.grid = grid;
    chunk.allocated = saved.data != nullptr;
    chunk.samples = Array<float3>(count, float3(0));
    if (saved.data) {
      chunk.samples.as_mutable_span().copy_from(
          Span(reinterpret_cast<const float3 *>(saved.data), count));
    }
    else {
      saved.samples_num = count;
      saved.data = reinterpret_cast<float (*)[3]>(MEM_new_array_zeroed<float3>(count, __func__));
    }
    for (int i = 0; i < count; i++) {
      const float3 value = chunk.samples[i] +
                           (float3(mdisps[grid].disps[i]) - expected[i]) / layer->influence;
      copy_v3_v3(saved.data[i], value);
    }
    edit.uid = layer->uid == 0 ? -1 : layer->uid;
    edit.chunks.append(std::move(chunk));
  }
}

static SculptLayerGrid *subdivided_grids(Object &object,
                                         const MultiresModifierData &mmd,
                                         const SculptLayerGrid *base,
                                         const SculptLayerGrid *delta,
                                         const int level,
                                         const MultiresSubdivideModeType mode)
{
  Mesh &mesh = *id_cast<Mesh *>(object.data);
  Mesh *temporary = BKE_mesh_copy_for_eval(mesh);
  Object work = dna::shallow_copy(object);
  bke::ObjectRuntime runtime;
  work.runtime = &runtime;
  work.mode = OB_MODE_OBJECT;
  work.data = &temporary->id;
  MultiresModifierData settings = dna::shallow_copy(mmd);
  if (settings.totlvl) {
    auto *mdisps = static_cast<MDisps *>(CustomData_get_layer_for_write(
        &temporary->corner_data, CD_MDISPS, temporary->corners_num));
    for (int grid = 0; grid < temporary->corners_num; grid++) {
      for (int i = 0; i < mdisps[grid].totdisp; i++) {
        float3 value(0);
        if (base[grid].data) {
          value += float3(base[grid].data[i]);
        }
        if (delta && delta[grid].data) {
          value += float3(delta[grid].data[i]);
        }
        copy_v3_v3(mdisps[grid].disps[i], value);
      }
    }
  }
  multiresModifier_subdivide_to_level(&work, &settings, level, mode);
  SculptLayerGrid *result = nullptr;
  if (settings.totlvl == level) {
    result = MEM_new_array<SculptLayerGrid>(mesh.corners_num, __func__);
    const auto *mdisps = static_cast<const MDisps *>(
        CustomData_get_layer(&temporary->corner_data, CD_MDISPS));
    for (int grid = 0; grid < mesh.corners_num; grid++) {
      result[grid].samples_num = mdisps[grid].totdisp;
      result[grid].data = MEM_dupalloc(mdisps[grid].disps);
    }
  }
  BKE_id_free(nullptr, temporary);
  return result;
}

bool subdivide(Object &object,
               MultiresModifierData &mmd,
               const int level,
               const MultiresSubdivideModeType mode)
{
  Mesh &mesh = *id_cast<Mesh *>(object.data);
  SculptLayerStack *stack = mesh.sculpt_layer_stack;
  if (native_single(mesh)) {
    mesh.sculpt_layer_stack = nullptr;
    multiresModifier_subdivide_to_level(&object, &mmd, level, mode);
    mesh.sculpt_layer_stack = stack;
    stack->level = mmd.totlvl;
    return true;
  }
  if (!stack || stack->level != mmd.totlvl || support_error(object, false)) {
    return false;
  }
  multires_flush_sculpt_updates(&object);
  SculptLayerGrid *base = subdivided_grids(object, mmd, stack->base_grids, nullptr, level, mode);
  if (!base) {
    return false;
  }
  Vector<SculptLayerGrid *> layers;
  for (const SculptLayer &layer : stack->layers) {
    if (layer.uid == 0) {
      layers.append(nullptr);
      continue;
    }
    bool populated = false;
    for (int grid = 0; grid < stack->grids_num; grid++) {
      populated |= layer.grids[grid].data != nullptr;
    }
    SculptLayerGrid *delta = populated ?
                                 subdivided_grids(
                                     object, mmd, stack->base_grids, layer.grids, level, mode) :
                                 MEM_new_array<SculptLayerGrid>(stack->grids_num, __func__);
    if (!delta) {
      free_grids(base, stack->grids_num);
      for (SculptLayerGrid *data : layers) {
        free_grids(data, stack->grids_num);
      }
      return false;
    }
    if (populated) {
      for (int grid = 0; grid < stack->grids_num; grid++) {
        bool nonzero = false;
        for (int i = 0; i < delta[grid].samples_num; i++) {
          const float3 value = float3(delta[grid].data[i]) - float3(base[grid].data[i]);
          copy_v3_v3(delta[grid].data[i], value);
          nonzero |= value != float3(0);
        }
        if (!nonzero) {
          MEM_SAFE_DELETE(delta[grid].data);
          delta[grid].samples_num = 0;
        }
      }
    }
    layers.append(delta);
  }
  /* Re-enter the native operation with authoring detached; temporary meshes have no stack. */
  mesh.sculpt_layer_stack = nullptr;
  multiresModifier_subdivide_to_level(&object, &mmd, level, mode);
  mesh.sculpt_layer_stack = stack;
  if (mmd.totlvl != level) {
    free_grids(base, stack->grids_num);
    for (SculptLayerGrid *data : layers) {
      free_grids(data, stack->grids_num);
    }
    return true;
  }
  free_grids(stack->base_grids, stack->grids_num);
  stack->base_grids = base;
  int index = 0;
  for (SculptLayer &layer : stack->layers) {
    free_grids(layer.grids, stack->grids_num);
    layer.grids = layers[index++];
  }
  stack->level = level;
  evaluate(mesh);
  return true;
}

void delete_higher(Mesh &mesh, const int level)
{
  SculptLayerStack *stack = mesh.sculpt_layer_stack;
  if (native_single(mesh)) {
    stack->level = level;
    return;
  }
  if (!stack || level >= stack->level || stack->topology_hash != topology_hash(mesh)) {
    return;
  }
  const int old_size = bke::subdiv::grid_size_from_level(stack->level);
  const int size = level ? bke::subdiv::grid_size_from_level(level) : 0;
  auto downsample = [&](SculptLayerGrid *grids) {
    if (!grids) {
      return;
    }
    for (int grid = 0; grid < stack->grids_num; grid++) {
      if (!grids[grid].data) {
        continue;
      }
      float (*data)[3] = size ? reinterpret_cast<float (*)[3]>(
                                    MEM_new_array<float3>(size * size, __func__)) :
                                nullptr;
      const int step = size ? (old_size - 1) / (size - 1) : 0;
      for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
          copy_v3_v3(data[y * size + x], grids[grid].data[y * step * old_size + x * step]);
        }
      }
      MEM_delete(grids[grid].data);
      grids[grid].data = data;
      grids[grid].samples_num = size * size;
    }
  };
  downsample(stack->base_grids);
  for (SculptLayer &layer : stack->layers) {
    downsample(layer.grids);
  }
  stack->level = level;
}

void record_mesh(Object &object, const Span<int> indices, const Span<float3> before, Edit &edit)
{
  Mesh &mesh = *id_cast<Mesh *>(object.data);
  SculptLayer *layer = active(mesh);
  if (!layer || native_single(mesh) || layer->influence <= 0.0f) {
    return;
  }
  auto &saved = layer->uid == 0 ? mesh.sculpt_layer_stack->base_positions : layer->positions;
  const Span<float3> positions = mesh.vert_positions();
  EditChunk chunk;
  Vector<int> changed;
  for (const int i : indices.index_range()) {
    if (positions[indices[i]] != before[i]) {
      changed.append(i);
    }
  }
  if (changed.is_empty()) {
    return;
  }
  if (edit.chunks.is_empty()) {
    edit.uid = layer->uid == 0 ? -1 : layer->uid;
    edit.mesh_allocated = saved != nullptr;
  }
  if (!saved) {
    saved = reinterpret_cast<float (*)[3]>(MEM_new_array_zeroed<float3>(mesh.verts_num, __func__));
  }
  chunk.indices.reinitialize(changed.size());
  chunk.samples.reinitialize(changed.size());
  for (const int n : changed.index_range()) {
    const int i = changed[n], vertex = indices[i];
    chunk.indices[n] = vertex;
    chunk.samples[n] = float3(saved[vertex]);
    float3 value = chunk.samples[n] + (positions[vertex] - before[i]) / layer->influence;
    copy_v3_v3(saved[vertex], value);
  }
  edit.chunks.append(std::move(chunk));
}

void restore_created_stack(Mesh &mesh, const SculptLayerStack &after, const bool undo)
{
  if (mesh.sculpt_layer_stack) {
    return;
  }
  mesh.sculpt_layer_stack = copy(&after);
  if (!undo) {
    /* Before the creating stroke, this new stack had no layer displacement buffers. */
    for (SculptLayer &layer : mesh.sculpt_layer_stack->layers) {
      MEM_SAFE_DELETE(layer.positions);
      if (layer.grids) {
        for (int i = 0; i < after.grids_num; i++) {
          MEM_SAFE_DELETE(layer.grids[i].data);
          layer.grids[i].samples_num = 0;
        }
      }
    }
  }
}

void swap_edit(Object &object, Edit &edit)
{
  Mesh &mesh = *id_cast<Mesh *>(object.data);
  if (!mesh.sculpt_layer_stack || !edit.uid) {
    return;
  }
  SculptLayerStack &stack = *mesh.sculpt_layer_stack;
  SculptLayer *layer = nullptr;
  for (SculptLayer &candidate : stack.layers) {
    if (candidate.uid == edit.uid || (edit.uid == -1 && candidate.uid == 0)) {
      layer = &candidate;
      break;
    }
  }
  if (!layer) {
    return;
  }
  if (!edit.chunks.is_empty() && edit.chunks[0].grid >= 0) {
    for (EditChunk &chunk : edit.chunks) {
      SculptLayerGrid &grid = edit.uid == -1 ? stack.base_grids[chunk.grid] :
                                               layer->grids[chunk.grid];
      const bool allocated = grid.data != nullptr;
      if (!grid.data) {
        grid.data = reinterpret_cast<float (*)[3]>(
            MEM_new_array_zeroed<float3>(chunk.samples.size(), __func__));
        grid.samples_num = chunk.samples.size();
      }
      MutableSpan data(reinterpret_cast<float3 *>(grid.data), grid.samples_num);
      for (const int i : data.index_range()) {
        std::swap(data[i], chunk.samples[i]);
      }
      if (!chunk.allocated) {
        MEM_SAFE_DELETE(grid.data);
        grid.samples_num = 0;
      }
      chunk.allocated = allocated;
      compose_grid(mesh, chunk.grid);
    }
    if (object.runtime->sculpt_session && object.runtime->sculpt_session->subdiv_ccg) {
      object.runtime->sculpt_session->subdiv_ccg->dirty.coords = false;
    }
  }
  else {
    auto &saved = edit.uid == -1 ? stack.base_positions : layer->positions;
    const bool allocated = saved != nullptr;
    if (!saved) {
      saved = reinterpret_cast<float (*)[3]>(
          MEM_new_array_zeroed<float3>(mesh.verts_num, __func__));
    }
    for (EditChunk &chunk : edit.chunks) {
      for (const int i : chunk.indices.index_range()) {
        std::swap(reinterpret_cast<float3 *>(saved)[chunk.indices[i]], chunk.samples[i]);
      }
    }
    if (!edit.mesh_allocated) {
      MEM_SAFE_DELETE(saved);
    }
    edit.mesh_allocated = allocated;
  }
}

}  // namespace blender::bke::sculpt_layers
