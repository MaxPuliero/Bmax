# Blender 5.2.3 integration audit

Implementation: [`776763085e8a`](https://github.com/MaxPuliero/Bmax/commit/776763085e8a3cdc2b17eae2680d6f22841847ac), included in `main`.

## Source and scope

Official release tag: `v5.2.3`, commit `e13b728c6880d3084d9d57c30031ed5df8b39dee`, released October 22, 2026. Previous upstream base: `v5.2.2`, commit `d13f752e3b9c4f8c261cda552b1021f8bcc0382c`. Integration starts from Bmax `08c66f5c43a6be487b107c728d0c4a3b6240e865` on `codex/blender-5.2.3-sync`.

The official range contains 124 commits and changes 250 files. Bmax imports the 155 production-source/script files through a three-way application of the exact release-to-release delta, without merging unrelated upstream history. Existing Bmax changes are retained. The 143 imported files without Bmax edits were checked against the official release byte-for-byte after line-ending normalization; the other 12 files merged cleanly and their resulting diffs were reviewed. An additional comparison of all added/removed Bmax lines, excluding only hunk context, confirmed the exact original Bmax delta is retained in every shared file.

To follow the repository's rule against adding development test files, new upstream fixtures and changes under `tests/`, plus the upstream `idprop_test.cc` change, are excluded from the production import. Existing inherited tests are retained. No new Bmax tests are stored in the repository. Library submodule pointers and build configuration do not change.

Official references: [5.2 LTS releases](https://www.blender.org/releases/5-2/), [release tag](https://projects.blender.org/blender/blender/src/tag/v5.2.3).

## Impact on Bmax

| Area | Upstream changes and compatibility assessment |
| --- | --- |
| Voxel Remesh, Fix Poles and By Loose Parts | No direct upstream edits to the Bmax remesh operator or voxel core; retain the optimized pipeline and saved optional-cleanup control. Compare enabled/disabled, adaptive, open/fair and separate-part outputs to the installed 5.2.2 build. |
| Decimate cache | No direct upstream edit to the cache implementation. The new modifier-reader size validation uses the existing modifier type metadata; Bmax's cache adds no DNA modifier member. Compare all three modes with cache enabled/disabled and check saved flags. |
| Multires masks | Upstream now ensures grid masks when subdividing an object with a sculpt-mask attribute, including Object Mode. This corrects missing mask transfer without changing Bmax color-grid logic. |
| Multires color and file reading | Stricter block/struct/array validation makes old Bmax files a priority. `GridPaintColor` and its nested float arrays are already written with matching element counts. A genuinely painted 5.2.2 fixture, not only a coarse color layer, is used for exact evaluated-color and modifier-application comparisons. |
| Bone picking and outlines | Upstream ignores non-bone hits in bone selection. Bmax drawing and hit encoding remain unchanged; verify actual viewport picking plus native defaults. |
| UV and Image Editor | Upstream fixes UV ring-selection hangs and image sampling outside repeated-image bounds. The Bmax opacity/framing logic and original-mesh UV diagnostics remain present. |
| Cycles and instance colors | Thread-state/session changes, point/hair shader fixes and color derivatives merge alongside the Bmax source-instance random logic; the custom node/storage paths remain present. |
| Texture painting/GPU | Import image lifetime/mipmap/VRAM fixes, OpenGL image unbinding and Vulkan cache fixes. Driver- and HIP-specific paths require hardware not covered by local CPU checks. |
| UI/branding | Preferences and file-open auto-run handling import the native fixes while Bmax themes, branding and report links stay present. |

The 12 shared files are Cycles object sync, OSL services, SVM geometry, shader nodes; Python WM operators and preferences UI; Image Editor operations and space registration; RNA space and user preferences; window handling and command-line arguments. No manual conflict resolution or Bmax algorithm rewrite was needed during application.

## Build and validation

The complete Release rebuild and INSTALL both succeeded. The release adds `SpaceType::struct_size`, changes global auto-execution state, and removes a member from Cycles `BlenderSession`; these are shared C++ layout changes. A forced `/t:Rebuild` of the complete `blender` target with project references, followed by INSTALL, is required; an incremental-only link is not considered sufficient. Existing build caches and dependencies are retained.

All verification scripts, generated scenes, golden buffers, logs and recovery files are kept in an external temporary directory. The user's existing Desktop instance is preserved. An optional attempt to fetch official corrupt-file fixtures via Git LFS required projects.blender.org authentication and was cancelled; it is not retried interactively. Those upstream fixtures are not part of the local validation claims.


The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe`, timestamp 2026-10-23 17:46:30 JST. It reports Bmax 5.2.3 LTS and base hash `08c66f5c43a6`, because the build preceded committing the integration.

Final runtime verification:

- **65 guarded-memory background assertions:** four remesh configurations spanning closed/open/fair, adaptive output, independent parts and cleanup enabled/disabled; three Decimate modes with cached/uncached equality; all six Split modes; native Manifold Boolean; armature defaults; legacy mesh/cache/camera flags and new save/reload. Exact geometry/inspected-attribute signatures match the 5.2.2 reference fixtures. A genuinely painted old Multires file retains the exact evaluated RGBA buffer and its applied modifier output. A full sculpt mask remains intact after Object Mode Multires subdivision and a Sculpt roundtrip.
- **41 GUI assertions per active backend, OpenGL and Vulkan:** remesh with both cleanup states, Face Set cuts retaining surface area and distinct IDs, actual Manifold Box Trim, viewport bone picking, redraw and exact one-step Undo/Redo with Sculpt Mode retained. Signatures cover positions, face connectivity and Face Set data where present.
- **Fresh Multires Paint on both backends:** a real Paint Hard stroke on a newly subdivided mesh changes the color data; all 1,536 evaluated Face Corner RGBA entries match the painted 5.2.2 reference exactly, including between backends. Old-file evaluation/application checks are independent of this fresh-paint test.
- **Cycles CPU smoke render:** a 64×64 render reads the saved Multires color attribute through an emission material, with nonzero color and finite pixel values.
- Python syntax and staged/unstaged diff checks pass. No manual source conflict resolution or feature-specific Bmax code adaptation was required. No new timing benchmarks were run; earlier 5.2.2 performance results are not relabeled as new 5.2.3 measurements.

One installed-runtime adjustment was needed: CMake INSTALL retains modules removed from source. The obsolete `_animsys_refactor.py` was verified against official 5.2.2 and removed from the installed modules directory with its matching `.pyc` cache, matching the 5.2.3 removal. No unrelated user/runtime files were removed.

These are focused fixture checks, not exhaustive coverage of every upstream fix. In particular, the official corrupt-file LFS fixtures were unavailable without authentication; local validation checks valid Bmax files against the stricter reader rather than claiming those malformed-file tests. HIP/other hardware-specific fixes, all possible production topology, image/UV edge cases and every rendering configuration were not independently reproduced. Existing Bmax regression notes remain the reference for their broader earlier coverage.

After inspection, all owned follow-up verification files were removed from the external temporary directory. The installed runtime, dependency/build caches, the user's original Desktop instance and unrelated files were preserved. Desktop copies and public binary downloads remain at their existing versions.
