# Bmax — Blender for artists

Bmax is a custom Blender 5.2.4 LTS build with tools for sculpting, rigging, UV editing and everyday scene work.

[**Download for Windows**](https://maxpuliero.gumroad.com/l/bmax) · [**Artist guide**](doc/bmax/manual/README.md) · [**What's new**](doc/bmax/manual/whats-new.md)

## Make room for your workflow

- **Sculpt and paint:** build adjustable Multires sculpt layers, paint color detail, control brush depth and blend symmetry. [Hold Shift to select and edit Smooth](doc/bmax/manual/sculpt.md#edit-smooth-while-holding-shift) before starting a stroke.
- **Rig with clearer bones:** adjust bone width and axis size, keep selected controls readable and align bones to a master.
- **Inspect UVs:** spot overlaps and flipped faces, outline shells and adjust the texture background independently.
- **Clean up meshes:** highlight open boundaries, fill holes, separate parts and control normal weighting.
- **Work comfortably:** manage Quick Favorites, carry your view between workspaces, edit selected objects together and [type numeric property values with Shift held](doc/bmax/manual/interface.md#enter-numbers-while-shift-is-already-held), using the number row or keypad.
- **Save textures selectively:** inspect modified images in a compact scrolling list, select individual images or all images, and save each UDIM set as one image. See [Saving and compatibility](doc/bmax/manual/compatibility.md#choose-which-modified-images-to-save).

Browse the [artist guide](doc/bmax/manual/README.md) for practical instructions, images and feature limits.

## Choose which modified images to save

With **Save Modified Images: Ask**, the project-save prompt lists modified images
and lets you choose which ones to save or pack with the project.
[Read the saving guide](doc/bmax/manual/compatibility.md#choose-which-modified-images-to-save).

<img src="doc/bmax/media/modified_images_save_prompt.png" alt="Project-save prompt listing modified base color, normal and roughness images with individual save checkboxes" width="659">

## Get started

Local Windows runtimes rebuilt on November 14 include NVIDIA CUDA/OptiX and AMD HIP kernels. See the [build audit](doc/bmax/BUILD_WINDOWS.md#cuda-optix-and-hip-rebuild-524-upgrade-2026-11-14) for tested hardware and package status. Public downloads are updated separately.

Download and extract the complete Windows package, then run **blender.exe** from its folder. Keep the accompanying files together.

The guide describes the current Bmax features. Recent improvements may arrive in a later download; see [What's new](doc/bmax/manual/whats-new.md) and the download's release notes.

## Share your work

Use **File > Save as Official…** to prepare a separate copy for official Blender. Read [Saving and compatibility](doc/bmax/manual/compatibility.md) for painted Multires assets, Sculpt Layers and instance colors.

## Get help

Choose **Help > Report a Bmax Bug**, or visit the [Bmax issue tracker](https://github.com/MaxPuliero/Bmax/issues). Include your version, reproduction steps and a screenshot or small example file when possible.

## About Bmax

Bmax is an independent Blender build by Max Puliero, based on Blender by the Blender Foundation and its contributors. It is licensed under the GNU General Public License, version 3; see [COPYING](COPYING). Bundled components retain their respective licenses.

For contributors: [development documentation](doc/bmax/DEVELOPMENT.md).

### GPU support in future Windows builds

Keep NVIDIA CUDA and OptiX, AMD HIP, and precompiled GPU kernels enabled:
`WITH_CYCLES_DEVICE_CUDA=ON`, `WITH_CYCLES_DEVICE_OPTIX=ON`,
`WITH_CYCLES_DEVICE_HIP=ON`, `WITH_CYCLES_CUDA_BINARIES=ON`, and
`WITH_CYCLES_HIP_BINARIES=ON`. Prepare the matching CUDA, OptiX and HIP toolchains
before configuring. Disabling kernel packaging can prevent supported GPUs from
appearing in Cycles on computers without the compilers installed.

Distribute the complete installed runtime, including `scripts/addons_core/cycles/lib`.
Verify CUDA/OptiX device detection and GPU rendering, and check that every expected
HIP kernel is packaged. AMD hardware rendering requires a separate AMD validation.
Follow the [current Windows build instructions](doc/bmax/BUILD_WINDOWS.md#cuda-optix-and-hip-rebuild-524-upgrade-2026-11-14).
