# Voxel Remesh performance investigation — 2026-10-23

This is an investigation of the existing Bmax Sculpt Voxel Remesh pipeline, based on `main` at `d3352ca7`. No remeshing implementation is changed by this work. The Local View development changes are independent. Candidate savings below are hypotheses from source inspection, not measured speedups from an optimization.

## Current pipeline

[`voxel_remesh_exec`](../../source/blender/editors/object/object_remesh.cc) copies the source, fills holes, optionally splits connected components, voxelizes with OpenVDB, always fixes poles, optionally projects volume and attributes, optionally fairs the newly filled patches, replaces the mesh, spatially reorganizes it, finishes Sculpt geometry undo, and invalidates the PBVH/drawing caches. The next viewport update must evaluate and draw the new mesh.

The core in [`mesh_remesh_voxel.cc`](../../source/blender/blenkernel/intern/mesh_remesh_voxel.cc) converts positions/triangles to OpenVDB arrays, calls `meshToLevelSet`, calls `volumeToMesh`, converts the output arrays back to a Blender mesh and calculates edges. This is the CPU OpenVDB path; there is no GPU voxelization call in this operator. Do not assume that all total time belongs to voxelization.

## Candidates, ordered by practical starting value

| Candidate | Evidence in current code | Proposed investigation | Conditions and risks |
| --- | --- | --- | --- |
| Skip BMesh creation when there are no boundary edges | `fill_remesh_holes` builds a complete BMesh, calculates face normals and then scans its edges even for a closed input. | Detect boundary edges with native mesh topology first; return immediately when there are none. Keep the existing fill operation for open input. Consider creating the writable source copy only when it is needed. | High priority for dense closed scans. The boundary test must count face uses correctly for loose edges and non-manifold input; it must not mistake an arbitrary non-manifold edge for a fillable boundary. No measured speedup yet. |
| Reduce repeated work in By Loose Parts | `remesh_loose_parts` groups faces, then allocates a full source-face selection for each component and calls `mesh_copy_selection` serially. Each part subsequently runs the full remesh pipeline. | Build component masks/index groups once and investigate direct extraction by indices. Profile extraction separately from OpenVDB. | Most relevant to many components; full-source selection initialization alone scales with component count times source face count. Do not parallelize all volumes without bounds: OpenVDB already uses threading internally and multiple volumes increase peak memory. Keep independent projection targets and disconnected results. |
| Reduce Fix Poles overhead | `BKE_mesh_remesh_voxel_fix_poles` makes another BMesh, performs sequential topology edits, four full vertex smoothing sweeps, normal updates and conversion back to Mesh. | Measure conversion, topology editing, smoothing and normals separately. Investigate compact mesh-array kernels and avoid redundant normal calculations only where equivalence is demonstrated. | Potentially valuable for fine output grids, but considerably higher risk. Smoothing updates coordinates in place: its result depends on order. A straightforward parallel loop or double-buffer rewrite changes the algorithm. Fix Poles remains mandatory in Bmax. |
| Avoid full mesh copies for Face Set-only projection | With Preserve Attributes off and Face Sets present, `remesh_part` copies the input and removes other attributes before calling generic reprojection. | Add a filtered transfer path that requests only Face Sets without copying/removing unrelated layers. | Preserve newly filled patch IDs, optional Fair Position, existing Face Sets, and domain semantics. Source inspection already shows generic reprojection sharing one BVH and parallel nearest searches; do not count those as missing optimizations. |
| Improve conversion and final organization | OpenVDB input/output array assembly contains serial loops; the final mesh is spatially reorganized and Sculpt PBVH is invalidated. | Profile array packing, edge generation, organization and the first viewport update. Consider parallel packing or an adapter only if these costs matter. | OpenVDB conversion copies may be a smaller fraction than meshing. Spatial organization supports later Sculpt locality; removing it can make the first brush stroke or later interaction slower. |

## Other observations

Preserve Volume uses the existing shrinkwrap projection, including a spatial tree and per-vertex queries. Preserve Attributes groups transfers by domain, shares a triangle BVH, and already uses parallel nearest searches. Corner attributes allocate an intermediate sampled point array per attribute before adaptation to corners. These stages can matter with dense output, many attributes, or large corner-color/UV layers, but their actual share requires measurement.

Fair Position returns immediately when there are no newly filled patches; disabling it on a closed mesh therefore does not remove the initial hole search or Fix Poles. Adaptivity simplifies the output during surface extraction: it does not avoid creating the input level set. Increasing voxel size changes geometric resolution and must not be presented as a same-quality optimization. Likewise, disabling attribute or volume preservation changes the result.

Caching the previous full remesh is a poor first target for ordinary Sculpt strokes: changes to the surface invalidate it. A useful incremental volume cache would require a much larger design for dirty regions, topology, projection and undo. A GPU rewrite is also a separate project, rather than a small switch in the current OpenVDB calls.

## Measurement plan and limits

Start with stage timings on dense closed and open meshes, a highly disconnected mesh, and inputs with and without masks, Face Sets, UVs and colors. Separate input-copy/hole-fill, part extraction, voxelization, Fix Poles, volume projection, attribute projection, fairing, spatial organization, Sculpt undo/PBVH and the first visible frame. Record peak working set as well as wall time. Use a warm-up and at least five fresh-input measurements; compare medians on identical scenes and settings.

No stage instrumentation is added here, in accordance with the request to investigate without modifying remesh code. End-to-end background timings can characterize the current pipeline but cannot identify each stage's share or predict a speedup. They also exclude interactive redraw and do not measure responsiveness while the operator runs. Correctness comparisons for a later implementation should include closed/open and non-manifold input, close disconnected shells, adaptive output, preserved volume and attributes, filled patch interiors/rims, save/reload and exact one-step undo/redo.

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

All benchmark scripts, temporary scenes, logs, captures, recovery files and crash diagnostics were kept outside the repository and removed after inspection. No native stage instrumentation or remesh implementation change was made.
