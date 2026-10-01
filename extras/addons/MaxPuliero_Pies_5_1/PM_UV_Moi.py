import bpy

if __package__:
    from ._registration import register_module, unregister_module
else:
    from MaxPuliero_Pies_5_1._registration import register_module, unregister_module

from bpy.types import Menu

bl_info = {
    "name": "UV TexTools // Hotkey: B",
    "description": "UV Moi PM",
    "author": "Max Puliero",
    "version": (1, 0),
    "blender": (4, 3, 0),
    "location": "3D View",
    "category": "Max Puliero Pies"
}

class UVTextools(bpy.types.Menu):
    bl_label = "TexTools"
    bl_idname = "UV_MT_UV_textools_pm"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        # left
        pie.operator("uv.mio3_straight", text="Straight", icon="UV_ISLANDSEL")
        # right
        pie.operator("uv.mio3_gridify", text="Quadrify", icon="MESH_GRID") 
        # bottom
        pie.separator()
        # top
        pie.separator()
        # top/left
        pie.separator()
        # top/right
        pie.separator()
        # bottom/left
        pie.separator()
        # bottom/right
        pie.separator()

classes = (UVTextools,)

shortcuts = ({'keymap': 'UV Editor',
  'space_type': 'EMPTY',
  'type': 'B',
  'value': 'PRESS',
  'menu': 'UV_MT_UV_textools_pm'},)

def register():
    register_module("MaxPuliero_Pies_5_1.PM_UV_Moi", classes, shortcuts)


def unregister():
    unregister_module("MaxPuliero_Pies_5_1.PM_UV_Moi", classes, shortcuts)


if __name__ == "__main__":
    register()
    
    # bpy.ops.wm.call_menu_pie(name="UV_MT_UV_textools_pm")