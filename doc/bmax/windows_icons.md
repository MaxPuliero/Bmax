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

## Initial portable taskbar attempt (2026-11-04; insufficient)

The user's running portable window had a class icon but no
`PKEY_AppUserModel_RelaunchIconResource`; its taskbar button showed the generic
application image. The local correction supplies the current executable path with
icon index 0 explicitly. The embedded artwork and ICO files are unchanged.
Executable and launcher paths now use wide Win32 APIs and quoted Unicode commands.
If the adjacent launcher is missing, relaunch uses the executable itself.

Native property-store reads confirmed the previously absent value and the corrected
resource path on the installed runtime and on a temporary Japanese/space-containing
portable alias. Native ExtractIconExW returned valid large and small icon handles
from both paths. This verifies taskbar metadata; cached pre-existing pinned shortcuts
were not reset. The correction is built locally, uncommitted and unpublished;
existing Desktop packages remain separate. The user subsequently confirmed that the
visible taskbar icon was still generic until file registration. These property and
extraction checks were insufficient to establish a visual fix. See the current [build record](BUILD_WINDOWS.md#portable-taskbar-icon-and-bone-name-mode-visibility-2026-11-04).

## Separate portable identity from file association (2026-11-04)

Registering Bmax also updates implicit taskbar shortcuts and a ProgID DefaultIcon,
which explains why the file-registration action can affect the taskbar image.
All copies previously reused `bmax.5.2`, allowing an old same-version shortcut to
supply another installation's metadata. The follow-up uses a stable, case-normalized
executable-path hash appended to the taskbar AppUserModelID. Process, windows and
recent-document entries use that same per-copy identity. Reopening the same path
keeps its identity; a different portable folder gets an independent identity.

File registration remains an explicit preference action. Startup writes no ProgID,
`.blend` association, Open With registration, thumbnail handler or existing pinned
shortcut. The existing file-handler ProgID stays unchanged, so users may keep a
different Blender/Bmax version associated with `.blend` files.

Relaunch metadata is supplied before publishing the window identity. The icon
reference uses `blender.exe,-1`, addressing numeric icon group 1, which already
contains the Bmax artwork. Microsoft documents a negative resource ID for executable
[RelaunchIconResource references](https://learn.microsoft.com/en-us/windows/win32/properties/props-system-appusermodel-relaunchiconresource).
The prior `,0` check only proved index-based extraction worked; it did not verify
this documented property format. Existing embedded artwork remains unchanged.

The follow-up runtime passed native checks from both the installed path and an
independent Japanese/space-containing portable alias. Each process/window used the
expected path-specific identity, neither identity had an HKCU/HKLM registration,
and `SHDefExtractIconW` returned valid 32/16-pixel Bmax icon handles for resource -1.
Registry snapshots confirmed that file associations and the registered launch
command/icon were unchanged. The user accepted the follow-up and requested source
publication on November 4. The automated checks did not capture the actual taskbar
image. Implementation: [`c6b2d4b917ab`](https://github.com/MaxPuliero/Bmax/commit/c6b2d4b917ab8884ba72c332a55ee28755bfc474). Source and binary status are in
the [follow-up build record](BUILD_WINDOWS.md#portable-taskbar-identity-independent-of-registration-2026-11-04-follow-up).
