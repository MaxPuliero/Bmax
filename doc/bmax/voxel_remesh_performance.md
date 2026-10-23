# Voxel Remesh performance investigation and local optimization — 2026-10-23

The initial investigation below examined Bmax Sculpt Voxel Remesh at `d3352ca7`, without changing its implementation. The user subsequently authorized stage profiling and optimizations that preserve the result and keep Fix Poles complete. The measured local development update, based on `main` at `23181336769d`, is documented in the final section. Initial hypotheses and preliminary measurements are retained as history and are separate from the verified before/after results.

## Pipeline examined in the initial investigation

[`voxel_remesh_exec`](../../source/blender/editors/object/object_remesh.cc) copies the source, fills holes, optionally splits connected components, voxelizes with OpenVDB, always fixes poles, optionally projects volume and attributes, optionally fairs the newly filled patches, replaces the mesh, spatially reorganizes it, finishes Sculpt geometry undo, and invalidates the PBVH/drawing caches. The next viewport update must evaluate and draw the new mesh.

The core in [`mesh_remesh_voxel.cc`](../../source/blender/blenkernel/intern/mesh_remesh_voxel.cc) converts positions/triangles to OpenVDB arrays, calls `meshToLevelSet`, calls `volumeToMesh`, converts the output arrays back to a Blender mesh and calculates edges. This is the CPU OpenVDB path; there is no GPU voxelization call in this operator. Do not assume that all total time belongs to voxelization.

## Initial candidates, ordered by practical starting value

| Candidate | Evidence in current code | Proposed investigation | Conditions and risks |
| --- | --- | --- | --- |
| Skip BMesh creation when there are no boundary edges | `fill_remesh_holes` builds a complete BMesh, calculates face normals and then scans its edges even for a closed input. | Detect boundary edges with native mesh topology first; return immediately when there are none. Keep the existing fill operation for open input. Consider creating the writable source copy only when it is needed. | High priority for dense closed scans. The boundary test must count face uses correctly for loose edges and non-manifold input; it must not mistake an arbitrary non-manifold edge for a fillable boundary. No measured speedup yet. |
| Reduce repeated work in By Loose Parts | `remesh_loose_parts` groups faces, then allocates a full source-face selection for each component and calls `mesh_copy_selection` serially. Each part subsequently runs the full remesh pipeline. | Build component masks/index groups once and investigate direct extraction by indices. Profile extraction separately from OpenVDB. | Most relevant to many components; full-source selection initialization alone scales with component count times source face count. Do not parallelize all volumes without bounds: OpenVDB already uses threading internally and multiple volumes increase peak memory. Keep independent projection targets and disconnected results. |
| Reduce Fix Poles overhead | `BKE_mesh_remesh_voxel_fix_poles` makes another BMesh, performs sequential topology edits, four full vertex smoothing sweeps, normal updates and conversion back to Mesh. | Measure conversion, topology editing, smoothing and normals separately. Investigate compact mesh-array kernels and avoid redundant normal calculations only where equivalence is demonstrated. | Potentially valuable for fine output grids, but considerably higher risk. Smoothing updates coordinates in place: its result depends on order. A straightforward parallel loop or double-buffer rewrite changes the algorithm. Fix Poles was mandatory during the initial investigation; the current control defaults to enabled and can be disabled explicitly. |
| Avoid full mesh copies for Face Set-only projection | With Preserve Attributes off and Face Sets present, `remesh_part` copies the input and removes other attributes before calling generic reprojection. | Add a filtered transfer path that requests only Face Sets without copying/removing unrelated layers. | Preserve newly filled patch IDs, optional Fair Position, existing Face Sets, and domain semantics. Source inspection already shows generic reprojection sharing one BVH and parallel nearest searches; do not count those as missing optimizations. |
| Improve conversion and final organization | OpenVDB input/output array assembly contains serial loops; the final mesh is spatially reorganized and Sculpt PBVH is invalidated. | Profile array packing, edge generation, organization and the first viewport update. Consider parallel packing or an adapter only if these costs matter. | OpenVDB conversion copies may be a smaller fraction than meshing. Spatial organization supports later Sculpt locality; removing it can make the first brush stroke or later interaction slower. |

## Other observations

Preserve Volume uses the existing shrinkwrap projection, including a spatial tree and per-vertex queries. Preserve Attributes groups transfers by domain, shares a triangle BVH, and already uses parallel nearest searches. Corner attributes allocate an intermediate sampled point array per attribute before adaptation to corners. These stages can matter with dense output, many attributes, or large corner-color/UV layers, but their actual share requires measurement.

Fair Position returns immediately when there are no newly filled patches; disabling it on a closed mesh therefore does not remove the initial hole search or Fix Poles. Adaptivity simplifies the output during surface extraction: it does not avoid creating the input level set. Increasing voxel size changes geometric resolution and must not be presented as a same-quality optimization. Likewise, disabling attribute or volume preservation changes the result.

Caching the previous full remesh is a poor first target for ordinary Sculpt strokes: changes to the surface invalidate it. A useful incremental volume cache would require a much larger design for dirty regions, topology, projection and undo. A GPU rewrite is also a separate project, rather than a small switch in the current OpenVDB calls.

## Measurement plan and limits

Start with stage timings on dense closed and open meshes, a highly disconnected mesh, and inputs with and without masks, Face Sets, UVs and colors. Separate input-copy/hole-fill, part extraction, voxelization, Fix Poles, volume projection, attribute projection, fairing, spatial organization, Sculpt undo/PBVH and the first visible frame. Record peak working set as well as wall time. Use a warm-up and at least five fresh-input measurements; compare medians on identical scenes and settings.

At the initial investigation stage no instrumentation was added, in accordance with the request to investigate without modifying remesh code. End-to-end background timings can characterize the current pipeline but cannot identify each stage's share or predict a speedup. They also exclude interactive redraw and do not measure responsiveness while the operator runs. Correctness comparisons for a later implementation should include closed/open and non-manifold input, close disconnected shells, adaptive output, preserved volume and attributes, filled patch interiors/rims, save/reload and exact one-step undo/redo.

The first implementation to evaluate would be the closed-mesh hole-fill fast path, followed by component extraction and filtered Face Set transfer if profiling confirms their importance. Optimize Fix Poles only after measuring it and defining the required numerical/visual equivalence.

## Current-build end-to-end measurements

Measured on 2026-10-23 with the installed local Release executable, reporting base hash `d3352ca742f4`, Intel Core i7-14700KF (20 cores / 28 logical processors), approximately 64 GiB RAM. No build or other verification process was running during these measurements; the user's existing Desktop session was preserved. These are CPU background measurements of the synchronous operator in **Sculpt Mode**, using fresh copies of identical input, one warm-up and five measured runs per configuration. Input reset, mode entry, explicit initial Undo push, mode exit and subsequent viewport updates are outside the timer. Global Undo was enabled with two history steps; the operator's Sculpt geometry-undo path is included. This is not a measurement of interactive latency or first-frame time.

Fixtures are closed radius-one icospheres, Adaptivity 0 and Fair Position off. The eight-part fixture joins separated spheres, spaced 2.5 units apart, into one mesh. Attribute cases contain a varying point mask, varying Face Sets and zero-initialized Face Corner float color. “Attributes off” has no extra test attributes. Preserve Volume changes the geometry and can change the extracted face count, so different settings are workload comparisons, not equivalent-output optimization results.

| Input triangles | Parts | Voxel size | Preserve Volume / Attributes | By Loose Parts | Output faces | Median ms | Range ms |
| ---: | ---: | ---: | --- | --- | ---: | ---: | ---: |
| 20,480 | 1 | 0.04 | off / off | off | 10,221 | 23.74 | 23.19–24.96 |
| 81,920 | 1 | 0.04 | off / off | off | 10,220 | 40.07 | 39.78–40.43 |
| 20,480 | 1 | 0.02 | off / off | off | 40,911 | 59.07 | 57.75–60.45 |
| 20,480 | 1 | 0.02 | on / off | off | 41,575 | 61.51 | 60.94–63.96 |
| 20,480 | 1 | 0.02 | on / on | off | 41,575 | 65.62 | 64.24–65.97 |
| 40,960 | 8 | 0.04 | off / off | off | 81,708 | 125.40 | 122.10–126.00 |
| 40,960 | 8 | 0.04 | off / off | on | 81,708 | 297.74 | 289.19–301.03 |
| 1,310,720 | 1 | 0.02 | off / off | off | 40,927 | 991.50 | 465.44–1093.05 |
| 1,310,720 | 1 | 0.01 | on / on | off | 164,704 | 1741.93 | 1635.32–1791.35 |

On this small eight-component fixture, By Loose Parts increases the median from 125.40 to 297.74 ms (about 2.37×), with the same output face count. This establishes a pipeline cost for independent processing, not how much of it can be eliminated: separate volumes, extraction and repeated cleanup/projection are all bundled together. Equal face counts do not establish exact geometric equivalence.

At voxel size 0.04, increasing input density from 20,480 to 81,920 triangles changes the median from 23.74 to 40.07 ms while output stays near 10,220 faces. The 1,310,720-triangle case also shows that fine input can be expensive even with only about 40,927 output faces. Its samples have a large spread (465–1,093 ms); this is a preliminary characterization, and no cause for the spread or stage-level attribution is established. The higher-resolution preserved-attribute case takes a median 1,741.93 ms for 164,704 output faces.

A first background attempt with Global Undo disabled stopped in Sculpt geometry-undo initialization because no undo stack existed. The completed measurements above use an explicitly initialized stack with Global Undo enabled. This test-setup correction does not modify the remesh implementation. No production Sculpt crash or new remesh performance improvement is claimed.

All benchmark scripts, temporary scenes, logs, captures, recovery files and crash diagnostics were kept outside the repository and removed after inspection. These initial measurements preceded native stage instrumentation and the remesh implementation changes below.

## Verified local optimization after authorization (2026-10-23)

Status: implementation [`83c95f81fd69`](https://github.com/MaxPuliero/Bmax/commit/83c95f81fd6990e1aebd003e7db2347742b91252) included in `main`, developed on `codex/voxel-remesh-performance` from `23181336769d`. Release INSTALL completed in the existing build directory. The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe`, timestamp 2026-10-23 15:16:18 JST, reporting base hash `23181336769d`. Desktop packages and the public download have not been updated. No DNA or existing shared C++ structure layout changed.

### Implemented changes

- The operator counts incident face uses directly from Mesh topology, saturating at two. Exactly one face identifies a boundary; loose edges and edges used by more than two faces do not. A mesh with no boundary edges skips the BMesh hole-search and writable source-copy preparation. Open meshes use the existing simple hole fill and patch Face Set assignment, working on a copy so cancellation preserves the source. The original source is only read on the closed path, apart from normal runtime cache construction.
- OpenVDB input uses one parallel-built index-space position array and an adapter over Blender's existing corner/triangle topology. It removes the object-space position and packed-triangle arrays. The world-to-index double calculation is rounded back to float exactly as in the former OpenVDB wrapper before voxelization. Narrow-band widths, isovalue, adaptation and signed-distance behavior are retained.
- Surface extraction uses the same OpenVDB `VolumeToMesh` mesher, then copies points and polygon pools directly into the final Blender arrays in parallel. It preserves point order, all-quads-before-triangles ordering, pool order and winding. Intermediate output vectors are removed; mesher arrays are released before edge generation.
- Fix Poles is exposed in both remesh panels, enabled by default, and saved per mesh. Disabling it skips only pole cleanup and its smoothing; adaptive output remains supported. An inverted flag in a previously cleared bit preserves enabled behavior for existing files without changing structure layouts. Enabled Fix Poles remains complete. Its function body is identical to the previous implementation after removing the opt-in timing statements, including the four in-place smoothing sweeps and normal recalculation. Component extraction and attribute/volume projection retain their existing implementations.
- A tiny component can lose its last faces during Fix Poles. The existing By Loose Parts empty-result check now runs immediately afterward, before projection. The former placement after attribute transfer could crash while adapting corner attributes to an empty result. The failure was reproduced in the baseline and candidate; the corrected operator reports cancellation and preserves the source mesh.

### Stage profiling

Set `BMAX_REMESH_PROFILE=1` **before starting Bmax** to print CPU stage durations in milliseconds to standard output; unset it or use `0` for the normal silent path. It adds no UI controls. Disabled profiling reads no clocks and emits no timing lines. Run an isolated operation when interpreting the output: concurrent modifier evaluation can interleave records.

Records cover source preparation/hole filling, input triangulation and buffers, mesh-to-level-set, surface extraction/output packing/edge creation, Fix Poles conversion/topology/smoothing/normals/conversion back, projection, fairing, geometry Undo, spatial organization and PBVH invalidation. Parent records include nested records; do not sum parents with children. Some final cleanup occurs after the last checkpoint. The authoritative total below is the externally timed synchronous operator, with profiling **disabled**.

Stage boundaries inside OpenVDB conversion differ between the wrapper and direct path. Previously the mesh-to-level-set wrapper included index-space transformation, and the surface-extraction wrapper included packing intermediate vectors. The new records separate these steps. Compare combined core time or `part.voxelization`, rather than interpreting one renamed/moved substage as an isolated algorithmic speedup.

Indicative baseline stage medians: the 1,310,720-triangle closed fixture spent 425.36 ms in hole search and 423.15 ms in the complete voxelization group. The optimized native boundary check took 2.23 ms and skipped BMesh preparation. In the fine-output fixture, baseline Fix Poles took 770.05 ms: approximately 108.31 ms converting to BMesh, 159.48 ms in topology operations, 98.82 ms smoothing, 360.30 ms normals/associated flags and 25.42 ms converting back/freeing. These component medians are not additive totals. Fix Poles remains the dominant cost in that fixture.

### Before/after measurements, profiling disabled

Fix Poles is enabled in every before/after timing below; the optional bypass is not part of these speedups. Same CPU/RAM as above; baseline executable contains only diagnostic timers around the previous algorithm. Both executables use the same installed runtime and the same fixture generator. Builds and other verification processes had finished before timing; the user's existing Desktop session was preserved. Each configuration uses one warm-up and five fresh-input measured operations in Sculpt Mode. Global Undo is enabled and initialized; input reset, mode transitions and explicit setup Undo pushes are outside the timer. The operator's geometry Undo and spatial organization are included. Interactive evaluation, PBVH reconstruction for drawing, redraw and subsequent strokes are outside the timer. The figures are synchronous CPU operator timings, not viewport FPS or guaranteed production-scene speedups.

| Fixture | Input triangles | Output faces | Before ms | After ms | Speedup |
| --- | ---: | ---: | ---: | ---: | ---: |
| Dense closed, no projection | 1,310,720 | 40,927 | 967.91 | 216.82 | 4.46× |
| Dense closed, preserved attributes | 327,680 | 114,431 | 697.60 | 557.82 | 1.25× |
| Dense closed, Face Sets only | 327,680 | 114,431 | 635.63 | 514.97 | 1.23× |
| Open surface, filled and faired | 68,180 | 24,951 | 210.72 | 194.46 | 1.08× |
| Eight independently remeshed parts | 163,840 | 213,184 | 838.99 | 744.62 | 1.13× |
| Fine output, preserved attributes | 20,480 | 256,649 | 996.35 | 992.42 | 1.00× |

Dense closed uses a radius-one icosphere at subdivision 9, voxel size 0.02, Preserve Volume/Attributes off. Dense attribute cases use subdivision 8, voxel size 0.012 and Preserve Volume on; the Face Sets-only variant disables Preserve Attributes. Open/fair uses subdivision 7 with faces above Z=0.65 removed, voxel size 0.025, attributes/volume preserved and Fair Position enabled. The eight-part fixture uses subdivision 6 spheres separated by 2.5 units, voxel size 0.025, By Loose Parts and preservation enabled. Fine output uses subdivision 6, voxel size 0.008 and preservation enabled. All use Adaptivity 0 in these timings. Attribute fixtures contain varying masks, Face Sets and edge weights, Float Face Corner color, and UVs. Geometry and attribute buffers in all six measured fixtures were additionally compared byte-for-byte with the baseline and matched.

The first dense-closed fixture's process peak working set fell from about 1,661 to 1,224 MiB; across the full six-fixture sequence, the process peaks were about 1,661 and 1,557 MiB. These Windows process counters include fixture preparation, Undo and allocator retention and are cumulative within each process; they are not isolated per-stage allocations. The dense-closed time improvement combines avoiding BMesh preparation, source-cache reuse and direct data conversion; individual changes were not separately benchmarked. The fine-output timing is effectively unchanged within run variability because Fix Poles dominates.

### Validation and scope

Final guarded-allocation comparison passed **512 assertions across 22 operator cases**, including closed/open and non-manifold input, loose wires, coarse voxels, Adaptivity 0.25/0.75/1.0, preserved volume on/off, preserved attributes on/off, Face Sets-only transfer, Float/Byte colors on Point/Corner, masks/UVs/edge attributes, translated coordinates, nearby disconnected components and fairing. Exact comparisons cover positions, topology order, vertex normals and all inspected attribute buffers. An additional **70 assertions** cover four Voxel Remesh modifier cases and source-preserving cancellation when a tiny part vanishes. Six more byte-for-byte comparisons cover the measured large fixtures. The modifier comparisons use the common core; Dual Contouring/Quadriflow paths are unchanged.

OpenGL and Vulkan each passed 222 GUI assertions covering coarse output, open/fair and separate parts, redraw, exact one-step Undo/Redo, Sculpt Mode and exact geometry/attribute save/reload; six viewport captures were inspected. All temporary scripts, fixtures, golden arrays, comparison binaries, logs, captures, preferences, recovery files and crash diagnostics were external and removed after verification. The installed executable, build cache and dependencies were retained. These checks and final cleanup are also recorded in the feature inventory and Windows build guide. The original Fix Poles source body comparison, C++ formatting and diff checks passed; disabled diagnostic output was verified silent. This work does not claim coverage of every possible degenerate mesh, material/attribute type or GPU/driver. Native stage timing measures CPU work, not a whole visible frame. An incremental-volume cache or GPU voxelizer is a separate design project.

Measured sample ranges (profiling off):

- `dense_closed`: before 942.76–983.65 ms; after 213.43–263.55 ms.
- `fine_output`: before 952.59–1019.30 ms; after 966.38–1009.28 ms.

### Restored Fix Poles control and publication

The user requested the manual Fix Poles switch, enabled by default, before publication. Both the Sculpt Remesh popover and Mesh Data > Remesh expose the editable setting. `ME_REMESH_NO_FIX_POLES` uses previously cleared Mesh flag bit 6 with inverse RNA mapping: zero retains cleanup on new meshes and pre-control files, including meshes with either value of the old positive bit. Historical pre-2.80 unused-bit migration still clears that bit. No DNA layout changes or density-based automatic switching were introduced. Copying remesh parameters retains the explicit choice, including after joining separately processed parts.

The final Release INSTALL passed 48 additional guarded-memory background checks covering defaults, editability, pre-control files, save/reload, both panel draw methods, exact enabled results against the prior installed build on four fixtures, and disabled equivalence to raw Voxel Remesh modifier output at Adaptivity 0 and 0.5. OpenGL and Vulkan each passed 52 additional Sculpt assertions for both states, adaptive/per-part processing, redraw, exact one-step Undo/Redo and mode/choice retention. The GUI harness explicitly records before/after Undo states to model normal interactive invocation; correcting its initial missing after-state push required no product Undo change. The original Fix Poles function remains identical apart from diagnostic timers; formatting, Python syntax and default-silent profiling checks passed. These follow-up checks did not repeat the six-fixture timing benchmark or add a claimed speedup for disabling cleanup.

The final installed runtime is the 2026-10-23 15:16:18 JST executable (base hash `23181336769d`, compiled before the implementation commit). Temporary follow-up scripts, scenes and logs were external and removed; the user's Desktop instance and unrelated existing files were preserved. Source publication does not update Desktop or public binary packages.
