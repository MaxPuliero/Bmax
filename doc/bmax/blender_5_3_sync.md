# Blender 5.3 integration and separate versions

**Subsequent image-save update, 2026-11-11:** Selective image saving is now committed
locally on both version branches (`e9115666` for 5.2.3, `ae98b7b4` for 5.3), with
separate verified local builds. This feature has not been pushed. See the current
[feature inventory](FEATURES.md#selective-modified-image-saving-2026-11-11) and
[build audit](BUILD_WINDOWS.md#selective-image-saving-2026-11-11). The migration
records and executable hashes below describe the earlier port, before this update.


## Source and status

Official release: `v5.3.0`, commit `02f0899d78e374b63e7181d9061e1f7ab03419cc`, released November 10, 2026. The port starts from Bmax `361ec41e` and integrates the release-to-release delta using the official 5.2.3 tree as the three-way base. Bmax keeps its imported-snapshot history; upstream Blender history is not published as Bmax history.

Release compilation and INSTALL succeeded in the separate 5.3 build tree. The new runtime passed the background, OpenGL/Vulkan and official-file checks below. Public downloads and Desktop packages have not been replaced. The existing 5.2.3 executable is unchanged.

## Version separation

| Version | Git branch | Source | Build / installed executable |
| --- | --- | --- | --- |
| Preserved Bmax 5.2.3 LTS | `codex/bmax-5.2.3` (`361ec41e`); `main` remains unchanged | `D:\blender_prj` | `D:\blender_build\octahedral_radius\bin\blender.exe` |
| Bmax 5.3.0 port | `codex/bmax-5.3` | `D:\blender_prj_5.3` | `D:\blender_build\bmax_5.3\bin\blender.exe` |

Linked worktrees share Git objects, but have separate branches, indexes, source files, Windows library checkouts, CMake caches and install prefixes. Never reconfigure the existing 5.2.3 build for 5.3. Its historical build directory name is retained. The repository's existing agent instructions describe the preserved 5.2.3 setup; this document records the new 5.3 paths and status without changing those instructions.

`origin` is the user's Bmax repository, `https://github.com/MaxPuliero/Bmax.git`. The local `upstream` remote only reads public Blender releases; its push URL is disabled. No changes are sent to Blender's repository. Both version branches are published on `origin`; `main` remains at `361ec41e`. The compiled implementation is [`998896d1`](https://github.com/MaxPuliero/Bmax/commit/998896d1d5682a005eef199bc7510dce0304f264). Later documentation commits do not change the compiled code.

## Assets and dependencies

Windows 5.3 libraries are pinned to `4d0478217fd7f53e785cdbac95de8f726f35cd55` in the new checkout. The original checkout and 5.2.3 libraries are unchanged.

Use `GIT_LFS_SKIP_SMUDGE=1`, `GIT_TERMINAL_PROMPT=0` and `GCM_INTERACTIVE=Never` for checkout preparation. Do not request Blender credentials. Required runtime assets unavailable through anonymous Git LFS were recovered from the public `blender-5.3.0.tar.xz` source archive; all twelve runtime files and the remaining installer artwork matched their exact SHA-256 in the 5.3 LFS pointers. The temporary archive was removed. Cached assets were reused.

Upstream development test changes and fixtures are excluded; inherited test files remain. No custom verification scripts or artifacts are stored in Git. GPU test-shader compilation respects `WITH_GTESTS` and `WITH_GPU_BACKEND_TESTS`; runtime shader compilation checks remain enabled.

## Build

Visual Studio 2022 / v143, MSVC 19.44.35229.0 and Windows SDK 10.0.26100.0 are available. CMake configuration and Release INSTALL succeeded. Conservative parallelism and GPU kernel omissions match the existing local 5.2.3 setup. The new optional DLSS Ray Reconstruction support remains disabled (`WITH_DLSS=OFF`), the upstream default for custom configurations. Runtime shader C++ compilation checks remain enabled.

```powershell
cmake -S D:\blender_prj_5.3 -B D:\blender_build\bmax_5.3 `
  -G "Visual Studio 17 2022" -A x64 -T v143 `
  -DWITH_GTESTS=OFF -DWITH_GPU_DRAW_TESTS=OFF -DWITH_GPU_BACKEND_TESTS=OFF `
  -DWITH_CYCLES_DEVICE_OPTIX=OFF -DWITH_CYCLES_CUDA_BINARIES=OFF `
  -DWITH_CYCLES_HIP_BINARIES=OFF -DWITH_CYCLES_ONEAPI_BINARIES=OFF `
  -DCMAKE_INSTALL_PREFIX=D:\blender_build\bmax_5.3\bin
if ($LASTEXITCODE -ne 0) { throw '5.3 configuration failed' }
cmake --build D:\blender_build\bmax_5.3 `
  --config Release --target INSTALL --parallel 2 -- /p:CL_MPCount=2
if ($LASTEXITCODE -ne 0) { throw '5.3 build failed' }
```

A clean separate build avoids linking 5.2.3 objects against changed 5.3 DNA/C++ layouts. Keep generated objects and runtime dependencies in the version's own build tree. Preserve the user's existing Blender sessions.

## Bmax API adaptations

- Preserve the existing feature delta with the 5.3 C/C++ header names, typed-list accessors and writable BMesh access.
- Keep Sculpt Layers and Multires colors compatible with the new displacement storage, which no longer stores a redundant `MDisps.level`. Validate resolution using the displacement sample count.
- Update the Multires brush cube-tip/texture APIs and preserve the source-instance random value in the new BSL material interface.
- Preserve weighted normals while respecting 5.3 GeometrySet/cage sharing and avoiding unnecessary copies of unchanged meshes.
- Use DNA-derived RNA defaults for serialized fields; EditBone retains explicit radius 0.02 and axis-size 0.03 defaults because it is not a serialized DNA type.
- Keep Decimate invalidation compatible with the standard Skin attributes replacing the removed CustomData Skin type.

## Verification and scope

- **66 background checks passed:** read a 5.2.3 Bmax file with custom bone settings, remesh flags, instance random, two Sculpt Layers and genuinely painted Multires color; compare 98 evaluated vertices, 96 faces and 384 color samples with 29 distinct colors. Initial load and 5.3 save/reload have maximum color error **0.0** against the 5.2.3 reference.
- Check 20 cm armature/Add Bone defaults at scene unit scales 1, 0.01 and 10; weighted normals with an Edit Mode subdivision cage; Manifold Boolean; multi-object Skin/custom-normal operators; Object Mode Fill Holes; and initialization of Bmax fields in a native official 5.3 file.
- Check fresh Multires Color Filter and Symmetrize Blend, finite persistent colors and unchanged geometry for a color-only filter. Check all three Decimate modes with cache on/off, invalidation after editing geometry, and Voxel Remesh preserving two disconnected parts.
- **Actual OpenGL and Vulkan checks passed:** Object/Pose viewport captures, UV overlays and four pinned UV corners, a native Manifold Box Trim with geometry restoration through undo/redo, and a finite, nonuniform 64-by-64 EEVEE render using Bmax Object Info instance random. The scripted trim check explicitly initializes and commits undo steps required by direct Python operator calls. It does not simulate mouse input. Pin-state and capture measurements match the existing 5.2.3 OpenGL reference.
- **Official Blender 5.3.0** (`02f0899d78e3`) opened both Save as Official outputs. The main copy retains native custom normals and the Multires levels; the static color backup has 98 vertices and 384 standard color samples with maximum error **0.0**. The Bmax source filename, dirty state and mode remain unchanged by conversion. The existing warning about instance-random material differences remains.
- Focused final RNA checks confirm Bone/EditBone radius 0.02 and axis size 0.03, Decimate cache enabled, Mesh Holes enabled and UV background opacity 1.0.
- Changed Python sources parse without generating caches. Manually adapted C++ files were formatted with the bundled clang-format 22.1.7. The final change passes whitespace checks except 22 dependency/patch files that are byte-identical to the official 5.3 tag; their inherited whitespace is intentionally preserved.

These are focused fixture checks, not exhaustive coverage of every Bmax tool, interactive brush gesture, selection/picking order, add-on, GPU or driver. Earlier 5.2 validation and performance records are not relabeled as 5.3 measurements. No new performance benchmark is claimed.

All verification scripts, scenes, captures, recovery files, logs and the temporary official runtime were kept outside Git and have been removed. Build caches, pinned dependencies and installed runtimes are retained. The user's existing Desktop Blender session is preserved.

## Preserved 5.2.3 runtime

`D:\blender_build\octahedral_radius\bin\blender.exe`: **98,135,552 bytes**, timestamp **2026-11-09 10:31:02 JST**, SHA-256 `412FB38DB57F96918D1CD9378BE00CAE73BF5D05908F348EFECE79DA88774ABF`. The 5.2.3 source files and executable were verified unchanged during this migration.


## Final 5.3 runtime audit

`D:\blender_build\bmax_5.3\bin\blender.exe`: **110,988,800 bytes**, timestamp **2026-11-11 11:57:23 JST**, SHA-256 `8057DE393C48AC108901EC076E9B89BC305209562C1E10678C9D67EA3528D459`.

The installed executable is byte-identical to `bin\Release\blender.exe`. It reports **Bmax 5.3.0**, build hash **998896d1d568**, branch **codex/bmax-5.3**, without a modified-source marker. Final startup and RNA defaults passed after installation from the clean implementation commit. Both version branches were checked against the remote; the preserved 5.2.3 branch and `main` still identify `361ec41e`.
