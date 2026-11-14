# Blender 5.2.4 integration (2026-11-14)

## Source

Official tag: `v5.2.4`, commit `cd5dc428cce1993a4b35986f227c6a24e42a365c`, released November 10, 2026.
Base: `v5.2.3`, commit `e13b728c6880d3084d9d57c30031ed5df8b39dee`.
Bmax starts from `48ae2926b1da579857cf54e3cd11b3146017240d` on the new local
`codex/bmax-5.2.4` branch. The existing `codex/bmax-5.2.3` branch is preserved.
No commit or push was requested for this rebuild.

The release delta changes 56 paths. The production import applies 46 files using
three-way application, excludes the changed inherited C++ test and five new test
fixtures, and handles Windows libraries separately. Other platforms' library pins
remain unchanged. The Windows x64 dependency checkout is updated to
`578ab7d2b549385552fa4c9145dfed2b7d30bdae`, matching the official release.
It updates Brotli, pip and certifi. The 5.3 dependency checkout is separate.

All 41 imported production files without Bmax changes match the official tag
byte-for-byte after newline normalization. The five shared files merge cleanly:
`properties_paint_common.py`, `armature_add.cc`, `interface_handlers.cc`,
`sculpt_ops.cc`, and `rna_object.cc`. Comparing added/removed lines against the
old and new upstream bases confirms that their complete preexisting Bmax delta
is retained. Local documentation and the existing untracked screenshot are preserved.

## Build and verification

Release INSTALL succeeded in the retained `D:\blender_build\bmax5.2.3` cache,
with CUDA, OptiX and HIP kernels included. The installed runtime reports 5.2.4 LTS.
No API adaptation of Bmax was necessary, and no DNA/shared C++ structure header
changed. All five shared files retain their full preexisting Bmax delta.

The 5.2.3 fixture preserves custom bone settings, all 98 evaluated Multires vertices
and 384 standard interpolated color samples exactly. Armature units/defaults,
Mesh Holes, compatibility-snapshot RNA, an actual Manifold Boolean and the updated
pip/certifi packages pass. CUDA and OptiX each detect the RTX 4070 Ti SUPER and render
successfully with CPU excluded; OptiX denoising also passes. All 35 packaged GPU
kernels and the installed executable match their build outputs by SHA-256.

The test fixture does not exercise custom painted-grid strokes. The optional
synthetic Shift UI harness failed on the unchanged 5.2.3 reference as well, so it
provides no new UI validation result. AMD hardware is unavailable; HIP compilation
and packaging are verified, without a hardware rendering claim.

See [Windows build audit](BUILD_WINDOWS.md#cuda-optix-and-hip-rebuild-524-upgrade-2026-11-14)
for commands, toolchains, executable hash and scope. The obsolete 59 bundled Python
files were verified against the old library commit and removed after INSTALL.
Verification artifacts were external to Git and removed after checking. Desktop/public packages remain unchanged;
source and documentation changes are local and uncommitted.

Official references: [release](https://www.blender.org/releases/5-2/),
[tag](https://projects.blender.org/blender/blender/src/tag/v5.2.4).
