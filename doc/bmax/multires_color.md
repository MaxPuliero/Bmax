# Multires color painting in Sculpt Mode

**November 7 source update:** Repeat Last Stroke, Blend Symmetry, Multires Color Filter and the Multires color-picker correction are included in source commit [631dd2d0](https://github.com/MaxPuliero/Bmax/commit/631dd2d0bc6f08fe52de072cce3c5bf462ab8aa4). Both local runtimes are installed and verified. Desktop copies and the Gumroad download package have not been replaced. Earlier build records retain their historical metadata and publication status.

Implemented in Bmax source on 2026-10-05, commit `92db31e2b2c8`. The local installed Windows build was compiled and verified with this implementation and its performance correction. Publishing the source does not publish a new downloadable Windows package.

## Usage

Create or select a mesh color attribute, add Multires and subdivide, then enter Sculpt Mode and use Paint, Blur, or Smear. Painting operates on the current Sculpt level. Sculpt masks and hidden grid samples restrict the brush. Color appears in the Sculpt viewport and is supplied to evaluated mesh color attributes for materials and modifier application.

The local November 7 extension enables all Sculpt **Color Filter** modes on Multires:
Fill, Hue, Saturation, Value, Brightness, Contrast, Red, Green, Blue and Smooth, including
negative Strength. They use the displayed Sculpt level, masks, hidden samples and native
automasking. Native color undo includes persistent high-resolution grids; completing a
lower-level filter propagates its delta into retained higher-resolution color. Pointwise
filters share the mesh implementation; Smooth uses stable neighboring grid samples across
seams. Geometry and other color attributes are retained. Persistent grids are committed once
at completion, rather than at each mouse move. Fully locked filters skip storage updates.
The first filter's undo retains exact coarse Float/Byte values and restores the initial
absence of color-grid storage. This coarse backup is limited to global filters; localized
brush strokes retain their sparse capture path. Native mouse-event checks covered dragging,
Esc cancellation, commit and Undo/Redo. An isolated 24,576-quad HUE fixture measured median
operator latency of 2.70 ms for Multires versus 0.38 ms for an applied mesh, after one warm-up
and three measurements each; this is not a viewport frame-rate or production guarantee.

The local November 7 Symmetrize Blend extension also handles the active color attribute.
Its **Vertex Paint** option defaults on: mirrored samples blend in scene-linear RGBA using
the same mask weighting and Strength as geometry. It processes the maximum Multires color
level using the currently visible Sculpt mask, retains unmatched regions and other color
attributes, and records colors with geometry in one native geometry undo step. It preserves
the subdivision levels; color changes are independent of geometry Sculpt Layers. This is a
whole-surface command, separate from the sparse brush-step performance path described below.
The source changes remain local and uncommitted; Desktop/public packages are unchanged.

Float Color and Byte Color attributes on Point and Face Corner domains are accepted. Multires detail is stored in float RGBA even when the coarse attribute is Byte Color. The coarse attribute remains synchronized at its own vertices/corners, with Byte Color conversion as needed. Different color attributes retain separate grids; renaming/removing attributes follows their associated grids.

## Storage and levels

`CD_GRID_PAINT_COLOR` owns a `GridPaintColor` array on the base mesh corner CustomData, named after the visible color attribute. Each corner has a level and a square RGBA sample array, analogous to the existing Multires mask storage. Nested buffers are copied, freed, and serialized explicitly. The Sculpt CCG keeps a runtime float4 array at the current resolution. Duplicate grid boundary samples are averaged to keep a continuous painted surface.

Grid UV (0,0) corresponds to the face center and (1,1) to the coarse corner. Before the first stroke, colors are interpolated from the base attribute. Runtime samples are bilinearly resampled from saved grids when changing levels. Lower-level strokes add their interpolated color delta to the existing higher-level samples, clamped to the paint range. Merely changing levels does not overwrite high-resolution detail. Delete Higher explicitly downsamples the stored grids. Undo records both the current grid samples and the exact persistent high-resolution arrays.

Evaluated subdivision meshes expose painted attributes as Float Color on the Face Corner domain. Applying Multires therefore retains the painted detail as a normal attribute, without requiring Bmax-specific grids afterward.

## Scope and limitations

- Classic Vertex Paint mode is unchanged. Dynamic Topology remains unsupported for Sculpt color painting.
- Mask by Color remains mesh-only and is unavailable with Multires; Color Filter is supported by the local November 7 extension described above.
- Painting stitches shared grid boundaries; independent color discontinuities at base face boundaries are not preserved by this vertex-like workflow.
- Runtime colors still scale with the current grid resolution. Persistent storage and undo allocate high-resolution buffers only for grids reached by painting or its boundary neighbors. Existing files may already contain full-surface buffers from the first version. Painting at a lower level can require resampling retained high-resolution detail at stroke completion; production-scale memory and latency are not guaranteed by the small benchmark below.
- Bmax-specific grid data requires this build. Official Blender cannot be relied on to retain it when re-saving. Apply Multires in Bmax before transferring a finished painted mesh to other builds.
- Topology editing, retopology/rebuild operations, external displacement files, and arbitrary modifier stacks have not been validated for color-grid preservation. External Multires displacement storage does not externalize the color arrays.

## Performance correction

Color strokes now capture undo only for affected PBVH nodes and their grid neighbors. Paint mixes pigment in lazy per-grid buffers; Blur/Smear copy only the grids being processed, reading unchanged neighboring grids directly. Seam stitching visits grid perimeters and averages each duplicate group once, without copying the full surface. Only the nodes owning written samples are tagged for GPU refresh.

Persistent grids are committed once at stroke completion. Existing buffers at the same resolution are reused; lower-level deltas are propagated to retained high-resolution grids once per completed stroke. Anchored strokes and cancellation restore only runtime colors while persistent arrays remain unchanged. Color-only strokes no longer mark Multires displacement coordinates as modified or force a full CCG rebuild between strokes. External render views and linked mesh users still receive the required evaluated-geometry update, and leaving Sculpt Mode flushes evaluation as usual.

The Windows benchmark used a cube subdivided into 384 base quads, Float Point colors, a 45-pixel Paint Hard brush, 20 scripted stroke inputs, a warm-up, and three timed strokes per case. It measured the synchronous brush operator, including undo and stroke completion, but not subsequent interactive GPU redraw. The same script and installed runtime were used before and after the correction; the painted-surface change was checked separately. Final measurements ran without concurrent verification processes.

| Evaluated quads | Original Multires, median | Corrected Multires, median | Applied mesh, median (final run) | Multires improvement |
| --- | --- | --- | --- | --- |
| 98,304 | 85.62 ms | 3.55 ms | 2.77 ms | 24.1x |
| 393,216 | 236.18 ms | 5.39 ms | 4.07 ms | 43.8x |

These timings establish the regression and its correction on this local setup, not a universal frame-rate guarantee. A localized brush benefits most; a brush covering the entire surface necessarily performs more work.

The final regression run used a base mesh with many PBVH nodes and checked geometry preservation, exact local undo/redo, continuous grid boundaries, level switching and lower-level edits, Blur/Smear, a full mask, an anchored stroke, separate Float Point/Byte Corner attributes, Edit Mode renaming, save/reload, and modifier application. OpenGL/Vulkan captures showed freshly painted runtime color, and Cycles CPU renders started directly in Sculpt Mode read the committed color correctly. All scripts, scenes, images, and logs were temporary and removed after verification.

## Validation

Windows Release compilation and installation succeeded using Visual Studio 2022 with two build jobs and `/p:CL_MPCount=2`. Disposable scripts and scenes outside the repository exercised real Sculpt brush operators.

- Paint creates more distinct colors than coarse mesh vertices and leaves geometry unchanged within 1e-6.
- Undo/redo restores high-resolution samples, including painting at Sculpt level 1 after painting at level 3.
- Switching level 3 to 1 and back preserves the saved detail exactly within 1e-6.
- Blur and Smear modify the painted grids without non-finite values.
- Attribute rename, saving/reopening a .blend, and applying Multires preserve the evaluated RGBA samples within 1e-6.
- Float/Byte attributes on Point/Corner domains, triangle and pentagon base faces, full Sculpt masking, switching between separately painted attributes, adding a subdivision level, and Delete Higher were exercised successfully.
- Entering/exiting Edit Mode, renaming a color attribute there, and deleting/recreating it retained or freed its associated grids correctly. The Mesh-to-BMesh conversion resolves legacy grid data separately from ordinary attributes with the same name.
- OpenGL and Vulkan Sculpt viewport captures displayed the painted color. A Cycles CPU emission material using the evaluated color attribute rendered the green painted detail with finite pixels.

The checks cover color storage and evaluation; they do not establish performance on production meshes or compatibility with every existing Sculpt brush option.
