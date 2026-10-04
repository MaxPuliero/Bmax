"""Run with the modified Blender: blender --factory-startup --python this_file.py."""
import bpy
from mathutils import Quaternion

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)

# The back face belongs to the same object: the ordinary outline cannot see the front opening.
vertices = [(-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1),
            (-1, 1, -1), (1, 1, -1), (1, 1, 1), (-1, 1, 1),
            (-0.6, -1, -0.6), (0.6, -1, -0.6), (0.6, -1, 0.6), (-0.6, -1, 0.6)]
faces = [(0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0), (4, 7, 6, 5),
         (0, 1, 9, 8), (1, 2, 10, 9), (2, 3, 11, 10), (3, 0, 8, 11)]
mesh = bpy.data.meshes.new('OpenBox')
mesh.from_pydata(vertices, [], faces)
mesh.update()
obj = bpy.data.objects.new('Open box: front opening', mesh)
bpy.context.collection.objects.link(obj)
obj.select_set(True)
bpy.context.view_layer.objects.active = obj

bpy.ops.mesh.primitive_cube_add(location=(3, 0, 0))
closed = bpy.context.object
closed.name = 'Closed box: no additional edges'
closed.select_set(True)
obj.select_set(True)
bpy.context.view_layer.objects.active = obj

for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type == 'VIEW_3D':
            space = area.spaces.active
            space.overlay.show_overlays = True
            space.overlay.show_outline_selected = True
            space.overlay.show_mesh_holes = True
            space.region_3d.view_rotation = Quaternion((0.70710678, 0.70710678, 0, 0))
            space.region_3d.view_perspective = 'ORTHO'
            space.region_3d.view_location = (1.5, 0, 0)
            space.region_3d.view_distance = 9

print('Toggle Viewport Overlays > Objects > Mesh Holes to compare the open and closed boxes.')
