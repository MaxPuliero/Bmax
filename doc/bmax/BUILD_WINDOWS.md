# Building Bmax on Windows

**Source publication, November 14:** the verified 5.2.4 integration is commit `e71eadef0c3bbd1669a5a95d845188ef21ff8873` on `codex/bmax-5.2.4`. GPU build requirements and the verified local runtimes are documented below. Source publication preserves the installed binary hashes and does not replace Desktop copies or public binary downloads. Earlier statements about uncommitted work describe the pre-publication audit.

## CUDA, OptiX and HIP rebuild; 5.2.4 upgrade (2026-11-14)

Both local Release INSTALL builds succeeded. Current runtimes are **Bmax 5.2.4 LTS**
and **Bmax 5.3.0**. The November 13 commands below are historical: they explicitly
omit GPU kernels and disable OptiX, causing NVIDIA/AMD device discovery to fail
when no matching compiler is installed. Use the configuration in this section.

The 5.2 checkout integrates official `v5.2.4` (`cd5dc428cce1993a4b35986f227c6a24e42a365c`)
on local branch `codex/bmax-5.2.4`, starting from Bmax `48ae2926`. See
[the integration audit](blender_5_2_4_sync.md) in the 5.2 checkout.
Its Windows libraries are pinned to `578ab7d2b549385552fa4c9145dfed2b7d30bdae`.
The 5.3 feature source remains `b70c7ea0` on `codex/bmax-5.3`, with its separate
`4d0478217fd7f53e785cdbac95de8f726f35cd55` Windows libraries.
The historical cache/install directory name `bmax5.2.3` is retained for 5.2.4;
CMake caches are not moved. No DNA/shared C++ structure header changed in the
5.2.4 delta. Existing caches were retained and affected components rebuilt.

### Toolchains

Each build has independent copies under `toolchains`:

- `cuda-12.8.1`: NVIDIA nvcc 12.8.93, cudart and CCCL from the
  [official 12.8.1 redistribution manifest](https://developer.download.nvidia.com/compute/cuda/redist/redistrib_12.8.1.json).
  Archive SHA-256 values were verified. The manifest is retained in the toolchain.
- `optix-9.0.0`: headers from [NVIDIA's v9.0.0 tag](https://github.com/NVIDIA/optix-dev/tree/v9.0.0).
- `hip-7.1.1`: HIP 7.1.51803-d3a86bd04, matching Blender's Windows release-toolchain
  reference. The [official AMD package](https://download.amd.com/developer/eula/rocm-hub/AMD-Software-PRO-Edition-26.Q1-Win11-For-HIP.exe)
  had a valid Advanced Micro Devices signature. Only SDKCore was extracted using
  an MSI administrative image. No AMD/NVIDIA driver was installed or changed.
  Installer hash and origin are retained in `bmax-sdk-source.json`.

CUDA/OptiX/HIP and both CUDA/HIP binary packaging options are ON. Default full
architecture lists are retained. HIP-RT, oneAPI and their existing configuration
remain unchanged; HIP-RT and oneAPI are disabled in these local builds.

### Configure and build

Use forward slashes in CMake cache paths. Before using the commands below,
create `toolchains/bmax-hip-parallel.props` in **each** build with this content:

```xml
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemDefinitionGroup Condition="'$(MSBuildProjectName)' == 'cycles_kernel_hip'">
    <CustomBuild>
      <BuildInParallel>true</BuildInParallel>
      <MaxProcesses>4</MaxProcesses>
      <MaxItemsInBatch>1</MaxItemsInBatch>
    </CustomBuild>
  </ItemDefinitionGroup>
</Project>
```

The override applies only to `cycles_kernel_hip`, allowing at most four independent
HIP commands per version. Native C++ remains at project parallelism 2 and
`CL_MPCount=2`; CUDA kernels remain serial. MSBuild dependency checks preserve
kernel/compression ordering. The active HIP compiler commands were checked to
retain their original source, architecture and optimization flags.

```powershell
$pairs = @(
  @('D:/blender_prj', 'D:/blender_build/bmax5.2.3'),
  @('D:/blender_prj_5.3', 'D:/blender_build/Bmax5.3')
)
foreach ($pair in $pairs) {
  $src = $pair[0]
  $build = $pair[1]
  cmake -S $src -B $build -G "Visual Studio 17 2022" -A x64 -T v143 `
    -DWITH_GTESTS=OFF -DWITH_GPU_DRAW_TESTS=OFF -DWITH_GPU_BACKEND_TESTS=OFF `
    -DWITH_CUDA_DYNLOAD=ON -DWITH_CYCLES_DEVICE_CUDA=ON `
    -DWITH_CYCLES_DEVICE_OPTIX=ON -DWITH_CYCLES_DEVICE_HIP=ON `
    -DWITH_CYCLES_CUDA_BINARIES=ON -DWITH_CYCLES_HIP_BINARIES=ON `
    -DWITH_CYCLES_CUDA_BUILD_SERIAL=ON `
    "-DCUDA_TOOLKIT_ROOT_DIR=$build/toolchains/cuda-12.8.1" `
    "-DCUDA_NVCC_EXECUTABLE=$build/toolchains/cuda-12.8.1/bin/nvcc.exe" `
    '-DCUDA_HOST_COMPILER=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe' `
    "-DOPTIX_ROOT_DIR=$build/toolchains/optix-9.0.0" `
    "-DHIP_ROOT_DIR=$build/toolchains/hip-7.1.1" `
    "-DHIP_HIPCC_EXECUTABLE=$build/toolchains/hip-7.1.1/bin/hipcc.exe" `
    "-DCMAKE_INSTALL_PREFIX=$build/bin"
  if ($LASTEXITCODE -ne 0) { throw "Configuration failed: $build" }
  cmake --build $build --config Release --target INSTALL --parallel 2 -- `
    /p:CL_MPCount=2 "/p:ForceImportBeforeCppTargets=$build/toolchains/bmax-hip-parallel.props"
  if ($LASTEXITCODE -ne 0) { throw "Build failed: $build" }
}
```

Keep the toolchains, `.props` files, compiled kernels, build caches and complete
installed runtimes. The executable alone is insufficient for distribution.

### Installed executable audit

Installed executables match `bin/Release/blender.exe` byte-for-byte.

| Version | Installed executable | Bytes | Timestamp (JST) |
| --- | --- | ---: | --- |
| 5.2.4 LTS | `D:\blender_build\bmax5.2.3\bin\blender.exe` | 98,243,072 | 2026-11-14T13:17:43+09:00 |
| 5.3.0 | `D:\blender_build\Bmax5.3\bin\blender.exe` | 111,097,856 | 2026-11-14T13:21:14+09:00 |

SHA-256:

- **5.2.4 LTS**: `E92EC0E9C6B6EF8BEA978608466B966F21FEC9B4ADAAD525B57B58746F0B2EE0`.
- **5.3.0**: `4F35F28C2C58365AC6442FF60675F3EAFB8875009B9F32C439ACE5CCB393EFD0`.

All **73 installed compressed GPU kernels** match their build outputs by SHA-256:
5.2.4 has 8 CUDA, 9 OptiX and 18 HIP files; 5.3 has 8 CUDA, 11 OptiX and 19 HIP files.
The CUDA count includes the generic PTX fallback.

### Verification and scope

Both runtimes expose CUDA, OptiX and HIP in Cycles. NVIDIA GeForce RTX 4070 Ti SUPER,
driver 621.24, is detected through CUDA and OptiX. Four real 64x64, four-sample renders
passed, with GPU explicitly selected and CPU excluded. CUDA logs confirm use of
precompiled kernels; OptiX logs confirm OptiX BVH construction, and both OptiX renders
also used OptiX denoising. Saved pixels are finite and nonuniform. These are smoke
checks, not performance benchmarks.

The 5.2.4 runtime reads an actual 5.2.3 Bmax fixture, preserving custom bone radius,
axis size and display settings. Its 98 evaluated Multires vertices and 384 standard
interpolated color samples match exactly (maximum error 0.0). Armature unit scales
1, 0.01 and 10, Octahedral/In Front/Wire defaults, Mesh Holes, snapshot RNA and an
actual Manifold Boolean passed. Bundled pip 26.2.2 and certifi 2026.10.19 passed.
INSTALL retains deleted Python files: 59 obsolete bundled files were compared with
the original dependency commit before removal; user additions were excluded.

AMD hardware is absent: all 37 HIP kernel variants across the two versions compiled
and their installed files were verified, but AMD device detection/rendering has not been tested.
The Multires fixture covers evaluated geometry and a standard color attribute;
custom painted-grid strokes and the full historical feature matrix were not rerun.
An optional synthetic Shift UI harness also failed against the unchanged 5.2.3
reference, so no new Shift/UI pass is claimed. The existing Bmax source delta in
all five shared upgrade files is preserved exactly.

Verification scripts, logs, scenes, images and isolated caches were external to Git
and removed after checking. User preferences and the open Desktop session were preserved. Desktop copies and public download packages were not
replaced. Source/documentation changes are local; no new commit or push was requested.

## Named local build directories (2026-11-13)

Current source: [48ae2926](https://github.com/MaxPuliero/Bmax/commit/48ae2926b1da579857cf54e3cd11b3146017240d) on `codex/bmax-5.2.3`.
The user requested fresh local builds named `bmax5.2.3` and `Bmax5.3`. The old build
root was empty at the start, so both versions were configured and compiled from
scratch; no old cache was moved or reused. Source behavior is unchanged.

| Version | Build/cache directory | Installed executable |
| --- | --- | --- |
| 5.2.3 LTS | `D:\blender_build\bmax5.2.3` | `D:\blender_build\bmax5.2.3\bin\blender.exe` |
| 5.3.0 | `D:\blender_build\Bmax5.3` | `D:\blender_build\Bmax5.3\bin\blender.exe` |

Configuration for this version:

```powershell
cmake -S D:\blender_prj -B D:\blender_build\bmax5.2.3 -G "Visual Studio 17 2022" -A x64 -T v143 -DWITH_GTESTS=OFF -DWITH_GPU_DRAW_TESTS=OFF -DWITH_GPU_BACKEND_TESTS=OFF -DWITH_CYCLES_DEVICE_OPTIX=OFF -DWITH_CYCLES_CUDA_BINARIES=OFF -DWITH_CYCLES_HIP_BINARIES=OFF -DWITH_CYCLES_ONEAPI_BINARIES=OFF -DCMAKE_INSTALL_PREFIX=D:\blender_build\bmax5.2.3\bin
cmake --build D:\blender_build\bmax5.2.3 --config Release --target INSTALL --parallel 2 -- /p:CL_MPCount=2
```

Full Release INSTALL succeeded. Installed and Release executables match:
**98,144,768 bytes**, **2026-11-13 13:11:03 JST**, SHA-256
`197E213705E0C29D219DA2350B03BE3CA39CC182D720C4378142CB51683EE266`. Windows dependencies stayed at each version's pinned commit.

Each new runtime passed ten isolated background checks: version, Bmax operators,
compatibility snapshot option, packaged NumPy, armature defaults and an actual
Manifold Boolean with finite geometry. OpenGL and Vulkan each passed three graphical
checks: Shift activates Smooth before a stroke, the header accepts `0.375` from numpad
events without Unicode, and releasing Shift restores the preceding brush. The two
versions passed 32 checks in total. These are packaging/smoke checks; they do not
repeat all historical feature matrices or claim a new performance result.

Scripts, logs, isolated configuration, recovery files and verification caches were
external and removed after checking. Build caches, dependencies and installed
runtimes remain. User sessions and Desktop/public packages were preserved. The path
documentation update is local; no new source commit or push was requested for this
rebuild. Older paths and executable hashes below are historical records.

**November 13 source revision:** [5d9e2cfe](https://github.com/MaxPuliero/Bmax/commit/5d9e2cfe5d6219d2dc6e63da8c8bd0e9aa6ca856) on `codex/bmax-5.2.3` contains
the final implementation audited below. The source and documentation commits do not
rebuild the installed executable: its pre-commit metadata may still identify an older
tracking revision. Current binary hashes are in the first audit entry; earlier entries
retain their build-time hashes. Desktop copies and the public download are unchanged.

## Shift+numpad follow-up (2026-11-13)

Installed standard runtime: `D:\blender_build\octahedral_radius\bin\blender.exe`.
Installed and Release executables match: **98,144,768 bytes**, **2026-11-13 12:02:22 JST**,
SHA-256 `3956C78D3F92DC2F6C3067BA555F6CB0ADF621AE6245B23B25372C1400C6EAAC`. The committed source revision is recorded above.
No DNA or shared C++ layout changed; the existing incremental build was used.

The 5.2 build used the normal INSTALL target:

`cmake --build D:\blender_build\octahedral_radius --config Release --target INSTALL --parallel 2 -- /p:CL_MPCount=2`

The 5.3 build used target `blender` with the same parallel options. While its standard
runtime was open, a complete auxiliary runtime was installed with:

`cmake --install D:\blender_build\bmax_5.3 --config Release --prefix D:\blender_build\bmax_5.3_numpad\bin`

Once the standard executable was free, it was updated with:

`cmake --install D:\blender_build\bmax_5.3 --config Release`

Both 5.3 executables have the same final hash.

13 explicit missing-Unicode numpad checks per version/backend passed on OpenGL/Vulkan,
including exact Smooth Strength and brush restoration. A Windows input-injection UI
check also entered `1.5` via Shift+numpad in a new empty 5.3 session, then cancelled
the edit. Earlier keypad tests supplied Unicode and did not cover this Windows case.
C++ formatting and Git whitespace checks passed. Verification scripts, scenes,
screenshots, logs and isolated caches were external and removed after checking.
Build dependencies/caches, both installed runtime packages and user sessions remain.
Desktop copies and the public download were not replaced. Earlier audit hashes below
are historical. See [scope](FEATURES.md#shiftnumpad-in-numeric-fields-2026-11-13-follow-up).

## Numeric fields with Shift already held (2026-11-13)

The shared numeric UI handler accepts digits, sign and decimal punctuation while a
Shift hold inherited when entering the field continues. It also preserves Ctrl+A and
clipboard shortcuts in that state. Releasing Shift returns to native Unicode entry;
pressing Shift only after editing starts retains expression symbols. This behavior
is included in the source revision above; Desktop/public packages are unchanged.

Release INSTALL succeeded using
`cmake --build D:\blender_build\octahedral_radius --config Release --target INSTALL --parallel 2 -- /p:CL_MPCount=2`.
The existing incremental build is appropriate: only private state in the interface
translation unit changed, with no DNA or existing shared C++ layout change.
Installed runtime: `D:\blender_build\octahedral_radius\bin\blender.exe`.

Installed and Release executables match: **98,144,768 bytes**,
**2026-11-13 11:18:46 JST**, SHA-256 `846047E07FEE99E1F8FAFA5FA88A2AA8E5E805DFC9D1A30A17257B1DBC485657`. Version metadata can still refer to the preceding
tracking commit and does not identify the later source commit.

22 native-event checks per version/backend passed on OpenGL and Vulkan. Coverage
includes the normal numeric editor and sliders, arithmetic/shifted symbols, clipboard
and selection shortcuts, normal text fields, object/render properties and exact numeric
Smooth Strength entry while Shift stays held. The preceding runtime reproduced the
Shift+123 -> !@# failure. See [scope and limitations](FEATURES.md#numeric-fields-with-shift-already-held-2026-11-13).
C++ formatting and Git whitespace checks passed. Scripts, captures, recovery files,
logs and isolated runtime caches were external and removed after verification;
build dependencies/caches and the user's existing Blender session were preserved.
Earlier records below describe their historical executables.

## Shift-held smoothing brush (2026-11-13)

Source is included in the November 13 revision above on this version's separate branch.
Release INSTALL succeeded using:
`cmake --build D:\blender_build\octahedral_radius --config Release --target INSTALL --parallel 2 -- /p:CL_MPCount=2`.
No DNA or existing shared C++ structure layout changed; the normal incremental build
was used. The installed runtime is `D:\blender_build\octahedral_radius\bin\blender.exe`.

Installed and Release executables match byte for byte: **98,144,256 bytes**,
**2026-11-13 10:37:34 JST**, SHA-256 `EF279AE3F66CFC1D1561C6EDDA69F4B4660F6A134B83A0C5781F715F076B005F`.
Embedded version metadata can refer to the preceding tracking commit and does not
identify the later source commit. Desktop copies and the public package are unchanged.

Both Bmax versions passed 72 native-event checks on each actual OpenGL/Vulkan backend,
including native header Strength dragging with Shift held, real brush strokes in mesh
Sculpt/Vertex/Weight/Texture Paint, consecutive strokes, retained smoothing settings,
left/right Shift and restoration after release during a stroke. Additional per-backend
17-case checks cover deformation and native Undo/Redo from an explicit fixture undo
baseline, Sculpt color Blur, Curves and the 2D Image Editor. See scope and limitations
in [FEATURES.md](FEATURES.md#shift-held-smoothing-brush-2026-11-13).

C++ formatting, Python keymap syntax and Git whitespace checks passed. All scripts,
scenes, screenshots, recovery files, logs and isolated user/runtime caches were external
to the repositories and removed after validation. Build caches/dependencies and existing
user sessions were preserved. Earlier records below describe their historical builds.

## Selective image saving (2026-11-11)

The committed source implements the project-save image checklist on this version's
separate branch. The list uses native TreeView scrolling; popup event routing sends
wheel and trackpad events to embedded scrollable views before the outer menu consumes
them, and refreshes view hit bounds after popup positioning. UDIM sets are listed
once per Image datablock, including the tile count.
No existing shared C++ structure layout or DNA changed.

Build command: `cmake --build D:\blender_build\octahedral_radius --config Release --target INSTALL --parallel 2 -- /p:CL_MPCount=2`.
The installed runtime is `D:\blender_build\octahedral_radius\bin\blender.exe`.
Implementation: source commit `e9115666e4cdb8105151617ecfeeced445b1c523` on `codex/bmax-5.2.3`,
included in the version-branch source publication with the November 13 update.
Desktop copies and the Gumroad package are unchanged.
The audited runtime below was built and verified before this commit from the same
implementation. Its embedded metadata may refer to the preceding tracking commit;
committing the source and documentation does not rebuild or replace that executable.

Real mouse-event checks on isolated OpenGL processes passed on both 5.2.3 and 5.3:
master all/none, per-image selection, partial-state display, collapse/reopen persistence,
Save and Cancel. Only the selected PNG changed on disk and became clean; unselected
PNGs retained identical SHA-256 hashes and dirty image buffers. Both mouse Cancel and
Esc left the project and image file hashes unchanged. Zero-selection saving preserved
all modified external images. All verification artifacts were external to the
repositories and have been removed.

Final Release INSTALL succeeded. Installed and Release executables match exactly:
**98,142,208 bytes**, **2026-11-11 18:17:29 JST**, SHA-256
`95486C2787DD45E418F89700DD397162C193538A3213C7ABE4C86B17A65D0708`.

The final 13-image checks passed on both versions: wheel scrolling down/up/down,
selections retained outside the visible rows, a single two-tile UDIM entry, generated
image packing, disabled invalid destinations, and the popup opening when only an
unsavable image is dirty. Selecting one external image, the UDIM set and the generated
image saved only those three Image datablocks. The dirty UDIM tile changed on disk;
the clean tile and all nine unselected external textures retained identical hashes.
The unselected textures and invalid-path image stayed dirty in memory. The generated
image became packed and clean. Scrollbar dragging was also checked. Physical trackpad
input was not exercised. C++ formatting and Git whitespace checks passed.

Temporary verification scripts, scenes, screenshots, logs and isolated runtime caches
were removed after verification; build caches, dependencies and user sessions were retained.


**November 7 source update:** Repeat Last Stroke, Blend Symmetry, Multires Color Filter and the Multires color-picker correction are included in source commit [631dd2d0](https://github.com/MaxPuliero/Bmax/commit/631dd2d0bc6f08fe52de072cce3c5bf462ab8aa4). Both local runtimes are installed and verified. Desktop copies and the Gumroad download package have not been replaced. Earlier build records retain their historical metadata and publication status.

**November 4 source update:** Quick Favorites management, Save as Official, workspace viewport continuity and removal of the Alt active-only exception are included in source commit [e2e19fc44af6](https://github.com/MaxPuliero/Bmax/commit/e2e19fc44af680341e6008954ea096dd863b9680) on `main`. Both local runtimes are rebuilt; Desktop copies and the Gumroad binary package were not replaced. Build records below retain their build-time metadata.

See the [complete feature inventory](FEATURES.md) for the implemented additions, and [AGENTS.md](../../AGENTS.md) for contributor and AI-agent instructions.

## UV selection visibility and pins (2026-11-09)

Shell Outline now resolves below editable UV edges, face dots and vertices.
Pinned vertex markers are also drawn in modes that hide ordinary vertex dots.
Pin/Unpin/Invert use selected edge endpoints or face corners in their respective
modes, with native vertex rules retained for Vertex/Island selection.

No DNA or shared C++ structure layout changed. The component build used
`bf_draw bf_editor_uvedit` with BuildProjectReferences=false, followed by the
normal complete `INSTALL --parallel 2 -- /p:CL_MPCount=2` build to regenerate
shader dependencies and update all out-of-date components from the current source.
INSTALL succeeded. The installed executable and Release executable match:
**98,135,552 bytes**, timestamp **2026-11-09 10:31:02 JST**, SHA-256
`412FB38DB57F96918D1CD9378BE00CAE73BF5D05908F348EFECE79DA88774ABF`.
Build metadata reports base source hash `d7011ba9b18b`; the UV changes are committed on main. Only the standard runtime at
`D:\blender_build\octahedral_radius\bin\blender.exe` was updated for this work.

243 background operator assertions and 19 graphical assertions per actual
OpenGL/Vulkan backend passed; see [coverage and limits](FEATURES.md#uv-editing-priority-and-pins-2026-11-09-local-source-change).
Opaque selection colors and vertex cores are unchanged by toggling Shell Outline;
Edge/Face markers each have ten red pin pixels at the tested UV corner.
C++ formatting and diff whitespace checks passed. Scripts, GPU captures, arrays,
logs and recovery files were external and removed after verification. Build caches
and dependencies were retained. The user's Desktop process was preserved;
Desktop copies, other runtime prefixes and public packages were not replaced.

## Multires color sampling fix (2026-11-07)

Non-merged sampling now reads the selected current CCG color sample. Previously the grid
sample index was passed to base-mesh color access, producing unrelated/out-of-range reads.
The ordinary mesh path also validates vertex indices. Sample Merged and the generic
color-dialog screen eyedropper retain display-color sampling, including shading.

Only paint_sample_color.cc changed; no shared layout or DNA changed. The Sculpt target and
blender were incrementally rebuilt, then INSTALL and the separate-prefix install updated
both local runtimes using the commands below. Executable: **98,114,560 bytes**, timestamp
**2026-11-07 23:03:36 JST**, SHA-256
`3984D47062FD4724B70EBD8915F30B3589D8D1E0A9C94D3F6E019A613BAA5DC6`.
Earlier records below describe historical builds. Source remains uncommitted; Desktop/public
packages are unchanged and no user session was closed.

An isolated graphical check sampled known Float Point colors at multiple locations before
Multires subdivision, immediately after each subdivision through level 3 and after changing
Sculpt levels 1/3/2 without restarting. Every sample matched the attribute within 2e-6.
Formatting/diff checks passed. External verification scripts, logs, scenes and isolated
configuration were removed afterward; build caches and runtime dependencies were retained.

## Multires Color Filter (2026-11-07)

All ten Sculpt Color Filter modes now support Multires color grids. The implementation
shares pointwise math with meshes, adds stable grid-neighbor Smooth/unsharp processing,
and uses native masks, hidden samples, automasking and color undo. Persistent changes are
committed once at completion; lower-level edits retain higher-level detail. Native modal
dragging, cancellation and commit are supported. The first filter's undo preserves exact
coarse Float/Byte values and the original absence of painted grid storage.

Changed native translation units: sculpt_filter_color.cc, sculpt_paint_color.cc and
sculpt_undo.cc. New filter buffers and undo bookkeeping are private to those translation
units; no shared C++ layout, DNA or file format changed. Incremental MSVC v143 compilation
of bf_editor_sculpt_paint and linking blender reused the fully rebuilt dependencies, followed
by INSTALL and the separate-prefix installation using the commands below.

Both local runtimes contain this version:
`D:\blender_build\octahedral_radius\bin\blender.exe` and
`D:\blender_build\symmetrize_blend\bin\blender.exe`.
Executable: **98,114,560 bytes**, timestamp **2026-11-07 22:32:49 JST**, SHA-256
`C887CDB76B1924F0CE9BA021E0DE221F896BDA0E02BDE823D5630899B5748976`.
No user process was closed. Source changes remain local and uncommitted; Desktop and public
packages are unchanged. Earlier executable records below are historical.

Isolated graphical OpenGL/Vulkan sessions each passed 110 cases across all ten filter modes,
Float/Byte Point/Corner combinations, masks, zero/negative Strength, comparisons with an
applied mesh, unchanged coordinates and Undo/Redo. Painted-detail checks covered lower-level
deltas (maximum checked error below 1e-7), level switching, positive/negative Smooth, masks,
hidden surfaces, native Paint, other attributes, serialization/read and modifier application.
First-use cases included nonuniform Byte colors, an existing Sculpt mask undo step and
automatic attribute creation, with exact coarse-color restoration. Native mouse-event
simulation passed dragging, Esc cancellation, commit and Undo/Redo. Final changes restricting
the coarse backup to global filters passed the first-use and painted-detail checks again.

An isolated OpenGL benchmark used a cube with 384 base quads, three Multires levels and
24,576 evaluated quads, compared with the same applied mesh. HUE at Strength 0.25 used one
warm-up and three measurements per backend, with undo outside the timed section and no
concurrent verification processes. The median synchronous operator times were **2.70 ms**
for Multires and **0.38 ms** for the applied mesh. This measures operator latency, not viewport
frame rate; it is a small correctness/performance fixture, not a production guarantee.

Formatting and diff checks passed. All temporary scripts, scenes, logs, isolated preferences
and caches were external and removed after verification; build caches and runtime dependencies
were retained.

## Blend Symmetry sidebar panel (2026-11-07)

The subsequent Python UI update adds **N > Tool > Blend Symmetry** in Sculpt Mode, with
Axis, Strength, Match Tolerance, Vertex Paint and direct execution. It reuses native operator
last properties. INSTALL and the separate-prefix installation below update the startup UI
scripts; no native rebuild or executable hash change is required for this panel.
An isolated graphical check verified Sculpt-only visibility, registered panel drawing,
the default enabled color option and forwarding all displayed settings to direct execution.
Both installed startup scripts match the source. Temporary verification artifacts were removed.

## Symmetrize Blend vertex colors (2026-11-07)

**Vertex Paint** now defaults on in Symmetrize Blend. It blends the active attribute in
scene-linear RGBA, using the geometry's masks, hidden-point protection and Strength. Ordinary
Float/Byte Point/Corner attributes retain their storage; native position/color undo steps are
grouped into one user-visible operation. Multires samples and stores the highest-level color
grids and includes them in geometry undo. The option off retains the geometry-only path.

Only sculpt_symmetrize_blend.cc changed native behavior; no shared layout or DNA changed.
Incremental compilation of bf_editor_sculpt_paint, linking blender, INSTALL and installation
to the separate prefix reused the commands below. No Blender instances were open during the
installations; no user session was closed. Both local runtime paths now contain this version:

- `D:\blender_build\octahedral_radius\bin\blender.exe`
- `D:\blender_build\symmetrize_blend\bin\blender.exe`

Executable: **98,103,808 bytes**, timestamp **2026-11-07 21:50:17 JST**, SHA-256
`C0086FEFA4D1E62AD0875D9DB17090F24FD0ED94528F306DE19DBC289411503B`.
Source changes remain uncommitted; Desktop/public packages are unchanged. Earlier executable
records below are historical.

OpenGL/Vulkan graphical checks used all four Float/Byte Point/Corner combinations, masks,
Strength, alpha, geometry-plus-color and color-only changes, option off, separate attributes
and one-step Undo/Redo. Maximum Float discrepancy was below 1.2e-7; ordinary Byte differences
from unquantized means stayed below 0.0038 in the checked scene-linear components. Multires
checks cover maximum/lower Sculpt levels, full masks, actual Paint Hard strokes and retained
detail after changing levels. Partial-region checks retained unmatched component colors exactly;
meshes loaded from a saved temporary copy retained coarse colors and applying Multires produced
standard Float Corner colors. These are correctness checks, not a performance benchmark.

C++ formatting and diff checks passed. All verification artifacts were external and removed
afterward; build caches and runtime dependencies were retained.

## Symmetrize Blend without active Multires (2026-11-07)

The operator now supports ordinary Sculpt mesh geometry, including disabled Multires and
Sculpt Base Mesh. It reuses native Topology Mirror matching and extends reliable pairs
through unique reversed faces to recover deformed vertices. Masks, hidden points and
unmatched face borders are locked as appropriate. Native Sculpt position deformation and
undo handle mesh updates without replacing topology or Multires displacement storage.

Only sculpt_symmetrize_blend.cc changed native behavior; no shared layout or DNA changed.
The Sculpt target and blender were incrementally rebuilt, then INSTALL updated the standard
runtime, using the same reference-skipping commands recorded below. The separate runtime
at `D:\blender_build\symmetrize_blend\bin\blender.exe` was **not** replaced: the user's
existing session is running there and was left open. It retains the previous Multires-only
version. Desktop/public packages are unchanged; source changes remain uncommitted.

`D:\blender_build\octahedral_radius\bin\blender.exe`: 98,095,616 bytes,
timestamp **2026-11-07 21:19:36 JST**, SHA-256
`6C5FED9063D3AA36842DBA7A2D5E73F854E835A21D5E6DEA2E2B5CE0607FBD3C`.
Earlier executable records below describe their respective historical builds.

Isolated graphical OpenGL/Vulkan checks passed for a deformed cube, Suzanne and UV sphere:
X/Y/Z averaging, full/soft masks, hidden vertices, Strength, shape keys, disabled Multires
and native Undo/Redo. Expected pair coordinates matched exactly in these fixtures. Partial
connected/disconnected regions preserved unmatched faces/components, and no matches finished
unchanged without an error. Active Multires Suzanne retained 7,958 evaluated vertices and
passed reflected-surface validation (maximum residual about 2.41e-7 Blender units).

C++ formatting and diff checks passed. No performance benchmark was run. Verification
scripts, scenes, logs, isolated preferences and caches were external and removed afterward.
Build caches and runtime dependencies were retained; no user process or project was modified.

## Suzanne and partial Symmetrize Blend (2026-11-07)

The original nearest-two uniqueness check rejected Suzanne because four base vertices occupy
two coincident positions with different eyelid/head connectivity. Candidate matches now use
neighbor connectivity to distinguish them, followed by reciprocal reversed-face validation.
The requested best-effort behavior also replaces whole-mesh rejection: matched regions are
processed, unmatched parts are kept, and an empty match is an informational no-op. Border
samples next to unmatched grids are locked, and only changed displacement arrays are committed.

Only sculpt_symmetrize_blend.cc changed native behavior; no shared layout or DNA changed.
The Sculpt target was incrementally compiled and blender linked using the reference-skipping
commands below (the already rebuilt dependencies were retained). With no Blender processes
open, both local installations were updated:

```powershell
cmake --build D:\blender_build\octahedral_radius --config Release --target bf_editor_sculpt_paint --parallel 2 -- /p:BuildProjectReferences=false /p:CL_MPCount=2
cmake --build D:\blender_build\octahedral_radius --config Release --target blender --parallel 2 -- /p:BuildProjectReferences=false /p:CL_MPCount=2
cmake --build D:\blender_build\octahedral_radius --config Release --target INSTALL -- /p:BuildProjectReferences=false
cmake --install D:\blender_build\octahedral_radius --config Release --prefix D:\blender_build\symmetrize_blend\bin
```

Current executables at both `D:\blender_build\octahedral_radius\bin\blender.exe` and
`D:\blender_build\symmetrize_blend\bin\blender.exe` match bin/Release/blender.exe:
98,089,984 bytes, timestamp **2026-11-07 20:39:39 JST**, SHA-256
`CACA5FED86930B50DB51DF30021CCC3C6183B58EECDE7472DA89DB7BA15C0066`.
Desktop/public packages were not updated. Source changes remain local and uncommitted.
Earlier binary records below are historical.

Regression validation used graphical factory-startup sessions with isolated temporary
configuration on actual OpenGL and Vulkan. Suzanne with two Multires levels produced 7,958
evaluated vertices: every reference pair passed averaging, with maximum positional error
about 2.41e-7 Blender units and reflected-surface residual about 2.39e-7. Undo/Redo and a full
mask passed. A cube plus a disconnected unmatched icosphere processed only the cube: all
962 evaluated points on the unmatched component stayed exactly unchanged (Vulkan). A cube
with a missing mirrored face processed its remaining valid region and passed Undo/Redo and
mask checks. An entirely unmatched component finished without errors or changes. The final
subsequent rebuild changed only the informational message text, removing grid terminology.

C++ formatting and diff checks passed; no performance benchmark was run. All temporary
scripts, scenes, logs and configuration/cache artifacts were external and removed after
verification. No existing user process or scene was closed or modified.

## Symmetrize Blend (2026-11-07)

This initial build record is superseded by the Suzanne/partial-matching correction above.

The new operator is a separate Sculpt translation unit, registered alongside native
Symmetrize and exposed in the Sculpt menu. No DNA or existing shared C++ layout changed;
the incremental MSVC v143 build reused the complete rebuild from Repeat Last Stroke.

```powershell
cmake --build D:\blender_build\octahedral_radius --config Release --target bf_editor_sculpt_paint --parallel 2 -- /p:BuildProjectReferences=false /p:CL_MPCount=2
cmake --build D:\blender_build\octahedral_radius --config Release --target blender --parallel 2 -- /p:BuildProjectReferences=false /p:CL_MPCount=2
cmake --install D:\blender_build\octahedral_radius --config Release --prefix D:\blender_build\symmetrize_blend\bin
```

Reference skipping is appropriate here only because the existing dependencies were already
fully rebuilt and no shared layout changed. CMake regenerated the project for the new file.
The install-prefix override did not change the CMake cache. Build caches and dependencies
were retained.

The standard runtime was open and was preserved: `D:\blender_build\octahedral_radius\bin\blender.exe`
remains the earlier Repeat Last Stroke build (SHA-256
`A3448241F7263BC1A915803F97AE3E6751B0FC81D81FBC2C424E2D28302315E7`).
The complete new runtime is `D:\blender_build\symmetrize_blend\bin\blender.exe`, 98,085,888 bytes,
timestamp **2026-11-07 15:50:03 JST**, SHA-256
`40E9221FE182D1962C2FC97EF50F19CB6B6CEFE7F71ED642CA5A066FF5924817`.
It matches bin/Release/blender.exe. Desktop/public packages were not replaced. Source
changes remain local and uncommitted, including the earlier Repeat Last Stroke addition.

Correctness checks ran in isolated graphical factory-startup sessions on actual OpenGL
and Vulkan. Temporary Object Mode copies evaluated the highest Multires level after native
flush, avoiding the active Sculpt object's base-cage readback. Fixtures included a three-level
cube, triangle-capped UV sphere and n-gon-capped cylinder. Each was asymmetrically sculpted
before comparing the output against mirrored vertex pairs on an undeformed reference mesh.

Verified X/Y/Z averaging, Strength 0.25, uniform soft mask 0.5, full-mask no-op, one-sided
mask anchoring, levels retained with the first Sculpt level displayed, Undo/Redo, and base
Float Corner RGBA preservation. Multiple Sculpt Layers retained the earlier geometry when
the correction layer was switched off and restored symmetry when switched back on. A mask
cleared at the visible low level was respected despite finer mask data remaining in storage.
A missing mirrored base face was rejected without changing geometry or level counts.
Maximum observed positional residual was about 3.88e-7 Blender units; no timing/frame-rate
benchmark was run. Installed UI equality, Python syntax, C++ formatting and diff checks passed.

All verification scripts, scenes, logs and configuration/cache files were external
temporary artifacts and removed after validation. The user's existing Blender process was
not closed or controlled.

## Repeat Last Stroke (2026-11-07)

A runtime-only shared pointer was added to SculptSession, so all components were rebuilt
with the existing cache/dependencies retained. The final Sculpt/BMesh implementation was
then compiled and linked against those rebuilt libraries before running the INSTALL target:

```powershell
cmake --build D:\blender_build\octahedral_radius --config Release --target blender --parallel 2 -- /t:Rebuild /p:CL_MPCount=2
cmake --build D:\blender_build\octahedral_radius --config Release --target bf_editor_sculpt_paint bf_bmesh --parallel 2 -- /p:BuildProjectReferences=false /p:CL_MPCount=2
cmake --build D:\blender_build\octahedral_radius --config Release --target blender --parallel 2 -- /p:BuildProjectReferences=false /p:CL_MPCount=2
cmake --build D:\blender_build\octahedral_radius --config Release --target INSTALL -- /p:BuildProjectReferences=false
```

The reference-skipping switches above apply only after the complete rebuild. For a fresh
build or any further shared-layout changes, rebuild all dependencies normally first.
The separate Sculpt/BMesh Rebuild also completed during this task; no build caches were removed.

Installed executable: `D:\blender_build\octahedral_radius\bin\blender.exe`, 98,079,232 bytes,
timestamp 2026-11-07 14:09:38 JST, SHA-256
`A3448241F7263BC1A915803F97AE3E6751B0FC81D81FBC2C424E2D28302315E7`.
It matches bin/Release/blender.exe. No user Blender instance was closed. Desktop/public
packages and the separate earlier runtimes were not replaced. Source changes remain
local, uncommitted and unpublished.

Validation used isolated graphical factory-startup sessions with temporary configuration:

- Mesh: original displacement x2/x3/x4, changed brush strength and view, separate Undo/Redo;
  a subsequent Mask stroke clears the geometry cache without moving vertices.
- Multires: two levels, 12,034 evaluated vertices; equal increments and Undo/Redo, including
  saving between repetitions. Maximum increment residual was about 1.21e-7 Blender units.
  Geometry was inspected through temporary Object Mode copies after native flush, since
  reading the evaluated active Sculpt object directly can return the base cage.
- Dyntopo: 897 vertices / 1,790 faces retained across repetitions; equal increments, saves
  and Undo/Redo. Maximum increment residual was about 5.98e-8 Blender units. New vertices
  use their birth position rather than an undefined pre-creation coordinate.
- Actual OpenGL and Vulkan backends passed geometry checks. A changed Multires sculpt
  level was rejected on Vulkan. Native mouse stroke + key 1 + Undo/Redo passed on OpenGL;
  the first simulated gesture was consumed by initial UI setup, so the input check used
  a second gesture. The shortcut and installed operator name were also checked.
- Boolean Manifold and Sculpt Trim passed on both backends, producing the expected
  volume 4 from a volume-8 cube cut in half.

These checks use Draw for the source geometry stroke; they are not a brush-wide benchmark
or a frame-rate claim. C++ formatting, Python syntax, installed executable identity and
Git diff checks passed. Verification scripts, scenes, logs, screenshots and isolated
configuration/cache files were temporary and outside the repository, and were removed afterward.

## Persistent base between modes and Flush Persistent (2026-11-05)

Shared SculptSession/ObjectRuntime layouts changed, so the existing build was rebuilt using:

```powershell
cmake --build D:\blender_build\octahedral_radius --config Release --target blender --parallel 2 -- /t:Rebuild /p:CL_MPCount=2
cmake --install D:\blender_build\octahedral_radius --config Release
```

An initial compile rejected a mesh-session-ID wrapper comparison. The field was corrected to
an equal-size uint64_t storing native numeric ID.session_uid, then dependency compilation
and Release installation finished successfully. Build caches and pinned libraries were retained.

The current standard executable is `D:\blender_build\octahedral_radius\bin\blender.exe`,
98,086,400 bytes, timestamp 2026-11-05 15:46:58 JST, SHA-256
`1DEA308D39BB6B12613E1D36799BCB06DA5BFD2C6953358C9F4A575010B87AD6`.
It matches bin/Release/blender.exe and includes the earlier stroke-Undo fix. The separate
persistent_undo_fix install remains the prior correction. Desktop/public copies were not
updated; the user's open Desktop process was preserved. Source implementation: [`a70d32267f1b`](https://github.com/MaxPuliero/Bmax/commit/a70d32267f1b433062447253efae9dd05171d605) on `main`.

181 assertions passed: 119 OpenGL data/operator checks, 7 native UI checks, 52 actual Vulkan
checks and 3 native Manifold Boolean/Sculpt Trim checks. See [the inventory](FEATURES.md) and
[technical notes](sculpt_thick.md) for cases and limits. Panel captures and the confirmation
dialog were inspected. C++ formatting, Python syntax, installed UI equality and diff checks
passed. Temporary scripts, scenes, logs, captures and isolated preferences were external and
removed. No new stroke benchmark was run; the collision solver is unchanged.

## Persistent base survives stroke Undo (2026-11-05)

Incremental Release blender compilation completed with parallelism 2 and CL_MPCount=2.
Only sculpt_ops.cc changes native behavior: Set/Update and limit toggles stop inserting
memfile Undo entries; capture explicitly calls WM_file_tag_modified. No shared layout,
geometry solver or GPU code changed, and no forced Rebuild was required.

The standard runtime was open (user process preserved), so a complete separate install used:

```powershell
cmake --build D:\blender_build\octahedral_radius --config Release --target blender --parallel 2 -- /p:CL_MPCount=2
cmake --install D:\blender_build\octahedral_radius --config Release --prefix D:\blender_build\persistent_undo_fix\bin
```

The ready executable `D:\blender_build\persistent_undo_fix\bin\blender.exe` is 98,080,768
bytes, timestamp 2026-11-05 14:33:40 JST, SHA-256
`E1399B9E1B28956FE478E5F4BA2DB9B5C3848297CAFE5D6B133E11C3A690EA4F`.
At that stage it matched bin/Release/blender.exe and the standard 14:15:53 runtime was left
untouched while in use. The mode-retention follow-up above has since updated the standard
installation. Desktop/public copies and this separate runtime were not replaced.
The install prefix override did not change the CMake cache. The source is now committed on `main`.

The previous runtime reproduced the failure on Multires: first brush Undo invalidated the
base; a no-memfile control retained it. The corrected runtime passed 105 targeted checks:
89 in ten OpenGL scenarios and 16 in two Vulkan scenarios (actual Vulkan confirmed).
Cases include ordinary mesh/Layer/Cloth, Multires Draw/Layer/Cloth, Undo-only geometry,
Redo geometry, missing-base toggle capture, limit toggles after capture, and Update between
two strokes. Set/Update has no artificial Undo checkpoint after it, and real strokes use
explicit operator Undo registration. Geometry returns to zero on Undo or the expected 0.02
stroke result on Redo; undoing a second stroke after Update leaves the first sculpt intact.
Persistent, the limit flag and a compatible base remain active throughout.

Format, syntax and runtime identity were checked. All temporary scripts, isolated settings,
scenes, recovery files and logs were outside the repository and removed after recording
results. The user's open Blender process, pre-existing files and build caches were preserved.

## Persistent controls refinement (2026-11-05)

Incremental Release INSTALL completed using the same MSVC v143 configuration, parallelism 2
and CL_MPCount=2. Changes add uniform native Persistent-group spacing, a single checkbox on
all brushes, automatic native Persistent activation on Set/Update, master gating of limits,
and 0.02 distance defaults with DNA-presence migration for old brush assets. Existing saved
distances, including zero, are preserved. Only default initializers changed in Brush DNA:
sizes, offsets and shared C++ layouts are unchanged, so no forced Rebuild was required.
Header/default/RNA dependencies were recompiled normally.

The installed `D:\blender_build\octahedral_radius\bin\blender.exe` is 98,080,768 bytes,
timestamp 2026-11-05 14:15:53 JST, SHA-256
`5ACCBE55EA4773005C78D8BF24D3C4904F2BE1975022FA1C1677056E458F7DF5`.
The standard runtime is updated. The source is now committed on main; Desktop/public
packages and official_save_preview were not replaced.

27 OpenGL data/brush assertions and 9 native confirmation/UI assertions passed on this
runtime. They cover new/RNA/stock defaults, preservation of saved zero/custom distances,
Set/Update auto-activation, default caps on Draw/Inflate Cloth/Layer, master-off suspension,
retained reference, cancel/confirm and the initially requested limit. Before/after, master-off
and Cloth captures were visually inspected: separator spacing is consistent and Persistent
appears once. An isolated Multires 2 -> 1 -> 2 probe showed base validity true/false/true while
the original session/storage survives. This refinement changes no GPU geometry algorithm;
Vulkan and the prior performance benchmark were not repeated.

Verification used graphical factory-startup sessions with isolated configuration and all
scripts, scenes, captures and logs outside the repository. Task artifacts were removed after
recording the results; pre-existing files and build dependencies were preserved.

## Persistent-base brush limits (2026-11-05)

The local main checkout adds per-brush Limit Up/Down, common Set/Update Persistent Base,
missing-base confirmation and approximate reference collision inside the Cloth solver.
Brush DNA and SculptSession layouts changed, so all dependencies were rebuilt with the
existing MSVC v143 Release configuration before INSTALL:

```powershell
cmake --build D:\blender_build\octahedral_radius --config Release --target blender --parallel 2 -- /t:Rebuild /p:CL_MPCount=2
cmake --build D:\blender_build\octahedral_radius --config Release --target INSTALL --parallel 2 -- /p:CL_MPCount=2
```

Initial DNA padding and MSVC Span-conversion errors were corrected. The final accelerator
optimization changes only sculpt_thick.cc implementation and uses a native point BVH; its
incremental INSTALL succeeded without another layout change. No cache/dependency deletion
or user-process termination was needed.

The installed `D:\blender_build\octahedral_radius\bin\blender.exe` is 98,080,768 bytes,
timestamp 2026-11-05 13:27:30 JST, SHA-256
`BE715221B01CC4AC7FF96FCFE36382F16B96AD14442DADB94139B92E15759609`.
The installed file and bin/Release executable match. The source is now committed on main;
official_save_preview remains the previous Bmax runtime used for comparison. Desktop copies
and the public download were not replaced.

The final installed binary passed 87 targeted checks: 34 OpenGL, 34 Vulkan, 10 extended
geometry/reference/shape-key/Manifold checks, 5 undo/redo/tangential-slide checks and 4 real
confirmation checks. Before/after panels and the native dialog were visually inspected.
The data tests validate both distance signs, Ctrl side swapping, world distance under
unapplied scale, Clay/Inflate/Draw/Cloth, current-level Multires, ordinary mesh save/reload,
zero-depth collision, masks, no snapping of prior violations, reference updates and unchanged
Basis shape keys. Manifold difference volume and half-cube Sculpt Trim passed after Rebuild.

Matched seven-measurement median stroke times were Draw 4.58 ms off / 6.92 ms on (263,169
vertices) and Cloth 39.69 ms off / 72.87 ms on (66,049 vertices). The previous build measured
4.67 / 39.87 ms. Each operator has 10 samples; a separately timed cold stroke and two further
warm-up strokes precede measurements. These are synchronous operator times, not FPS.
See [full interpretation and limitations](sculpt_thick.md).

Early fixture attempts used unsupported background viewport/stroke calls, omitted required
tilt fields, used invalid direction enum names, kept a stale override during file reload,
or omitted explicit Python Undo registration. Corrected tests ran in separate graphical
factory-startup sessions; final sessions used an isolated configuration directory. Temporary
scripts, scenes, settings, captures, recovery files and task logs were external and removed.
Pre-existing diagnostic files, user data, runtimes and build dependencies were preserved.

## Remove the Alt active-only exception (2026-11-04)

The Release blender and locales targets and CMake installation completed with
parallelism 2 and CL_MPCount=2. Changes are limited to property-selection guards,
the shared multi-edit invocation helper, the material selector and nine tooltip
translations. No DNA or shared layout changed. Existing explicit API arguments,
Python execution defaults and array clipboard handling were retained.

The standard executable at `D:\blender_build\octahedral_radius\bin\blender.exe`
is 98,063,360 bytes, timestamp 2026-11-04 14:05:57 JST, SHA-256
`75BD0444933C27C1A31E6F9B7E6C3EA1CD21DB343943F20EE54E079E9176D5B1`.
A full CMake install also updated `D:\blender_build\official_save_preview\bin`,
including the executable, matching symbols and locale catalogs. Both runtimes
include all current local changes. Source remains local/uncommitted on main;
Desktop and public packages are unchanged.

Six focused OpenGL cases cover Alt numeric stepping and modifier toggles,
material unlink propagation, multi-object modifier/Geometry Data invocations and
preservation of explicit active-only/Python execution behavior. Unselected data
served as a control. Nine installed MO lookups contain the shortened translated
tooltip and no longer contain the old Alt instruction. Changed C++ formatting,
diff whitespace and matching executable hashes were checked. See the [inventory](FEATURES.md#selected-item-editing-without-the-alt-exception-2026-11-04-local-follow-up)
for validation limits. Early harness attempts had unsupported double-click values
or lacked separate hover/activation events; the final sequence tests real numeric
stepping instead of claiming a successful typed-entry regression.

Temporary scripts, isolated settings, captures, scenes, recovery files and logs
were external and removed after verification. Required runtimes, build caches,
dependencies and pre-existing user data were preserved. No commit or push was made.

## Continuous workspace viewport (2026-11-04)

The Release blender target and CMake installation completed from D:\blender_prj
with parallelism 2 and CL_MPCount=2. The native change records last-focused 3D
window regions at screen focus updates and carries navigation/visibility during
workspace switches. Isolation uses distinct native slots and separately owned
restore values; drawing resources, shading, overlays, and locked panel axes are
preserved. No DNA or shared structure layout changed, so no forced Rebuild was
required. Public editor declarations caused a broad dependency rebuild.

The user had closed the previously occupied standard runtime before installation;
no user process was closed by the agent. The installed executable is
`D:\blender_build\octahedral_radius\bin\blender.exe`, 98,063,360 bytes,
timestamp 2026-11-04 13:53:21 JST, SHA-256
`6F2F6D38B2C17CA89DB9F7E3084BC1218B6B72DB7DAF32F84950611E9633F25E`.
It is byte-identical to bin/Release/blender.exe and the updated separate
`D:\blender_build\official_save_preview\bin\blender.exe`. Both runtimes now
include the local Quick Favorites manager, Save as Official and workspace
continuity. Metadata reports base ceae7c71f6c4, built at 04:53:01 UTC, while
source remains local/uncommitted on main. Desktop/public packages were not updated.
The earlier records below describe previous executable states.

115 interactive assertions passed on each actual backend, OpenGL and Vulkan;
13 further assertions per backend cover Quad View isolation/restore. Ten native
reload checks verify saved membership and independent isolation after reopening.
Coverage includes bidirectional navigation, ordinary Hide, Local Collections,
object-type visibility, separate shading/overlays, local camera frame pan/zoom,
different pinned scenes and tracking of the last smaller viewport in a split
layout. Screenshots were visually inspected. The camera is not moved or keyed by
this transfer. No performance benchmark or general rig/render regression was
needed for this navigation-only change; the native 16-slot isolation limits remain.

Initial compilation found a misspelled lock constant and missing string include;
they were corrected before successful linking. Initial fixture failures used the
wrong namespace for Hide Collection, sampled camera navigation before redraw and
used an RNA collection containment test with an object instead of a name. The
corrected harnesses passed. These failures did not involve user scene data.

Changed C++ formatting, diff whitespace, installed runtime hashes and source/startup
script agreement passed. Verification used only isolated temporary settings, native
simulated events, scripts and scenes outside the repository. Temporary artifacts
were removed afterward. Build caches, dependencies, both ready runtimes and
pre-existing user data were retained. No source commit or push was performed.

## Save as Official (2026-11-04)

The Release native build completed with parallelism 2 and CL_MPCount=2. Mesh RNA
adds preparation of disposable copies and detection of Bmax color grids; a hidden
copy-only save option writes the source snapshot without normal save side effects.
No DNA or shared layout changes were introduced. The initial INSTALL reached the
executable copy while the user had the standard runtime open, so that copy failed.
The user process was preserved. A complete side-by-side installation succeeded:

```powershell
cmake --install D:\blender_build\octahedral_radius --config Release --prefix D:\blender_build\official_save_preview\bin
```

The ready runtime is `D:\blender_build\official_save_preview\bin\blender.exe`,
98,058,240 bytes, timestamp 2026-11-04 12:38:14 JST, SHA-256
`3692B1F3AA9C0DB78D05BD2D728EED76DDF6422EF50F1D549B3865BCD6E0205F`.
It matches bin/Release/blender.exe and reports base ceae7c71f6c4. Later Python
refinements were copied into this runtime. The standard executable at
`D:\blender_build\octahedral_radius\bin\blender.exe` remains the preceding
Quick Favorites build while in use. Startup scripts guard the new native APIs,
so that executable safely hides the unsupported Save as Official command.
The CMake installation prefix/cache was not changed. Desktop and public packages
were not replaced. Source is local/uncommitted.

Background conversion checks passed, including 13 official Blender 5.2.3 output
checks and 16 edge cases. Real sculpt strokes created a two-layer stack; real
paint strokes covered Float/Corner and Byte/Point attributes. Output geometry and
both evaluated color buffers matched exactly. Standard custom-normal encoding
introduced at most 0.000053 component error. Dirty byte images and float RGBA
images retained their pixels without altering source pixels or files, even with
Always Save Modified Images and automatic packing enabled on the source. Linked
collections localized correctly without unwanted extra objects, and normal Save
As still executed its save handlers. See the [inventory](FEATURES.md#save-as-official-2026-11-04-local-implementation)
for scope and limits.

Interactive File-menu/file-browser saves passed on actual OpenGL and Vulkan,
including the warning panel, asynchronous completion, and return to an unchanged
source in Sculpt Mode. Captures were visually inspected. Early harness issues
were corrected: opening a file invalidated a nonpersistent timer; evaluating and
clearing a borrowed sculpt mesh during viewport drawing was removed from fixture
creation. A partial color-library file required a second isolated save to give it
an active scene before opening in official Blender. Float EXR alpha metadata was
corrected after the pixel round-trip check detected double premultiplication.

Syntax, changed C++ formatting, runtime/source script agreement and diff whitespace
checks passed. All verification scripts, scenes, logs, captures, isolated settings,
recovery files and diagnostic crash files were external and removed after checking.
Build caches, dependencies, installed runtimes and the user's session were retained.

## Quick Favorites manager (2026-11-04)

Release INSTALL completed from D:\blender_prj with parallelism 2 and CL_MPCount=2.
The new RNA API exposes the existing bUserMenu/bUserMenuItem lists with read-only
metadata and native move/remove methods. The Python preferences panel and operators
use these lists directly; Q supplies its native editor/context to the manager.
No DNA or shared layout changes were made, so no forced full Rebuild was required.
The RNA additions regenerated dependencies and caused a broad incremental build.
Initial RNA generation rejected the reserved Python name items; the public API
uses entries while its SDNA storage remains the original items list. Locale targets
were rebuilt and installed after translation refinements.

The installed D:\blender_build\octahedral_radius\bin\blender.exe is timestamped
2026-11-04 11:44:51 JST, 98,054,144 bytes, SHA-256
`6430AFA168DD7B97B47B93295E48CDCBC60471AF7FBE3A397FE55206281EDBFF`,
matching bin/Release/blender.exe. It reports base hash ceae7c71f6c4; this feature is
local and uncommitted. The installed startup scripts match source, and all nine
MO catalogs contain the eleven new control/tooltip translations.

77 native assertions passed on the final runtime. Five GUI checks per OpenGL/Vulkan
backend plus seven OpenGL node-editor/language checks passed, with real popup and
preferences captures inspected. An isolated native preference-file comparison
preserved exact operator properties, enum invocation settings, labels, property
paths/indices and list grouping apart from the requested moves/removals. The edited
file survived a read/save round trip through official Blender 5.2.3 with identical
favorites metadata. Source and installed catalog checks passed. See the [inventory](FEATURES.md#native-quick-favorites-manager-2026-11-04-local-implementation) for scope and harness notes.

Verification used copies of user preferences with add-ons disabled in the copies,
isolated config/script/extension directories, factory startup and native simulated
events. Original preferences and the user's Desktop session were preserved.
Temporary scripts, preferences, captures, logs, recovery files and incidental reader
bytecode were removed; required build caches and installed runtime files remain.
Source has not been committed/pushed; Desktop and Gumroad packages are separate.

## Hide bone axes in Object Mode (2026-11-04)

Incremental Release INSTALL completed on `main` with the documented parallelism 2
and CL_MPCount=2. A pose-drawing condition hides bone axes in Object Mode without
modifying the saved Axes flag; Edit/Pose retain the selected-bones filter and final
always-in-front axis pass. No DNA, shared layout or evaluation change was made.

The installed `D:\blender_build\octahedral_radius\bin\blender.exe` is timestamped
2026-11-04 11:03:44 JST, 98,037,760 bytes, SHA-256
`9DD853338B8C798BAB94EAEEA8BD96B75C553FA1CB19BD574CBD96E98E1398D1`,
matching `bin/Release/blender.exe`. It reports base hash `8bd735d85ca2`; compilation
preceded implementation commit [`304254a5c167`](https://github.com/MaxPuliero/Bmax/commit/304254a5c167240276db466546cf39f3bf072273), now on `main`.

Thirteen GUI viewport states on each actual backend, OpenGL and Vulkan, verified
Axes on/off in Object/Edit/Pose, selected and unselected bones, unchanged Axes
settings during mode changes, and exact return-state image equality. The old
runtime reproduced Object Mode axes (160 changed pixels); the corrected one has
zero changed pixels there, 158 in selected Edit/Pose, and zero on unselected bones.
Names remained off. Pose and Object captures were visually inspected. The change
does not alter the always-in-front implementation; occluder-specific and full rig
regressions were not repeated. C++ formatting and diff whitespace checks passed.

The user accepted this follow-up and requested source publication. Existing user
sessions, Desktop copies and the public binary package were preserved. Verification scripts,
images, logs and recovery files were external and removed after checking; build
caches and runtime dependencies were retained.

## Portable taskbar identity independent of registration (2026-11-04 follow-up)

The user reported that the preceding icon attempt was insufficient: manually
registering Bmax made the taskbar image appear immediately. The follow-up uses a
stable per-executable-path AppUserModelID for process, windows and recent documents,
sets relaunch metadata before the window identity, and references the embedded
numeric Bmax icon as `blender.exe,-1`. Startup makes no file-association or pinned
shortcut registration changes. The explicit Register/Unregister controls retain
their existing file-handler behavior. See [Windows icon details](windows_icons.md#separate-portable-identity-from-file-association-2026-11-04).

Release INSTALL completed with the existing parallelism 2 and CL_MPCount=2. No DNA
or shared C++ layout changed. The installed executable is timestamped
2026-11-04 10:46:05 JST, 98,037,760 bytes, SHA-256
`C7870EC14632D403143014023A06AB3A282B0C8B5A529565D319696D0163AFAC`,
matching `bin/Release/blender.exe`. It retains base hash `c9f23c567b8f` and includes
the previously verified bone-name Object Mode correction.

Two independent factory-startup GUI processes, launched from the installed path and
a Japanese/space-containing temporary portable alias, passed native window/process
identity consistency, expected stable path-hash identity, distinct identities,
absence of HKCU/HKLM registration for those identities, quoted relaunch paths, and
valid 32/16-pixel `SHDefExtractIconW` handles using resource ID -1. Full association
subtree snapshots were unchanged during each check; external default association,
registered launcher and DefaultIcon snapshots were also unchanged before/after.
Changed C++ formatting and diff checks passed. The user accepted the follow-up
and requested source publication on November 4. No direct taskbar screenshot was
captured by the automated shell checks. The earlier
OpenGL/Vulkan bone-label checks were not repeated for this Windows-only follow-up.

Implementation: [`c6b2d4b917ab`](https://github.com/MaxPuliero/Bmax/commit/c6b2d4b917ab8884ba72c332a55ee28755bfc474)
on `main`. Compilation preceded this commit, so the executable retains its base
hash. Existing user sessions, Desktop copies and the public package were preserved. Temporary scripts, logs, recovery
files and the portable alias were external and removed after checking. Build cache,
dependencies and installed runtime were retained.

## Portable taskbar icon and bone-name mode visibility (2026-11-04)

Incremental Release INSTALL completed from `D:\blender_prj` on `main` with local,
uncommitted corrections, using parallelism 2 and CL_MPCount=2. No DNA or shared
C++ structure layout changed. Build caches and dependencies were retained.

The installed `D:\blender_build\octahedral_radius\bin\blender.exe` is timestamped
2026-11-04 10:29:19 JST, 98,035,200 bytes, SHA-256
`9D7A7EDA07E4AC76089B5DC632BFE2B0B064F4C2614E0000652E83B1B2511214`.
It matches `bin/Release/blender.exe` and reports base hash `c9f23c567b8f`.

Nine viewport states on each of OpenGL and Vulkan verified Object Mode name
suppression, enabled/disabled Edit/Pose labels, selected bones, unchanged Names
settings during mode switches, and exact return-state image equality. The prior
Desktop executable reproduced the Object Mode label defect in a separate process.
Native property-store reads verified the explicit icon resource and quoted relaunch
command, including a Japanese/space-containing portable path. Changed C++ formatting
and diff whitespace checks passed. See [the inventory](FEATURES.md#portable-taskbar-icon-and-bone-names-by-mode-2026-11-04).

Existing user sessions and the Desktop portable were preserved. Source changes
have not been committed or published; the public download was not updated.
Verification scripts, screenshots, logs, recovery files and the temporary path
alias were external and removed after checking. To use this correction now, launch
the installed executable above; copying or distributing a portable package is a
separate step.

## Static Layer 1 and native single-layer path (2026-11-01)

The implementation in source commit [`af594efca925`](https://github.com/MaxPuliero/Bmax/commit/af594efca9257511279692edd73f24b2f9084832), included in `main` and developed on
`codex/sculpt-layers-prototype`, uses
Layer 1 as a fixed base without an Influence slider. Only Layer 2 onward has
adjustable influence. The singleton uses native stroke/undo without a base copy,
layer delta recording or a surface comparison at stroke completion. Adding Layer 2
captures the current sculpt as the static base. Removing the final overlay or
baking releases authoring buffers and returns to the native singleton path.

Bake retains the visible weighted geometry as static Layer 1 and removes the
other layers. There is no global slider restoring the original unsculpted mesh;
Undo restores the pre-Bake state. Earlier prototype files are migrated by folding
the old first layer at its saved influence into the static base, retaining their
visible shape. This supersedes the earlier reversible Layer 1 described below.

The existing Release INSTALL command completed using MSVC v143, parallelism 2 and
CL_MPCount=2. This static-base optimization does not change DNA or shared C++
structure layouts relative to the earlier local prototype; static Layer 1 reserves
UID zero in the existing data. The initial DNA build had already received its full
Rebuild. Dependencies, build cache and runtime files were retained.

The installed `D:\blender_build\octahedral_radius\bin\blender.exe` is timestamped
2026-11-01 00:48:57 JST, 98,010,624 bytes, SHA-256
`E95963B3C14CE03556E9C25BCE88B9190006B940169EA8A99028D62884933C5A`.
It matches `bin/Release/blender.exe`. Compilation preceded implementation commit
[`af594efca925`](https://github.com/MaxPuliero/Bmax/commit/af594efca9257511279692edd73f24b2f9084832); binary metadata still reports the starting hash `15f7bbc89857` and modified
feature branch. The source is published on `main`. Desktop copies and the public
download were not updated by source publication.

Verification passed **137 checks on OpenGL and 84 on Vulkan**. OpenGL includes
extra benchmark validation. Checks cover fixed/read-only Layer 1, absence of all
authoring buffers for singleton strokes/undo, native high/low stroke comparisons,
Layer 2 isolation and weights, edits on the static base with overlays present,
cage level 0, exact overlay-payload independence, base/overlay Undo/Redo, Bake and
its Object/Sculpt undo transitions, final-overlay removal, single/multiple-layer
subdivision and Delete Higher, save/reload, previous-prototype migration, color
painting, modifier application, Remesh and Dyntopo. The actual GPU backend is
asserted. The installed panel was visually inspected on Vulkan: Layer 1 has no
slider, Layer 2 has its row slider, and Remove is disabled with Layer 1 selected.
UI syntax, installed-source equality and both revised tooltips passed in all nine
installed locale catalogs. This is not exhaustive brush or color validation.

The OpenGL synchronous-brush benchmark uses the same installed Bmax executable,
two worker threads, a cube with 96 base quads and Multires level 5 (98,304 evaluated
quads), a 90-pixel Draw brush and three input samples per stroke. Cases ran in
native/single/single/native order, with three warm-up and nine measured strokes per
block: **18 measurements per case**. Hit positions and the viewport are refreshed
outside each timed operator. Every warm-up and measured stroke is checked to
deform geometry, and final shapes match. Timing includes the brush operator and
native undo, excluding setup, redraw and the subsequent explicit Object Mode flush.

| Case | Median |
| --- | ---: |
| Internal native Multires reference | 0.630 ms |
| Static Layer 1 only | 0.634 ms |

The observed difference is about 0.6%; block medians range from 0.625–0.644 ms for
the reference and 0.625–0.646 ms for the singleton. This is a small local measurement,
not an FPS result or proof of equal performance on every mesh. The reference uses
the existing native path with authoring bypassed by a no-axis Mirror modifier; it
is not a separate official Blender executable. Additional layers still pay their
authoring costs. Tiny metadata and constant-time checks remain for a singleton.

Verification scripts, scenes, logs, captures and recovery files use an external
task directory and are removed after checking. No user Blender session is closed.
No tests or compiled artifacts are added to the source repository. Implementation
and documentation are committed separately; publishing source does not replace the
installed executable or distribute a new binary package.

## Multires sculpt layer revision (2026-11-01)

Historical record: the static-base revision above supersedes its Layer 1 and Bake behavior.

Local uncommitted work on `codex/sculpt-layers-prototype`, based on
`15f7bbc89857`, now makes layers a Multires workflow. Layer 1 is created at
influence 1 when a supported Multires modifier is added and cannot be deleted.
Bake merges visible weighted contributions into Layer 1 at influence 1 while
preserving the original base. Sculpt recording supports levels 0 through the
highest level; adding subdivision and Delete Higher retain authoring contributions.
Ordinary mesh sculpting, Remesh and Dyntopo do not record layers. This replaces the
October 31 Bake behavior and highest-level-only restriction described below.

Release INSTALL completed with the existing MSVC v143 configuration:

```powershell
cmake --build D:\blender_build\octahedral_radius --config Release --target INSTALL --parallel 2 -- /p:CL_MPCount=2
```

This revision does not change DNA or shared C++ structure layouts. The full
`/t:Rebuild` required for the initial Mesh DNA change was already completed for the
October 31 build. Kernel/editor changes in this revision were recompiled and the
runtime and all nine locale catalogs installed. Build caches and dependencies were
retained.

The installed executable at
`D:\blender_build\octahedral_radius\bin\blender.exe` is timestamped
2026-11-01 00:18:36 JST, 98,009,088 bytes, with SHA-256
`BA55C44BADB6E5A96A2081357CC04EE2476EC72EF964FB341A883C6CA650FDB9`.
It matches `bin/Release/blender.exe`. Its base hash identifies the starting commit,
not a published commit containing the layer implementation.

Verification uses native Draw strokes and compares layered high/low-level edits
against a native Multires reference without authoring data. It checks immediate
Layer 1 creation, its protected lifetime, independent layers, original-base
restoration, lower-level detail propagation, exact persistent payload Undo/Redo,
Bake from Layer 1 at 0.35 to a merged Layer 1 at 1, Bake Undo/Redo in Object and
Sculpt modes, subdivision with weighted contributions, level switching without
strokes, Delete Higher, save/reload, Multires application, and layer-free Remesh
and Dyntopo. Additional checks cover level 0 cage strokes mixed with grid strokes,
their reversible Bake, Paint preserving layered geometry, geometry weights
preserving colors, and native comparisons for Simple/Linear subdivision in Sculpt
Mode. **123 checks passed on OpenGL and 123 on Vulkan**, with the actual GPU
backend asserted by each run. The high/low native stroke comparison matched the
reference geometry within 2e-5; the Simple/Linear comparisons differed by at most
1.2e-7. This is a correctness check, not a performance benchmark or exhaustive
brush/color regression.

UI Python syntax and installed translations for the revised Bake tooltip,
Multires requirement and existing panel/operator labels passed in all nine locales.
All verification scripts, scenes, logs and recovery files use an external task
directory and are removed afterward. No user Blender instance is closed. No
commit, push, Desktop-copy update or public-download update was performed.

## Native sculpt layers local build (2026-10-31)

Historical record: this executable has been superseded locally by the revision above.

The local `codex/sculpt-layers-prototype` branch now includes native Mesh and
Multires authoring, persistence, Undo/Redo, RNA and the Mesh Data panel. This is
uncommitted work based on `15f7bbc89857`, not published `main`. The earlier
backend-only milestone below is historical and has been superseded locally.

The Mesh DNA change received a complete rebuild of the executable and all project
references before installation, preserving dependencies and the build directory:

```powershell
cmake --build D:\blender_build\octahedral_radius --config Release --target blender --parallel 2 -- /t:Rebuild /p:CL_MPCount=2
cmake --build D:\blender_build\octahedral_radius --config Release --target INSTALL --parallel 2 -- /p:CL_MPCount=2
```

Later editor/UI fixes used incremental Release INSTALL with the same compiler and
parallelism. The final installed executable is
`D:\blender_build\octahedral_radius\bin\blender.exe`, timestamped
2026-10-31 23:43:14 JST, 98,003,968 bytes, with SHA-256
`D510248FF8D8F6E246CD9ED36BF2AD1FBA3DB013A51F91A2F6CA50161A11023A`.
It reports Bmax 5.2.3 LTS, base hash `15f7bbc89857` and the modified feature branch;
the base hash does not identify a committed sculpt-layer implementation.

Executable verification uses native geometry strokes and the actual requested GPU
backend: **97 checks passed on OpenGL and 97 on Vulkan**, including Bake while
Multires remains in Sculpt Mode. It checks automatic Layer 1, first-stroke undo/redo and base recovery,
independent layers, active selection, influence 0/0.5/1, partial-influence recording,
save/reload, independent mesh copies, removal, Multires persistent grids and
composition, repeated influence toggles without drift, and Multires application.
Bake checks preserve weighted geometry on ordinary mesh and Multires, restore
layers/weights through undo, reapply through redo, and verify the new Layer 1 base
after baking. The panel was inspected in the installed application: sliders are
beside each layer and Bake All Layers appears below the list; no reorder buttons
are present.

Native Manifold Boolean and Sculpt Trim, including Trim undo/redo, passed after
the shared-layout rebuild. Existing armature defaults and the installed Mesh Holes
translations also passed smoke checks. New panel/operator labels are installed in
all nine Bmax catalogs. This is not an exhaustive brush/color regression or a
performance benchmark. Verification scripts, scenes, logs and caches are external
temporary files and are removed after checking; no tests are added to the source.
No user Blender session was closed. Desktop copies and public downloads were not
updated, and no commit or push was made.

The final catalog-only operator-context correction used the existing `locales`
target followed by `cmake --install D:\blender_build\octahedral_radius --config Release`.
Installed lookups passed for panel labels and all four operator-context labels in
each of the nine catalogs; the executable hash above remained unchanged.

## Sculpt layer backend local build (2026-10-31, historical)

At the user's request, Release INSTALL completed from `D:\Bmax` on the local
`codex/sculpt-layers-prototype` branch, based on `15f7bbc89857` with uncommitted
backend and documentation changes:

```powershell
cmake --build D:\blender_build\octahedral_radius --config Release --target INSTALL --parallel 2 -- /p:CL_MPCount=2
```

The prototype adds an independent translation unit and header to `bf_blenkernel`;
it does not change DNA or pre-existing shared C++ layouts, so a forced Rebuild
was not required. The build regenerated shader targets and linked and installed
the executable successfully, retaining caches and dependencies.

The installed `D:\blender_build\octahedral_radius\bin\blender.exe` is timestamped
2026-10-31 19:20:40 JST, is 97,981,440 bytes, and reports Bmax 5.2.3 LTS,
base hash `15f7bbc89857`, and `codex/sculpt-layers-prototype (modified)`.
It matches `bin/Release/blender.exe` with SHA-256
`4B104829D9CF942234DB4BC82A832F63447F527901E580ECC71B2D3FFE4602FA`.

An inline factory-startup background smoke check passed startup, Mesh Holes,
Octahedral Radius/Axis Size defaults, 20 cm new armatures with Octahedral/In
Front/Wire display, native Manifold Boolean (14 vertices, nine faces), and two
Multires subdivisions (98 evaluated vertices). Installed Italian, Japanese,
French, and Spanish MO catalogs contain translated Mesh Holes labels and tooltips.
The initial sandbox launch was denied by Windows with `0xC0000022`; execution
outside that sandbox passed normally. Interactive viewports, GPU backends,
Sculpt Trim, and the full regression suite were not repeated for this build.

The [sculpt layer backend](sculpt_layers_design.md) is internal groundwork with
no Sculpt Mode/UI/persistence integration. Compilation does not make sculpt
layers available to users; unused backend code may be removed by the linker.
There was no commit, push, Desktop-copy update, or public-download update.
No existing Blender process was closed. No verification scene or script was
saved, and the external temporary build log was removed after inspection.

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

## UV diagnostics and Modified Edges local build (2026-10-30)

Incremental Release INSTALL completed on `codex/uv-diagnostics-modified-edges` using the documented parallelism 2 and CL_MPCount=2. No DNA or shared kernel structure layout changed. The installed `D:\blender_build\octahedral_radius\bin\blender.exe` is timestamped 2026-10-30 10:05:16 JST and reports base hash `fad8b2fa426f`; the implementation is included in source commit [`5b1bd4c7a742`](https://github.com/MaxPuliero/Bmax/commit/5b1bd4c7a742151fc546a4dfc0665e5fcac762eb) on `main`. Compilation preceded that commit, so executable metadata retains the base hash. Desktop copies and public packages were not updated.

Twenty-eight GUI cases per backend passed on OpenGL and Vulkan, with matching diagnostic pixel counts. See [the inventory](FEATURES.md#uv-diagnostics-follow-modified-edges-2026-10-30) for fixtures and limits. The build regenerated RNA with existing default-value diagnostics but completed successfully; background image analysis and both GPU processes exited normally. C++ formatting and diff checks passed. Scripts, captures, logs, preferences and recovery files from verification were external and removed; build caches, runtime dependencies and the existing Desktop session were preserved.

## Select Through Vertices and Wireframe Knife local build (2026-10-30)

Release INSTALL completed on `codex/select-through-wireframe-knife` with parallelism 2 and CL_MPCount=2. The active CMake cache uses source `D:\blender_prj`; the earlier `D:\Bmax` build record above describes its separate setup. Adding a flag in the existing `View3D::flag2` field did not change DNA sizes/offsets or shared structure layouts, so a forced Rebuild was not required; the common-header dependency nevertheless caused a broad compilation. The installed `D:\blender_build\octahedral_radius\bin\blender.exe` is timestamped 2026-10-30 11:03:01 JST and reports base hash `54bc53feb418`. The implementation is included in source commit [`ec3e80c36fac`](https://github.com/MaxPuliero/Bmax/commit/ec3e80c36fac9033e6dc35851d72adb700b64cd1) on `main`; compilation preceded that commit, so executable metadata retains the base hash. Desktop copies and public downloads have not been updated.

Sixty-eight interactive checks passed on each actual backend, OpenGL and Vulkan, with matching results, plus three separate background old-file/default and option reload checks. See [the inventory](FEATURES.md#select-through-vertices-and-wireframe-knife-defaults-2026-10-30) for cases and limits. The factory-startup processes used isolated preferences, internal event simulation and external temporary files. Existing user sessions were retained. C++ formatting of changed code, Python UI/keymap syntax and diff whitespace checks passed. Temporary scripts, scenes, captures, logs, preferences and recovery files were removed; compilation caches and runtime dependencies were retained.

## Bmax localization catalog installation (2026-10-30)

The `locales` target built the updated Japanese, French, Spanish, Simplified Chinese, German, Russian, Portuguese/Portuguese-Brazil and Italian MO catalogs. `cmake --install D:\blender_build\octahedral_radius --config Release` installed them and the Geometry Data Python panel with explicit translation markers. No production C++ source changed for this work; the previously verified executable remains timestamped 2026-10-30 11:03:01 JST, reporting base hash `54bc53feb418`. Build caches and runtime dependencies were retained.

Coverage and preservation checks passed for nine PO files, 1,431 installed MO keys and 5,724 runtime translation API lookups. OpenGL GUI language switching and real menu/panel draw checks completed in all nine locales. See [localization notes](localization.md) for counts, procedure and scope, including standard label elision at narrow widths. The implementation is included in source commit [`1d0507ec174c`](https://github.com/MaxPuliero/Bmax/commit/1d0507ec174c14d8814a9ad5cde9e7b66aae9a23) on `main`; Desktop copies and public packages have not been updated. External scripts, captures, expected values, audit data, logs, isolated preferences and recovery files were removed after checking.

## Unified Select Through local build (2026-10-30)

Release INSTALL completed on `codex/select-through-elements` with the documented parallelism 2 and CL_MPCount=2. The existing saved flag and RNA identifier are retained while vertex, edge and face selection share one visible Select Through toggle. No DNA or shared C++ structure layout changed. The common generated dependencies nevertheless required a broad incremental compilation. The installed `D:\blender_build\octahedral_radius\bin\blender.exe` is timestamped 2026-10-30 12:55:13 JST and reports base hash `97e896e6079d`. The earlier localization work and the new label/tooltip translations are included in the runtime files; the implementation is included in source commit [`1d0507ec174c`](https://github.com/MaxPuliero/Bmax/commit/1d0507ec174c14d8814a9ad5cde9e7b66aae9a23) on `main`.

474 checks passed on each active backend, OpenGL and Vulkan, with matching results. Coverage includes all seven mesh selection mode combinations, Solid/Material Preview/EEVEE Rendered, Cycles CPU Rendered, click/Box/Lasso/Circle, subtraction, hidden geometry, multi-object and perspective selection, face-selection Undo/Redo, persistence and localized labels/tooltips. A previous-executable file retained the enabled flag in ten saved 3D views. The two new messages passed all 18 installed-catalog lookups. See [the inventory](FEATURES.md#unified-select-through-for-mesh-elements-local-change-2026-10-30) for detailed scope and harness notes. Python syntax, changed C++ formatting and diff checks passed. Verification files were external and removed; build caches, dependencies and the user's Desktop process were retained. Desktop copies and public downloads were not updated.

## Interactive Scale local build verified on 2026-10-30

The source checkout used for this build is `D:\blender_prj`, on the local `codex/clamp-viewport-scaling` branch. Release INSTALL completed with parallelism 2 and CL_MPCount=2, including the nine locale catalogs. No DNA or shared C++ structure layout changed. The final installed `D:\blender_build\octahedral_radius\bin\blender.exe` has timestamp 2026-10-30 15:32:28 JST and reports base hash `c3815376a5cf` with the modified feature branch. It is byte-identical to `bin/Release/blender.exe`. The implementation is included in source commit [`1dcd95fe7eb0`](https://github.com/MaxPuliero/Bmax/commit/1dcd95fe7eb06988cb127f1de2d49f198f5c9fcb) on `main`; compilation preceded the implementation commit, so the executable metadata identifies the base revision and build-time branch. The user confirmed the feature.

Native simulated-event checks passed 195 assertions on each backend, OpenGL and Vulkan, plus 21 focused OpenGL assertions for constraints, directional gizmo input, snapping, proportional and multi-object editing. The modal header was visually inspected, 27 installed translations passed lookup/placeholder checks, and factory-startup background smoke checks passed. See the [feature inventory](FEATURES.md#interactive-scale-zero-clamp-2026-10-30) for coverage and limitations. Temporary verification files and logs were external and removed after checking. Existing user sessions, compilation caches, dependencies, Desktop copies and public downloads were preserved.
