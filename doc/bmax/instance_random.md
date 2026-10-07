# Instance source random colors

## Controls

Viewport Shading > Color exposes **Instances** when Object or Wireframe color is Random. The two color modes share one viewport setting. It is disabled by default and does not change selection/active highlights. It is a setting of each View3D shading configuration, including scene Workbench shading, rather than a global user preference affecting unrelated viewports.

The shader **Object Info** node has an independent **Instances** checkbox. It changes only Random; Location, Color, Alpha, Object Index, and Material Index retain their existing behavior. Different nodes in the same material can use either random interpretation simultaneously. The shader path supports EEVEE and Cycles SVM/OSL. MaterialX retains Blender's existing limited Object Info translation.

## Source identity

Objects sharing their original data-block, such as Alt+D linked duplicates, share the new random value. Ordinary independent duplicates retain different values. Object and collection instances resolve the same original geometry source, including references from Geometry Nodes. Different members of a collection are grouped by their geometry data rather than by the collection container.

Anonymous generated Geometry Nodes prototypes cannot be identified by their common "Mesh" names. A temporary map keyed by the actual evaluated geometry ID assigns a prototype ordinal within the generator, combined with the generator's original data identity. Repeated and nested copies of the same prototype share its ordinal. Coordinates, transforms and individual placement IDs do not enter the source hash. Generated prototype colors can change when the set or traversal order of prototypes changes; the ordinal is not a persistent asset identifier.

Realize Instances produces ordinary geometry and does not preserve this grouping as an attribute. No geometry attributes are added or baked by this feature.

## Implementation and cost

The shared source resolver lives with Blender's dupli generation in BKE. `DupliObject` carries a second random ID while preserving the existing per-instance `random_id`. The source map is temporary and grows with the number of distinct generated prototypes, not the number of vertices. Geometry is not copied or scanned to calculate identity.

Draw ObjectInfos reuses an existing padding float for the source random value. The viewport setting chooses which value is used for wire colors; Workbench selects the source hue only when enabled. EEVEE's node selects between both values without changing other Object Info outputs. Cycles stores both values per object and chooses the corresponding SVM opcode or OSL attribute per node.

The viewport option uses an unused existing shading flag bit; the node uses an unused bit in `custom1`. Neither changes the saved DNA structure layout or adds node sockets. Existing files default to the original behavior. Official Blender ignores the Bmax interpretation of those bits and uses its existing Random behavior; no new custom node type is required.

## Source and runtime status

Implementation commit: `f68b422571e9`, integrated into `main`. The local Release runtime was fully rebuilt and installed before the source commit; it reports parent hash `4e6f1c34a32a`. Separately copied Desktop packages and the public Windows download have not been updated.

## Validation on 2026-10-07

170 background assertions passed using 17 visible samples covering Alt+D, independent data, collection instances, Geometry Nodes object sources, two distinct generated prototypes, and nested instances. Cycles CPU SVM and OSL and EEVEE agreed on grouping. Transform updates preserved source values. Two nodes with different checkbox states worked in one material, and Object Info Color was unchanged. Both viewport and node settings survived save/reload.

OpenGL and Vulkan Solid and Wireframe captures grouped the same sources and distinguished independent sources. Legacy OpenGL captures were pixel-identical with the setting disabled; wire grouping comparisons allowed antialiasing intensity differences while checking hue. The visible viewport and node checkbox layouts were inspected. Manifold Boolean and actual Sculpt Box Trim passed; Vulkan used `--debug-gpu`, disabling persistent pipeline cache. Official Blender 5.2.2 (`d13f752e3b9c`) opened the saved enabled fixture and rendered its 17 samples with native legacy Random values.

## Instance evaluation timings

Previous local build (`e25830f92dc9`, before this feature) versus the new local build (reported parent hash `4e6f1c34a32a`), using 20,000 Geometry Nodes instances. Each case used one warm-up followed by five measured runs, serially, with median wall-clock time for a host transform update plus iteration of `depsgraph.object_instances`. These timings measure instance evaluation/enumeration, not viewport FPS or render time. Source geometry was created once before timing; instances were not realized.

| Prototype | Previous build | New build |
| --- | ---: | ---: |
| Referenced quad, 4 vertices / 1 face | 5.169 ms | 5.293 ms |
| Referenced grid, 641,601 vertices / 640,000 faces | 5.135 ms | 5.139 ms |
| Generated GN grid, 640,000 vertices / 638,401 faces | 3.524 ms | 3.459 ms |

The differences are small relative to run-to-run variation and show no cost scaling with prototype vertex count in these cases. The source ID is prepared regardless of the checkbox, so these compare the feature's shared bookkeeping against the old build, rather than checkbox-on versus checkbox-off. The previously supplied Desktop `test.blend` was no longer present, so the heavy cases used temporary generated geometry. Temporary test scripts, scenes, captures, logs and benchmark outputs were removed after validation.
