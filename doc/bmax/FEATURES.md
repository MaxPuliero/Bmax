# Bmax feature inventory

This is the current implemented feature list for `main`, checked against the Bmax commits and source code on **2026-10-08**, including the complete history on `main`, the changed source files relative to the imported snapshot, and the per-feature implementation notes. Dates below are the dates the changes were committed to the published Bmax source, in Japan time. They are not separate binary release dates.

For demonstrations and usage details, see the [main README](../../README.md). For compilation, see [Building Bmax on Windows](BUILD_WINDOWS.md). Contributor and AI-agent rules are in [AGENTS.md](../../AGENTS.md).

## Connected Octahedral joints and extrusion sizes (2026-10-26)

Implementation: [`f90af1103223`](https://github.com/MaxPuliero/Bmax/commit/f90af110322384598de0ff545780af5be75ac4d6), included in `main`. Connected joints use the parent's tip-sphere radius and evaluated pose basis when trimming an Octahedral child's root. The child retains its own body width and tail radius, while the shared sphere keeps the parent's size and picking. Unconnected roots use their own sphere. Bendy segment trimming uses the same endpoint spaces, and singular sphere transforms omit the body safely. Bone evaluation and endpoint geometry are unchanged.

Standard extrusion (including root extrusion, mirrored and forked creation) and click-extrude copy Octahedral Radius and Axis Size from the source bone. Click-extrude also retains the source display type, matching standard extrusion. B-Bone width inheritance and gesture-defined length retain their existing behavior. No DNA or shared C++ layout changes are involved.

Release INSTALL succeeded. The installed runtime is `D:\blender_build\octahedral_radius\bin\blender.exe`, timestamp 2026-10-26 11:06:58 JST, reporting base hash `b98cf52093d0`. The installed runtime includes this correction; its metadata predates the implementation commit. Desktop packages and public downloads are unchanged.

Validation passed 85 guarded-memory background assertions covering tip/root, mirrored and forked extrusion, source-specific dimensions, mode conversion, and save/reload. Both OpenGL and Vulkan passed eight click-extrude assertions and ten offscreen viewport cases: smaller/larger/equal parent radius, disconnected roots, segmented/curved Bendy Bones, angled joints, Pose Mode, nonuniform scale and shear. The reproduced 24-pixel centerline gap became zero on both backends. Equal-radius and disconnected Edit Mode captures are byte-identical to the previous runtime. Nine fixtures match exactly between backends; the curved Bendy fixture has 48 differing pixels in a small overlapping-facet region. Representative corrected captures were visually inspected. GPU picking, interactive undo/redo and production rigs were not revalidated in this focused check. All verification files and the temporary comparison executable were removed afterward; build caches and runtime dependencies were retained.

## Blender 5.2.3 LTS synchronization (2026-10-23)

Implementation: [`776763085e8a`](https://github.com/MaxPuliero/Bmax/commit/776763085e8a3cdc2b17eae2680d6f22841847ac), included in `main`. The upstream base is now `v5.2.3` at `e13b728c6880d3084d9d57c30031ed5df8b39dee`. The release-to-release delta covers 124 upstream commits; 155 production-source/script files were imported. All 143 uncustomized imported files match the official tag, and the exact Bmax added/removed lines are preserved in all 12 shared files. New upstream test fixtures were not imported into this repository, following its test-file policy.

The update imports native fixes for Multires masks after Object Mode subdivision, bone picking, UV ring selection, texture painting/image lifetime, Cycles, and invalid-file validation. Bmax remesh, Fix Poles, By Loose Parts, Decimate caching, Face Set cuts/Split, Multires color, custom theme/UI and branding remain present. No manual Bmax source adaptation was needed; the obsolete installed `_animsys_refactor.py` module was removed after verifying it matched the official old copy.

A forced complete Release rebuild with project references followed by INSTALL succeeded because `SpaceType`, global state and Cycles session layouts changed. The installed `D:\blender_build\octahedral_radius\bin\blender.exe` is the 2026-10-23 17:46:30 JST build, reporting Bmax 5.2.3 LTS and base hash `08c66f5c43a6` (built before the implementation commit).

Validation passed 65 guarded-memory background assertions and 41 GUI assertions on each active backend, OpenGL and Vulkan. Compared remesh outputs, all six Split modes, all three Decimate modes and Manifold Boolean match the 5.2.2 references. Legacy remesh/cache/camera flags survive reading and new save/reload; genuinely painted Multires grids retain exact evaluated RGBA samples and modifier-application output. Object Mode Multires subdivision retains a full sculpt mask. GUI tests cover remesh with cleanup on/off, Face Set cutting, native Manifold Trim, bone picking, redraw and exact single-step Undo/Redo in Sculpt Mode. Fresh Multires Paint strokes on both backends produce exactly the reference evaluated colors, and a Cycles CPU color-attribute render passes. No new timing benchmark was run, and no additional speedup is claimed.

See [the integration audit and coverage limits](blender_5_2_3_sync.md). Official corrupt-file LFS fixtures required authentication and were not downloaded; local checks do not claim that coverage. The user's Desktop instance and distributed packages are unchanged. All follow-up verification files were external and removed after checking; build caches and runtime dependencies were retained.

## Voxel Remesh performance and optional Fix Poles (2026-10-23)

Implementation commit: [`83c95f81fd69`](https://github.com/MaxPuliero/Bmax/commit/83c95f81fd6990e1aebd003e7db2347742b91252), included in `main`; developed on `codex/voxel-remesh-performance` from `23181336769d`. Release INSTALL and validation are complete; Desktop packages and the public Windows download are unchanged.

The operator uses a native boundary-edge check before hole filling. Closed meshes skip the full BMesh hole search and source-copy preparation; open meshes keep the existing automatic fill and patch Face Sets. OpenVDB reads triangle topology through an adapter with the same float index-space rounding as the previous wrapper, and transfers extracted polygon pools directly into the final arrays while preserving vertex/face order and winding. Intermediate input/output buffers are reduced. Fix Poles is again editable in the Sculpt Remesh popover and Mesh Data panel, enabled by default and saved per mesh. A negative flag in a previously cleared bit keeps existing files enabled without changing DNA layouts; explicit disabling survives remesh and save/reload. When enabled, its complete topology cleanup, four smoothing sweeps and normals work remain unchanged apart from opt-in stage timers. Adaptivity does not disable the control. Voxel Size, Adaptivity, Preserve Volume/Attributes, Fair Position and per-part processing retain their settings and successful output behavior.

A pre-existing By Loose Parts crash was reproduced when Fix Poles removed the last faces of a tiny component and corner-attribute projection ran before the empty-result check. That check now runs immediately after Fix Poles: the operation cancels with the existing error message and leaves source geometry intact.

With profiling disabled, one warm-up and five fresh-input Sculpt runs on the same i7-14700KF/64 GiB setup measured 967.91 → 216.82 ms on a 1,310,720-triangle closed input (about 4.46×), and 697.60 → 557.82 ms on a 327,680-triangle closed input with preserved attributes (about 1.25×). A 256,649-face output stayed effectively unchanged at 996.35 → 992.42 ms because Fix Poles dominated. These synchronous timings include the operator's geometry Undo and spatial organization but exclude setup, interactive evaluation/redraw and later strokes. The first dense fixture's process peak working set was approximately 1,661 → 1,224 MiB, including setup, Undo and allocator retention. No universal speedup or viewport FPS improvement is claimed. See [full measurements, stage boundaries and limits](voxel_remesh_performance.md#verified-local-optimization-after-authorization-2026-10-23).

Validation passed 512 guarded-allocation comparison assertions across 22 operator cases, 70 additional assertions covering four Remesh modifier cases and source-preserving failed-part cancellation, and byte-for-byte comparisons on all six measured large fixtures. Both OpenGL and Vulkan passed 222 GUI assertions each for coarse remeshing, hole fill/fairing and loose parts, redraw, exact one-step Undo/Redo, staying in Sculpt Mode, and exact geometry/attribute save/reload. Six viewport captures were visually inspected. C++ formatting, source diff and default-quiet diagnostic checks passed. Optional `BMAX_REMESH_PROFILE=1` at process startup prints CPU stage timings; parent and child records overlap and must not be summed together.

The restored toggle additionally passed 48 guarded-memory background checks: editable/default-true RNA, enabled behavior in pre-control files, exact enabled-output comparison with the prior build on four closed/open/adaptive/per-part fixtures, bypass equivalence to raw voxel output, saved choices, and both panel draw paths. Each active GPU backend (OpenGL and Vulkan) passed 52 additional Sculpt checks for both states, adaptive/separate parts, redraw and exact single-step Undo/Redo. The script explicitly pushes before/after states to model interactive undo; an initial missing after-state push was a harness correction, with no product Undo code change.

The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe`, timestamp 2026-10-23 15:16:18 JST, reporting base hash `23181336769d`. No DNA or existing shared C++ structure layout changed. The user's Desktop session was preserved. Temporary verification scripts, fixtures, golden arrays, logs, captures, preferences, recovery files, crash diagnostics and comparison binary were external and removed; compilation caches and installed runtime dependencies were retained.

## Local View lighting and camera references (2026-10-23)

Implementation commit: [`cdbaf6fefc0c`](https://github.com/MaxPuliero/Bmax/commit/cdbaf6fefc0c749cc268662e3d9d5ebbb36b3d95), included in `main`. Release INSTALL and runtime validation are complete, and the user confirmed that the lighting and camera-reference behavior works. Publishing the source does not update the separately distributed binary.

Lights outside Local View remain available to Material Preview/Rendered shading, respecting Scene Lights and the existing hidden/type/collection filters. Their overlays, origins, names, light gizmos and picking shapes are excluded. Lights included in the isolation keep their ordinary widgets and selection behavior. Solid/Wireframe isolation retains the existing filtering.

**Camera Data Properties > Background Images > Keep in Local View** is saved per camera and defaults off, including older files. It retains only the active viewport camera, including reference images, while Local View is active. The exception is evaluated dynamically so toggling the setting or changing the viewport camera takes effect without re-entering isolation. Standard visibility restrictions still apply. Entering isolation with Frame Selected enabled preserves an already active camera view when the option is on. Local-view membership bits are unchanged, so retained objects do not affect the framing bounds or keep an otherwise empty isolation alive.

Validation passed 33 background assertions with guarded allocation, covering defaults, dynamic enable/disable and camera switching, hidden/type restrictions, membership, default framing, Edit Mode, explicitly isolated lights, exit and save/reload. Nine OpenGL and eleven Vulkan viewport captures confirmed actual scene-light contribution in Material Preview and EEVEE Rendered, retained references, hidden light widgets and normal explicit-light display. Vulkan additionally checked Cycles Rendered with CPU rendering. Image comparisons found the same light/reference/widget pixel counts across backends for the shared fixtures. Turning Extras off changed only the selected cube's 14 origin pixels in the excluded-light scene; explicitly including the light added 738 widget pixels. This does not claim coverage of every light/engine combination, GPU picking, camera stereo or linked-library cases. Source formatting and Python UI syntax checks also passed. The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe`, timestamp 2026-10-23 11:14:24 JST, reporting base hash `d3352ca742f4`. The user's running Desktop session was preserved. All verification files were external and removed afterward. The requested camera-panel screenshot is retained as a documentation asset and shown in the README.

The option uses an unused camera flag bit; saved structure sizes and shared C++ layouts are unchanged. The drawing change is confined to overlays for excluded lights; the remesh implementation is untouched. See the [Voxel Remesh performance investigation](voxel_remesh_performance.md) for current pipeline costs, candidates and limits. Desktop packages and the public download have not been updated.

## Decimate result cache (2026-10-22)

Implementation commit: [`bb4bbad2a787`](https://github.com/MaxPuliero/Bmax/commit/bb4bbad2a787c5b424c461bc6d45c6540827b89c), included in `main`. The local Release INSTALL is verified; Desktop packages and the public Windows download remain separate.

**Cache Result** is enabled by default in the Decimate panel for all three modes. Disabling it releases existing snapshots on the next evaluation and bypasses cache comparison/storage. The choice is saved per modifier using a previously unused negative flag bit, so existing files also default to enabled without a migration. Decimate keeps one input/result pair per evaluated modifier and reuses the result when both its settings and incoming mesh match exactly. Later modifier changes no longer require rerunning decimation for an identical input; changes to upstream geometry, attributes, UVs, colors, weights, or Decimate settings are detected automatically. The Apply operator benefits when it supplies the same input. Saved structure sizes and shared C++ layouts are unchanged. Unsupported legacy custom-data layers conservatively bypass caching. The new build passed 69 background comparison/lifecycle cases with guarded memory allocation, including exact coordinate/normal comparison, upstream edits, all Decimate modes, undo/redo, and save/reload. On 327,680 input triangles, median downstream parameter updates took 0.252 ms versus 1,294.420 ms before; one Apply operation took 2.818 ms versus 1,328.242 ms. These synchronous background timings exclude interactive undo and drawing. The Cache Result checkbox follow-up is installed and passed 34 additional guarded-allocation checks, including old-file defaults, on/off cycles in all modes, geometry/normal equality, persistence, duplication, and undo/redo. See [technical notes and validation](decimate_cache.md). Desktop packages and the public download have not been updated.

## Hair Curves guide theme (2026-10-08)

- **Hair Curves guide theme:** dedicated Width/Selected/Unselected controls under Preferences > Themes > 3D Viewport > Hair Curves. Editable guides use four physical pixels by default, with yellow selected and dark yellow unselected colors in Edit/Sculpt Mode. Sculpt Cage Opacity remains effective; soft selection blends the colors. Geometry Nodes generated strands and physical hair radii retain their existing behavior. Previous preferences acquire these defaults without resetting other theme settings. The local runtime is updated and verified on OpenGL/Vulkan. Source commit: `0fedd7e24fb9`. The Desktop packages and public binary download have not been updated.

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
| Mesh Holes | 3D Viewport > Overlays > Objects > Mesh Holes highlights open boundary edges of selected meshes in Object Mode. Enabled by default, including older saved viewports. Uses the selection color and half the selection-outline width, with a minimum of two physical pixels. Supports evaluated and subdivision meshes, X-Ray, In Front, clipping, and scene occlusion. Closed geometry, internal shared edges, loose edges, and edges incident to more than two faces receive no extra boundary lines. |
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

## Instance source random colors added to the source on 2026-10-07

Implementation commit: `f68b422571e9`, integrated into `main`. The local Windows runtime is installed; separately copied Desktop packages and the public download have not been updated.

Viewport Shading > Color exposes **Instances** for Random object/wireframe colors with one shared setting per viewport. Object Info nodes expose an independent checkbox affecting only Random, with EEVEE and Cycles SVM/OSL support. Both settings default off. Alt+D shared data, object/collection instances, Geometry Nodes references, generated prototypes, and nested instances group by geometry source. Realize Instances produces ordinary geometry. Generated prototype colors can change when prototype ordering changes.

170 background assertions, OpenGL/Vulkan viewport grouping and visible UI checks, actual Manifold Sculpt Trim, Manifold Boolean, and opening/rendering in official Blender passed. With Instances disabled, legacy OpenGL Solid/Wireframe captures were pixel-identical and Cycles values matched the previous build. See [technical notes and performance results](instance_random.md).

## Outliner range activation added to the source on 2026-10-07

Implementation commit: `87036d985cd3`, integrated into `main`. Shift-click makes the clicked endpoint active while retaining the range selection. Ctrl+Shift adds the range to the previous selection. Subsequent range selection starts from the new active endpoint. Data activation follows the existing selection-sync setting and mode-lock rules. Source publication and Windows packages remain separate.

The local Release runtime is installed. Nine GUI operator cases passed using simulated cursor positions and invoking the same item-activation operator as mouse selection: plain click, downward/upward Shift ranges, repeated range endpoints, Ctrl selection, and Ctrl+Shift ranges. Checks verified both the actual ViewLayer active object and the complete selected object set. The previous build reproduced the old A-active result for A to Shift+D; the new build retained A-D selection and activated D. Separately copied Desktop packages and the public Windows download have not been updated.

## Compatibility and persistence

The custom bone display properties, origin-axis scale compensation, UV diagnostic settings, background opacity, and UV view aspect are saved in Bmax `.blend` files. Multires Sculpt color uses named per-attribute RGBA grids saved with the base mesh; applying Multires materializes the detail as a regular evaluated color attribute. Old-file migration supplies defaults when the corresponding fields are absent. Explicit zero opacity values survive saving and reopening. Mesh Data weighting is also saved per mesh, defaults to Unweighted for older files, and is not baked into custom normals. Official Blender uses its standard automatic normals when no custom normals or normal modifier is present; re-saving there discards the Bmax weighting settings. Multires paint grids require Bmax; apply Multires in Bmax before transferring finished painted geometry to other builds. Official Blender may discard Bmax-specific properties when re-saving a file.

## Removed items

The bundled Max Puliero Pie Menu List addon was removed on 2026-10-02 (`00fbd0ac`) and is not included in the current feature list.

The removal of development tests and the branch cleanup are maintenance changes, not feature removals. All features listed above remain in `main`.

## Viewport polishing added to the source on 2026-10-08

Implementation commit: `5f6eba240126`.

- Enabled bone axes always draw in front of scene geometry and both armature layers; clipping and object selection keep their existing behavior.
- New armature objects default to Octahedral, In Front and object Wire. New bones default to Octahedral independently of their armature display type. Add Bone enables In Front and Wire on the armature object. Existing per-bone settings and copied/extruded display types are preserved.
- Mesh Holes uses at least two physical pixels, retaining the half-theme-width rule above that minimum. Its label and tooltip have Japanese, Italian, French and Spanish translations.

The local runtime is updated and verified; source publication does not update the separately copied Desktop packages or public Windows download. See the build guide for verification details.

## Bone body shape update added to the source on 2026-10-08

Implementation commit: `2e4febbdb1df`.

Octahedral display uses a flat square base and a tapered body between the real endpoint sphere boundaries. The Head and Tail sphere geometry, centers, radius and selection IDs are unchanged. Curved/rolled Bendy Bone segment bodies join at the actual neighboring segment positions, without intermediate spheres or radius-sized gaps at internal joints. Sphere trimming can cover multiple short segments; a fully covered body is omitted. Pose scale/shear and object scale remain effective, and bone evaluation is unchanged. The B-Bone box display type retains its existing geometry.

The local runtime is updated and verified. Source publication does not update the separately copied Desktop packages or public Windows download. Build and verification details are recorded in the build guide.

## Occluded Selection Outline theme (2026-10-13)

Preferences > Themes > 3D Viewport exposes **Occluded Selection Outline** as one RGBA color in the main category, directly below **Object Selected**. There is no child panel, separate opacity slider or custom-color checkbox. RGB controls the occluded object-outline color; alpha controls opacity (default orange, alpha 89/255, approximately 35%). Visible outlines and X-Ray retain their existing behavior. Bone-body wires, Edit Mode edges, origins and Mesh Holes are separate overlays.

The RNA getter/setter combines the existing three RGB storage bytes with the existing opacity byte, preserving ThemeSpace size and all member offsets. A version-2 initialization sentinel migrates the previous percentage-opacity controls and preserves zero alpha. The old custom-color field is used only to migrate the earlier local preferences; it is not exposed or consulted by rendering. Shared overlay globals retain their layout.

The local Windows Release runtime is rebuilt and installed in `D:\blender_build\octahedral_radius\bin`. The implementation is included in source commit `ad308b73519a` on `main`. Desktop packages and the public download have not been updated.

Six viewport cases and a GPU object pick passed per backend (OpenGL and Vulkan). The generated main-category panel was checked to contain exactly one RGBA entry, with the exact label and no previous child panel or separate controls. Green/blue RGB and alpha 0/0.5/1 changed only the occluded outline; the visible portion was unchanged. X-Ray ignored both RGB and alpha. Each nonzero-alpha comparison affected the same 1,124 occluded pixels.

Legacy local custom RGB plus 37% opacity migrated to RGBA, and legacy zero opacity remained zero. RGBA 0/0.5/1 alpha saved and reloaded correctly; zero also survived restart. Editing the new color left the neighboring theme title color intact. Verification scripts, preferences, captures and recovery files were external and removed afterward.

## Outline occlusion depth comparison: local update (2026-10-20)

The outline prepasses no longer shift clip-space Z toward the viewer by `1e-3`. Resolve compares unbiased camera-space distances using a relative tolerance of `1e-6` and a minimum margin corresponding to two representable scene-depth steps, whichever is larger. This replaces the old fixed projected-depth epsilon. Background depth is excluded and X-Ray bypasses the new comparison. Pixels without an outline are discarded before depth reads and reconstruction. No textures, geometry passes, draw calls, saved fields or selection shaders were added.

Release INSTALL completed in the existing local build directory. The implementation is included in source commit `1f04cb0020e9` on `main`; the installed runtime includes it. Compilation preceded the source commit, so the executable metadata does not identify this implementation commit. Desktop copies and the public Windows download are unchanged.

Ten Solid-mode offscreen viewport cases passed on each of OpenGL and Vulkan: perspective/orthographic projection, near clips 0.001/0.01/0.1/1 with far clip 1000, plus near 0.01/far 100000. A cube partially covered by a plane 0.02 units in front retained exactly 620 occluded pixels in perspective and 1,236 in orthographic across these clip settings. Before the change, the perspective case only detected occlusion at near clip 1, and none of the orthographic cases detected it. Alpha zero hid those portions; alpha one displayed them in the configured green. The two backends produced matching case counts, and the perspective captures were visually inspected.

Coincident, intersecting and X-Ray fixture images were byte-identical to the previous runtime on each backend. Outline width 4 also classified the covered section correctly. A coincident selected mesh retained its color whether created before or after its unselected twin. GPU picking and non-mesh prepass visual coverage were not revalidated in this focused check.

A pre-existing In Front limitation was also observed: Workbench writes zero scene depth at In Front pixels (`workbench_overlay_depth_frag.glsl`), while the common outline resolve compares against that scene depth. Such outlines can therefore receive the occluded color. Both the original depth predicate and the revised predicate classify that synthetic depth as occluding; this update does not redesign the In Front path.

Performance check: a selected 1,310,720-triangle icosphere, 512 x 512 offscreen Solid viewport, five warm-up draws and 21 timed draws per configuration. Timings include synchronous RGBA8 readback and NumPy conversion, not isolated GPU timestamps or interactive FPS. With outlines enabled, OpenGL medians were 2.033 ms before and 2.014 ms after. Vulkan measured 2.796 ms before, 6.061 ms in the first updated run and 1.63 ms on repetition; outline-disabled timings also varied substantially (2.188 / 5.861 / 1.96 ms). The Vulkan measurements do not isolate a reliable before/after cost. No additional work proportional to polygon count was introduced, but these measurements are not a universal performance guarantee.

Depth reconstruction cannot restore information already lost in the scene buffer. Extremely close surfaces or clipping ratios beyond the tested range may remain ambiguous; transparent occluders continue to follow the viewport's existing depth policy. Verification scripts, captures, scenes, logs and result files were external and removed after inspection; build caches and runtime dependencies were preserved.

### Orthographic signed-depth correction (2026-10-20)

The user-provided body/hood scene exposed a sign error in commit `1f04cb0020e9`: the comparison used `abs(view_z)` as a distance. Blender's orthographic viewport can include geometry on either side of view Z = 0, and absolute values can reverse or collapse depth ordering there. The original synthetic orthographic checks placed geometry entirely on the negative-Z side and missed this case. This is an implementation bug, distinct from finite depth-buffer precision.

The follow-up in source commit `42dbb0dd6bf2` on `main` uses signed depth `-view_z` for both surfaces and the precision-step reconstruction. Absolute values apply only to the magnitude used for relative tolerance. It adds no texture reads, buffers, draw calls or geometry passes. Release INSTALL completed before the source commit; the installed runtime includes the correction, although its executable metadata predates this implementation commit. The separately copied Desktop executables are still the earlier build.

The supplied file was opened read-only in separate factory-startup sessions. Actual viewport matrices were updated for orthographic view distances 0.2, 0.5, 0.8909104466 and 1.5. Alpha-zero/alpha-one image differences detected 2,315 / 910 / 504 / 295 occluded pixels on OpenGL and 2,315 / 910 / 505 / 295 on Vulkan; before the correction the hidden parts retained the visible color. Visual inspection confirmed that the hood outlines behind the forehead and face disappear at alpha zero and use the configured green at alpha one. The perspective control was byte-identical to the previous build for both alpha settings on each backend. GPU picking selected the body and hood correctly on both backends.

A separate synthetic symmetric orthographic projection tested five eye offsets and three clipping extents. At the ordinary tested extent 1000, the same 2,460-pixel hidden section was detected for every eye offset, including positive view Z and crossings of zero. A case exactly on a clip plane and a 0.02-unit gap at extreme extent 100000 remain subject to actual clipping and quantization respectively; this correction does not claim to remove those limits. No new performance benchmark was taken for this arithmetic-only follow-up; the earlier medians above belong to the first update.

All scripts, image arrays, captures, logs, result files, isolated configuration, and session recovery were placed in a dedicated external temporary directory and removed after validation. The original user file and existing Blender sessions were preserved.

## Outliner collection selection highlights (2026-10-13)

View Layer and Scenes displays highlight collections containing selected object rows, including nested ancestor collections. Collapsing a collection retains its derived highlight. Active selected objects propagate the active-row fill and border; other selected objects propagate the normal selected-row fill. The master Scene Collection row is unchanged. Shared collection paths and objects linked into multiple collections are evaluated by collection membership, so every containing branch receives the highlight.

The draw code collects selected object IDs through the complete tree, including closed descendants, and memoizes collection membership results for that draw. It does not modify TSE_SELECTED/TSE_ACTIVE or scene selection state, so collection-selection operators retain their existing behavior. Normal hover, search and drag/drop overlays remain in their existing order. No DNA or shared C++ layout changes are involved.

The local Release runtime is rebuilt and installed in `D:\blender_build\octahedral_radius\bin`. The implementation is included in source commit `d86a7a944cdf` on `main`. The user-provided comparison is included in the README as `doc/bmax/media/outliner_collection_highlight.png`; Desktop packages and the public download have not been updated.


Validation passed eleven View Layer cases on each of OpenGL and Vulkan: expanded three-level nesting; inner, middle and outer collection collapse; deselection while collapsed; selection moving to another collapsed branch; multi-selection and active-object changes; shared subcollections; objects linked to multiple collections; and clearing selection. Pixel measurements confirmed the expected highlighted row counts and unchanged master root. Existing object selection remained intact, and no inherited ancestor highlights appeared in the context as actually selected collections. An additional expanded Scenes case passed on both backends. Custom theme colors distinguished active and selected-row propagation. Scripts, scenes, captures and recovery files were temporary and removed afterward.

The active object remains identifiable after deselection through a brighter name. This text uses an 80% blend from the Outliner Text color toward Text Highlight: the default changes from RGB 195 to RGB 243 (`#F3F3F3`), below pure white. Selected active/other objects keep their existing orange theme colors, and hidden/excluded elements retain the usual fading. Seven cases per backend (OpenGL and Vulkan) verified an ordinary object, active deselected object, active selected object, another selected object, custom theme text/highlight colors, and hidden-object fading. Pixel checks confirmed unchanged unrelated text and selected-object colors; active and selection state remained intact. The local runtime is updated, the implementation is included in source commit `d86a7a944cdf`, and external verification files were removed.

## UV overlay curve crash correction (2026-10-14)

Object Mode UV diagnostics verify the original object's type before reading mesh data. Curves, surfaces, and text may be evaluated as meshes for drawing, but their original data is not a mesh. These objects keep the evaluated UV wire/face display and do not enter the original-mesh diagnostic path. Selected mesh objects retain diagnostics on their original UVs, including with modifiers.

The implementation is included in source commit `bcb82e789062` on `main`. Release INSTALL completed successfully in the existing build directory, and the corrected runtime is installed in D:\blender_build\octahedral_radius\bin. Desktop packages and the public download have not been updated.

The previous Desktop build reproduced the reported Mesh::active_or_default_uv_map_name / MeshUVs::object_sync crash when UV overlays were enabled for selected non-mesh objects, even with individual diagnostic filters off. The corrected build passed OpenGL and Vulkan GUI checks for beveled curves, extruded text, NURBS surfaces, individual and combined diagnostic filters, mixed-object selection, copy/paste into another scene, and deselection/reselection. Original object types were preserved. Object/Edit Mode mesh diagnostic screenshots with an Array modifier were byte-identical to the previous build in OpenGL and matched across backends. Validation used synthetic fixtures; the production .blend was not supplied. The user subsequently confirmed that the installed correction also resolves the reported problem. Verification files were external and removed afterward.

## Sculpt voxel, Face Set cuts, Bevel Mask, and Split (2026-10-21)

The implementation is included in source commit `b98ad0b25666` on `main`, developed from base `6f14da92` on `codex/sculpt-voxel-face-set-mask`. Full Release rebuild and subsequent incremental INSTALLs completed before the source commit; the installed runtime passed the verification below and still reports base hash `6f14da92f1c9`. Desktop packages and the public Windows download have not been updated.

- Saved mesh options **By Loose Parts** and **Fair Position** are exposed in Sculpt Remesh and Mesh Data, both initially disabled. Ctrl+R reads the same options. Components are remeshed and projected separately, then joined without welding.
- Every destructive remesh runs the existing simple native Fill Holes before voxelization and runs optional Fix Poles afterward (default enabled), including with Adaptivity. Optional Fair Position runs last, only on the interiors of the newly filled Face Sets after their reprojection. No pre-remesh tangency/refinement or separate fair-fill command remains. All stages form one operator and one undo step.
- **Sculpt > Fill Holes** exposes simple native filling as a separate Sculpt operation, creating new patch Face Sets by default. **Sculpt > Split** exposes By Face Sets, By Material, Masked, Visible, Faces by Edges, and Faces & Edges by Vertices; every variant keeps one object. The Face Sets menu also has a direct split shortcut. Masks replace Edit Mode selection for edge/vertex variants; empty masks mean the visible surface. Topology undo/redo stays in Sculpt Mode. Multires/Dyntopo are disabled for both operations, and Split is disabled with shape keys.
- Box/Lasso/Polyline/Line Face Set gestures have **Cut Geometry** enabled by default, with an explicit assignment-only option for the former behavior. Cutting inserts edges on base meshes and rejects Multires/Dyntopo; it does not separate objects or delete either side of the surface.
- **Bevel Mask** appears directly below Smooth Mask. Radius measures distance along mesh edges or sculpt-grid connectivity in object space. Profiles reuse Blender's falloff functions. The Custom curve is independent of brush/cavity curves and supports scene copy, save, and reload. The implementation handles mesh, Multires grids, and Dyntopo.

See [technical scope and validation](sculpt_voxel_masks.md). The later Sculpt Fill Holes/Split addition passed 95 GUI assertions per backend (OpenGL and Vulkan), covering all six modes, exact geometry undo/redo, custom data, point-only/non-manifold cases, native Edit Mode equivalence, and split/remesh integration. The corrected hole workflow passed 191 background assertions and 49 GUI assertions per backend (OpenGL and Vulkan), including equivalence to simple manual fill followed by remesh, fixed remeshed rims/existing regions during fairing, adaptive pole cleanup, and exact single-step undo/redo. Earlier checks covered the unchanged Face Set/Bevel Mask tools with 81 background assertions, 54 GUI assertions and 11 Multires/Dyntopo assertions per backend; the old pre-remesh fairing checks are superseded. Multires mask viewport crops exactly matched after undo, redo, and save/reload. Native Manifold Boolean and Sculpt Trim passed after the full DNA rebuild. A missing Multires dirty tag in mask undo/redo was corrected so restored masks are persisted. No performance benchmark is claimed.
