# Windows icon alpha and DPI sizes

Source implementation: `5c0d7e11`, 2026-10-07. The application icon and blend-file icon are `release/windows/icons/winblender.ico` and `winblenderfile.ico`. Both are referenced by `winblender.rc` and embedded in the Windows executable resources.

## Report and diagnosis

The reported Explorer Details view showed a smooth large Bmax preview but jagged small icon edges. The old ICO resources contained seven PNG-only entries: 16, 24, 32, 48, 64, 128 and 256 pixels. Their decoded pixels already included antialiased partial alpha values. Native Win32 rendering at an exact stored size matched their expected alpha composition, so the source artwork did not lack antialiasing. Rendering at a missing intermediate size required Windows to select and rescale another entry; the native comparison at 96 pixels showed less smooth edges than an exact-size frame.

The particular Explorer Details-pane rendering/cache path was not automated. The resource update addresses both size selection and compatibility: exact intermediate sizes plus conventional bitmap/alpha/mask payloads for small icons. Existing cached icons and older separately copied executables do not change when the source or installed local executable is updated.

## Encoding

The existing 1024 x 1024 `release/datafiles/bmax_logo.png` artwork is unchanged. Each icon frame is resized directly from that image with an alpha-aware Lanczos filter; RGB is zeroed only where alpha is exactly zero. Partial alpha values around the edge remain intact.

Both ICO resources contain these native square sizes:

`16, 20, 24, 32, 40, 48, 56, 64, 72, 80, 96, 128, 144, 160, 192, 256`.

Entries up to 96 pixels use a 32-bit bottom-up BGRA DIB, double-height bitmap header and a DWORD-aligned one-bit AND mask. The mask is transparent exactly where alpha is zero. Entries of 128 pixels and above use PNG payloads to keep larger previews compact. The ICO directory records one plane and 32-bit color depth. Both files have identical contents and are 202,115 bytes each.

[Microsoft's DrawIconEx documentation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-drawiconex) describes alpha composition for 32-bit icons and the separate legacy mask path. The resource includes both alpha and masks without baking in a background color.

## Validation

- All 16 encoded frames round-trip to their intended RGBA pixels exactly.
- Every bit of every DIB AND mask matches its frame's zero-alpha pixels.
- Native PrivateExtractIconsW/CreateIconFromResourceEx and DrawIconEx checks covered exact and intermediate sizes. The compiled executable's extracted icons matched expected alpha composition on dark (32,32,32) and white (255,255,255) backgrounds at all 16 sizes, with at most one channel level of rounding difference.
- Magnified native render comparisons were visually inspected. No logo shape or branding color was redesigned.
- The Windows Release build and installation retain the previous C++ mesh-tools changes. The source update does not alter Blender's geometry or GPU drawing algorithms.

All verification scripts, extracted frames, native render comparisons and build logs were temporary, outside the repository, and removed afterward. Required build caches and runtime files were retained. The user's existing Blender session, scan file, Desktop package and public download were preserved.
