# Object Mode Separate and fast Fill Holes

Implemented on 2026-10-07 in source commit `47896472`; the installed Windows runtime has been updated in `D:\blender_build\octahedral_radius\bin`. The separately copied Desktop package and public download have not been updated.

## Usage

- In Object Mode, use **Object > Separate > By Loose Parts / By Material**, or **right click > Separate**. These menu entries call the existing native C++ Separate operator; Selection is intentionally absent from the Object Mode submenu.
- In Object Mode, **Object > Clean Up > Fill Holes** or **right click > Fill Holes** fills the boundary loops of selected editable mesh objects. Loose wire edges are left alone. Mesh datablocks shared by multiple selected objects are processed once; all users of a shared mesh see its geometry change.
- In Edit Mode, **Mesh > Clean Up > Fill Holes** fills loops formed by the selected edges. Select all edges to fill all surface openings. Partial boundary selections are not automatically expanded to the complete loop. The existing general edge-net behavior remains available for wire selections and branched networks.
- In **Adjust Last Operation (F9)**, **Sides = 0** means no limit and is now the default; a positive value limits the maximum number of edges in a filled hole.
- Enable **New Face Sets** in F9 to assign a distinct Sculpt Face Set to every connected newly filled patch. This option defaults off. Existing Face Set IDs are preserved, including negative IDs. If the mesh has no Face Sets, the original faces receive ID 1 and the new patches receive different IDs. With the option off, no Face Set attribute is created; existing attributes are inherited as usual.

The processing is native C++. The Python changes only expose the operators in the standard Blender menus. No addon, new mesh format, or DNA/shared structure layout change is required.

## Implementation

The `holes_fill` BMesh operator classifies input boundary edges once, visits their connected components, and directly creates a single ngon for each simple cycle. It checks the side limit before creating the face. Input edges are classified before filling because adding a cap changes their boundary status. Per-operator flags keep visitation local. An already existing face is never returned as a newly created patch.

Incomplete selections, branched boundary components, and selected loose wires retain the general edge-net path. Internal edges and edges incident to more than two faces are not repaired. Filling holes changes neither original vertex coordinates nor the original surface connectivity. The existing adjacent-face attribute propagation supplies cap materials, UVs, colors, custom face attributes, and winding.

Caps remain ngons, with Blender supplying render/remesh triangulation. The tool does not physically triangulate the original mesh or add cap diagonals to its topology. It closes the boundary; it does not reconstruct missing scan detail or choose a new interpolated surface shape.

`use_new_face_sets` is exposed on both `bpy.ops.mesh.fill_holes` and `bmesh.ops.holes_fill`. Face Set assignment runs after normal attribute propagation, grouping only newly created faces through shared edges. Each new group gets a fresh positive ID, avoiding collisions with existing IDs. The additional layer is created only when the operation actually fills a hole.

Object Mode temporarily converts the original mesh to BMesh without switching the object's mode, modifies its surface openings, writes it back only if faces were created, and invalidates geometry/sculpt caches. Modifiers are preserved and are not applied. Edit Mode uses the existing edit BMesh and selection/undo update machinery. A no-op returns Cancelled without creating a Face Set layer.

## Validation

The Windows Release build compiled and installed successfully with Visual Studio 2022 using the existing CMake INSTALL target, parallelism 2 and CL_MPCount=2. No shared structure layout changed, so an incremental build was sufficient.

Fifty-five functional assertions passed for Object/Edit modes, distinct patch Face Sets, preservation of existing IDs, ID wrap-around, UVs, corner colors, face/point attributes, materials, exact vertex coordinates and shape-key coordinates, cap winding, save/reload, side limits, partial selections, closed meshes, loose-wire fallback, shared datablocks, multiple objects, duplicate-face avoidance, and Separate menu/operator results.

Separate GUI sessions checked real Undo and Redo in Object and Edit Mode, including removing/restoring the Face Set layer and the patch IDs. OpenGL and Vulkan viewport checks confirmed Sculpt Mode Face Set display. The visible F9 popup was checked for Sides = 0 and New Face Sets. Temporary verification scripts, scenes, logs, screenshots, and recovery files were kept outside the repository and removed after validation. The user's existing Blender session and original scan file were preserved.

## Performance on the supplied scan

Input: `C:\Users\MassimilianoNavaPuli\Desktop\test.blend`, one mesh with 314,279 vertices, 938,797 edges, 624,503 triangular faces, three materials, and 28 simple boundary loops containing 4,084 boundary edges. The original file was never saved by these checks.

Factory startup, background mode, autoexec disabled, global undo disabled. Each method had one warm-up and three measured runs, reloading the original file before every run. Times are medians of synchronous execution, excluding file loading, interactive undo storage, and GPU redraw.

| Operation | Median | Result |
| --- | ---: | --- |
| Separate By Loose Parts, Object Mode | 0.392 s | 7 objects, all input faces retained |
| Separate By Material, Object Mode | 1.203 s | 3 objects, all input faces retained |
| Fill Holes, Object Mode, Face Sets off | 0.308 s | 28 new caps, zero remaining boundary edges |
| Fill Holes, Object Mode, New Face Sets on | 0.337 s | 28 caps and 28 distinct new Face Sets, zero boundary edges |
| Fill Holes, Edit Mode, New Face Sets on | 0.203 s | Same 28 caps/Face Sets; mesh already in Edit Mode |
| Enter Edit Mode, select all, fill with Face Sets, exit Edit Mode | 0.409 s | Complete mode round trip |

The previous Fill Holes implementation took 10.752 s for the same Edit Mode round trip and created only 20 faces, leaving 2,008 boundary edges. The new round trip including Face Sets is about 26 times faster in this specific case; outcomes differ because the new path also closes the eight previously missed loops.

All measured Fill Holes runs left the original vertex positions exactly unchanged. The 59 pre-existing edges incident to more than two faces remained untouched. After filling the actual scan, Voxel Remesh completed at a coarse voxel size of approximately 0.102566 Blender units (largest object dimension divided by 128): 35,180 vertices, 35,174 faces, zero boundary edges, and all edges manifold. This functional check confirms the intended fill-then-remesh workflow on the supplied input; it is not a performance guarantee or validation of every remesh resolution.
