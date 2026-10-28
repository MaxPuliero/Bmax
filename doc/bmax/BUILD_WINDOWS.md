# Building Bmax on Windows

See the [complete feature inventory](FEATURES.md) for the implemented additions, and [AGENTS.md](../../AGENTS.md) for contributor and AI-agent instructions.

## Branch and source

`main` is the stable Bmax branch and includes Mesh Holes, armature display controls, UV diagnostics, origin axes, modifier defaults, 20 cm new bones, coincident selection outlines, and Multires Sculpt color painting. Use a temporary feature branch for a new change, then integrate it into `main` when ready.

The retired branches are preserved by tags `archive/armature-ui-2026-10-05` and `archive/bmax-publication-2026-10-05`. These tags are historical snapshots, not the default build source.

Bmax uses an imported Blender 5.2.3 source snapshot. Keep this source base when updating dependencies; do not replace it with upstream Blender's current development branch. Inherited LFS assets download from Blender's public server; uploads target GitHub Bmax. Reading the public upstream assets does not require permission to modify Blender's repository. Bmax artwork and demonstrations are ordinary Git files.

## Existing local build

The verified local setup uses these paths:

| Item | Path |
| --- | --- |
| Source checkout | `D:\Bmax` |
| CMake build directory | `D:\blender_build\octahedral_radius` |
| Installed executable | `D:\blender_build\octahedral_radius\bin\blender.exe` |

The build directory's name is historical: it now builds all of Bmax, including Mesh Holes. Keep the directory to reuse its compilation cache. These commands assume the existing Visual Studio 2022 / MSVC v143 configuration and installed dependencies; a fresh machine also needs the dependencies described in the Windows build instructions linked from the root README. Earlier build records below used `D:\blender_prj`; the current checkout is `D:\Bmax`.

For a fresh configuration matching the local build, initialize the pinned Windows library submodule and explicitly select the generator and runtime destination:

```powershell
git -C D:\Bmax submodule update --init --checkout --depth 1 -- lib/windows_x64
if ($LASTEXITCODE -ne 0) { throw 'Windows dependencies could not be initialized' }

cmake -S D:\Bmax -B D:\blender_build\octahedral_radius `
  -G "Visual Studio 17 2022" -A x64 -T v143 `
  -DWITH_GTESTS=OFF -DWITH_GPU_DRAW_TESTS=OFF -DWITH_GPU_BACKEND_TESTS=OFF `
  -DWITH_CYCLES_DEVICE_OPTIX=OFF -DWITH_CYCLES_CUDA_BINARIES=OFF `
  -DWITH_CYCLES_HIP_BINARIES=OFF -DWITH_CYCLES_ONEAPI_BINARIES=OFF `
  -DCMAKE_INSTALL_PREFIX=D:\blender_build\octahedral_radius\bin
if ($LASTEXITCODE -ne 0) { throw 'Initial CMake configuration failed' }
```

Run in PowerShell. Inspect the working tree before switching branches; preserve any unfinished changes first.

```powershell
git -C D:\Bmax status --short
git -C D:\Bmax switch main
if ($LASTEXITCODE -ne 0) { throw 'Could not switch to main' }
git -C D:\Bmax log -1 --oneline

cmake -S D:\Bmax -B D:\blender_build\octahedral_radius `
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
## Final source build verified on 2026-10-06

Source commit `6a2340f5a1dd` on `main` was compiled as Windows Release and installed with Visual Studio 2022 using the documented `blender` and `INSTALL` targets, `--parallel 2`, and `/p:CL_MPCount=2`. The existing fully rebuilt mesh-layout-dependent components were retained; no further DNA or shared-layout changes occurred. The installed executable reports hash `6a2340f5a1dd`.

The installed runtime passed the original Manifold Boolean crash reproduction and checked XYZ Euler plus all three UV fill defaults at 0.5. Box, Lasso, Polyline, and Line Trim passed in both OpenGL and Vulkan on closed/manifold cubes with positive signed volume and no modifiers; no Trim was run on Multires. Each Difference operation completed and removed the intended side of the cube. These are final smoke checks following the comprehensive Trim regression documented above.

The Help bug-report preset produced the Bmax GitHub issue URL with the installed hash and GPU backend. The standalone launcher also passed using bundled Python in isolated `-I` mode, preserving build information and targeting the same Bmax tracker. Checks generated URLs without opening a browser or submitting further issues. All verification scripts, logs, and results were temporary and removed afterward.

The ready local runtime is `D:\blender_build\octahedral_radius\bin`; source/documentation publication does not update a separately copied Desktop package or the public Windows download.
## Object mesh tools local build verified on 2026-10-07

The Object Mode Separate menus in source commit `47896472`, fast C++ Fill Holes in Object/Edit modes, and optional New Face Sets were compiled and installed as Windows Release using the existing INSTALL target with parallelism 2 and CL_MPCount=2. No DNA or shared C++ structure layout changed. The installed binary is `D:\blender_build\octahedral_radius\bin\blender.exe`. The separately copied Desktop package and public download were not updated.

Fifty-five background functional assertions passed, followed by GUI Undo/Redo and Sculpt Face Set display checks on OpenGL and Vulkan, plus the visible F9 options popup. The supplied 624,503-face `test.blend` passed all benchmark cases and a Voxel Remesh after filling. See [object mesh tools validation](object_mesh_tools.md) for exact methods and medians. Verification scripts and artifacts were temporary and were removed after checking; the original input file and build cache were preserved. The source changes are committed; source publication does not update the separately copied Desktop package or public download.

## Windows icon resource update verified on 2026-10-07

The resources in implementation commit `5c0d7e11` were built and installed with the same Windows Release INSTALL command. Both ICO files contain 16 alpha-antialiased native sizes, with 32-bit DIB/AND-mask payloads for entries through 96 pixels and PNG payloads for larger previews. Windows resource extraction and DrawIconEx checks on the compiled executable passed all 16 sizes on dark and light backgrounds, with alpha composition error at most one channel level. See [Windows icon validation](windows_icons.md).

The local runtime remains `D:\blender_build\octahedral_radius\bin`. No user Blender session was closed, no Desktop copy was overwritten, and the public Windows download was not replaced. Verification scripts, extracted icon images, comparisons and build logs were temporary and removed afterward.

## Local Mesh Data normal weighting build verified on 2026-10-07

Source commit `6f8c2738`, integrated into `main`, adds Mesh DNA fields, so the executable and all references were rebuilt using the documented Release `blender` target with `/t:Rebuild`, parallelism 2 and CL_MPCount=2, followed by `INSTALL`. A subsequent incremental INSTALL included Edit Mode Undo handling, without a further shared-layout change. The installed runtime is `D:\blender_build\octahedral_radius\bin`; it was compiled before the source commit, so its reported parent hash `e25830f92dc9` does not identify the normal weighting implementation.

80 background checks passed, including weighted/custom normal precedence, exact legacy Unweighted output, serialization and old-file migration, generated Geometry Nodes realization and Manifold Boolean. GUI checks on OpenGL and Vulkan passed Object/Edit Undo/Redo, the actual Mesh Data panel and shading changes, and real Manifold Sculpt Box Trim. Official Blender 5.2.2 opened and re-saved a Bmax fixture, showing base automatic normals and preserving custom normals; Bmax reopened the official save with default weighting settings. See [normal weighting validation and timings](mesh_normal_weighting.md).

The supplied scan measured 3.73 ms for cold Unweighted normals and 5.72-6.37 ms for the three weighted modes at Weight 50. Edit geometry evaluation measured 1.82 ms Unweighted versus 24.40-26.88 ms weighted, excluding subsequent normal read/drawing and undo. These are background medians, one warm-up plus five timed runs. Temporary verification files were removed. The source implementation is included in `main` as `6f8c2738`. Desktop packages and public downloads were not updated by this local development build.

## Local instance random colors build verified on 2026-10-07

Source commit `f68b422571e9`, integrated into `main`, extends shared DupliObject and Cycles object structures. All executable references were rebuilt with the Release `blender` target, `/t:Rebuild`, parallelism 2 and `/p:CL_MPCount=2`, followed by the normal `INSTALL` target. The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe`; its reported parent hash `4e6f1c34a32a` does not identify this implementation, which was committed after compilation. Desktop copies and the public Windows package are separate.

Validation passed 170 background assertions, EEVEE/Cycles CPU SVM and OSL renders, OpenGL and Vulkan viewport grouping and UI captures, save/reload, official Blender opening/rendering, Manifold Boolean, and actual Sculpt Box Trim on both backends. Vulkan used `--debug-gpu` to disable pipeline-cache persistence. Benchmark setup and medians are recorded in [instance random notes](instance_random.md). Verification scripts, scenes, images and logs were temporary and removed after checking; build/runtime dependencies and existing user files were preserved.

## Local Outliner range activation build verified on 2026-10-07

Source commit `87036d985cd3`, integrated into `main`, changes only range endpoint activation in `outliner_select.cc`, with no saved DNA or shared C++ layout changes. Release INSTALL passed with parallelism 2 and `/p:CL_MPCount=2`. The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe` and reports parent hash `144481d9f58a` because it was compiled before the implementation commit. Nine GUI item-activation cases verified selected object sets and the ViewLayer active object, including both range directions, Ctrl+Shift and repeated endpoints. The previous Desktop build reproduced the original behavior. Temporary verification scripts, screenshots, logs and session files were removed afterward. Existing user sessions and Desktop packages were preserved.

## Local viewport polishing verified on 2026-10-08

Release INSTALL completed with parallelism 2 and CL_MPCount=2 from `main` at base commit `4628efb04e04`, with the polishing implementation subsequently committed as `5f6eba240126`. The installed binary reports the base hash because compilation preceded the implementation commit. No saved DNA or shared library data layout changed. The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe`; Desktop packages and the public download were not updated.

Background checks passed new armature objects and Add Bone with unitless, metric (scale 1 and 0.01), and imperial units; 20 cm length compensation; explicit Octahedral bone display; In Front and object Wire defaults; Python-created objects/bones; preservation of existing per-bone display types; duplication and extrusion retaining B-Bone display; and save/reload of both new defaults and user-selected Solid/Stick settings with In Front disabled. The installed Japanese, Italian, French and Spanish catalogs contain the Mesh Holes label and revised tooltip.

Separate OpenGL and Vulkan sessions captured selected bone axes in Object, Edit and Pose Mode behind an occluding mesh with armature In Front disabled, plus X-Ray and Wireframe. Enabled axes remained visible above the mesh; disabling Axes removed them. GPU object picking selected the armature on each backend. The isolated Mesh Holes comparison used a boundary ring with another face behind its opening so the ordinary selection outline did not mark the internal boundary. At theme outline width 1 with Smooth Wires off, each vertical boundary added exactly two physical pixel columns on both backends (columns 336-337 and 478-479); the ON/OFF difference contained 1,132 pixels on each. Smooth Wires on was also captured. These are focused checks of the polishing changes, not a repeat of the complete historical overlay regression. Verification scripts, scenes, captures and logs were temporary and removed afterward.

## Local bone body shape build verified on 2026-10-08

The local body-shape update was compiled and installed as Release with parallelism 2 and CL_MPCount=2. The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe`, reporting base hash `c7ff32accae1` because compilation preceded the implementation commit `2e4febbdb1df`. No saved DNA or shared library structure layout changed, so no full layout rebuild was required. Desktop packages and the public download were not updated; existing user sessions remained open.

Octahedral display now uses a flat square base and a pyramid body between endpoint sphere boundaries. The display calculation trims in the spheres' unscaled space, then preserves the evaluated pose/object transforms. Cropped outer segment bases are made tangent to the endpoint sphere, including B-Bone roll/scale compensation. Internal segment bodies meet at the actual next segment origin and receive no spheres or radius offsets. Radii spanning several segments trim or omit those covered segments. Overlapping spheres omit a fully covered body. Head/Tail sphere rendering and selection code, the B-Bone box display type, bone evaluation and saved data are unchanged.

A baseline was captured before the source change. The final OpenGL and Vulkan checks each completed 21 viewport cases and six GPU picks: single bodies, radius 0.02/0.04, a bone shorter than the two sphere radii, 8/32 segments, curves, roll, transverse spline scale, nonuniform object/pose scale, parent-induced shear, solid and transparent display, X-Ray, scene occlusion/In Front, coincident selected bones in both creation-order positions, and the B-Bone box display. GPU picks covered the single body, Head, Tail and three interior segment positions. Exposed Head/Tail outer hemispheres were pixel-identical to the baseline in four normal/scaled settings on both backends; the ordinary B-Bone box viewport matched the baseline. Coincident selected/active display retained the same scene pixels in both ordering cases. Occlusion removed the hidden portion of the body and In Front restored it.

A saved eight-segment curved bone with nonuniform parent/child/object scales and child rotation reopened with exactly the same evaluated pose matrix, Head/Tail coordinates, nine deformation-boundary matrices and radius as the pre-change reference. This confirms the tested deformation/save-reload case, rather than every possible rig. Verification scripts, scenes, captures, session recovery, logs and results were created in a dedicated external temporary directory and removed after inspection. Build caches and runtime dependencies were retained.


## Hair Curves guide theme validation (2026-10-08)

The local Release INSTALL includes dedicated Hair Curves Width/Selected/Unselected theme controls. The fields replace 12 existing padding bytes in ThemeSpace with two RGBA colors, one width byte, and three padding bytes; the size and offsets of pre-existing fields remain unchanged. Shared overlay globals retain their layout because guide settings use push constants.

Temporary GUI fixtures with a Geometry Nodes pass-through modifier ran 13 cases on each of OpenGL and Vulkan: Edit/Sculpt Mode, 4/8 pixel widths, custom colors, curve/point selection, fractional selection, cage opacity including zero, overlay visibility, cyclic curves, Catmull-Rom, and implicit all-selected data. Pixel measurements confirmed 4 and 8 pixels; guide pixels agreed between backends. Cage Opacity 0.4 was checked against straight alpha compositing in linear color space. The existing sculpt cage depth bias is retained; an additional fixture verified occlusion beyond that bias on both backends.

An Object Mode screenshot matched the previous build exactly on OpenGL. Loading an isolated user preference file from the previous build initialized the new settings while retaining its custom wire color; saving and reopening retained custom guide colors and width 9. The Hair Curves theme panel registered successfully. Fixtures confirmed unchanged source point positions. All fixture scenes, scripts, screenshots, configurations, recovery files, and logs were kept outside the repository and removed after verification. The Desktop packages and public download require their own update.

## Local Occluded Selection Outline RGBA validation (2026-10-13)

Release INSTALL completed with parallelism 2 and CL_MPCount=2. **Occluded Selection Outline** is one RGBA entry directly in the main 3D Viewport theme category, without a child panel or separate checkbox/opacity control. An RNA getter/setter combines existing RGB and alpha storage while preserving ThemeSpace size and member offsets. Shared overlay globals remain unchanged. The installed runtime is `D:\blender_build\octahedral_radius\bin`; the implementation is included in source commit `ad308b73519a` on `main`. The executable reports base hash `a86c24a216fb` because compilation preceded the source commit. Desktop packages and the public download have not been updated.

Six viewport cases plus GPU picking passed per backend (OpenGL and Vulkan). Registration and the generated main-category panel confirmed one four-component color with the exact label, without the former child panel or separate RNA controls. Color changes and alpha 0/0.5/1 affected only occluded outline pixels; visible portions and X-Ray retained their behavior. Legacy percentage/custom-color migration, zero-alpha migration, RGBA save/reload and zero alpha after restart passed. See the [feature inventory](FEATURES.md) for details. Temporary scripts, preference files, captures and recovery files were removed; build caches and runtime dependencies were retained.

The subsequent Python UI ordering adjustment places Occluded Selection Outline directly after Object Selected in the main category. The installed `space_userpref.py` was updated directly without C++ changes, and the actual panel draw order was checked through the installed runtime.

## Local Outliner collection highlight validation (2026-10-13)

Release INSTALL completed with parallelism 2 and CL_MPCount=2 after changing `outliner_draw.cc`. No DNA or shared C++ layout changes were added. The installed runtime is `D:\blender_build\octahedral_radius\bin`; the implementation is included in source commit `d86a7a944cdf` on `main`. The executable reports base hash `a86c24a216fb` because compilation preceded the source commit. Desktop packages and the public Windows download have not been updated.

Eleven View Layer cases and one expanded Scenes case passed on each of OpenGL and Vulkan. Checks covered nested and collapsed collections, deselection, changes of active object, multiple selected branches, shared subcollections and multiply linked objects. Theme-colored row pixels confirmed active/selected propagation; the master Scene Collection row retained its appearance. Viewport selection and collection-selection semantics remained intact. See the [feature inventory](FEATURES.md). The user-provided README comparison is a retained documentation asset; verification scripts, scenes, screenshots and recovery files were external and removed after validation.

The subsequent active-deselected object text adjustment was compiled and installed with the same Release INSTALL command. Seven cases per backend (OpenGL and Vulkan) confirmed the default RGB 243 near-white text, unchanged active/selected orange colors and unrelated object text, theme-color blending, hidden-object fading, and unchanged active/selection state. This changes only `outliner_draw.cc`, with no layout changes. Verification scripts, captures and recovery files were temporary and removed afterward; the implementation is included in source commit `d86a7a944cdf` on `main`.

## Local UV overlay curve crash correction verified on 2026-10-14

The first Release INSTALL completed with parallelism 2 and CL_MPCount=2 from `main` at base commit `9af31f1d`, before the verified UV original-data type guard was committed as `bcb82e789062`. No DNA or shared C++ layout changed, so an incremental build was sufficient. The corrected executable is D:\blender_build\octahedral_radius\bin\blender.exe; Desktop packages and the public download have not been updated.

A separate factory-startup GUI instance of the previous Desktop build reproduced the same UV overlay stack as the reported crash. Overlays disabled passed; enabling overlays crashed before any individual diagnostic filter was enabled. The installed correction passed the corresponding sequence on OpenGL and Vulkan, including each diagnostic filter, beveled curves, text, NURBS surfaces, mixed selection and copy/paste across scenes. Debug startup logs confirmed the actual GPU backends. The mesh Object/Edit Mode diagnostic captures were byte-identical to the previous OpenGL build, including original UV handling with an Array modifier, and matched across backends. Original non-mesh object types remained unchanged. The user's production file was not supplied; the user subsequently confirmed that the installed correction resolves the reported problem.

All verification scripts, screenshots, result files, crash logs, clipboard buffers, temporary preferences and build logs were kept in a dedicated external temporary directory and removed after verification. Existing user Blender sessions, source files and compilation caches were preserved.

## Local outline depth comparison verified on 2026-10-20

Release INSTALL completed with parallelism 2 and CL_MPCount=2 after changing only outline GLSL sources. No DNA or shared C++ layout changes were made. The local runtime at `D:\blender_build\octahedral_radius\bin\blender.exe` contains the implementation included in source commit `1f04cb0020e9` on `main` (compiled before that commit); the Desktop copies and public Windows download were not updated. Existing user sessions were kept open.

See the [feature inventory](FEATURES.md#outline-occlusion-depth-comparison-local-update-2026-10-20) for the ten clipping cases per backend, coincident/intersecting/X-Ray image comparisons, selection order checks, the 1,310,720-triangle performance method and medians, and the finite-depth-precision and pre-existing In Front limitations. This focused check did not repeat GPU picking or all non-mesh prepass cases. All verification files were external and removed afterward; compilation caches and runtime dependencies were retained.

### Orthographic follow-up on the supplied scene (2026-10-20)

The correction included in source commit `42dbb0dd6bf2` on `main` was built and installed before the commit with the same Release INSTALL command, parallelism 2 and CL_MPCount=2. Only the outline resolve GLSL arithmetic changed; no DNA or shared C++ layout change required a full rebuild. The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe`. The open Desktop copies and public download were not updated.

The supplied body/hood file reproduced the unsigned-depth bug on both OpenGL and Vulkan. Corrected captures passed four actual orthographic zoom settings, symmetric orthographic depth sign cases, unchanged perspective controls and GPU picks for both meshes. See the [signed-depth validation and limitations](FEATURES.md#orthographic-signed-depth-correction-2026-10-20). No new performance timings are claimed. TEMP/TMP and the configuration directory were redirected to the dedicated external verification folder, including session recovery; all generated files were removed afterward. The source scene and existing user sessions were preserved.

## Sculpt voxel and mask development build verified on 2026-10-21

The development branch `codex/sculpt-voxel-face-set-mask`, based on `main` at `6f14da92` and subsequently included in source commit `b98ad0b25666`, completed Release `blender` with `/t:Rebuild`, followed by `INSTALL`, using the existing build directory, `--parallel 2`, and `/p:CL_MPCount=2`. A full rebuild was required by the added persistent curve pointer in `Sculpt`; the mesh remesh toggles reuse existing flag bits. The subsequent mask undo dirty-tag correction changed no layout and was installed incrementally.

The installed runtime is `D:\blender_build\octahedral_radius\bin\blender.exe` and reports base hash `6f14da92f1c9`. Compilation preceded source commit `b98ad0b25666` on `main`; its base hash therefore differs from the implementation commit. Desktop copies and the public download were not updated, and the user's running Desktop session was preserved.

Validation passed 81 background assertions, 54 GUI assertions and 11 specialized Multires/Dyntopo assertions on each of OpenGL and Vulkan, plus exact Multires mask viewport comparisons across undo, redo, and save/reload. Native Manifold Boolean and Sculpt Trim passed after the full rebuild. See [scope and limitations](sculpt_voxel_masks.md#validation). Verification scripts, scenes, logs, captures, preferences, and recovery files were external and removed; build caches and dependencies were retained.

### Simple hole fill before remesh; optional Fair Position afterward (2026-10-21)

The follow-up correction completed Release INSTALL with `--parallel 2` and `/p:CL_MPCount=2`. It changes native remesh logic, RNA and Python UI, with no new DNA or shared C++ layout change. The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe` (file timestamp 2026-10-21 11:50:51 JST, base hash `6f14da92f1c9`). The correction is included in source commit `b98ad0b25666` on `main`; Desktop copies and the public download are unchanged.

At that verification point, the workflow was simple native Fill Holes, voxelization and mandatory Fix Poles, then optional Fair Position limited to the reprojected new hole Face Sets. Validation passed 191 background assertions and 49 GUI assertions per backend (OpenGL and Vulkan), including high/max Adaptivity, separate overlapping components, options save/reload, exact equivalence to manual fill/remesh with fairing off, fixed remeshed rims/existing regions during fairing, and exact one-step undo/redo in Sculpt and Object Mode. The actual popover and resulting patch geometry were visually inspected. See [details and fixture limitations](sculpt_voxel_masks.md#validation). All follow-up scripts, scenes, logs, captures, preferences and recovery files were external and removed; build caches and runtime dependencies were retained.

### Fill Holes and Split directly in Sculpt Mode (2026-10-21)

The subsequent native Sculpt operators and menu entries completed incremental Release INSTALL with `--parallel 2` and `/p:CL_MPCount=2`. Function declarations changed in the Sculpt header, but no DNA or shared C++ structure layout changed. The installed runtime remains `D:\blender_build\octahedral_radius\bin\blender.exe`, with executable timestamp 2026-10-21 12:05:36 JST and base hash `6f14da92f1c9`. The operators are included in source commit `b98ad0b25666` on `main`; Desktop copies and the public download were not updated.

All 95 GUI assertions passed on each of OpenGL and Vulkan. Checks cover six Split modes staying in one object and Sculpt Mode, fixed face positions/custom data, exact undo/redo, point-only and non-manifold Face Set contacts, masked edge/vertex equivalence to native Edit Mode splitting, loose wires, standalone simple Fill Holes and shape-key coordinate preservation, unsupported representation polls, installed menu entries, and Split followed by By Loose Parts remesh. The actual Split submenu was visually inspected on both backends. All verification files and session recovery were kept in the unique external temporary folder and removed after validation; existing user sessions, build caches and runtime dependencies were retained.

### Source publication (2026-10-21)

The verified Sculpt suite is included in source commit `b98ad0b25666` on `main`. README, the feature inventory, and the technical notes describe the final simple-fill/remesh/optional-fair workflow, six same-object Sculpt Split modes, Face Set topology gestures, and Bevel Mask. The user confirmed that the Sculpt commands work. This source publication does not replace the locally installed executable or the separately distributed Desktop/public packages; the installed executable remains the verified 12:05:36 JST build described above.


## Local Decimate cache build verified on 2026-10-22

Release INSTALL completed with `--parallel 2` and `/p:CL_MPCount=2` on development branch `codex/decimate-result-cache`, based on `main` at `55c61952fd05`. Only `source/blender/modifiers/intern/MOD_decimate.cc` changes behavior, using the existing modifier runtime pointer; no DNA or shared C++ layout change was made. The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe`, timestamp 2026-10-22 11:21:16 JST, reporting base hash `55c61952fd05`. The verified implementation is included in source commit `bb4bbad2a787` on `main`; compilation preceded that commit. Desktop copies and the public download are unchanged, and the existing user session remained open.

All 69 background comparison/lifecycle cases passed with guarded allocation, including exact geometry and normals, upstream and downstream changes, all Decimate modes, weights/attributes/materials, Edit Mode transitions, duplication/deletion, undo/redo, Apply, and save/reload. See [cache notes](decimate_cache.md) for the synchronous benchmark method, results, memory tradeoff, and limits. No viewport FPS or interactive undo timing is claimed. Generated verification files and build logs were external and removed after checking; build caches and dependencies were retained.


### Saved Cache Result checkbox (2026-10-22)

The follow-up Release INSTALL completed with `--parallel 2` and `/p:CL_MPCount=2`. A negative bit in the existing Decimate flag field stores `use_cache`; no member sizes/offsets or shared C++ layout changed. The checkbox appears above Face Count in all three Decimate modes, defaults on for new modifiers and old files, and releases/bypasses the cache when off on evaluation. Source changes are in `MOD_decimate.cc`, `rna_modifier.cc`, and the Decimate enum in `DNA_modifier_types.h`.

The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe`, timestamp 2026-10-22 11:45:18 JST, reporting base hash `55c61952fd05`. All 34 follow-up checks passed with guarded allocation: defaults including old-file loading, on/off equality in all modes, other flags, duplication, no-op disabling, save/reload, undo/redo, invalidation and uncached Apply. A separate synchronous benchmark confirmed the cache benefit disappears when off and returns when re-enabled; see [details and limits](decimate_cache.md#cache-result-checkbox-follow-up-2026-10-22). Source commit `bb4bbad2a787` includes both cache and checkbox on `main`; the binary was compiled before that commit. Desktop/public packages are unchanged. Temporary verification artifacts were external and removed, preserving build dependencies and the user's open Desktop session.


## Local Isolate lighting and camera reference build (2026-10-23)

Release INSTALL completed with `--parallel 2` and `/p:CL_MPCount=2` on development branch `codex/isolate-lights-camera`, based on `main` at `d3352ca742f4`. The camera setting uses an unused existing flag bit, with no DNA member or shared C++ layout change; the existing dependency build recompiled affected components and installed the updated camera UI. No remeshing code changed. The implementation is included in source commit [`cdbaf6fefc0c`](https://github.com/MaxPuliero/Bmax/commit/cdbaf6fefc0c749cc268662e3d9d5ebbb36b3d95) on `main`; compilation preceded this commit, so the installed binary continues to report its base hash.

The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe`, timestamp 2026-10-23 11:14:24 JST, reporting base hash `d3352ca742f4`. Validation passed 33 guarded-allocation background assertions, nine OpenGL and eleven Vulkan viewport captures (including Cycles CPU Rendered on Vulkan), and the source formatting/Python UI syntax checks. See [the feature inventory](FEATURES.md#local-view-lighting-and-camera-references-2026-10-23) for coverage and limits. The separate [remesh investigation](voxel_remesh_performance.md) includes current-pipeline timings in Sculpt Mode, not an implemented optimization.

The user's existing Desktop session, separately copied packages and public download were preserved. Temporary scripts, scenes, logs, screenshots, preferences, recovery files and benchmark crash diagnostics were external and removed after verification. Build caches, runtime dependencies and installed binaries were retained.

The user subsequently confirmed that both Local View changes work. The requested camera-panel screenshot is retained in `doc/bmax/media/keep_in_local_view.png` and included in the README. This source publication leaves the previously verified local executable and separately distributed Windows packages at their existing versions.


## Local Voxel Remesh performance build (2026-10-23)

Development branch `codex/voxel-remesh-performance`, based on `main` at `23181336769d`, completed Release INSTALL with the existing command, `--parallel 2` and `/p:CL_MPCount=2`. A first build added only opt-in timing checkpoints to the prior algorithm for the baseline. The candidate added the native closed-mesh preparation fast path and direct OpenVDB transfers; a final incremental build moved the existing empty-part check ahead of projection to correct the baseline-reproduced tiny-part crash. No DNA or existing shared C++ structure layout changed, so a forced full rebuild was unnecessary. The source changes are published on `main`; the optional Fix Poles control was restored afterward and verified as described below.

The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe`, timestamp 2026-10-23 13:15:25 JST, reporting base hash `23181336769d`. Fix Poles retains its complete prior algorithm. Validation passed 512 guarded-allocation operator comparisons, 70 modifier/cancellation assertions, six large-fixture byte comparisons, and 222 GUI assertions per backend on OpenGL and Vulkan, including one-step Undo/Redo and save/reload in Sculpt Mode. Six actual viewport captures were inspected. See [the inventory](FEATURES.md#voxel-remesh-performance-and-optional-fix-poles-2026-10-23) and [measured stage timings, before/after results and scope](voxel_remesh_performance.md#verified-local-optimization-after-authorization-2026-10-23).

For isolated CPU stage profiling, set `$env:BMAX_REMESH_PROFILE='1'` before launching the executable and inspect standard output. Leave the variable unset or set it to `0` for normal silent operation. The authoritative operator benchmarks were taken with profiling disabled. Stage timings exclude asynchronous viewport work and include overlapping parent/child records.

The user's existing Desktop instance and separately distributed packages were preserved. All test scripts, inputs, golden data, logs, screenshots, temporary preferences, recovery files, crash diagnostics and the separately named baseline executable were external and removed after verification. Build caches, dependencies and the installed runtime were retained.


## Restored Fix Poles control and remesh source publication (2026-10-23)

Release INSTALL completed with the existing `--parallel 2` and `/p:CL_MPCount=2` command after restoring the editable default-on Fix Poles checkbox in both remesh panels. The setting uses a previously cleared bit in the existing Mesh flag field; no DNA or shared structure size/offset changed, so a forced full rebuild was not required. Existing files retain enabled cleanup, explicit disabling survives remesh and save/reload, and Adaptivity keeps the control available.

The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe`, timestamp 2026-10-23 15:16:18 JST, reporting base hash `23181336769d`. It contains implementation [`83c95f81fd69`](https://github.com/MaxPuliero/Bmax/commit/83c95f81fd6990e1aebd003e7db2347742b91252), compiled before committing. The implementation and benchmark documentation are published on `main`; separate Desktop packages and the public download remain unchanged.

The follow-up passed 48 guarded-memory background assertions and 52 GUI assertions on each active backend, OpenGL and Vulkan. Coverage includes legacy/new defaults, editability, save/reload, both panel draw paths, exact enabled-output comparisons against the previous installed build, disabled-output equivalence to raw voxel meshing, adaptive output, By Loose Parts, redraw and exact single-step Sculpt Undo/Redo with mode and choice retained. The script records explicit before/after Undo states to model interactive invocation; the initial missing after-state push was a verification-harness issue. The full enabled cleanup algorithm is unchanged. Python syntax, C++ formatting, source diff and normal silent profiling checks passed. The earlier six-fixture benchmark remains the documented timing source, with Fix Poles enabled in both builds.

Follow-up scripts, scenes, logs and recovery data were kept in a dedicated external verification directory and removed after checking; build caches, installed runtime dependencies, the user's existing Desktop session and unrelated files were preserved.


## Blender 5.2.3 full rebuild and verification (2026-10-23)

The stable-release delta from upstream `v5.2.2` to `v5.2.3` is integrated in [`776763085e8a`](https://github.com/MaxPuliero/Bmax/commit/776763085e8a3cdc2b17eae2680d6f22841847ac) on `main`. `SpaceType::struct_size`, global auto-run state and the Cycles session member changes alter shared C++ layouts, so the complete executable target and referenced projects were rebuilt before INSTALL:

```powershell
cmake --build D:\blender_build\octahedral_radius --config Release --target blender --parallel 2 -- /t:Rebuild /p:CL_MPCount=2
cmake --build D:\blender_build\octahedral_radius --config Release --target INSTALL --parallel 2 -- /p:CL_MPCount=2
```

Both commands completed successfully. The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe`, timestamp 2026-10-23 17:46:30 JST, reporting Bmax 5.2.3 LTS with base hash `08c66f5c43a6`; compilation preceded the integration commit. Existing caches, dependencies and build configuration were preserved. Submodule pointers did not change.

INSTALL does not automatically prune removed Python modules. The old installed `5.2/scripts/modules/_animsys_refactor.py` was verified byte-for-byte against the official 5.2.2 source after line-ending normalization, then removed together with its matching bytecode cache. The file is intentionally absent from 5.2.3 source; unrelated files were retained.

The new runtime passed 65 guarded-memory background checks, 41 GUI assertions on each active backend, fresh Multires painting on both backends with exact reference evaluated RGBA, and a Cycles CPU render using the saved painted attribute. Native Manifold Boolean and actual Sculpt Manifold Trim were exercised after the complete rebuild. See [the audit](blender_5_2_3_sync.md) for exact comparisons, scope and the unavailable authenticated upstream LFS fixtures. The earlier remesh/Decimate timing tables remain measurements of the preceding build; this integration adds no new performance claim.

All verification scripts, scenes, golden data, logs, rendered images and recovery files were external and removed afterward. The user's Desktop session was preserved; source publication and local installation do not update Desktop or public binary packages.

## Connected Octahedral joints local build verified on 2026-10-26

Release INSTALL completed with the documented parallelism 2 and CL_MPCount=2. CMake regeneration caused a broad dependency rebuild; no DNA or shared C++ layout was changed for this correction. The installed `D:\blender_build\octahedral_radius\bin\blender.exe` is timestamped 2026-10-26 11:06:58 JST and reports base hash `b98cf52093d0`, while the correction is included in source commit `f90af1103223` on `main`. Compilation preceded the source commit, so the reported hash identifies the base revision.

85 guarded-memory background assertions, eight click-extrude assertions per backend and ten offscreen cases each on OpenGL/Vulkan passed. The shared-joint gap reproduced in the old executable disappeared, and equal-radius/disconnected reference images stayed unchanged. See [the feature inventory](FEATURES.md#connected-octahedral-joints-and-extrusion-sizes-2026-10-26) for coverage and backend differences. Verification scripts, scenes, images, arrays, logs, recovery files and the comparison executable were temporary and removed. Existing sessions, Desktop packages and public downloads were preserved.

## Align Master local build verified on 2026-10-26

Incremental Release INSTALL completed with the documented parallelism 2 and CL_MPCount=2. No DNA or shared C++ layout changed. The installed `D:\blender_build\octahedral_radius\bin\blender.exe` is timestamped 2026-10-26 12:19:13 JST and reports base hash `30cb581ed3ac`; Align Master is included in source commit `1a7224cacda9` on `main`. Compilation preceded the implementation commit, so executable metadata retains the base hash.

626 guarded-memory background assertions and three GUI assertions each on OpenGL/Vulkan passed. The native dialog and axis buttons were visually inspected; Undo/Redo and saved rest orientations were verified. See [Align Master coverage](FEATURES.md#align-master-2026-10-26). Temporary scripts, scenes, screenshots, recovery files and build logs were removed. Build caches, dependencies, existing user sessions, Desktop copies and public packages were preserved.

The immediate-execution follow-up uses Blender's native redo popup and treats an empty axis mask as a successful no-op. Real OpenGL popup interaction verified five matrix states, original-state restoration with all axes disabled, closing the popup before Undo/Redo and normal process exit. Vulkan interaction was not repeated for this follow-up. Its temporary scripts, JSON state history, logs, recovery and crash diagnostics were removed after verification.

The persistent redo-panel correction finishes the invoke normally after immediate XYZ alignment. OpenGL UI checks verified the bottom-left panel before/after viewport zoom and orbit, F9 reopening, shared controls, empty-mask restoration and re-enabling X. Five background assertions passed. This panel follow-up was not repeated on Vulkan. Its temporary fixture, logs and diagnostics were removed; the user's Desktop session was preserved.


## Default multi-selection editing local build (2026-10-28)

Incremental Release INSTALL completed on `codex/default-multi-object-edit` with the documented parallelism 2 and CL_MPCount=2. No DNA or shared C++ structure layout changed; the new per-button color backup is private to the UI handler translation unit. The installed `D:\blender_build\octahedral_radius\bin\blender.exe` is timestamped 2026-10-28 12:06:15 JST and reports base hash `b0f127ecee25`. The implementation is included in source commit [`d46c5d5ecdbe`](https://github.com/MaxPuliero/Bmax/commit/d46c5d5ecdbeb3984b5f811c486c3318c8ea100e) on `main`. Compilation preceded the commit, so executable metadata retains the base hash. Desktop copies and public packages are unchanged.

Validation passed 81 native operator/data/mode assertions, 25 initial UI checks, six color/slot follow-up checks, four protection/clipboard checks and a final material-selector smoke check. See the [feature inventory](FEATURES.md#default-multi-selection-property-editing-2026-10-28) for coverage and limitations, including the preserved Sculpt Mask restriction, shared data behavior, OpenGL-only UI verification and the small BMesh memory-pool shutdown report. C++ formatting, Python UI syntax and diff whitespace checks passed. No performance claim is made.

Verification used factory startup, isolated temporary preferences and internal event simulation in separate Blender processes. Existing user sessions were preserved. Scripts, scenes, captures, preferences, recovery files and logs were external and removed after checking; build caches and runtime dependencies were retained.

## Fresh Windows build from the current checkout (2026-10-28)

The current machine uses `D:\Bmax` rather than the earlier `D:\blender_prj` checkout. No prior build directory was present. The pinned `lib/windows_x64` submodule at `60d6e96b917568278d400a4024c98da0fb777338` was initialized, and a fresh Visual Studio 2022 / MSVC v143 x64 configuration completed with the commands above. All components were compiled from scratch; Release INSTALL completed with parallelism 2 and CL_MPCount=2, retaining the downloaded libraries and new compilation cache.

The installed executable is `D:\blender_build\octahedral_radius\bin\blender.exe`, timestamp 2026-10-28 22:49:14 JST. It reports Bmax 5.2.3 LTS, source hash `fad8b2fa426f`, and `main (modified)` because this build-guide update was in the working tree during compilation. Production source files were unchanged. The complete runtime is in the adjacent `bin` directory; distribute that directory rather than the executable alone.

An inline factory-startup background check passed for startup/version, Mesh Holes enabled, Octahedral Radius and Axis Size defaults, 20 cm new armatures with Octahedral/In Front/Wire display, native Manifold Boolean, and Cycles/internationalization availability. The installed Italian, Japanese, French and Spanish MO catalogs contain translated Mesh Holes labels and tooltips. This was a build smoke check; interactive viewport, GPU backends and the full feature regression suite were not repeated. No test scene or script was saved, and the external temporary build log was removed after inspection. Desktop copies and the public download were not updated.
