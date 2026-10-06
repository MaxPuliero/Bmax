# Bmax feature inventory

This is the current implemented feature list for `main`, checked against the Bmax commits and source code on **2026-10-05**. Dates below are the dates the changes were committed to the published Bmax source, in Japan time. They are not separate binary release dates.

For demonstrations and usage details, see the [main README](../../README.md). For compilation, see [Building Bmax on Windows](BUILD_WINDOWS.md). Contributor and AI-agent rules are in [AGENTS.md](../../AGENTS.md).

## Added to the published source on 2026-10-01

Commit: `a0f495c4`.

| Feature | Implemented behavior |
| --- | --- |
| Absolute Octahedral Radius | Per-bone radius controls the octahedral body's half-width and endpoint spheres independently of rest-bone length. Default: `0.02` Blender units. Available in Edit and Pose Mode under N sidebar > Item. |
| Pose scale and shear display | Octahedral bones retain evaluated pose scaling and shear, including nonuniform scaling. The change affects viewport display, not bone evaluation or constraints. |
| Independent Axis Size | Per-bone display-axis length independent of radius and bone length. Default: `0.03` Blender units, available in Edit and Pose Mode under N sidebar > Item. Object scale still applies. |
| Names and axes for selected bones | The armature Names and Axes toggles draw these overlays only for selected bones. Selecting a head or tail in Edit Mode also qualifies the bone. |
| Pose/Edit hide synchronization | Per-bone hidden state transfers between Pose and Edit Mode. Hidden bones are deselected; Bone Collection visibility remains a separate filter. |
| Mode pie gesture after a Tab tap | With Tab for Pie Menu enabled, tapping and releasing Tab allows confirming an enabled item by moving beyond its outer edge without clicking. This is scoped to the mode pie opened with Tab. |
| Bmax branding | Custom splash logo and Windows icons. The executable remains `blender.exe` and uses Blender 5.2 preferences and startup configuration. |

## Added to the published source on 2026-10-02

Commit: `ac5982c9`.

| Feature | Implemented behavior |
| --- | --- |
| Origin axes during transforms | Affect Only > Origins axes start at `0.2` Blender units. Uniform scaling is visible during the gesture and removed from the display on confirmation. Per-axis scaling remains visible. Cancellation, Undo/Redo, and save/reload preserve the intended state; object geometry keeps its world-space position. |
| Weighted Normal defaults | New modifiers use Face Area & Angle with Keep Sharp enabled. |
| Triangulate defaults | New modifiers have Keep Normals enabled. |
| Displace default | New modifiers use Strength `0.1`. Existing saved modifier settings are preserved. |
| UV Shell Outline | White outline drawn two physical pixels inward along UV shell boundaries, including holes. Width stays constant when zooming; internal UV edges are excluded. |
| UV Overlap | Red highlighting of the intersecting area, including partial overlap and overlap between objects. Intensity controls opacity from `0` to `1`; the display updates as UV shells move. |
| Flipped UVs | Magenta highlighting of faces with reversed UV orientation. Red takes priority where overlap and flipped-face diagnostics coincide. |
| Shared UV controls in Object and Edit Mode | The same Geometry panel, diagnostic settings, intensities, opacity, and face visibility controls work in both modes. Object Mode inspects selected mesh objects. Original UVs are used, so repeated modifier geometry does not create false overlaps. |
| Independent UV background opacity | UV Editor > Overlays > Image > Opacity changes the image independently of UV geometry. Zero is transparent, one is fully visible; the setting also takes effect when the main Overlays toggle is off. |
| Stable UV framing when changing images | Adding, removing, or switching images preserves the UVs' size and position on screen, including resolution/aspect changes and automatic image changes from the active material. Pan and zoom remain usable. |

## Added to the published source on 2026-10-05

Source commits: Mesh Holes `a5cd97e8`; 20 cm bone defaults and armature selection ordering `cebe29eb`; Object Mode wire selection for empty, curve, lattice, and loose mesh geometry `9e6f22f9`.

| Feature | Implemented behavior |
| --- | --- |
| Mesh Holes | 3D Viewport > Overlays > Objects > Mesh Holes highlights open boundary edges of selected meshes in Object Mode. Enabled by default, including older saved viewports. Uses the selection color and half the selection-outline width. Supports evaluated and subdivision meshes, X-Ray, In Front, clipping, and scene occlusion. Closed geometry, internal shared edges, loose edges, and edges incident to more than two faces receive no extra boundary lines. |
| Default new bone length | New armatures and Add Bone in Edit Mode default to 20 cm, adjusted for scene unit scale (`0.2` Blender units at standard metric scale). Explicit sizes are respected; existing bones, duplicates, and extrusions are unchanged. Armature object scale still applies. |
| Coincident selection outlines | Armature display submits unselected elements first, selected elements next, and selected active elements last. Selection outlines no longer depend on bone creation order at equal depth; existing depth tests and picking are retained. Object Mode also orders empty shapes/image frames, legacy curve wires, lattice cages, and loose mesh edges/points across their overlay passes. Coincident selected mesh-object outlines were also verified. |

See [Mesh Holes implementation and validation notes](mesh_holes.md).

## Multires Sculpt color painting added on 2026-10-05

Implementation commit: `92db31e2b2c8`.

Bmax implements Sculpt Mode color painting with Multires: Paint, Blur, Smear, per-attribute RGBA grid storage, seam stitching, level changes, undo/redo, save/reload, and transfer to a regular evaluated color attribute when applying the modifier. Float/Byte and Point/Corner attributes are supported. Sculpt masks block painting. Color Filter and Mask by Color report that Multires is unsupported. Classic Vertex Paint mode is unchanged. Color brush processing, GPU updates, and undo are localized to affected grids and their neighbors; persistent colors are committed once per stroke, and color-only strokes avoid geometry rebuilds between strokes.

The local benchmark measured median stroke times of 3.55 ms on 98,304 evaluated quads and 5.39 ms on 393,216 quads, versus 85.62 ms and 236.18 ms before optimization. These synchronous operator measurements exclude interactive GPU redraw and are specific to the documented test setup. See [Multires color implementation and validation](multires_color.md) for usage, persistence, compatibility, performance, and current limits.

This feature is part of the source on `main`; source publication does not update a separately copied Desktop build or publish a new Windows download.

## Bone display and UV overlay updates added on 2026-10-06

Implementation commit: `ee9bcdf37b32`. Source publication does not update the separately copied Desktop package or the public Windows download.

| Feature | Implemented behavior |
| --- | --- |
| Octahedral Bendy Bone display | For bones with multiple B-Bone segments, Octahedral draws a body per evaluated segment along the curve in Edit and Pose Mode. Octahedral Radius controls transverse size independently of B-Bone display widths, with spline and pose scaling retained. Only real bone endpoints get spheres; segment picking selects the whole bone. Single-segment display is preserved. |
| Independent UV fill opacities | Overlap, Flipped UVs, and Faces have separate opacity sliders in the shared Geometry panel, defaulting to 0.5 (50%) for new settings. Faces opacity is independent of UV lines and paint-mode face opacity. Saved values, including zero, are retained; missing properties migrate to the new defaults. |
| Object Mode UV line priority | UV wire lines are submitted after Faces and Flipped/Overlap diagnostic fills, keeping the lines visible above their colors at the chosen UV line opacity, without automatic quarter-strength fading. Edit Mode keeps its existing line order and white shell outlines remain on top. |
| Wire endpoint radius | Head and Tail spheres in Wire Edit Mode use the per-bone Octahedral Radius independently of rest-bone length. Selection and endpoint picking use the same sphere geometry. |
| Default bone rotation mode | Newly created pose channels and the Rotation Mode property default use XYZ Euler. Existing channels and copied bone rotation modes are preserved. |

Bendy Bone compatibility of this update was verified on 2026-10-06: newly created bones using XYZ Euler display segmented curvature in Wire and B-Bone modes, in Edit and Pose Mode. Set B-Bone Segments above 1 to enable the curved display; the creation default remains one segment. This preserves existing support rather than adding a new deformation feature.

## Compatibility and persistence

The custom bone display properties, origin-axis scale compensation, UV diagnostic settings, background opacity, and UV view aspect are saved in Bmax `.blend` files. Multires Sculpt color uses named per-attribute RGBA grids saved with the base mesh; applying Multires materializes the detail as a regular evaluated color attribute. Old-file migration supplies defaults when the corresponding fields are absent. Explicit zero intensity and opacity values survive saving and reopening. Official Blender may discard Bmax-specific properties when re-saving a file.

## Removed items

The bundled Max Puliero Pie Menu List addon was removed on 2026-10-02 (`00fbd0ac`) and is not included in the current feature list. The built-in Tab mode pie gesture remains implemented.

The removal of development tests and the branch cleanup are maintenance changes, not feature removals. All features listed above remain in `main`.
