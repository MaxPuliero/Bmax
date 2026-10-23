# Sculpt voxel and mask tools

## Status

Implemented in source commit `b98ad0b25666` on `main`, developed on `codex/sculpt-voxel-face-set-mask` from base `6f14da92`. A full Release rebuild and INSTALL succeeded on 2026-10-21 because `Sculpt` now owns a persistent `CurveMapping` pointer. The subsequent simple-fill/post-remesh Fair Position correction was compiled and installed incrementally, without another DNA or shared C++ layout change. The installed runtime is `D:\blender_build\octahedral_radius\bin\blender.exe` and reports base hash `6f14da92f1c9`; compilation preceded the source commit, so that base hash does not identify the complete installed feature set. The separately copied Desktop runtime and public download are unchanged.

## Voxel Remesh

The later [performance and optional Fix Poles update](voxel_remesh_performance.md#restored-fix-poles-control-and-publication), implemented in [`83c95f81fd69`](https://github.com/MaxPuliero/Bmax/commit/83c95f81fd6990e1aebd003e7db2347742b91252), supersedes the original mandatory-cleanup behavior described in the historical validation notes.

The Sculpt Remesh popover and Mesh Data remesh panel expose **Fix Poles** (initially enabled), **By Loose Parts** and **Fair Position** (initially disabled), all saved per mesh. Ctrl+R uses the same native destructive remesh operator in Sculpt Mode.

Every invocation runs these stages in one operator and one undo step:

1. **Simple Fill Holes**, when boundary edges exist; closed meshes skip BMesh preparation. A temporary mesh uses the same native `holes_fill` operation and boundary selection as the existing Object Mode Fill Holes, without a sides limit. It adds ordinary caps and distinct Face Sets, without refining, adding interior vertices, moving the source surface, or applying fairing. Preparation runs once before splitting components, so hole IDs are unique across parts.
2. **Voxel Remesh**, followed by optional **Fix Poles** (default enabled), then the existing optional Preserve Volume projection and attribute reprojection against the prepared source. The Fix Poles checkbox remains editable with adaptive output and its choice survives remesh and save/reload. Enabled cleanup retains the full topology and smoothing algorithm.
3. **Fair Position**, only when enabled. After voxelization, pole cleanup, projection, and attribute transfer, the existing Blender position fairing solver processes vertices surrounded exclusively by one newly filled Face Set. Patch rims, existing surface regions, and non-manifold boundaries stay fixed. Face Sets are retained even when Preserve Attributes is disabled; other attributes follow that setting. With no newly filled holes, this stage is a no-op.

There is no fairing before voxelization and no separate Fill Holes and Fair Tangency command. The existing Object/Edit Mode **Mesh Fill Holes** operator remains available and unchanged. A failed remesh leaves the original mesh untouched.

**By Loose Parts** finds vertex-connected components, extracts their faces, voxelizes each independently, and joins the resulting meshes without welding. Fix Poles, Preserve Volume, and attribute reprojection use each part's own source, preventing projection onto a nearby part. Intersecting or overlapping components remain topologically separate, although their surfaces can still intersect. If a surface component disappears at the requested voxel size, the operator cancels without replacing the input; use a smaller voxel size. Loose wires and isolated points are not voxel surfaces.

Simple hole filling retains the native fill algorithm's limits on arbitrary non-manifold or nested boundary networks. Fair Position smooths the post-remesh patch; it does not guarantee tangency or a self-intersection-free surface on arbitrary inputs.

## Fill Holes and Split in Sculpt Mode

**Sculpt > Fill Holes** runs the same simple native boundary fill as Object Mode on the active sculpt mesh, with no remeshing or fairing. F9 exposes Sides (zero fills all boundary loops) and New Face Sets (enabled by default). Surface holes are filled; loose wire loops are ignored. Original vertices remain fixed.

**Sculpt > Split** keeps all disconnected pieces in the same object, with six modes:

- **By Face Sets** partitions vertices and incident face loops by Face Set ID, including point-only contacts and non-manifold edges. Faces sharing an ID retain their connectivity.
- **By Material** performs the same partition using material indices.
- **Masked** separates faces whose vertices all meet Mask Threshold from the rest.
- **Visible** separates visible faces from hidden faces.
- **Faces by Edges** uses the existing BMesh edge split on edges whose two vertices meet the mask threshold. With no active mask, it uses the visible surface.
- **Faces & Edges by Vertices** uses native vertex splitting at vertices meeting the threshold, or the visible surface with no active mask. This includes loose wires.

Hidden faces stay together as a protected region for grouped splits. Edge/vertex variants protect vertices incident to hidden faces. Split changes connectivity, preserving face positions, Face Sets, masks, material assignments and ordinary mesh custom data. The Face Sets menu also provides **Split by Face Sets**. Neither operation switches out of Sculpt Mode, and each supports one undo/redo step. Inputs with nothing to fill or disconnect cancel without changing geometry.

Both commands require a base mesh without Multires or Dynamic Topology. Split is also disabled for shape-key meshes because Sculpt geometry undo does not remap the key datablocks. Fill Holes can retain shape-key coordinates because it adds no vertices. The original Object/Edit Mode operators remain unchanged.

## Face Set gestures

Box, Lasso, Polyline, and Line tools expose **Cut Geometry** in their settings, defaulting on. An open set of projected cutter side walls is intersected with the sculpt surface through Blender's BMesh intersection tool. Cutter geometry is removed afterward, retaining the divided original surface and assigning the selected region a new Face Set. No boolean volume or deletion of the sculpt surface is needed, so open surfaces are supported. Newly cut vertices and faces interpolate ordinary mesh custom data.

The operation respects mirror symmetry, front-face filtering, hidden faces, and the existing face-mask eligibility threshold. It cuts through eligible surfaces in the projection direction, including overlapping eligible layers; Front Faces Only tests orientation and is not an occlusion test. Neither side is detached. Line supports the existing active side and Limit to Segment controls. Lasso classification uses the floating point outline to avoid rounding the new boundary to the old face centers.

Topology cutting requires a base mesh without Multires or Dynamic Topology. Turn Cut Geometry off to keep the previous Face Set assignment path on those sculpt types. Highly degenerate or coplanar intersections retain the limitations of Blender's floating point BMesh intersection solver. Gesture projection uses the base mesh; deform modifiers can change the displayed location relative to that surface.

## Bevel Mask

**Mask > Bevel Mask** is directly below Smooth Mask. F9 exposes **Radius**, **Profile**, and **Boundary**. Radius is the distance on either side of the mask boundary in object-local units; Boundary defaults to mask value 0.5. The default profile is Smooth. Sharp, Sharper, Linear, Smoother, Sphere, Root, Inverse Square, Constant, and Custom use Blender's brush falloff functions.

Threshold crossings are interpolated on surface edges. A bounded Dijkstra traversal measures distance on either side, then evaluates the selected profile across the transition band. Larger radii widen the band. This is an edge-graph distance, so very coarse or directionally biased topology limits the accuracy of the width. Separate components do not exchange mask values. Hidden vertices are barriers and retain their masks; vertices outside the requested band keep their prior values. Geometry is unchanged.

Mesh edges, Multires grid neighborhoods (including duplicate grid-boundary samples), and Dyntopo edges provide the connectivity. The neighbor graph uses contiguous storage. Mask undo snapshots are pushed only for changed nodes before any mask writes. Restoring a Multires mask through undo/redo marks the runtime grids as modified, so saving or reevaluating writes the restored mask into persistent storage; the previous mask-undo path omitted this tag. The Custom curve belongs to Sculpt settings independently of brushes and cavity curves; its lifetime covers deep copy, freeing, blend write, and blend read.

## Validation

The subsequent Sculpt Fill Holes/Split addition passed **95 GUI assertions on each of OpenGL and Vulkan**. All six split modes preserve one object and Sculpt Mode, face positions, Face Sets, material indices, UVs and copied point attributes, with exact one-step undo/redo. Additional fixtures cover point-only Face Set contacts, a non-manifold edge with multiple faces sharing an ID, masked edge/vertex equivalence to the existing Edit Mode operators, loose wires, no-op inputs and unsupported Multires/Dyntopo/shape-key split polls. Sculpt Fill Holes exactly matches the existing Object Mode fill, leaves vertices fixed and wire loops unfilled, assigns new cap IDs by default, and preserves existing shape-key coordinates. A split cube followed by By Loose Parts voxel remesh remains in two separate components. The actual six-item Split submenu was visually inspected on both backends. These fixtures do not cover every production topology or modifier stack.

The corrected simple-fill/remesh/Fair Position workflow passed on the newly installed Release runtime:

- **191 background assertions**: automatic fill with Fair Position off exactly matches existing Object Mode Fill Holes followed by remesh. The manual fill preserves every source coordinate and adds no vertices. Ten cases cover ordinary and concave wavy rims, separate and overlapping loose parts, Preserve Attributes off, Preserve Volume off, Adaptivity 0.25/0.75/1.0, and Face Set IDs near integer overflow. Each new patch keeps its distinct Face Set. Fair Position retains topology counts, changes cap interiors, and leaves all remeshed pre-existing surface regions and patch rims exactly fixed. Outputs have finite bounded coordinates and closed manifold topology. No-hole fairing is an exact no-op; mesh options survive save/reload.
- **49 GUI assertions per backend**, on OpenGL and Vulkan: five actual operator cases cover Sculpt with fairing off/on, overlapping loose parts with fairing, adaptive loose parts with fairing, and Object Mode with fairing. Each invocation uses UI-style undo registration. One undo exactly restores the complete open source in the same mode; one redo exactly restores the final geometry and Face Sets. The installed popover exposes By Loose Parts and Fair Position, removes the old toggles, and has one Remesh action.
- Post-remesh wavy-rim geometry and the actual Remesh popover were visually inspected on both backends. The user's screenshot was not accompanied by its source mesh; the exact production mesh was not reproduced.

The user subsequently confirmed that the Sculpt commands work on 2026-10-21.

These focused checks validate the corrected ordering and selected fixtures, not every possible boundary network. The external verification scripts, scenes, screenshots, logs, isolated configurations, and recovery files were removed afterward.

Before the hole-workflow correction, the installed development build passed:

- **81 background assertions** covered per-component remeshing, attribute transfer, mask profiles, radius, hidden masks, independent scene curves, save/reload, and the previous hole-fairing implementation. The previous pre-remesh fairing checks are superseded by the corrected workflow described above.
- **54 GUI assertions per backend**, on OpenGL and Vulkan: real Box/Lasso/Polyline/Line operators cut an inner boundary inside a single large quad, preserve its area and surface plane, retain one connected surface without internal holes or cutter wires, and support exact geometry undo/redo. Further cases cover mirror symmetry, fully masked faces, Front Faces Only, assignment-only behavior, the previous fair-fill and voxel-remesh undo/redo, mask undo/redo, and native Manifold Boolean/Sculpt Trim after the full DNA rebuild. Python operator calls explicitly enable undo to match UI invocation.
- **11 additional assertions per backend**: Bevel Mask on three-level Multires, unchanged base coordinates and levels, explicit rejection of topology cuts on Multires with assignment-only still available, and finite fractional Dyntopo masks on an unchanged planar surface.
- **Multires image comparisons on both backends**: a 440 by 440 viewport crop changed after beveling; the pre-operation crop matched undo exactly, and the post-operation crop matched redo and save/reload exactly (maximum component difference zero). This caught the missing dirty tag in Multires mask undo/redo before the final correction.

Representative Polyline and Multires captures were visually inspected. The native F9 panel displayed Radius, Profile, Boundary, and the independent Custom curve correctly. These are focused fixture checks, not a guarantee for every production mesh or modifier configuration. No benchmark or interactive performance claim is made.

Verification scripts, scenes, logs, captures, preferences, and recovery files were confined to a dedicated external temporary directory and removed after inspection. Build caches and runtime dependencies were retained. No verification files were added to Git.
