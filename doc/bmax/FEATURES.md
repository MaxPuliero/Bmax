# Bmax feature inventory

This is the current implemented feature list for `main`, checked against the Bmax commits and source code on **2026-10-05**. Dates below are the dates the changes were committed to the published Bmax source, in Japan time. They are not separate binary release dates.

For demonstrations and usage details, see the [main README](../../README.md). For compilation, see [Building Bmax on Windows](BUILD_WINDOWS.md).

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

Commit: `a5cd97e8`.

| Feature | Implemented behavior |
| --- | --- |
| Mesh Holes | 3D Viewport > Overlays > Objects > Mesh Holes highlights open boundary edges of selected meshes in Object Mode. Enabled by default, including older saved viewports. Uses the selection color and half the selection-outline width. Supports evaluated and subdivision meshes, X-Ray, In Front, clipping, and scene occlusion. Closed geometry, internal shared edges, loose edges, and edges incident to more than two faces receive no extra boundary lines. |

See [Mesh Holes implementation and validation notes](mesh_holes.md).

## Compatibility and persistence

The custom bone display properties, origin-axis scale compensation, UV diagnostic settings, background opacity, and UV view aspect are saved in Bmax `.blend` files. Old-file migration supplies defaults when the corresponding fields are absent. Explicit zero intensity and opacity values survive saving and reopening. Official Blender may discard Bmax-specific properties when re-saving a file.

## Removed or still planned

The bundled Max Puliero Pie Menu List addon was removed on 2026-10-02 (`00fbd0ac`) and is not included in the current feature list. The built-in Tab mode pie gesture remains implemented.

These items remain planned and are not implemented in `main`:

- Relationship lines restricted to selected bones.
- Default newly created bone length of `0.2` Blender units.
- Reliable selection outlines for coincident bones/objects.

The removal of development tests and the branch cleanup are maintenance changes, not feature removals. All features listed above remain in `main`.
