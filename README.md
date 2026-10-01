# Bmax

Bmax is a custom Blender 5.2.2 build focused on clearer armature visualization and a more convenient rigging interface. Bone evaluation, constraints, and animation mathematics retain Blender's existing behavior.

## Features

### Absolute Octahedral Radius

Each bone has an **Octahedral Radius** control in **3D Viewport > N sidebar > Item**, available in Edit and Pose Mode. One absolute radius controls the octahedral body's half-width and both endpoint spheres, independently of rest-bone length. The default for newly created bones is 0.02 Blender units (2 cm with standard metric units). In Pose Mode, evaluated pose scaling and shear remain visible, including nonuniform scaling.

![Octahedral Radius demonstration](doc/bmax/media/bmax_radius.webp)

### Independent Axis Size

**Axis Size** appears directly below Octahedral Radius. It controls the display axes independently of bone radius and length. The default is **0.03 Blender units**, or **3 cm** with standard metric units. The object's scale still affects viewport display.

![Axis Size demonstration](doc/bmax/media/bmax_axis.webp)

### Names and axes for selected bones

The armature's **Names** and **Axes** toggles display these overlays only for selected bones. In Edit Mode, selecting a head or tail also qualifies the bone. Existing master toggles still enable or disable each overlay.

![Selected bone names demonstration](doc/bmax/media/bmax_name.webp)

### Hide/unhide synchronization

Bone hide state transfers between Pose and Edit Mode when switching modes. Hidden bones are deselected during transfer. Bone collection visibility remains a separate visibility filter.

### Mode pie gesture

For the mode pie opened with **Tab for Pie Menu**, tapping and releasing Tab allows selection by moving past the outer edge of an enabled menu item without clicking. This adjustment is scoped to the mode pie opened with Tab.

### Branding and Windows executable

Bmax includes a custom splash logo and Windows icons. The executable is deliberately named **blender.exe**, with **blender-launcher.exe** as its launcher. It uses the existing Blender 5.2 preferences and startup configuration.

## Included addon

The corrected **Max Puliero Pie Menu List 1.0.1** is included in [extras/addons/MaxPuliero_Pies_5_1](extras/addons/MaxPuliero_Pies_5_1). It adds registration guards and shortcut deduplication, improves unregister cleanup, removes a duplicate selection operator, and gives the Multires decrease operator a unique identifier. Existing shortcut combinations and PRESS bindings are retained. Install the folder as a legacy addon, or ZIP that folder before installing it through Preferences.

The intermittent custom-pie input issue is still under investigation; no definitive cause has been established for the observed executable-name differences.

## File compatibility

Bmax uses Blender's `.blend` format. Its custom Octahedral Radius and Axis Size values are stored in Bmax files. Official Blender does not expose these properties and may discard them when re-saving a file. Standard rigging and animation data continue to use Blender's existing structures.

## Build and source

This repository publishes a source snapshot based on Blender **v5.2.2**, upstream commit **d13f752e3b9c4f8c261cda552b1021f8bcc0382c**, plus the Bmax changes. It does not include Blender's complete upstream Git history. Bmax development is published on a dedicated `codex/bmax-publication` branch; `main` points to the published version.

Use [Blender's Windows build instructions](https://developer.blender.org/docs/handbook/building_blender/windows/). Git LFS is required: `.lfsconfig` points to Blender's public LFS server for inherited assets. Bmax-owned artwork and the animated demonstrations are stored directly in this repository. Dependency submodules retain their upstream Blender URLs. Precompiled libraries and local build output are not committed.

The Windows Release build has compiled successfully using Visual Studio 2022 / MSVC v143. The current local build omits precompiled CUDA, HIP, and oneAPI kernels. This publication contains source code rather than a packaged Windows binary.

The demonstrations are animated WebP files resized to **35%** of their original dimensions (448 × 336), retaining frame timing and looping.

## Planned work

- Relationship lines restricted to selected bones.
- Default newly created bone length of 0.2 Blender units (20 cm in standard metric scenes).
- Reliable selection outlines for coincident bones/objects, after further viewport investigation.

These items are not implemented in this published version.

## License and attribution

Bmax is derived from Blender, developed by the Blender Foundation and its contributors. Blender is licensed under the **GNU General Public License, version 3**; see [COPYING](COPYING) and the existing source-file notices for applicable terms. Bundled third-party components retain their respective licenses. Addon author credits remain in its source files.

This is an independent custom build. See the [original Blender README](doc/bmax/README_BLENDER_UPSTREAM.md) for upstream project links.
