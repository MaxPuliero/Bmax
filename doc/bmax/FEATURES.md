# Bmax feature inventory

This is the current implemented feature list for `main`, checked against the Bmax commits and source code on **2026-10-07**, including the complete history on `main`, the changed source files relative to the imported snapshot, and the per-feature implementation notes. Dates below are the dates the changes were committed to the published Bmax source, in Japan time. They are not separate binary release dates.

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
| Bmax branding | Custom splash logo and Windows icons, Bmax window titles and command-line version output, and a separate Bmax Windows application identity. The executable remains `blender.exe` and uses Blender 5.2 preferences and startup configuration. |

## Added to the published source on 2026-10-02

Commit: `ac5982c9`.

| Feature | Implemented behavior |
| --- | --- |
| Origin axes during transforms | Affect Only > Origins axes start at `0.2` Blender units. Uniform scaling is visible during the gesture and removed from the display on confirmation. Per-axis scaling remains visible. Cancellation, Undo/Redo, and save/reload preserve the intended state; object geometry keeps its world-space position. |
| Weighted Normal defaults | New modifiers use Face Area & Angle with Keep Sharp enabled. |
| Triangulate defaults | New modifiers have Keep Normals enabled. |
| Displace default | New modifiers use Strength `0.1`. Existing saved modifier settings are preserved. |
| UV Shell Outline | White outline drawn two physical pixels inward along UV shell boundaries, including holes. Width stays constant when zooming; internal UV edges are excluded. |
| UV Overlap | Red highlighting of the intersecting area, including partial overlap and overlap between objects. Opacity controls the fill from `0` (transparent) to `1` (opaque); the display updates as UV shells move. |
| Flipped UVs | Magenta highlighting of faces with reversed UV orientation. Red takes priority where overlap and flipped-face diagnostics coincide. |
| Shared UV controls in Object and Edit Mode | The same Geometry panel, diagnostic settings, fill opacities, and face visibility controls work in both modes. Object Mode inspects selected mesh objects. Original UVs are used, so repeated modifier geometry does not create false overlaps. |
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
| Independent UV fill opacities | Overlap, Flipped UVs, and Faces have separate opacity sliders in the shared Geometry panel, defaulting to 0.5 (50%) for new settings. Faces opacity is independent of UV lines and paint-mode face opacity. Saved values, including zero, are retained; missing properties migrate to the new defaults. |
| Object Mode UV line priority | UV wire lines are submitted after Faces and Flipped/Overlap diagnostic fills, keeping the lines visible above their colors at the chosen UV line opacity, without automatic quarter-strength fading. Edit Mode keeps its existing line order and white shell outlines remain on top. |
| Wire endpoint radius | Head and Tail spheres in Wire Edit Mode use the per-bone Octahedral Radius independently of rest-bone length. Selection and endpoint picking use the same sphere geometry. |
| Default bone rotation mode | Newly created pose channels and the Rotation Mode property default use XYZ Euler. Existing channels and copied bone rotation modes are preserved. |

## Bmax bug reporting

Implementation commit: `2334bd5436b8`.

**Help > Report a Bmax Bug**, the standalone system-information launcher, and Windows debug helpers direct Bmax reports to [GitHub Bmax issues](https://github.com/MaxPuliero/Bmax/issues). Runtime and standalone forms pre-fill version and system information and ask for an official Blender comparison. This routes custom-build reports through Bmax rather than Blender's upstream tracker.

## Object mesh tools added to the source on 2026-10-07

Implementation commit: `47896472`. The updated local Windows runtime is installed in `D:\blender_build\octahedral_radius\bin`; the Desktop package and public download remain separate.

| Feature | Implemented behavior |
| --- | --- |
| Object Mode Separate menus | Object > Separate and the Object context menu offer By Loose Parts and By Material, calling the existing C++ Separate operator. |
| Fast Fill Holes in Object and Edit Mode | Object > Clean Up and the Object context menu fill boundary loops of selected editable meshes without switching modes. Edit Mode retains selected-edge control. Simple cycles use direct C++ cap creation; other selected edge networks retain the general path. Default Sides is 0, with no hole size limit. Original non-manifold geometry is not repaired. |
| Face Sets for new hole patches | Optional New Face Sets in F9 assigns a different Sculpt Face Set to each connected new patch. Existing IDs are retained. With no pre-existing layer, original faces receive ID 1. The option defaults off and creates no layer on a no-op. |

The supplied 624,503-face scan measured 0.392 s for Loose Parts, 1.203 s for Material, 0.308 s for Object Fill Holes, 0.337 s with New Face Sets, and 0.203 s for the Edit Mode fill operator. All 28 boundary loops were closed and original positions stayed exact. These are background medians, one warm-up plus three runs, without interactive undo or redraw. Functional checks cover attributes, keys, shared data, selections, save/reload, Undo/Redo, and a successful Voxel Remesh of the filled scan. See [object mesh tools notes](object_mesh_tools.md).

## Windows icon update added to the source on 2026-10-07

Implementation commit: `5c0d7e11`. Both application and blend-file ICO resources use antialiased 32-bit bitmap entries through 96 pixels with matching transparency masks, and PNG entries from 128 to 256 pixels. Sixteen native sizes include intermediate DPI dimensions. The 1024-pixel Bmax logo artwork is unchanged.

All 16 frames passed round-trip pixel checks, all bitmap masks matched fully transparent pixels, and Windows native extraction/drawing of the compiled executable matched expected alpha composition on dark and light backgrounds within one channel level. See [Windows icon notes](windows_icons.md). The installed local runtime is updated; source publication does not update an existing Desktop copy or public download.

## Mesh Data normal weighting added to the source on 2026-10-07

Object Data Properties > Normals exposes Unweighted, Face Area, Corner Angle, and Face Area & Angle. Unweighted preserves the existing automatic normal calculation. Weight and Threshold affect the three weighted modes. Sharp edges/flat faces remain effective; explicit custom normals and the normal modifier take precedence over the data setting. Settings are saved per mesh, without generating a persistent custom-normal attribute. Official Blender ignores the extra fields and uses standard automatic normals in their absence. See [implementation and validation](mesh_normal_weighting.md). The local runtime has been fully rebuilt and verified: 80 background checks, OpenGL/Vulkan Undo and shading checks, Manifold Boolean/Sculpt Trim, and an official Blender file round trip passed. Cold normal calculation on the supplied scan measured 3.73 ms Unweighted and 5.72-6.37 ms weighted at Weight 50; weighted Edit geometry evaluation adds about 23-25 ms over Unweighted, excluding drawing. Source commit: `6f8c2738`. The separately copied Desktop packages and public Windows download have not been updated.

## Compatibility and persistence

The custom bone display properties, origin-axis scale compensation, UV diagnostic settings, background opacity, and UV view aspect are saved in Bmax `.blend` files. Multires Sculpt color uses named per-attribute RGBA grids saved with the base mesh; applying Multires materializes the detail as a regular evaluated color attribute. Old-file migration supplies defaults when the corresponding fields are absent. Explicit zero opacity values survive saving and reopening. Mesh Data weighting is also saved per mesh, defaults to Unweighted for older files, and is not baked into custom normals. Official Blender uses its standard automatic normals when no custom normals or normal modifier is present; re-saving there discards the Bmax weighting settings. Multires paint grids require Bmax; apply Multires in Bmax before transferring finished painted geometry to other builds. Official Blender may discard Bmax-specific properties when re-saving a file.

## Removed items

The bundled Max Puliero Pie Menu List addon was removed on 2026-10-02 (`00fbd0ac`) and is not included in the current feature list.

The removal of development tests and the branch cleanup are maintenance changes, not feature removals. All features listed above remain in `main`.
