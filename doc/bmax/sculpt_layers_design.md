# Native sculpt layers: implementation and research

Updated: 2026-11-01 (JST).

Status: **experimental native Multires layer integration implemented** in source
commit [`af594efca925`](https://github.com/MaxPuliero/Bmax/commit/af594efca9257511279692edd73f24b2f9084832), included in `main` and developed on `codex/sculpt-layers-prototype`. The current
implementation is described first. The research and numerical prototype below
are historical milestones and future design references, not a feature checklist.

Source audited: `D:\Bmax`, branch `main`, commit
`15f7bbc898576577f723bf6b0345649614cf4c7c`. The initial audit did not change an
executable. Subsequent user-requested implementation rebuilt and installed the
local runtime, as recorded in the build guide.

## Current integration: static Layer 1 (2026-11-01)

Layer 1 is now a static sculpt base at full intensity, with no slider and a
read-only Influence RNA property. Other layers retain individual weights. Layer 1
cannot be removed. Bake incorporates the visible weighted result into this static
base and removes other layers; it no longer retains the original unsculpted mesh
behind a first-layer slider. Undo restores the pre-Bake authoring state.

### Native single-layer path

A persisted first-layer UID of zero identifies the static base. When this is the
only layer, Mesh/MDisps are authoritative: no authoring positions, grid payloads or
base snapshot are allocated. The existing Mesh DNA layout is unchanged. Metadata
and a constant-time predicate remain, but stroke start/end skip topology scans,
layer delta extraction, extra native flushes and layer undo archives. The ordinary
native position undo is used. Single-layer subdivision runs the native operation
once without temporary field copies; Delete Higher only updates metadata.

The first additional layer captures current cage positions and MDisps once. That
snapshot becomes the static base beneath independent additional contributions.
Removing the final overlay or baking keeps the resulting native geometry, releases
the snapshot/payloads and resumes the singleton path. Explicit layer commands can
still use full geometry undo snapshots; that cost is not per brush event.

### Multiple-layer authoring

`BKE_sculpt_layers_mesh.hh` and `intern/sculpt_layers_mesh.cc` implement lifecycle,
composition and recording, separately from the historical numerical prototype.
Mesh DNA holds metadata and optional authoring buffers. Normal no-main evaluated
copies contain composed geometry without deep-copying authoring data.

Cage deltas support Sculpt level 0. Grid deltas use native Multires tangent-space
displacement. At stroke completion, native reshape transports edits to retained
high resolution. The adapter compares persistent displacement with the prior
composition and assigns the difference to the active overlay. If Layer 1 is active,
the difference updates the static base itself; it does not create a first-layer
payload. Undo edit UID -1 identifies these base edits, positive IDs identify
secondary layers, and zero means no authoring edit. Changed grids or vertices are
swapped exactly during authoring Undo/Redo and the evaluated view is rebuilt.
Current additional-layer weights are composed afterward.

Native full-surface reshape and the grid comparison occur once per completed
multiple-layer stroke. Base snapshots, geometry-command undo and subdivision can
still process/copy full surfaces. Native CustomData COW is unchanged. Per-layer
buffers and local undo do not establish a production performance guarantee.

With multiple layers, subdivision transforms the static base and each populated
overlay using native subdivision on temporary no-main meshes. An overlay is the
difference between subdivided base-plus-overlay and subdivided base. Catmull-Clark,
Simple and Linear use the requested mode. Delete Higher downsamples the static
base and overlays by native retained sample spacing. Merely changing displayed
levels leaves authoring resolution intact. Single-layer operations bypass this
extra representation entirely.

### Persistence and previous prototypes

UID zero is reserved for static Layer 1 without changing structure sizes/offsets.
On reading prior prototype files, the old first-layer contribution multiplied by
its stored influence is folded into the saved base, preserving the cached visible
mesh. Its buffers are then freed and its influence fixed to 1. If it is the only
layer, redundant base buffers are also released. For example, an old first layer
at 0.35 keeps that visible shape as the new static base. The old unweighted hidden
first-layer shape is not exposed as a slider in this workflow.

Authoring supports fixed base topology, single-user editable meshes without shape
keys, packed displacement, Sculpt Base Mesh disabled and no other enabled modifier.
Ordinary mesh strokes, Remesh and Dyntopo have no recording. Applying/removing
Multires and replacing topology discard authoring data. Colors remain separate.
Animation, per-layer masks, locks, visibility toggles and reordering are absent.
Arbitrary topology edits and other geometry tools are outside validated scope.

The [current build record](BUILD_WINDOWS.md#static-layer-1-and-native-single-layer-path-2026-11-01)
includes functional validation, migration and the limited synchronous native brush
benchmark. The earlier October 31 and early November 1 semantics are superseded:
Layer 1 is no longer a reversible displacement layer. The numerical prototype and
research below remain historical and do not establish current interface behavior.

## Historical first implementation milestone (2026-10-31)

The local `codex/sculpt-layers-prototype` branch adds an experimental backend in
`source/blender/blenkernel/BKE_sculpt_layers.hh` and
`source/blender/blenkernel/intern/sculpt_layers.cc`, registered in the existing
`bf_blenkernel` target. This is preparatory work for stage A, not completion of
that stage. No DNA, existing shared C++ layout, or editor was changed. The initial
backend verification did not install a runtime; the subsequent user-requested
full build is recorded below. No commit or publication was performed.

The backend implements:

- Stable layer IDs, names, duplication/removal, strength 0–1, visibility, and lock.
- Sparse grid payloads, with the authoritative base provided by the caller rather
  than copied into each layer. Empty layers allocate no sample arrays.
- Deterministic additive composition, including output/base aliasing and rejection
  of non-finite inputs or arithmetic overflow without partially writing output.
- Stroke transactions which freeze metadata, stage net grid deltas, replace
  repeated previews rather than accumulate them, and cancel without altering
  persistent layer buffers. Recording requires a visible unlocked layer at weight 1.
- Local commit and move-only undo/redo records which exchange grid ownership,
  including removal/restoration of previously absent or entirely zero grids.
- Domain/revision checks and rejection of cross-stack or out-of-order undo.
  The adapter still owns history ordering and disposal of abandoned redo branches.

Operations on one stack are serial. Inputs are vectors already expressed in the
storage basis; the backend does not invent or evaluate Multires tangent frames.
The domain revision is supplied by the caller, not computed from Blender topology.
Composition currently allocates a temporary buffer for one grid to guarantee
alias/error behavior; scratch-buffer reuse is a future measured optimization.

The next integration work is still necessary: a CCG/PBVH adapter collecting actual
post-constraint/post-seam changes, basis conversion, DNA persistence and lifecycle,
native sculpt/global undo interleaving, a layer-aware Multires flush/evaluator,
level handling, guarded operators, and UI. This backend cannot currently be used
to sculpt, save layers to a `.blend`, or manipulate layers from Python/RNA.

### Verification actually completed

MSVC v143 C++20 compilation passed with `/W4 /WX` in standalone Debug and optimized
Release harnesses. Each run passed **309,992 checks**, mostly numerical comparisons
against an independent dense oracle after 150 seeded strokes across five layers,
73 grids, and nine samples per grid. Contract checks cover previews, cancellation,
local memory accounting, weights/visibility/locking, duplication independence,
domain errors, invalid sizes/indices, non-finite data, overflow, and undo/redo.
A domain of one million grids with a single edited grid verifies sparse payload
accounting without constructing a million-grid surface.

A separate optimized Release harness passed **183 additional checks** for
multi-grid commit/undo, allocation-failure injection before commit mutation,
retry after allocation failure, zero-grid removal, and history branching. An
earlier Debug allocation-injection experiment did not complete and was interrupted;
it is not included in passing coverage. These checks do not exercise Blender's
guarded allocator, actual subdivision, PBVH, GPU drawing, or file I/O.

The real project command completed successfully:

```powershell
cmake --build D:\blender_build\octahedral_radius --config Release --target bf_blenkernel --parallel 2 -- /p:CL_MPCount=2
```

This compiles the new translation unit into the Bmax kernel library. The initial
backend verification did not perform `INSTALL` or an executable-level test.
There is no interactive performance claim. Harness files and compiler outputs
were external temporary artifacts; no tests were added to the repository.

### Subsequent local executable build (2026-10-31)

At the user's request, Release `INSTALL` completed from the modified prototype
branch with parallelism 2 and `CL_MPCount=2`. The installed executable at
`D:\blender_build\octahedral_radius\bin\blender.exe` is timestamped
2026-10-31 19:20:40 JST, reports Bmax 5.2.3 LTS, base hash `15f7bbc89857`, and
`codex/sculpt-layers-prototype (modified)`. It matches `bin/Release/blender.exe`
byte-for-byte. See [the build record](BUILD_WINDOWS.md#sculpt-layer-backend-local-build-2026-10-31)
for validation scope.

The new backend is compiled in the kernel library but has no editor/evaluation
callers; unused code may be omitted by the executable linker. This build does
not expose sculpt layers in the interface or save them in `.blend` files.
Desktop copies and public downloads were not updated.

## Original research recommendation (future design reference)

Implement an opt-in native C++ layer system, starting with a narrow Multires
prototype. Store independent vector displacement contributions, rather than
duplicated sculpt objects or a stack of base-mesh shape keys. Keep the existing
sculpt path when no stack exists.

Start with Multires because it is the harder storage/evaluation problem and an
important Bmax workflow. A successful fixed-topology mesh prototype alone would
not validate Multires. Support for ordinary meshes should follow through a
separate backend, sharing layer metadata and transaction semantics.

Do not begin with all brushes, topology changes, animation, layer masks, and both
geometry backends at once. First prove composition, local recording, undo,
persistence, and lower-level detail preservation.

## Existing approaches and evidence

- [Blender Studio: Layered Sculpting for Einar](https://studio.blender.org/blog/layered-sculpting-for-einar/)
  describes attribute-based sculpt layers transferred to Multires, practical
  synchronization and reprojection difficulties, and the potential of tangent-space
  detail. This is historical production evidence, not a Blender 5.2 benchmark.
- [Sculpt Layers by Yey!](https://superhivemarket.com/products/sculpt-layers)
  advertises Multires support, attributes/Geometry Nodes, and shape-key support.
  Its product page lists Blender 4.0–5.1 at the time of research. Neither its
  Blender 5.2 compatibility nor its performance was tested here.
- [Sculpt Layers for Multires by beni](https://extensions.blender.org/add-ons/sculpt-layers-multires/)
  advertises Blender 4.2+ support. Its public
  [core implementation](https://github.com/beni-bd/sculpt-layers-multires/blob/main/core.py)
  uses auxiliary objects, shell shape keys, strength synchronization, and
  `bpy.ops.object.multires_reshape`. This is useful workflow reference material;
  it does not demonstrate local native layer updates.
- [FadiMHussein/blender-sculpting-layers](https://github.com/FadiMHussein/blender-sculpting-layers)
  is an older shape-key/proxy approach whose README lists testing on Blender 2.83
  and 2.92 and leaves heavy-sculpt performance untested. It is not evidence of
  current compatibility.

Shape keys can deform the base mesh underneath Multires; they do not themselves
store independent high-resolution Multires grids. Avoid describing the two
features as universally incompatible.

## Findings from the Bmax source

Paths below are relative to the repository root. Function names are the audit
anchors; line numbers may change.

| Area | Source and observed behavior | Design consequence |
| --- | --- | --- |
| Existing Layer brush | `source/blender/editors/sculpt_paint/mesh/brushes/layer.cc`: persistent positions, normals, and displacement factors | A capped-height brush is not an independent layer stack. Keep this brush behavior. |
| Mesh deformation | `source/blender/editors/sculpt_paint/mesh/sculpt.cc`: `PositionDeformData::deform` handles evaluated positions, inverse deformation matrices, and dependent shape keys | Capturing only visible coordinates is insufficient for a future mesh backend with deforming modifiers. |
| Stroke transaction | Same file: `SculptPaintStroke::done`, `restore_position_from_undo_step`, `flush_update_step`, `flush_update_done` | Recording, cancel/anchored restoration, undo completion, and evaluation invalidation must be coordinated. |
| Multires flush | `source/blender/blenkernel/intern/multires.cc`: `multires_flush_sculpt_updates` calls `multiresModifier_reshapeFromCCG` when grids are dirty | A stack needs an explicit flush path; the current combined result must not accidentally overwrite the base or be applied twice. |
| Full-grid reshape | `source/blender/blenkernel/intern/multires_reshape_util.cc`: `multires_reshape_store_original_grids` duplicates displacement buffers across all grids; tangent conversion traverses all grid coordinates | Calling the existing wrapper once per layer or stroke does not satisfy the desired local-work architecture. |
| CCG writeback | `source/blender/blenkernel/intern/multires_reshape_ccg.cc`: `multires_reshape_assign_final_coords_from_ccg` visits all grids and may also write masks | Geometry layer commits must not incidentally modify mask storage. |
| Lower-level propagation | `source/blender/blenkernel/intern/multires_reshape_smooth.cc` preserves detail with smoothing and tangent-space transforms | Reuse the numerical semantics, but investigate a bounded affected region. Bilinear color resampling is not a geometry solution. |
| Evaluation and seams | `source/blender/blenkernel/intern/subdiv_displacement_multires.cc` reads `MDisps` and converts neighboring displacement vectors to object space before combining them | Never average raw tangent-space vectors from differently oriented grids. |
| Evaluated outputs | `source/blender/modifiers/intern/MOD_multires.cc`: both mesh and CCG paths attach the Multires displacement evaluator | Layer composition must reach rendering, modifier application, and non-sculpt evaluation as well as the sculpt viewport. |
| Undo | `source/blender/editors/sculpt_paint/mesh/sculpt_undo.cc`: per-node coordinates and persistent color grids; `StepData` records grid dimensions | Coordinate undo alone cannot restore a new persistent displacement stack. |
| Persistent data | `source/blender/makesdna/DNA_meshdata_types.h`: `MDisps`, `GridPaintColor`; `source/blender/blenkernel/intern/mesh.cc`: mesh copy/free/read/write callbacks | New storage needs a complete ownership and serialization design. Adding fields is not sufficient. |

The existing [Multires color implementation](multires_color.md) is a useful
reference for local buffers, persistent undo, and stroke-end commits. Geometry
changes still require bounds, normals, coordinate invalidation, and other updates
that color-only strokes deliberately avoid. Do not copy that early-exit policy.

## Composition and data ownership

For a fixed base surface and its tangent frames, define the top-resolution
displacement at a grid sample as:

`D_total = D_base + sum(visible[i] * strength[i] * D_layer[i])`

Convert the resulting vector through Blender's existing tangent convention to
obtain the displaced surface. Layer order has no geometric effect for this
additive model. Reordering the list is organizational; it must not imply an
ordered deformation stack or physically correct blending of large rotations.

Proposed persistent ownership is on the Mesh datablock, consistent with existing
Multires displacement. Object-linked mesh users therefore share layers. Active
editing state can be session-local; do not silently make layer values object-local.
Initially reject recording on multi-user meshes and ask the user to make a
single-user mesh through the normal Blender workflow. Passive evaluation must
still be correct for every user.

Proposed storage separates:

- Metadata: stable layer identifier independent of display name, flags, strength,
  schema version, and topology generation.
- Base displacement: preserve existing Multires sculpting when enabling a stack.
  Existing `MDisps` is the proposed base source, not also the composed result.
- Layer displacement: float3 buffers allocated only for touched grids initially;
  absent grid data means zero. Per-grid allocation is not per-sample sparsity.
- Runtime: current-level composed coordinates, transaction snapshots, affected
  sample/grid sets, and a mapping from persistent indices to current PBVH nodes.

Do not key persistent data by PBVH node addresses or node ordering. Both can change
when the tree is rebuilt. Grid identity is valid only while base connectivity and
corner ordering are unchanged; matching counts alone cannot validate topology.

Prefer a dedicated DNA-owned stack with explicit copy/free/blend-read/blend-write
support and runtime implicit sharing where practical. The exact container and
placement remain implementation decisions to settle before a DNA change. Do not
repurpose `GridPaintColor`, add ordinary point attributes pretending to be grids,
or assume several named `CD_MDISPS` layers will work through existing consumers.

Evaluation should read base plus layer contributions without a permanent full
composed duplicate. Whether to extend the displacement evaluator directly or use
a temporary composed adapter needs a measured prototype. A temporary adapter
must have explicit lifetime/ownership and cannot mutate original data during
depsgraph evaluation. Never sum again over an already composed `MDisps` cache.

## Recording transaction

Initial recording requires an unlocked, visible active layer at strength 1.
Other layers remain visible at their chosen strengths. Refuse unsupported
recording states with a clear message; do not silently change a strength.

1. Before the first write to an affected region, snapshot the current composed
   coordinates and the active layer's persistent state, including absent buffers.
2. Run the supported brush on the composed surface using existing masking,
   visibility, clipping, symmetry, and constraint behavior.
3. Track actual written samples and boundary neighbors. Capture the final result
   after clipping and seam synchronization, not just the requested brush vector.
4. At successful stroke completion, derive the net change from the transaction's
   initial surface. Convert it to the active layer's representation and propagate
   it to retained high resolution when required. Commit only that layer.
5. Keep runtime and persistent states consistent before finalizing the undo step
   or servicing an evaluation flush. During an active stroke, any forced flush
   needs a defined read-only composed snapshot or safe transaction synchronization.
6. On cancel, restore runtime coordinates and discard uncommitted data. Anchored
   previews must restore the same transaction baseline rather than accumulating
   successive previews into persistent displacement.

This stroke-end difference approach is a prototype choice. It must not require a
whole-surface snapshot. `PositionDeformData` and grid translation helpers are
candidate write sites, but no single helper has been established as covering all
sculpt operators. Unsupported coordinate-writing operators must be guarded while
a stack is active; outside Sculpt Mode, edits must also be handled explicitly.

Smooth initially means smoothing the composed visible surface and recording the
correction in the active layer. It can therefore compensate details from other
layers; hiding those other layers later can reveal that compensation. A separate
"smooth this layer only" tool would have different semantics and is deferred.

## Multires invariants and unresolved numerical work

- Level switching without sculpting never rewrites saved layer samples.
- A low-level stroke preserves high-level residual detail of the active layer and
  leaves other persistent layers unchanged.
- Boundary stitching uses common object-space vectors, converting back into each
  grid's own frame. Test face centers, edges, extraordinary vertices, and non-quads.
- Repeated switching, weight changes, and hide/show operations do not accumulate
  coordinate drift. Reconstruct from authoritative data when needed.
- New subdivision levels and Delete Higher must update every layer coherently;
  defer these commands until their own undo and storage behavior is implemented.
- Apply Base, Conform Base, Reshape, Unsubdivide, Rebuild Subdivisions, topology
  changes, and external displacement storage initially require baking/removing
  the stack through an explicit workflow. Do not silently bake or discard data.

The main open problem is a local equivalent of detail-preserving reshape. Its
support region may include neighboring faces or subdivision stencils beyond
immediate grid boundaries. Derive the region from actual dependencies and compare
against the full native operation. Do not assume a one-ring neighborhood is enough.
If equivalent local propagation is not achieved, report the limitation and narrow
the prototype to highest-level recording; do not claim low-level support.

Degenerate tangent frames need a finite, deterministic failure policy that leaves
persistent data unchanged. Large base edits and deforming modifier combinations
are later validation targets, not an automatic consequence of tangent storage.

## Undo, persistence, and compatibility

Extend sculpt undo to identify the active layer by stable ID and restore its
exact persistent samples, allocation state, level, and current composed positions.
Do not reconstruct high-resolution undo data from a lower-level coordinate result.
Record neighbor writes once, and protect snapshot creation in threaded execution.
Deleting a layer must not invalidate older undo records through dangling pointers.

Layer creation, deletion, weights, visibility, and baking need appropriate native
operator/property undo. Validate interleaving these edits with strokes, mode exits,
file reloads, and global undo; avoid making every stroke a whole-mesh Geometry undo.

Implement nested-buffer copy, free, versioning, serialization, duplication,
append/link, and evaluated-data ownership. Existing files without a stack retain
the original path. A future Bmax-only stack has no assured round trip through
official Blender. Provide explicit export/bake to ordinary Multires displacement
or a standard mesh; do not promise that unknown custom data will survive re-saving.

Apply Multires must produce the visible combined geometry. Removing the modifier
with an unbaked stack must be guarded. Color grids, masks, hidden samples, Face
Sets, and base shape keys must not be rewritten by layer bookkeeping.

## Memory and performance targets

Ten million stored float3 samples require 120,000,000 bytes (about 114.4 MiB) per
fully populated layer, excluding metadata, duplicate grid boundaries, runtime,
undo, and allocator overhead. Ten populated layers are 1.2 GB in decimal units
before those other costs. Sample counts are not unique evaluated vertex counts.

Use lazy grid allocation first; consider smaller tiles only if real workloads
justify the indexing and boundary complexity. Runtime geometry is still dense at
the current level. Undo compression can use existing infrastructure, but its CPU
and peak-memory costs need measurement.

Incremental weight changes can update a cached composition using the layer's
support and the weight difference, provided topology, level, and frames match.
Include neighbor normal/bounds updates. Periodic authoritative recomposition must
bound floating-point drift. A layer covering the entire surface inherently has a
global update cost; no constant-time slider claim is justified.

## Stages and acceptance gates

| Stage | Deliverable | Gate before expanding scope |
| --- | --- | --- |
| A | Internal two-layer Multires composition and Draw recording at the highest existing level | Base preserved; layer isolation; equivalent visible geometry; boundaries continuous; no additional full-surface snapshots per stroke |
| B | Persistent storage, local undo/redo, save/reload, cancel and anchored strokes; Smooth and Grab | Exact restored persistent data; finite geometry; no allocation leaks; rendering and applied output match viewport |
| C | Lower-level editing with retained high-resolution detail | Comparison against native propagation; unchanged inactive layers; bounded affected region established and measured |
| D | User-facing stack: add, rename, duplicate, delete, strength, visibility, lock, Solo, bake | Interleaved undo; repeatable composition; modifier/mode safeguards; localized labels and tooltips |
| E | Ordinary mesh backend and selected additional operations | Shape-key and modifier interaction explicitly specified and tested, rather than inherited accidentally |

The first usable release requires A–D, not just the visual demonstration in A.
Stages A–C use a single-user mesh with a single enabled Multires modifier,
no shape keys or other deforming/topology modifiers, and unchanged base
coordinates/connectivity. Restrict base editing and unvalidated operators while
the experimental stack exists. This deliberately narrow fixture does not imply
that the final system must exclude shape keys; compatibility is separate work.

Initial strengths should support 0–1 for blending; negative/extrapolated weights,
animation/drivers, groups, masks per layer, ordered blends, and reprojection can
follow separately. Solo is a temporary preview that preserves visibility flags;
finish or cancel an active stroke before any stack mutation.

## Validation plan, not executed results

Use disposable scripts/scenes outside the repository and remove them afterward.
Keep build caches, dependencies, runtime files, and existing user sessions.

- Correctness: two independent layers, varied weights, hide/show/Solo, duplicate,
  delete, merge/bake when offered, layer changes between strokes, and empty strokes.
- Geometry: Draw, Smooth, Grab, symmetry, masks, hidden grids, clipping, grid seams,
  triangle/pentagon bases, extraordinary points, and degenerate input.
- Levels: high-to-low-to-high without writes; low-level strokes; repeated changes;
  exact persistent undo/redo; subdivision/Delete Higher once supported.
- Lifecycle: cancel, anchored preview, scene save, load, duplication, shared mesh
  evaluation, mode changes, interleaved global/sculpt undo, and unsupported commands.
- Evaluation: actual OpenGL and Vulkan viewports, picking of the composed surface,
  Cycles output, modifier application, and an explicit baked transfer to Blender.
- Regression: existing Bmax color Paint/Blur/Smear, attributes, masks, Face Sets,
  unlayered sculpting, and normal Multires flush behavior.
- Performance: same scene/stroke, warm-up, at least five timed samples and median;
  separate stroke operator, stroke-end commit, next-stroke startup, slider latency,
  GPU redraw, peak RAM, persistent allocation, and undo memory. Vary surface size,
  painted fraction, level, and number of layers. Compare against unlayered Bmax.

No performance ratio is claimed by this document. A synchronous brush-operator
measurement alone cannot establish interactive frame rate.

Any implementation that changes DNA or shared C++ layout requires a full
`/t:Rebuild` before `INSTALL`, following [the Windows build guide](BUILD_WINDOWS.md),
including Boolean Manifold and Sculpt Trim smoke checks. Research alone does not
require a build or an update to the implemented-feature inventory.
