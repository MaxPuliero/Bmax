import bpy

if __package__:
    from ._registration import register_module, unregister_module
else:
    from MaxPuliero_Pies_5_1._registration import register_module, unregister_module

from bpy.types import Menu

bl_info = {
    "name": "Edit Mesh Merge // Hotkey: M",
    "description": "Merge PM",
    "author": "Max Puliero",
    "version": (1, 0),
    "blender": (4, 3, 0),
    "location": "3D View",
    "category": "Max Puliero Pies"
}

class EditMeshMerge(bpy.types.Menu):
    bl_label = "Merge"
    bl_idname = "VIEW3D_MT_merge_pm"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        # left
        pie.operator("mesh.merge", text="Collapse").type='COLLAPSE'
        # right
        pie.operator("mesh.merge", text="Center").type='CENTER'
        # top
        pie.operator("mesh.merge", text="At Last").type='LAST'
        # bottom
        pie.operator("mesh.merge", text="At First").type='FIRST'

classes = (EditMeshMerge,)

shortcuts = ({'keymap': 'Mesh',
  'space_type': 'EMPTY',
  'type': 'M',
  'value': 'PRESS',
  'menu': 'VIEW3D_MT_merge_pm'},)

def register():
    register_module("MaxPuliero_Pies_5_1.PM_Edit_Mesh_Merge", classes, shortcuts)


def unregister():
    unregister_module("MaxPuliero_Pies_5_1.PM_Edit_Mesh_Merge", classes, shortcuts)


if __name__ == "__main__":
    register()