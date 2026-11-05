/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "BLI_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

namespace blender {
struct Brush;
struct Depsgraph;
struct Object;
struct Sculpt;
struct SculptSession;
namespace ed::sculpt_paint::thick {
struct Surface;
struct NodePositions {
  Vector<Array<float3>> positions;
};
/** Prepare the immutable nearest-sample surface once, outside parallel brush tasks. */
void prepare(Object &object, const Brush &brush);
bool is_active(const SculptSession &ss);
/** Project a proposed point into the allowed band; do not worsen pre-existing violations. */
float3 constrain(const SculptSession &ss, const float3 &before, const float3 &proposed);
NodePositions capture(const Depsgraph &depsgraph,
                      const Object &object,
                      const IndexMask &node_mask);
void apply(const Depsgraph &depsgraph,
           const Sculpt &sd,
           Object &object,
           const IndexMask &node_mask,
           const NodePositions &before);
}  // namespace ed::sculpt_paint::thick
}  // namespace blender
