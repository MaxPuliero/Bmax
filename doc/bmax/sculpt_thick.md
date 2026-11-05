# Persistent-base sculpt brush limits

Implementation: [`a70d32267f1b`](https://github.com/MaxPuliero/Bmax/commit/a70d32267f1b433062447253efae9dd05171d605) on `main`.

## Controls and reference ownership

All mesh Sculpt brushes expose the native Set Persistent Base command. Its label becomes
Update Persistent Base when Object.has_sculpt_persistent_base reports a compatible reference.
Limit Up and Limit Down are saved on Brush; distances are nonnegative RNA DISTANCE values.
The reference belongs to mesh data / the object's Sculpt session, not to an individual brush.
A toggle operator asks to store the surface before enabling a missing-base limit. Escape or
Cancel makes no change. Slider editing is available only with a valid base and an enabled
corresponding limit. Dyntopo disables acquisition and controls.

A separator and one Persistent checkbox are shown for every brush, matching native Layer/Cloth
spacing. Set/Update and enabling a limit automatically enable the active brush's native
BRUSH_PERSISTENT flag. Persistent gates the approximate limits and retains its native Layer/Cloth
meaning. Turning it off suspends limits and distance editing without deleting the reference.
The limit defaults are 0.02 Blender distance units (2 cm at standard scene unit scale).
DNA-presence migration initializes older stock assets only when fields are missing; saved zero
and custom values remain unchanged.

Set/Update and limit toggles do not create geometric Undo steps. They are working settings;
a brush Undo/Redo preserves the captured reference while its Sculpt session and topology
remain compatible. Reference capture still marks the project modified. Generic memfile steps
at capture previously discarded runtime-only Multires base data on the following stroke Undo.

Ctrl uses the native stroke-invert flag and temporarily swaps both enabled sides and their
distances. It does not mutate the saved settings or replace the native brush inversion.
For example, Up 2 cm / Down 1 cm becomes Up 1 cm / Down 2 cm for an inverted stroke.

## Implementation

Set Persistent Base uses evaluated positions and normals on ordinary meshes, retaining native
attribute sharing when evaluated coordinates match mesh coordinates. Multires uses the native
persistent arrays at the current sculpt grid layout. Native topology counts are recorded as
mesh ID metadata. These counts and grid dimensions detect common invalidation cases without
scanning all geometry at each dab. Same-count topology edits require manual reacquisition.

sculpt_thick.cc builds an immutable world-space point BVH of sampled base points and normals.
It is reused across strokes while source pointers, counts and object transform remain the
same; explicit base updates discard it. Up to one million evenly indexed samples bound its
memory use on very dense meshes. A point's nearest sample defines a tangent plane; signed
normal distance determines the permitted interval. This approximates collision and is not
an exact offset surface, raycast, signed distance field or continuous collision solver.

The common brush dispatcher captures only affected nodes when a limit is active, then corrects
changed points after native deformation, smoothing, gravity and optional cloth simulation.
Unchanged points, including masked points, are not projected. Axis locks and mirror clipping
are applied to corrections. Native shape-key/crazyspace deformation paths remain in use.
Cloth additionally projects active, nonzero-factor simulated points after each constraint
iteration and after force/velocity integration. Its simulation positions are corrected, not
only the visible mesh. Self-collision is outside scope.

Points already outside a newly configured band are allowed to move back toward it, but the
projection does not make them snap to the boundary. The previous point's distance to the same
sample plane expands the permitted interval only as needed to avoid a discontinuous jump.
Large tangential movement can change the nearest sample and the local notion of signed distance.

## Persistence and limits

Brush settings serialize through DNA. Ordinary-mesh base coordinates/normals and count metadata
save with mesh data. Multires uses native runtime-only Persistent Base storage. Leaving Sculpt parks its arrays
on the object runtime and reentry restores them through ownership moves, without full-point
copies. Reopening a project requires a new capture. Different grid layouts cannot use the
same native reference.
In a verified 2 -> 1 -> 2 transition, the base is inactive at level 1 and reactivates at level 2
including across Sculpt-session destruction/recreation. Capturing at another level replaces
the previous base. No new
Multires disk format is introduced. Shared mesh users share persistent attributes.

The approximation can be inaccurate at sharp corners, narrow cavities, nearby opposing walls,
coarse sampling and large motions. It provides no self-collision or robust tunneling prevention.
Shape-key capture and deformation were verified, including an unchanged Basis key.
General modifier stacks, sharp/nearby reference surfaces, seams and additional native Sculpt
Layers were not exhaustively verified; source integration alone does not establish correctness.

## Performance and validation status

Disabled limits skip node snapshots, spatial queries and geometry correction. With active
limits, temporary snapshots and corrections are confined to affected nodes. A cached spatial
query is needed for each moved point; Cloth repeats queries during solver iterations. Capture
and cache creation have a one-time whole-reference cost. The native persistent storage costs
about 24 bytes per mesh point for coordinates and normals (Multires also keeps its native
4-byte displacement factor). The point BVH cache is bounded to one million samples; its sampled coordinates, normals,
bounding boxes and nodes add memory beyond the native base and temporary per-node stroke data.

A full MSVC Rebuild was performed because Brush DNA and SculptSession layouts changed, followed
by incremental corrections and INSTALL. The final local runtime passed 87 checks: 34 each on
actual OpenGL/Vulkan, 10 extended checks, 5 undo/sliding checks and 4 native popup checks.
These cover both distances, asymmetric Ctrl swapping, unapplied nonuniform scale, Draw, Clay,
Inflate, Cloth, Multires, mesh save/reload, zero depth, masks, reference updates, shape keys,
Manifold Boolean and Sculpt Trim, stroke undo/redo, tangential movement and confirmation
cancel/accept. Captures of the real panels and dialog were inspected. This is targeted
validation, not exhaustive coverage of all brush modes or simulation configurations.

### Matched synthetic-stroke benchmark (before the controls refinement)

All times below are milliseconds for a synchronous operator containing 10 stroke samples,
OpenGL, fixed planar grids and brush settings. The first cold stroke was timed separately;
two additional warm-up strokes preceded seven measured strokes. Values are medians.
The prior runtime is official_save_preview, which contains the previous Bmax source build;
it is not an official Blender performance comparison.

| Brush / vertices | Previous build | New, limits off | New, both limits on |
| --- | ---: | ---: | ---: |
| Draw / 263,169 | 4.67 | 4.58 | 6.92 |
| Cloth / 66,049 | 39.87 | 39.69 | 72.87 |

In this workload the enabled overhead was approximately 51% for Draw and 84% for Cloth,
relative to the new disabled path. Absolute costs were about 2.34 and 33.17 ms per complete
synthetic stroke. The disabled path was comparable to the previous build within normal
timing variation. These measurements are not interactive frame rates or guarantees for
other meshes, brush footprints or solver settings.

Prepared-scene reference capture took 0.69 ms for the Draw grid and 0.13 ms for the Cloth grid.
First enabled strokes, including acceleration preparation, took 21.89 and 53.33 ms respectively;
first disabled strokes took 5.42 and 34.98 ms. Cold normal evaluation and different densities
can change capture/preparation costs.

The point BVH uses full three-dimensional bounding-box distances to keep nearest searches
efficient when a brush moves away from a nearly planar base. The geometric approximation
remains a nearest sampled tangent plane, not triangle collision.

All test scripts, scenes, screenshots, settings and logs were created outside the repository
and removed after their results were recorded. Pre-existing files and build caches were retained.
The source is committed on `main`; the standard runtime is installed, while Desktop/public
packages and the separate previous runtime were not replaced.


## Controls refinement verification

The follow-up passed 36 targeted assertions on OpenGL: 27 data/brush checks and 9 real
confirmation checks. These verify fresh/RNA/Essentials defaults, preservation of explicit
zero/custom values through library round trips, automatic Persistent activation on Set and
Update for Draw/Inflate Cloth/Layer, default caps on all three, activation through the
missing-base toggle, suspension with master off and reference retention. Panel captures
were inspected for spacing, checkbox uniqueness and disabled distance fields. The Multires
level-switch probe above was also observed. Initial GPU/undo validation and benchmark results
remain recorded above; this UI/default/master-switch follow-up was not re-benchmarked or
repeated on Vulkan. Native limit geometry math and shared layouts are unchanged.


## Persistent-base Undo regression fix

A faithful Set -> stroke -> Undo/Redo sequence reproduced Multires base loss, with Persistent
still checked but the base invalid. The old Set/Toggle OPTYPE_UNDO flag inserted a generic
memfile step between Sculpt steps. Avoiding that snapshot preserved runtime data in a
control experiment; the fix removes those Undo flags and explicitly marks capture dirty.
It adds no copies of the full reference to individual stroke history entries.

The corrected complete runtime is `D:\blender_build\persistent_undo_fix\bin\blender.exe`;
at that stage the running standard runtime was preserved. It passed 89 OpenGL assertions over ten scenarios
and 16 assertions on two requested Vulkan scenarios, with actual Vulkan confirmed. The tests
retain operator Undo registration for real brush strokes and insert no artificial checkpoint
after Set/Update. Mesh, Layer, Cloth, Multires Draw/Layer/Cloth, missing-base capture, limit
toggles and Update between strokes are covered. Base/flag validity is inspected in Sculpt;
Multires geometry is evaluated only after exiting Sculpt, avoiding borrowed CCG mesh access.
The source is now committed on `main`. No performance rerun was needed for removal of two Undo
flags and one dirty notifier; the collision math and shared layouts are unchanged.


## Mode retention and explicit release

SculptPersistentBase holds the native coordinates, normals, displacement factors, grid layout
and captured mesh session identity. ObjectRuntime owns it only while parked outside Sculpt;
SculptSession owns it while active. Copy reset clears the parked pointer, avoiding reference
aliasing in object duplicates or evaluated copies. Restore checks mesh identity and topology
counts; incompatible data is freed. Only one captured subdivision layout is stored.

Flush Persistent releases parked and active arrays, the sampled collision BVH, native base
attributes and the four owned topology-count ID properties. It preserves geometry and all
saved limit settings, and switches off the active brush's Persistent flag. It is available
in the brush panel or through F3 in Object Mode, including when a retained level is inactive.
It is disabled during active strokes/filters and when no data remains. Like capture, release
is working state and adds no Undo entry. Shared mesh attributes retain native sharing rules.

Outside Sculpt, retained Multires arrays cost approximately 28 bytes per grid point (about
28 MB per million points), excluding allocator overhead. The collision BVH is not retained
outside Sculpt. Parking/restoring does not copy the point buffers; a later limited stroke
rebuilds its collision cache. This follow-up was not independently benchmarked.

A full MSVC Rebuild of the shared-layout change and Release installation completed. The
standard runtime now includes both this follow-up and the previous Undo correction; the
separate undo-fix runtime and Desktop/public packages remain unchanged. Verification passed
181 assertions: 119 OpenGL operator/data, 7 native UI, 52 actual Vulkan and 3 Manifold/Trim.
Real Draw, Layer and Cloth strokes retain the original cap over repeated mode cycles;
Flush preserves geometry and distances; level recovery, mesh replacement, topology changes,
object copies/deletion and Undo/Redo after mode changes passed. The synchronous harness
explicitly evaluates the dependency graph after Undo/Redo before evaluating exit geometry.
Native Update/Flush/enabled states and the missing-base recapture dialog were inspected.
Temporary verification files were external and removed; build caches and user sessions remain.
