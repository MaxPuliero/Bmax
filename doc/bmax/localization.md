# Bmax interface localization

## Scope

The Bmax-specific interface is localized in Japanese (`ja`), French (`fr`), Spanish (`es`), Simplified Chinese (`zh_HANS`), German (`de`), Russian (`ru`), Portuguese (`pt` and `pt_BR`) and Italian (`it`). The two Portuguese variants retain their existing catalog conventions; new Brazilian strings use regional terminology where applicable. Bmax, Blender, UV, XYZ, key names and format placeholders remain intact.

The audit compares the current source with imported Blender snapshot `1d407f04`, separating Bmax interface additions from the later upstream corrective release. It covers 159 context/message keys per catalog, including reused standard labels and separate operator contexts. Existing valid upstream translations are preserved. Standard Blender controls continue to use their upstream catalogs; this update does not claim complete translation of Blender itself.

Covered Bmax text includes:

- Octahedral Radius, Axis Size, selected-bone display help, Align Master controls, descriptions and errors.
- UV shell boundaries, Overlap and Flipped diagnostics, independent opacity controls, Mesh Holes and Select Through Vertices.
- Hair Curves theme settings and the occluded selection outline theme.
- Mesh Data normal weighting, Decimate Cache Result, instance random colors and Keep in Local View.
- Sculpt Fill Holes, Split, Bevel Mask, Face Set Cut Geometry, remesh controls and relevant Multires color limitations.
- Multi-object editing help, Geometry Data Add controls, Wireframe Knife help, Bmax bug reporting, factory settings and About Bmax.

Features that introduce no additional visible text, such as selection priority and bone geometry corrections, retain the existing localized Blender controls.

## Interface extraction

The three Geometry Data Add labels now use complete literal messages inside `iface_` calls instead of concatenating an English prefix with a data name. Their buttons use `translate=False` after that explicit translation. This makes the labels extractable and prevents translating an already localized result. Operator names and tooltips retain their own translation contexts. Mesh operations and defaults are unchanged.

## Usage

Choose the language in **Preferences > Interface > Translation**. Enable **Interface**, **Tooltips** and **Reports** for the corresponding text. Existing preferences and language choices are preserved by installation; the user selects the desired language normally.

## Validation and installation (2026-10-30)

The nine PO catalogs received 1,099 missing/fuzzy context-message translations: ja 118, fr 118, es 118, zh_HANS 120, de 131, ru 120, pt 121, pt_BR 129 and it 124. Approved translations present before this change were compared and preserved unchanged. PO parsing, unique active keys, nonempty non-fuzzy coverage and printf placeholders were checked.

All 1,431 audited keys passed against installed MO files, which also matched their generated build outputs. The running installed executable passed 5,724 lookups through `pgettext`, interface, tooltip and report translation APIs across the nine locales. Disabling interface translation restored the English label. The formatted hole-fill report retained its integer substitution.

An isolated OpenGL GUI session switched through all nine locales and captured the real mesh Select menu and a temporary popup reusing the Geometry Data panel draw function. The new labels and Japanese, Chinese and Cyrillic glyphs were visually inspected. Standard Blender label elision remains in narrow panels. This was a localization check; geometry operations, all possible panel widths and Vulkan rendering were not retested. No native-speaker review is claimed.

Catalogs were built with `locales` and installed with `cmake --install`; the already verified C++ executable remains unchanged. This catalog-only procedure assumes the installed executable already matches the production C++ source:

```powershell
cmake --build D:\blender_build\octahedral_radius --config Release --target locales --parallel 2 -- /p:CL_MPCount=2
cmake --install D:\blender_build\octahedral_radius --config Release
```

Use the regular full build/INSTALL procedure when C++ changes also require compilation. The installed locale files are under `bin/5.2/datafiles/locale`, and the updated panel is under `bin/5.2/scripts/startup/bl_ui`. All verification scripts, audit data, expected translations, captures, preferences, logs and recovery files were external and removed after checking. Build caches and runtime dependencies were retained.

The implementation is included in source commit [`1d0507ec174c`](https://github.com/MaxPuliero/Bmax/commit/1d0507ec174c14d8814a9ad5cde9e7b66aae9a23) on `main`. Desktop copies and public download packages have not been updated.

## Unified Select Through follow-up

The visible toggle is now named Select Through and its tooltip includes vertices, edges and faces. Both strings have translations in the same nine catalogs. The original vertex-only translations are retained for historical/compatibility use; the saved property identifier is unchanged. The localization and selection extension are included in source commit [`1d0507ec174c`](https://github.com/MaxPuliero/Bmax/commit/1d0507ec174c14d8814a9ad5cde9e7b66aae9a23) on `main`. The two added translations in each catalog (18 total) passed installed MO checks and interface/tooltip lookups on both OpenGL and Vulkan. The shared-toggle extension passed 474 selection, state and translation checks per backend; see the current feature inventory. The executable and catalogs are installed locally.

## Interactive Scale follow-up (2026-10-30)

Source commit [`1dcd95fe7eb0`](https://github.com/MaxPuliero/Bmax/commit/1dcd95fe7eb06988cb127f1de2d49f198f5c9fcb), included in `main`, adds Allow Negative Scaling, its tooltip and its formatted On/Off header in all nine Bmax locales (27 additional translations). Existing On/Off translations are reused. The earlier audit counts describe the previous published localization revision; this extension is tracked separately in the feature inventory.

All 27 additional messages passed installed-MO lookup and printf-placeholder checks after the full Release INSTALL. The installed runtime and source branch are recorded in the [Scale inventory entry](FEATURES.md#interactive-scale-zero-clamp-2026-10-30).
