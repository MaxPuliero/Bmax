import bpy

if __package__:
    from ._registration import register_module, unregister_module
else:
    from MaxPuliero_Pies_5_1._registration import register_module, unregister_module

from bpy.types import Operator, Menu

bl_info = {
    "name": "View // Hotkey: ALT+RMB",
    "description": "View PM",
    "author": "Max Puliero",
    "version": (1, 0),
    "blender": (4, 3, 0),
    "location": "3D View",
    "category": "Max Puliero Pies"
}

# Global variable to keep track of the view state
is_top_view = True
is_front_view = True
is_right_view = True

class ToggleTopBottomView(Operator):
    bl_idname = "view3d.toggle_top_bottom_view"
    bl_label = "Toggle Top/Bottom View"

    def execute(self, context):
        global is_top_view
        if is_top_view:
            bpy.ops.view3d.view_axis(type='BOTTOM')
        else:
            bpy.ops.view3d.view_axis(type='TOP')
        is_top_view = not is_top_view
        return {'FINISHED'}

class ToggleRightLeftView(Operator):
    bl_idname = "view3d.toggle_right_left_view"
    bl_label = "Toggle Right/Left View"

    def execute(self, context):
        global is_right_view
        if is_right_view:
            bpy.ops.view3d.view_axis(type='LEFT')
        else:
            bpy.ops.view3d.view_axis(type='RIGHT')
        is_right_view = not is_right_view
        return {'FINISHED'}

class ToggleFrontBackView(Operator):
    bl_idname = "view3d.toggle_front_back_view"
    bl_label = "Toggle Front/Back View"

    def execute(self, context):
        global is_front_view
        if is_front_view:
            bpy.ops.view3d.view_axis(type='BACK')
        else:
            bpy.ops.view3d.view_axis(type='FRONT')
        is_front_view = not is_front_view
        return {'FINISHED'}

class Viewmode(bpy.types.Menu):
    bl_label = "PIE_MT_View"
    bl_idname = "VIEW3D_MT_view_pm"  # Change this line

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        # Rest of the code remains the same
        
        
        # left
        pie.separator()
        # right
        pie.operator("view3d.toggle_right_left_view", text="Right/Left", icon="TRIA_RIGHT")
        # bottom
        pie.operator("view3d.toggle_front_back_view", text="Front/Back", icon="RADIOBUT_ON")
        # top
        pie.operator("view3d.toggle_top_bottom_view", text="Top/Bottom", icon="TRIA_UP")
        # top/left
        pie.operator("view3d.view_persportho", text="Ortho/Perspective", icon="VIEW_PERSPECTIVE")
        # top/right
        pie.operator("view3d.localview", text="Isolate", icon="LAYER_ACTIVE").frame_selected=False
        # bottom/left
        pie.separator()
        # bottom/right
        pie.operator("view3d.view_selected", text="Frame Selection", icon="SNAP_FACE_CENTER")

classes = (ToggleTopBottomView, ToggleRightLeftView, ToggleFrontBackView, Viewmode,)

shortcuts = ({'keymap': '3D View',
  'space_type': 'VIEW_3D',
  'type': 'RIGHTMOUSE',
  'value': 'PRESS',
  'alt': True,
  'menu': 'VIEW3D_MT_view_pm'},)

def register():
    register_module("MaxPuliero_Pies_5_1.PM_View", classes, shortcuts)


def unregister():
    unregister_module("MaxPuliero_Pies_5_1.PM_View", classes, shortcuts)


if __name__ == "__main__":
    register()
