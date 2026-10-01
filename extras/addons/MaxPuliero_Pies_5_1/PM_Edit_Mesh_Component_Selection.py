import bpy

if __package__:
    from ._registration import register_module, unregister_module
else:
    from MaxPuliero_Pies_5_1._registration import register_module, unregister_module

from bpy.types import Menu

bl_info = {
    "name": "Mesh Component Selection // Hotkey: RMB",
    "description": "Mesh Component Selection PM",
    "author": "Max Puliero",
    "version": (1, 0),
    "blender": (4, 3, 0),
    "location": "3D View",
    "category": "Max Puliero Pies"
}

class EditMeshCompSel(bpy.types.Menu):
    bl_label = "Mesh Component Selection"
    bl_idname = "VIEW3D_MT_mesh_component_selection_pm"

    def draw(self, context):
        layout = self.layout
        obj = context.active_object

        if obj and obj.type == 'MESH' and obj.mode == 'EDIT':
            pie = layout.menu_pie()
            # left
            pie.operator("mesh.select_mode", text="Vertex", icon="VERTEXSEL").type='VERT'
            # right
            pie.operator("mesh.select_linked", text="Select Linked", icon="UV_ISLANDSEL")
            # bottom
            pie.operator("mesh.select_mode", text="Face", icon="FACESEL").type='FACE'
            # top
            pie.operator("mesh.select_mode", text="Edge", icon="EDGESEL").type='EDGE'
            # top/left
            pie.separator()
            # top/right
            pie.separator()
            # bottom/left
            pie.separator()
            # bottom/right
            pie.operator("wm.call_menu", text="Context Menu", icon="ALIGN_JUSTIFY").name='VIEW3D_MT_edit_mesh_context_menu'

classes = (EditMeshCompSel,)

shortcuts = ({'keymap': 'Mesh',
  'space_type': 'EMPTY',
  'type': 'RIGHTMOUSE',
  'value': 'PRESS',
  'menu': 'VIEW3D_MT_mesh_component_selection_pm'},)

def register():
    register_module("MaxPuliero_Pies_5_1.PM_Edit_Mesh_Component_Selection", classes, shortcuts)


def unregister():
    unregister_module("MaxPuliero_Pies_5_1.PM_Edit_Mesh_Component_Selection", classes, shortcuts)


if __name__ == "__main__":
    register()

    
    # bpy.ops.wm.call_menu_pie(name="VIEW3D_MT_mesh_component_selection_pm")
