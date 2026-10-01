import bpy

if __package__:
    from ._registration import register_module, unregister_module
else:
    from MaxPuliero_Pies_5_1._registration import register_module, unregister_module

from bpy.types import Menu

bl_info = {
    "name": "LoopTools // Hotkey: ALT+C",
    "description": "LoopTools PM",
    "author": "Max Puliero",
    "version": (1, 0),
    "blender": (4, 3, 0),
    "location": "3D View",
    "category": "Max Puliero Pies"
}



class Circle(bpy.types.Operator):
    bl_idname = "lt.circle"
    bl_label = "Circle"
    def execute(self, context): 
        bpy.ops.mesh.looptools_circle(custom_radius=False, fit='best', flatten=True, influence=100, lock_x=False, lock_y=False, lock_z=False, radius=1, angle=0, regular=True)
        return {'FINISHED'}
    
class Flatten(bpy.types.Operator):
    bl_idname = "lt.flat"
    bl_label = "Flatten"
    def execute(self, context): 
        bpy.ops.mesh.looptools_flatten(influence=100, lock_x=False, lock_y=False, lock_z=False, plane='best_fit', restriction='none')
        return {'FINISHED'}
    


class LoopTools(Menu):
    bl_label = "LoopTools"
    bl_idname = "VIEW3D_MT_Looptools_pm"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        # left
        pie.separator()
        # right
        pie.separator()
        # bottom
        pie.operator("lt.circle", icon="MESH_CIRCLE")
        # top
        pie.operator("lt.flat", icon="MESH_PLANE")
        # top/left
        pie.separator()
        # top/right
        pie.separator()
        # bottom/left
        pie.separator()
        # bottom/right
        pie.separator()

classes = (Circle, Flatten, LoopTools,)

shortcuts = ({'keymap': 'Mesh',
  'space_type': 'EMPTY',
  'type': 'C',
  'value': 'PRESS',
  'alt': True,
  'menu': 'VIEW3D_MT_Looptools_pm'},)

def register():
    register_module("MaxPuliero_Pies_5_1.PM_LoopTools", classes, shortcuts)


def unregister():
    unregister_module("MaxPuliero_Pies_5_1.PM_LoopTools", classes, shortcuts)


if __name__ == "__main__":
    register()


    #bpy.ops.wm.call_menu_pie(name="VIEW3D_MT_Looptools_pm")