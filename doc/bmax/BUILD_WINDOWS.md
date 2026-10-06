# Building Bmax on Windows

See the [complete feature inventory](FEATURES.md) for the implemented additions, and [AGENTS.md](../../AGENTS.md) for contributor and AI-agent instructions.

## Branch and source

`main` is the stable Bmax branch and includes Mesh Holes, armature display controls, UV diagnostics, origin axes, modifier defaults, 20 cm new bones, coincident selection outlines, and Multires Sculpt color painting. Use a temporary feature branch for a new change, then integrate it into `main` when ready.

The retired branches are preserved by tags `archive/armature-ui-2026-10-05` and `archive/bmax-publication-2026-10-05`. These tags are historical snapshots, not the default build source.

Bmax uses an imported Blender 5.2.2 source snapshot. Keep this source base when updating dependencies; do not replace it with upstream Blender's current development branch. Inherited LFS assets download from Blender's public server; uploads target GitHub Bmax. Reading the public upstream assets does not require permission to modify Blender's repository. Bmax artwork and demonstrations are ordinary Git files.

## Existing local build

The verified local setup uses these paths:

| Item | Path |
| --- | --- |
| Source checkout | `D:\blender_prj` |
| CMake build directory | `D:\blender_build\octahedral_radius` |
| Installed executable | `D:\blender_build\octahedral_radius\bin\blender.exe` |

The build directory's name is historical: it now builds all of Bmax, including Mesh Holes. Keep the directory to reuse its compilation cache. These commands assume the existing Visual Studio 2022 / MSVC v143 configuration and installed dependencies; a fresh machine also needs the dependencies described in the Windows build instructions linked from the root README.

Run in PowerShell. Inspect the working tree before switching branches; preserve any unfinished changes first.

```powershell
git -C D:\blender_prj status --short
git -C D:\blender_prj switch main
if ($LASTEXITCODE -ne 0) { throw 'Could not switch to main' }
git -C D:\blender_prj log -1 --oneline

cmake -S D:\blender_prj -B D:\blender_build\octahedral_radius `
  -DWITH_GTESTS=OFF -DWITH_GPU_DRAW_TESTS=OFF -DWITH_GPU_BACKEND_TESTS=OFF
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }

cmake --build D:\blender_build\octahedral_radius `
  --config Release --target INSTALL --parallel 2 -- /p:CL_MPCount=2
if ($LASTEXITCODE -ne 0) { throw 'Bmax build failed' }
```

The conservative parallelism also limits MSVC compilation within each project; a full rebuild at higher parallelism exhausted the precompiled-header heap on this machine.
After changing DNA or shared C++ structure layouts (for example, adding a CustomData type), force a rebuild of the executable and all project references before installation. A successful incremental INSTALL alone does not prove that every library uses the new layout. Old objects can link successfully and then crash when reading mesh fields. Keep the configured build directory and dependencies; rebuild its targets instead of deleting the directory.

```powershell
cmake --build D:\blender_build\octahedral_radius `
  --config Release --target blender --parallel 2 -- /t:Rebuild /p:CL_MPCount=2
if ($LASTEXITCODE -ne 0) { throw 'Bmax full rebuild failed' }

cmake --build D:\blender_build\octahedral_radius `
  --config Release --target INSTALL --parallel 2 -- /p:CL_MPCount=2
if ($LASTEXITCODE -ne 0) { throw 'Bmax installation failed' }
```

Following layout changes, verify mesh operations using geometry realization, such as Manifold Boolean and Sculpt Trim, in addition to the feature that introduced the change.

`INSTALL` updates the executable and runtime files in `bin`. Close any process running from that output directory before installing. A separately copied desktop package is not updated by this command. Run or distribute the complete installed runtime directory, not the executable alone.

The existing configuration omits precompiled CUDA, HIP, and oneAPI kernels. OptiX is disabled because its SDK is not installed. These are the settings of this local build, not requirements for every Bmax build.

## Check the installed build

Use an inline background check rather than creating a test script. Factory startup avoids loading the user's preferences, and the check does not save a scene.

```powershell
& 'D:\blender_build\octahedral_radius\bin\blender.exe' `
  --background --factory-startup --disable-autoexec --python-exit-code 1 `
  --python-expr "import bpy; print('BMAX_BUILD', bpy.app.version_string, bpy.app.build_hash.decode(), bpy.app.build_branch.decode()); assert bpy.types.View3DOverlay.bl_rna.properties['show_mesh_holes'].default is True; print('OCTAHEDRAL_RADIUS', bpy.types.Bone.bl_rna.properties['octahedral_radius'].default); print('AXIS_SIZE', bpy.types.Bone.bl_rna.properties['axis_size'].default); print('BMAX_STARTUP_OK')"
if ($LASTEXITCODE -ne 0) { throw 'Bmax startup check failed' }
```

Compare the printed build hash with the source commit used for compilation. Expected defaults are Mesh Holes enabled, Octahedral Radius approximately `0.02`, and Axis Size approximately `0.03` Blender units. This checks startup and registered properties; it does not replace a visual viewport check.

## Temporary files and tests

Do not add or retain Bmax development test scripts, test scenes, captures, caches, or build logs in the source repository unless explicitly requested. If a verification needs files, use a temporary directory outside the repository and remove the files created for that verification afterward. Keep the compilation cache and required runtime dependencies.

The Bmax-specific development tests were removed from Git and their CMake registrations were removed. The inherited Blender tests remain in the source tree, but the local build disables the test targets.

## Last verified build

On 2026-10-05, `main` at source commit `9e6f22f93ed9` compiled and installed successfully as **Bmax 5.2.2 LTS**, Windows Release, using Visual Studio 2022. Background checks covered 25 assertions for the 20 cm defaults across metric, imperial, and unitless scenes, explicit sizes, and duplication. Invoked viewport operators also respected scene unit scale. Fifty viewport captures across OpenGL and Vulkan verified coincident selection outlines in Edit, Pose, and Object Mode, mesh-object outlines, occlusion, and In Front. Object-origin dots are a separate overlay and were excluded from the outline comparison. Verification scripts and captures were created outside the repository and removed afterward.

The Object Mode wire-selection follow-up was verified with 144 viewport captures across OpenGL and Vulkan: empty axes/cubes/circles/image frames, legacy curves, lattice cages, loose mesh edges and vertices, and coincident pairs across these overlay types. Forty-eight first/last-order comparisons matched, including selected-active priority. Sixteen GPU picking checks selected the expected objects. Solid-mode occlusion, In Front, and Wireframe were checked for empties, curves, lattices, and loose edges. Object-origin markers were excluded from the contour comparisons. All verification files were temporary and removed after the checks.

The Multires Sculpt color implementation in source commit `92db31e2b2c8` was compiled and installed on 2026-10-05; compilation and validation took place before committing the tested source changes. Validation covered Paint/Blur/Smear, Multires levels, exact color undo/redo, save/reload, modifier application, Float/Byte and Point/Corner attributes, triangles/ngons, masks, separate attributes, Edit Mode lifecycle, OpenGL/Vulkan viewport display, and a Cycles CPU color-attribute material render. See [Multires color notes](multires_color.md) for scope and limitations. This does not update the separately copied Desktop package or the published download.

The local Multires color performance correction was compiled and verified on 2026-10-05. The final regression includes localized undo and anchored strokes; OpenGL/Vulkan viewport checks and Cycles renders from Sculpt Mode passed. A before/after benchmark measured about 24x and 44x lower synchronous stroke time on 98,304 and 393,216 evaluated quads respectively. See [Multires color performance notes](multires_color.md) for the exact method and limits. Temporary verification files were removed; compilation caches and runtime dependencies were preserved.

## Local bone display update verified on 2026-10-06

The source later committed as `ee9bcdf37b32` was compiled and installed in the existing build directory before committing. Wire Edit Mode Head/Tail spheres use Octahedral Radius independently of bone length. New pose channels and the RNA Rotation Mode default use XYZ Euler.

Nine viewport captures per backend (OpenGL and Vulkan) checked equal endpoint sizes for different bone lengths, length changes, radius changes, nonuniform object scale, inverse creation order with coincident selected/active bones, occlusion, and In Front. Each backend also passed four GPU endpoint-picking checks. Doubling the radius increased the measured sphere diameter from 75 to 149 pixels; changing bone length preserved the diameter within one pixel.

Background checks covered new armatures, Add Bone, Python-created bones, extrusion, the XYZ Euler property default, existing Quaternion bones, duplication retaining the source rotation mode, and save/reload of rotation modes and Wire radius. The verified implementation is recorded in source commit `ee9bcdf37b32`. The Desktop package and public download were not updated. All verification files were temporary and removed afterward.

Bendy Bone compatibility was additionally verified in the installed build on 2026-10-06, without further source changes or recompilation. Six viewport captures per backend checked new-armature and Add Bone bones with eight segments in Wire and B-Bone display, in Edit and Pose Mode, with XYZ Euler pose rotation. Wire changed from a straight centerline to the expected curved segmented line; B-Bone displayed the curved segment boxes. Save/reload preserved segment counts, curvature, display type, and XYZ Euler. The verification files were temporary and removed afterward.

## Octahedral Bendy Bone display verified on 2026-10-06

The source later committed as `ee9bcdf37b32` was further updated, compiled, and installed to draw an octahedral body per B-Bone segment in Edit and Pose Mode. The existing B-Bone spline matrices drive the display; bone evaluation and deformation are unchanged. Segment transverse size uses Octahedral Radius before spline and pose scaling, independently of B-Bone display widths. Only the real Head and Tail have endpoint spheres, and every segment retains the whole bone's selection ID. Single-segment display is preserved.

Fourteen viewport captures per backend (OpenGL and Vulkan) checked one and eight segments, straight and curved display, switching back to one segment, radius changes, zero and nonzero B-Bone display widths, XYZ Euler pose roll, nonuniform pose scale, parent nonuniform scale with child roll, coincident selection in inverse creation order, occlusion, and In Front. Straight eight-segment bones displayed eight octahedral bodies, while one-segment bones displayed one. Changing B-Bone widths produced identical Octahedral pixels; doubling Octahedral Radius increased measured body width from 18 to 34 pixels. Three GPU body-picking checks per backend selected the bone at different interior segments. Save/reload retained Octahedral display, segment count, and radius.

The installed build is `D:lender_buildoctahedral_radiusinlender.exe`. The implementation is recorded in source commit `ee9bcdf37b32`; the Desktop copy and public download were not updated. Verification files were temporary and removed afterward.
## UV fill opacity and Object Mode line priority verified on 2026-10-06

The source later committed as `ee9bcdf37b32` was compiled and installed with independent Overlap, Flipped UVs, and Faces opacity controls. All three default to 0.5 for new settings. Object Mode submits UV lines after face and diagnostic fills and uses the chosen UV line opacity without the automatic quarter-strength fading applied to modifier/paint guides. Faces opacity remains a multiplier of the existing theme face alpha.

Background checks verified constructor and RNA defaults, migration from a file saved by the previous installed build, and save/reload at 0, 0.37, and 1. Migration supplied the two new defaults while preserving existing overlap opacity 0.27, line opacity 0.83, paint face opacity 0.12, and image opacity 0.66. Explicit zero values survived reopening.

Twenty-two viewport captures per backend (OpenGL and Vulkan) checked all three fills at 0, 0.5, and 1 in Object and Edit Mode, combined diagnostics, and Object Mode line priority with and without smooth wires. Raster line cores remained unchanged above fully opaque diagnostics; smooth line cores retained contrast. Red retained priority where both diagnostics coincide. A separate full-window capture verified the three readable Opacity sliders at 0.500 in the widened Geometry panel. The final panel-width change updated the installed Python runtime directly after the successful C++ INSTALL build.

The implementation is recorded in source commit `ee9bcdf37b32`; validation and installation preceded the commit. The installed build is `D:\blender_build\octahedral_radius\bin\blender.exe`; the Desktop copy and public download were not updated. Verification scripts, scenes, captures, results, and logs were temporary and removed afterward.
## Full rebuild and Sculpt Trim regression verified on 2026-10-06

Tracked in [Bmax issue #1](https://github.com/MaxPuliero/Bmax/issues/1).

The previously installed incremental build crashed in `geometry::preprocess_meshes`, reached from Manifold Boolean and Sculpt Trim. The existing `realize_instances.obj` was dated 2026-10-02, before the 2026-10-05 CustomData layout change. The failure was reproduced both by a Manifold Boolean modifier on two closed cubes and by the real Sculpt Box Trim operator on a closed cube without Multires. This established a build consistency regression rather than a need to change the Trim algorithm.

A complete `/t:Rebuild` of the `blender` target and its project references, followed by `INSTALL`, completed successfully from `main` at `942bc89b318e`. The installed executable and all mesh-layout-dependent libraries now use the same source layout. The original Boolean reproduction passed afterward without source algorithm changes.

Thirty real Sculpt Trim cases per backend (OpenGL and Vulkan) passed: Box, Lasso, Polyline, and Line with Manifold, Exact, and Float solvers; Difference, Union, and Join where supported. Before every case the input cube was verified to be closed/manifold, with positive signed volume and no modifiers. Trim was never tested on Multires. Geometry changed as expected, coordinates remained finite, and the ordinary color attribute survived. All 30 resulting vertex/face-count pairs per backend matched the same cases run in a separate Blender 5.2.2 official session. Sixty temporary Bmax viewport captures recorded the results; representative Difference and Union captures were inspected.

Nine Boolean modifier cases across the three solvers and Difference/Union/Intersect also passed. Geometry Nodes realization and joining passed, as did XYZ Euler and the three 50% UV fill defaults. These focused checks do not establish compatibility with every production mesh or every Boolean option.

The corrected installed runtime is `D:\blender_build\octahedral_radius\bin`. The user's running official Blender Desktop session was left open. No Desktop package or public download was changed. Verification files were created outside the repository and removed afterward; configured build directories and dependencies were retained.
## Bmax bug-report routing verified on 2026-10-06

Source commit `2334bd5436b8` routes Help > Report a Bmax Bug and the standalone system-information launcher directly to `https://github.com/MaxPuliero/Bmax/issues/new`, with an encoded Markdown body containing build/system information, reproduction fields, and an official Blender comparison. Windows debug helpers and the packaged HTML readme point to the same Bmax tracker. A Bmax issue template is included. The Trim build regression was recorded in [Bmax issue #1](https://github.com/MaxPuliero/Bmax/issues/1) before these commits.

The changed Python scripts, Windows helpers, and packaged readme were copied into the existing corrected local runtime; no C++ source changed or required recompilation. A separate GUI session verified both the runtime function and the Help preset URL, OS/GPU/backend/version information, and the visible Bmax menu label. The startup URL generator also passed with the bundled Python in isolated `-I` mode and correctly read the installed build information. These checks generated URLs without opening a browser or submitting further reports. Temporary scripts, logs, and captures were removed afterward. The Desktop copy and public download were not updated.