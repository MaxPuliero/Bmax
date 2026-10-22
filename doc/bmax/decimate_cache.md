# Decimate result cache

## Behavior

Decimate automatically retains the most recent input mesh, modifier settings, and resulting mesh in runtime memory. When the stack evaluates again, it compares the incoming mesh and settings before deciding whether to reuse the result. This covers Collapse, Un-Subdivide, and Planar modes. The **Cache Result** checkbox is enabled by default and appears in all three modes. Disable it to free existing snapshots on the next evaluation, skip input comparisons, and run normal uncached decimation. Re-enabling it creates a new entry on the next valid calculation.

An unchanged input avoids mesh-to-BMesh conversion and decimation when adding or editing later modifiers. Editing the base mesh, changing an earlier modifier, changing a shape key, or updating an external dependency still evaluates the upstream stack; if that changes the incoming geometry or data, Decimate recomputes. The cache does not accelerate that first evaluation or a genuinely changed input.

Apply uses the same modifier evaluation callback and can reuse its result when its input is identical. Blender applies a modifier to its normal application input, which can differ from the viewport input, particularly for a modifier that is not first in the stack. The cache preserves those existing application semantics and recomputes when needed. Extra data requirements from later modifiers or a different evaluation context may also require an initial fresh evaluation.

## Ownership and comparison

The cache uses the existing `ModifierData.runtime` pointer and depsgraph runtime backup keyed by modifier persistent UID. It is private to each evaluated modifier and dependency graph, is not serialized, and is recreated after opening a file. The `use_cache` preference itself is saved per modifier, using `MOD_DECIM_FLAG_DISABLE_CACHE` (bit 4) in the existing flag field. This negative encoding enables caching for both new modifiers and older files with the bit unset, without migration or a structure layout change. The free-data and runtime-free callbacks release both mesh snapshots when the modifier or evaluated object goes away. Duplicating a modifier starts with an empty runtime cache.

The input comparison checks vertex/edge/face/corner counts, face offsets, all stored generic attributes (including topology arrays), vertex group names and order, normal weighting settings (also used by the BMesh conversion), active face and selection history, and supported legacy custom-data layers. Layers marked `CD_FLAG_NOCOPY` are ignored consistently with BMesh conversion and evaluated mesh copying; requiring them would defeat reuse on meshes with unused vertex groups. Shared buffers can be recognized by pointer identity because the snapshots retain ownership through Blender implicit sharing; newly produced buffers are compared by their actual contents rather than their allocation address. There is no hash collision risk. Deform weights are compared through their nested arrays. Unknown legacy layers, including pointer-containing Multires grid layers, bypass caching conservatively.

The result is returned as an evaluated mesh copy. Blender implicit sharing protects the cached buffers from downstream mutation. Non-geometric mesh parameters, active attribute names, material pointers, and edit-mesh pointers are refreshed from the current input; changes to normal weighting invalidate the cache entry. The geometry algorithm itself is unchanged.

One input/result pair is retained per evaluated Decimate modifier. Shared buffers reduce the typical overhead, but keeping a snapshot can force later edits to allocate private buffers, and newly generated upstream data can temporarily coexist with the previous input. This trades RAM for avoiding repeated expensive decimation. Cache hits still pay for input comparison, the evaluated mesh wrapper, and downstream work. There is no disk cache, saved-file layout change, or shared C++ structure layout change.

## Local status and validation

Release `INSTALL` completed with `--parallel 2` and `/p:CL_MPCount=2`. For the initial automatic cache build, only `MOD_decimate.cc` changed C++ behavior; no DNA or shared C++ layout change was required. The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe`, timestamp October 22, 2026, 11:21:16 JST, reporting base hash `55c61952fd05`. The implementation was developed on `codex/decimate-result-cache` from that base and is included in source commit [`bb4bbad2a787`](https://github.com/MaxPuliero/Bmax/commit/bb4bbad2a787c5b424c461bc6d45c6540827b89c) on `main`. Desktop copies and the public Windows download have not been updated; the user's existing Desktop session remained open.

The new build passed 54 main and 15 additional background cases with `--debug-memory`, with no allocator errors or leak report. The same fixtures also passed the previous build to validate the reference procedure. Cached evaluation was compared to a separate fresh object/mesh/modifier copy, using Blender's mesh comparison at threshold zero plus exact coordinates and corner normals. Cases covered:

- Collapse, Un-Subdivide, and Planar; ratio/no-op settings, triangulation, symmetry, weights, inversion, and group order.
- Downstream deformation and topology changes, adding/removing/reordering modifiers, two Decimate modifiers, and different downstream data requirements.
- Base coordinates/topology, upstream Displace/Subdivision, shape keys, texture dependency updates, UVs, colors, point/face attributes, materials including deletion/replacement, smooth shading, and normal weighting.
- Edit Mode round trips, duplicate/remove/recreate modifier, Apply with a later modifier, undo/redo of ratio and Apply, and saving/reopening.

These are background correctness and lifecycle checks. No viewport FPS, redraw, GPU backend, or interactive undo timing is claimed; no display code changed.

### Performance method

Each build started in a separate `--background --factory-startup --disable-autoexec` process, without guarded allocation or a compilation running. The fixture is an eight-level icosphere (327,680 input triangles), Collapse ratio 0.25, followed by Simple Deform Twist. Timing wraps the operation plus synchronous `view_layer.update()` and evaluated-object retrieval. For downstream changes, one warm-up sample is excluded and the median of the following seven is reported. Identical-input DATA tags use seven samples; changed ratios use three samples. Initial evaluation, adding the later modifier, and Apply are single observations, not medians. The last changed ratio is 0.242; both builds produce 79,298 base faces after Apply. These timings include the complete evaluation call sequence and must not be interpreted as timings of one internal decimation algorithm invocation.

The previous Desktop runtime reports `6f14da92f1c9`; the updated local runtime reports `55c61952fd05`. Results from the final sequential runs, in milliseconds:

| Operation | Previous build | Cached build |
| --- | ---: | ---: |
| Initial evaluation, one observation | 1247.371 | 614.256 |
| Add downstream Simple Deform, one observation | 1228.803 | 0.638 |
| Change downstream angle, median of seven | 1294.420 | 0.252 |
| Tag identical input for reevaluation, median of seven | 1309.477 | 0.241 |
| Change Decimate ratio, median of three | 1290.151 | 654.421 |
| Apply Decimate, one observation | 1328.242 | 2.818 |

Changing Decimate settings still performs genuine decimation; the implementation does not change the decimation algorithm. A separate 327,680-triangle run with an unused vertex group also reused the cache (downstream median 0.239 ms). Comparisons and downstream work still have a cost, results depend on the mesh and modifiers, and some apply/data-mask changes require fresh evaluation. These fixtures do not guarantee the same interactive speedup in every production scene.

All generated scripts, scenes, logs, JSON timing records, and recovery files were external temporary verification artifacts and were removed afterward. Build caches, dependencies, and existing user files were retained.

## Cache Result checkbox follow-up (2026-10-22)

The source adds a saved `use_cache` Boolean named **Cache Result**, enabled by default. It is a non-animatable execution/memory option. The Decimate panel displays it above Face Count in all modes. Turning it off releases the evaluated modifier cache before the no-op and minimum-face checks and bypasses cache lookup and storage. Hidden/unevaluated modifiers release their retained runtime entry when next evaluated or freed.

This follow-up changes the Decimate implementation, RNA registration, and an enum declaration in `DNA_modifier_types.h`; no members, sizes, or offsets are added or changed. Release INSTALL completed with `--parallel 2` and `/p:CL_MPCount=2`. The installed `D:\blender_build\octahedral_radius\bin\blender.exe` has timestamp 2026-10-22 11:45:18 JST and reports base hash `55c61952fd05` (compilation preceded the implementation commit). Desktop copies and the public download are unchanged.

All 34 follow-up background checks passed with guarded allocation and no allocator errors or leak report. Checks cover the RNA label/default and non-animatable flag, enabling cache on a file made by the previous build while preserving other flags, defaults for new modifiers, repeated on/off cycles in all three modes, equality against uncached evaluation including exact coordinates/corner normals, preservation of other Decimate flags, duplication, disabling with no-op settings, saving/reopening both states, undo/redo of the preference, base/ratio edits, and Apply with caching disabled. These supplement the 69 checks of the initial implementation; they are not a claim that the entire earlier suite was rerun for this checkbox change.

A separate synchronous background switch benchmark used a seven-level icosphere (81,920 input triangles), ratio 0.25, followed by Simple Deform. After each toggle, the dependency graph was warmed. Each state measured six downstream angle changes, discarded the first, and reported the median of five: **0.130 ms enabled**, **108.353 ms disabled**, and **0.334 ms enabled again**. Timing includes `view_layer.update()` and evaluated-object retrieval and excludes drawing/interactive undo. This confirms bypass and reactivation on the fixture, not an interactive FPS guarantee. All temporary follow-up scripts, files, logs and backups were removed; build dependencies and caches were preserved.


## Source publication (2026-10-22)

The cache and default-on **Cache Result** checkbox are included together in source commit [`bb4bbad2a787`](https://github.com/MaxPuliero/Bmax/commit/bb4bbad2a787c5b424c461bc6d45c6540827b89c) on `main`. The user confirmed that Decimate is noticeably faster in their workflow. That confirmation adds no new benchmark numbers to the measured fixtures above. The installed local executable remains the verified 11:45:18 JST build reporting base hash `55c61952fd05`, because it was compiled before committing. Publishing the source does not update Desktop copies or the public Windows download.
