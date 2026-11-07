# Gumroad newsletter draft — November 7, 2026

Subject: Coming to Bmax: stroke amplification, Blend Symmetry and more Multires color tools

Hi everyone,

The next Bmax sculpting update is taking shape, with several additions now published in the source code:

- **Repeat Last Stroke:** press **1** to add the original stroke displacement again. Repeated presses give you 2×, 3×, 4× the original effect. It works on regular meshes and Multires; Dyntopo repetition keeps the current topology fixed.
- **Blend Symmetry:** blend both sides of your sculpt while keeping masked areas protected. Unmasked counterparts meet at their average, while a fully masked side becomes the reference. Matching regions are processed even when other parts have no usable counterpart. Find it in **Sculpt > Symmetrize Blend** or **N > Tool > Blend Symmetry**.
- **Color symmetry:** Vertex Paint is enabled by default in Blend Symmetry, so the active color attribute can follow the same mask-aware blend. Both regular meshes and Multires color are supported.
- **Multires Color Filter:** Fill, Hue, Saturation, Value, Brightness, Contrast, Red, Green, Blue and Smooth now work with Multires color. Editing at a lower level retains the higher-resolution color detail through the existing delta propagation path.
- **Multires color picking fix:** Shift+X now reads the correct grid color after subdivision and Sculpt level changes, instead of treating a grid sample as a base-mesh vertex.

The tools have been checked with native Undo/Redo on OpenGL and Vulkan. The generic color-dialog eyedropper still samples the displayed image, including shading; use Shift+X to sample the painted attribute.

**Availability:** this is a source update. The local Windows builds are verified, but the Gumroad download package has not been replaced as part of this announcement. I'll announce the packaged Windows update separately when it is available.

Source and feature details: https://github.com/MaxPuliero/Bmax

Thanks for supporting Bmax and for reporting the issues you find!

Max
