/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace blender::bke::remesh {

/** Opt-in CPU stage timings; disabled unless BMAX_REMESH_PROFILE=1 at startup.
 * Parent stages include nested stages and must not be summed with their children. */
class ProfileTimer {
  using Clock = std::chrono::steady_clock;
  bool enabled_;
  Clock::time_point start_;

 public:
  ProfileTimer()
  {
    static const bool enabled = [] {
      const char *value = std::getenv("BMAX_REMESH_PROFILE");
      return value && value[0] == '1';
    }();
    enabled_ = enabled;
    if (enabled_) {
      start_ = Clock::now();
    }
  }

  void step(const char *stage)
  {
    if (!enabled_) {
      return;
    }
    const double milliseconds =
        std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
    std::printf("BMAX_REMESH_PROFILE %s %.6f\n", stage, milliseconds);
    start_ = Clock::now();
  }
};

}  // namespace blender::bke::remesh
