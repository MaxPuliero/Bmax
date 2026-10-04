# Mesh Holes viewport overlay

The option is **enabled by default**, including viewports saved before the option existed. Disabling it is saved with the viewport.

Enable **3D Viewport > Overlays > Objects > Mesh Holes** with **Outline Selected**. The overlay draws edges incident to exactly one face on selected meshes in Object Mode, using the selection color and half the theme outline width in physical pixels. It excludes loose edges, shared edges, and edges with three or more face users. Hidden faces still count towards topology, so hiding a face does not create a false opening along its shared edges.

Visible boundaries respect scene depth. X-Ray and In Front objects show their boundaries without scene occlusion. World clipping is supported. The ordinary outer selection outline continues to be drawn.

The compact boundary index buffer is built on demand and uses the existing mesh cache invalidation. Ordinary evaluated meshes and subdivision meshes use the same boundary rule. The GPU expands each boundary segment into two triangles with coverage antialiasing when Smooth Wires is enabled or the stroke is narrower than one pixel.

## Reproduce the viewport comparison

```sh
blender --factory-startup --python tests/python/overlay/mesh_holes_demo.py
```

The selected open box has a front opening and a backing face in the same mesh. Its opening is invisible to the ordinary object-ID outline; Mesh Holes adds a thin outline around it. The neighboring selected closed cube receives no additional lines.

## Automated checks

Configure with `WITH_GTESTS=ON`, `WITH_GPU_DRAW_TESTS=ON`, and `WITH_GPU_BACKEND_TESTS=ON`, then build `blender` and `blender_test`.

```sh
blender_test --gtest_filter="*mesh_boundary_edges*:*mesh_holes_shader*"
```

These tests cover shared edges independently of winding, hidden edges and faces, closed geometry, non-manifold edges, loose-only geometry, subdivision boundaries, and creation of the regular and clipped shaders.

## Verified Windows build (2026-10-05)

MSVC 19.44, Ninja, optimized Release (`/O2 /Ob2 /DNDEBUG`), AMD Radeon 8060S. All six boundary and shader tests passed on OpenGL and Vulkan. The GUI viewport was captured on both backends: ON/OFF, X-Ray, occlusion, In Front, subdivision, deselection, Smooth Wires disabled, the overlay panel, and a dense perforated mesh. The ON/OFF image difference was confined to the front opening; the closed cube was unchanged. Factory-startup viewports and the RNA default both reported ON.

Cached redraw measurements with VSync disabled (median of three 80-redraw runs per state, alternating ON/OFF after ten warm-up redraws):

| Backend | Scene | OFF (ms) | ON (ms) | Difference (ms) |
| --- | --- | ---: | ---: | ---: |
| OpenGL | Open and closed boxes | 1.798 | 1.827 | +0.030 |
| OpenGL | 4,489 openings | 1.888 | 1.968 | +0.080 |
| Vulkan | Open and closed boxes | 2.915 | 2.944 | +0.029 |
| Vulkan | 4,489 openings | 3.014 | 2.987 | -0.028 |

The dense mesh contains 40,405 vertices and 35,512 faces. These measurements support enabling the option by default on this machine; small negative differences are measurement noise. Initial boundary extraction remains linear in mesh topology, and is repeated when the mesh cache is invalidated.

On Windows, `blender_test.exe` is in `bin/tests`; add the build's `bin` and `bin/blender.shared` directories to the test process PATH when invoking it directly.
