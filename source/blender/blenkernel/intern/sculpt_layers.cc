/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_sculpt_layers.hh"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>

namespace blender::bke::sculpt::layers {

namespace {

using Grid = std::vector<Vector3>;
using Grids = std::map<int, Grid>;

struct Layer {
  LayerInfo info;
  Grids grids;
};

struct Stroke {
  LayerID layer;
  /** Empty vectors are tombstones for grids which become entirely zero. */
  Grids pending;
};

bool finite(const Vector3 &value)
{
  return std::all_of(value.begin(), value.end(), [](float v) { return std::isfinite(v); });
}

bool zero(const Grid &grid)
{
  return std::all_of(grid.begin(), grid.end(), [](const Vector3 &v) {
    return v[0] == 0.0f && v[1] == 0.0f && v[2] == 0.0f;
  });
}

/** Node handles exchange ownership without allocation or element copies. */
void exchange_grid(Grids &a, Grids &b, int grid)
{
  auto node_a = a.extract(grid);
  auto node_b = b.extract(grid);
  if (!node_b.empty()) {
    a.insert(std::move(node_b));
  }
  if (!node_a.empty()) {
    b.insert(std::move(node_a));
  }
}

}  // namespace

struct UndoStep::Impl {
  uint64_t owner;
  LayerID layer;
  uint64_t before_state;
  uint64_t after_state;
  bool applied = true;
  std::vector<int> grids;
  Grids alternate;
};

struct Stack::Impl {
  Domain domain;
  uint64_t owner;
  LayerID next_layer = 1;
  uint64_t state = 0;
  uint64_t next_state = 1;
  std::vector<Layer> layers;
  std::optional<Stroke> stroke;

  explicit Impl(Domain domain) : domain(domain)
  {
    static std::atomic<uint64_t> next_owner{1};
    owner = next_owner.fetch_add(1);
  }

  Layer *find(LayerID id)
  {
    const auto it = std::find_if(
        layers.begin(), layers.end(), [id](const Layer &layer) { return layer.info.id == id; });
    return it == layers.end() ? nullptr : &*it;
  }

  bool valid_grid(int grid) const
  {
    return grid >= 0 && grid < domain.grids_num;
  }

  void change_state()
  {
    state = next_state++;
  }
};

UndoStep::UndoStep() = default;
UndoStep::~UndoStep() = default;
UndoStep::UndoStep(UndoStep &&) noexcept = default;
UndoStep &UndoStep::operator=(UndoStep &&) noexcept = default;

bool UndoStep::valid() const
{
  return bool(impl_);
}

size_t UndoStep::changed_grids() const
{
  return impl_ ? impl_->grids.size() : 0;
}

Stack::Stack(const Domain domain)
{
  if (domain.grids_num <= 0 || domain.samples_per_grid <= 0 || domain.level <= 0 ||
      domain.topology_revision == 0)
  {
    throw std::invalid_argument("Sculpt layers require a non-empty versioned grid domain");
  }
  impl_ = std::make_unique<Impl>(domain);
}

Stack::~Stack() = default;

Domain Stack::domain() const
{
  return impl_->domain;
}

std::vector<LayerInfo> Stack::layers() const
{
  std::vector<LayerInfo> result;
  result.reserve(impl_->layers.size());
  for (const Layer &layer : impl_->layers) {
    result.push_back(layer.info);
  }
  return result;
}

StorageInfo Stack::storage() const
{
  StorageInfo result{};
  for (const Layer &layer : impl_->layers) {
    result.populated_grids += layer.grids.size();
    for (const auto &[index, data] : layer.grids) {
      result.samples += data.size();
    }
  }
  result.payload_bytes = result.samples * sizeof(Vector3);
  return result;
}

Status Stack::add(std::string name, LayerID &r_id)
{
  if (impl_->stroke) {
    return Status::StrokeInProgress;
  }
  const LayerID id = impl_->next_layer;
  if (id == std::numeric_limits<LayerID>::max()) {
    return Status::InvalidValue;
  }
  impl_->layers.push_back({{id, std::move(name), 1.0f, true, false}, {}});
  impl_->next_layer++;
  impl_->change_state();
  r_id = id;
  return Status::Ok;
}

Status Stack::duplicate(const LayerID id, std::string name, LayerID &r_id)
{
  if (impl_->stroke) {
    return Status::StrokeInProgress;
  }
  const Layer *source = impl_->find(id);
  if (!source) {
    return Status::InvalidLayer;
  }
  if (impl_->next_layer == std::numeric_limits<LayerID>::max()) {
    return Status::InvalidValue;
  }
  /* Explicit duplication may copy the layer. Ordinary strokes never do. */
  Layer copy = *source;
  copy.info.id = impl_->next_layer;
  copy.info.name = std::move(name);
  impl_->layers.push_back(std::move(copy));
  r_id = impl_->next_layer++;
  impl_->change_state();
  return Status::Ok;
}

Status Stack::remove(const LayerID id)
{
  if (impl_->stroke) {
    return Status::StrokeInProgress;
  }
  const auto it = std::find_if(impl_->layers.begin(),
                               impl_->layers.end(),
                               [id](const Layer &layer) { return layer.info.id == id; });
  if (it == impl_->layers.end()) {
    return Status::InvalidLayer;
  }
  impl_->layers.erase(it);
  impl_->change_state();
  return Status::Ok;
}

Status Stack::rename(const LayerID id, std::string name)
{
  if (impl_->stroke) {
    return Status::StrokeInProgress;
  }
  Layer *layer = impl_->find(id);
  if (!layer) {
    return Status::InvalidLayer;
  }
  if (layer->info.name == name) {
    return Status::NoChange;
  }
  layer->info.name = std::move(name);
  impl_->change_state();
  return Status::Ok;
}

Status Stack::set_strength(const LayerID id, const float strength)
{
  if (impl_->stroke) {
    return Status::StrokeInProgress;
  }
  Layer *layer = impl_->find(id);
  if (!layer) {
    return Status::InvalidLayer;
  }
  if (!std::isfinite(strength) || strength < 0.0f || strength > 1.0f) {
    return Status::InvalidValue;
  }
  if (layer->info.strength == strength) {
    return Status::NoChange;
  }
  layer->info.strength = strength;
  impl_->change_state();
  return Status::Ok;
}

Status Stack::set_visible(const LayerID id, const bool visible)
{
  if (impl_->stroke) {
    return Status::StrokeInProgress;
  }
  Layer *layer = impl_->find(id);
  if (!layer) {
    return Status::InvalidLayer;
  }
  if (layer->info.visible == visible) {
    return Status::NoChange;
  }
  layer->info.visible = visible;
  impl_->change_state();
  return Status::Ok;
}

Status Stack::set_locked(const LayerID id, const bool locked)
{
  if (impl_->stroke) {
    return Status::StrokeInProgress;
  }
  Layer *layer = impl_->find(id);
  if (!layer) {
    return Status::InvalidLayer;
  }
  if (layer->info.locked == locked) {
    return Status::NoChange;
  }
  layer->info.locked = locked;
  impl_->change_state();
  return Status::Ok;
}

Status Stack::compose_grid(const int grid,
                           const std::span<const Vector3> base,
                           const std::span<Vector3> output) const
{
  if (!impl_->valid_grid(grid)) {
    return Status::InvalidGrid;
  }
  if (base.size() != size_t(impl_->domain.samples_per_grid) || output.size() != base.size()) {
    return Status::InvalidSize;
  }
  /* Compute before writing to support aliasing and leave output intact on invalid/overflow data.
   */
  Grid result(base.begin(), base.end());
  for (const Vector3 &value : result) {
    if (!finite(value)) {
      return Status::InvalidValue;
    }
  }
  for (const Layer &layer : impl_->layers) {
    if (!layer.info.visible || layer.info.strength == 0.0f) {
      continue;
    }
    const Grid *data = nullptr;
    if (impl_->stroke && impl_->stroke->layer == layer.info.id) {
      const auto pending = impl_->stroke->pending.find(grid);
      if (pending != impl_->stroke->pending.end()) {
        data = &pending->second;
      }
    }
    if (!data) {
      const auto stored = layer.grids.find(grid);
      if (stored != layer.grids.end()) {
        data = &stored->second;
      }
    }
    if (!data || data->empty()) {
      continue;
    }
    for (size_t i = 0; i < result.size(); i++) {
      for (int axis = 0; axis < 3; axis++) {
        result[i][axis] += layer.info.strength * (*data)[i][axis];
      }
      if (!finite(result[i])) {
        return Status::InvalidValue;
      }
    }
  }
  std::copy(result.begin(), result.end(), output.begin());
  return Status::Ok;
}

Status Stack::begin_stroke(const LayerID id, const Domain evaluated_domain)
{
  if (impl_->stroke) {
    return Status::StrokeInProgress;
  }
  if (evaluated_domain != impl_->domain) {
    return Status::InvalidDomain;
  }
  const Layer *layer = impl_->find(id);
  if (!layer) {
    return Status::InvalidLayer;
  }
  if (layer->info.locked || !layer->info.visible || layer->info.strength != 1.0f) {
    return Status::LayerNotRecordable;
  }
  impl_->stroke.emplace(Stroke{id, {}});
  return Status::Ok;
}

Status Stack::stage_grid_delta(const int grid, const std::span<const Vector3> net_delta)
{
  if (!impl_->stroke) {
    return Status::NoStroke;
  }
  if (!impl_->valid_grid(grid)) {
    return Status::InvalidGrid;
  }
  if (net_delta.size() != size_t(impl_->domain.samples_per_grid)) {
    return Status::InvalidSize;
  }
  const Layer &layer = *impl_->find(impl_->stroke->layer);
  const auto before = layer.grids.find(grid);
  Grid after(net_delta.size());
  for (size_t i = 0; i < net_delta.size(); i++) {
    if (!finite(net_delta[i])) {
      return Status::InvalidValue;
    }
    for (int axis = 0; axis < 3; axis++) {
      after[i][axis] = (before == layer.grids.end() ? 0.0f : before->second[i][axis]) +
                       net_delta[i][axis];
    }
    if (!finite(after[i])) {
      return Status::InvalidValue;
    }
  }
  if (zero(after)) {
    after.clear();
  }
  if ((before == layer.grids.end() && after.empty()) ||
      (before != layer.grids.end() && before->second == after))
  {
    impl_->stroke->pending.erase(grid);
    return Status::NoChange;
  }
  impl_->stroke->pending.insert_or_assign(grid, std::move(after));
  return Status::Ok;
}

Status Stack::commit_stroke(UndoStep &r_undo)
{
  if (!impl_->stroke) {
    return Status::NoStroke;
  }
  if (impl_->stroke->pending.empty()) {
    impl_->stroke.reset();
    r_undo = UndoStep{};
    return Status::NoChange;
  }
  /* Allocate all undo bookkeeping before changing any persistent data. */
  auto undo = std::make_unique<UndoStep::Impl>();
  undo->owner = impl_->owner;
  undo->layer = impl_->stroke->layer;
  undo->before_state = impl_->state;
  undo->after_state = impl_->next_state;
  undo->grids.reserve(impl_->stroke->pending.size());
  for (const auto &[grid, data] : impl_->stroke->pending) {
    undo->grids.push_back(grid);
  }
  Grids &persistent = impl_->find(undo->layer)->grids;
  for (const int grid : undo->grids) {
    auto before = persistent.extract(grid);
    if (!before.empty()) {
      undo->alternate.insert(std::move(before));
    }
    auto after = impl_->stroke->pending.extract(grid);
    if (!after.mapped().empty()) {
      persistent.insert(std::move(after));
    }
  }
  impl_->change_state();
  impl_->stroke.reset();
  r_undo.impl_ = std::move(undo);
  return Status::Ok;
}

Status Stack::cancel_stroke()
{
  if (!impl_->stroke) {
    return Status::NoStroke;
  }
  impl_->stroke.reset();
  return Status::Ok;
}

bool Stack::stroke_in_progress() const
{
  return impl_->stroke.has_value();
}

size_t Stack::staged_grids() const
{
  return impl_->stroke ? impl_->stroke->pending.size() : 0;
}

Status Stack::swap_undo(UndoStep &undo)
{
  if (impl_->stroke) {
    return Status::StrokeInProgress;
  }
  if (!undo.impl_ || undo.impl_->owner != impl_->owner) {
    return Status::InvalidUndo;
  }
  UndoStep::Impl &record = *undo.impl_;
  if (impl_->state != (record.applied ? record.after_state : record.before_state)) {
    return Status::InvalidUndo;
  }
  Layer *layer = impl_->find(record.layer);
  if (!layer) {
    return Status::InvalidUndo;
  }
  for (const int grid : record.grids) {
    exchange_grid(layer->grids, record.alternate, grid);
  }
  record.applied = !record.applied;
  impl_->state = record.applied ? record.after_state : record.before_state;
  return Status::Ok;
}

}  // namespace blender::bke::sculpt::layers
