# Building Bmax on Windows

## Branch and source

`main` is the stable Bmax branch and includes Mesh Holes, armature display controls, UV diagnostics, origin axes, and modifier defaults. Use a temporary feature branch for a new change, then integrate it into `main` when ready.

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
  --config Release --target INSTALL --parallel 8
if ($LASTEXITCODE -ne 0) { throw 'Bmax build failed' }
```

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

On 2026-10-05, source commit `5b8675ae7948` on `main` compiled and installed successfully as **Bmax 5.2.2 LTS**, Windows Release, using Visual Studio 2022. Background startup confirmed the matching source hash, Mesh Holes enabled by default, Octahedral Radius `0.02`, and Axis Size `0.03`. No persistent test files were created, and the temporary build log was removed.
