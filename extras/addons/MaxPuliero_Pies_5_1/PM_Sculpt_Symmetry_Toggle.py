import bpy

if __package__:
    from ._registration import register_module, unregister_module
else:
    from MaxPuliero_Pies_5_1._registration import register_module, unregister_module

from bpy.types import Menu

bl_info = {
    "name": "Sculpt Symmetry // Hotkey: ALT+X",
    "description": "Sculpt Symmetry PM",
    "author": "Max Puliero",
    "version": (1, 0),
    "blender": (4, 3, 0),
    "location": "3D View",
    "category": "Max Puliero Pies"
}


class symx(bpy.types.Operator):
    bl_idname = "sym.x"
    bl_label = "X Symmetry"

    def execute(self, context):
        obj = bpy.context.object
        obj.use_mesh_mirror_x = not obj.use_mesh_mirror_x
        return {'FINISHED'}


class symy(bpy.types.Operator):
    bl_idname = "sym.y"
    bl_label = "Y Symmetry"

    def execute(self, context):
        obj = bpy.context.object
        obj.use_mesh_mirror_y = not obj.use_mesh_mirror_y
        return {'FINISHED'}


class symz(bpy.types.Operator):
    bl_idname = "sym.z"
    bl_label = "Z Symmetry"

    def execute(self, context):
        obj = bpy.context.object
        obj.use_mesh_mirror_z = not obj.use_mesh_mirror_z
        return {'FINISHED'}


class symmetry(Menu):
    bl_label = "Symmetry"
    bl_idname = "VIEW3D_MT_Symmetry_pm"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        # left
        pie.operator("sym.x", icon="EVENT_X")
        # right
        pie.operator("sym.y", icon="EVENT_Y")
        # bottom
        pie.separator()
        # top
        pie.operator("sym.z", icon="EVENT_Z")
        # top/left
        pie.separator()
        # top/right
        pie.separator()
        # bottom/left
        pie.separator()
        # bottom/right
        pie.separator()

classes = (symx, symy, symz, symmetry,)

shortcuts = ({'keymap': 'Sculpt',
  'space_type': 'EMPTY',
  'type': 'X',
  'value': 'PRESS',
  'alt': True,
  'menu': 'VIEW3D_MT_Symmetry_pm'},)

def register():
    register_module("MaxPuliero_Pies_5_1.PM_Sculpt_Symmetry_Toggle", classes, shortcuts)


def unregister():
    unregister_module("MaxPuliero_Pies_5_1.PM_Sculpt_Symmetry_Toggle", classes, shortcuts)


if __name__ == "__main__":
    register()



    #bpy.ops.wm.call_menu_pie(name="VIEW3D_MT_Symmetry_pm")

