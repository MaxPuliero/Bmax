import bpy

if __package__:
    from ._registration import register_module, unregister_module
else:
    from MaxPuliero_Pies_5_1._registration import register_module, unregister_module

from bpy.types import Menu

bl_info = {
    "name": "Curve Component Selection // Hotkey: RMB",
    "description": "Curve Component Selection PM",
    "author": "Max Puliero",
    "version": (1, 0),
    "blender": (4, 3, 0),
    "location": "3D View",
    "category": "Max Puliero Pies"
}

class EditCurveCompSelect(bpy.types.Menu):
    bl_label = "Curve Component Selection"
    bl_idname = "VIEW3D_MT_curve_component_selection_pm"

    def draw(self, context):
        layout = self.layout
        obj = context.active_object

        if obj and obj.type == 'CURVE':
            pie = layout.menu_pie()
            # left
            pie.separator()
            # right
            pie.operator("curve.select_linked", text="Select Linked", icon="UV_ISLANDSEL")
            # bottom
            pie.separator()
            # top
            pie.separator()
            # top/left
            pie.separator()
            # top/right
            pie.separator()
            # bottom/left
            pie.operator("wm.search_menu", text="Search", icon="VIEWZOOM")
            # bottom/right
            pie.operator("wm.call_menu", text="Context Menu", icon="ALIGN_JUSTIFY").name='VIEW3D_MT_edit_curve_context_menu'

classes = (EditCurveCompSelect,)

shortcuts = ({'keymap': 'Curve',
  'space_type': 'EMPTY',
  'type': 'RIGHTMOUSE',
  'value': 'PRESS',
  'menu': 'VIEW3D_MT_curve_component_selection_pm'},)

def register():
    register_module("MaxPuliero_Pies_5_1.PM_Curve_Component_Selection", classes, shortcuts)


def unregister():
    unregister_module("MaxPuliero_Pies_5_1.PM_Curve_Component_Selection", classes, shortcuts)


if __name__ == "__main__":
    register()

    # bpy.ops.wm.call_menu_pie(name="VIEW3D_MT_curve_component_selection_pm")