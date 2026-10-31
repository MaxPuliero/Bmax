/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 *
 * Experimental numerical backend for additive sculpt layers. Not attached to Mesh DNA or Sculpt
 * Mode yet. The adapter must provide stable grid indices, a topology revision that changes for
 * connectivity/corner-order/base-frame changes, and net stroke deltas in the storage basis.
 *
 * Keeping this backend independent of PBVH, DNA, and evaluated meshes permits verification of its
 * transaction semantics before connecting it to Blender's geometry and persistence lifecycles.
 * Access to a Stack is serial: a future PBVH adapter must gather parallel writes before staging.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace blender::bke::sculpt::layers {

using Vector3 = std::array<float, 3>;
using LayerID = uint64_t;

/** All grids have the same resolution. This backend initially records at the highest level only.
 */
struct Domain {
  uint64_t topology_revision;
  int grids_num;
  int samples_per_grid;
  int level;

  bool operator==(const Domain &) const = default;
};

enum class Status {
  Ok,
  NoChange,
  InvalidDomain,
  InvalidLayer,
  InvalidGrid,
  InvalidSize,
  InvalidValue,
  StrokeInProgress,
  NoStroke,
  LayerNotRecordable,
  InvalidUndo,
};

struct LayerInfo {
  LayerID id;
  std::string name;
  float strength;
  bool visible;
  bool locked;
};

struct StorageInfo {
  size_t populated_grids;
  size_t samples;
  /** Payload only; excludes container capacity, allocator overhead, and undo held by the caller.
   */
  size_t payload_bytes;
};

/**
 * A local stroke undo record. Holds alternate buffers only for changed grids. Swap exchanges these
 * with live buffers without reconstructing geometry. IDs and state tokens reject stale,
 * out-of-order, and cross-stack application. The adapter must integrate metadata edits with its
 * own undo history.
 */
class UndoStep {
 public:
  UndoStep();
  ~UndoStep();
  UndoStep(UndoStep &&) noexcept;
  UndoStep &operator=(UndoStep &&) noexcept;
  UndoStep(const UndoStep &) = delete;
  UndoStep &operator=(const UndoStep &) = delete;

  bool valid() const;
  size_t changed_grids() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  friend class Stack;
};

class Stack {
 public:
  /** Invalid domains throw std::invalid_argument; individual operations report Status instead. */
  explicit Stack(Domain domain);
  ~Stack();
  Stack(const Stack &) = delete;
  Stack &operator=(const Stack &) = delete;
  Stack(Stack &&) = delete;
  Stack &operator=(Stack &&) = delete;

  Domain domain() const;
  std::vector<LayerInfo> layers() const;
  StorageInfo storage() const;

  Status add(std::string name, LayerID &r_id);
  Status duplicate(LayerID id, std::string name, LayerID &r_id);
  Status remove(LayerID id);
  Status rename(LayerID id, std::string name);
  Status set_strength(LayerID id, float strength);
  Status set_visible(LayerID id, bool visible);
  Status set_locked(LayerID id, bool locked);

  /**
   * base is the caller-owned authoritative displacement in the same basis as layer vectors.
   * Includes staged edits during a stroke. Output may alias base; an error leaves output
   * unchanged. Composition is additive, deterministic in insertion order, and independent of PBVH
   * layout.
   */
  Status compose_grid(int grid, std::span<const Vector3> base, std::span<Vector3> output) const;

  /** Capture the target and freeze all metadata until commit/cancel. Strength must equal 1. */
  Status begin_stroke(LayerID id, Domain evaluated_domain);

  /**
   * Replace the NET displacement since stroke start for this grid, not an incremental dab delta.
   * This makes repeated/anchored previews idempotent. The adapter owns coordinate snapshots and
   * frame conversion, and must submit seam-neighbor writes too. Persistent buffers stay unchanged.
   * Invalid data leaves the previous preview unchanged. No zero-only persistent grids are stored.
   */
  Status stage_grid_delta(int grid, std::span<const Vector3> net_delta);

  /**
   * Transfer staged data into the active layer and return a move-only undo record. NoChange closes
   * an empty stroke and returns an invalid record. No whole-layer/surface copy is made on commit.
   * Allocation failures propagate as exceptions; preparation precedes persistent mutation.
   */
  Status commit_stroke(UndoStep &r_undo);
  Status cancel_stroke();
  bool stroke_in_progress() const;
  size_t staged_grids() const;

  /**
   * Repeated calls alternate Undo/Redo. Metadata mutations invalidate existing stroke records.
   * The caller owns history and must discard abandoned redo records after branching.
   */
  Status swap_undo(UndoStep &undo);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace blender::bke::sculpt::layers
