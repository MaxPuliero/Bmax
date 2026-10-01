bl_info = {
    "name": "Sculpt Mask // Hotkey: M",
    "description": "Sculpt Mask Menu",
    "author": "Max Puliero",
    "version": (0, 1, 0),
    "blender": (4, 3, 0),
    "location": "M key",
    "warning": "",
    "doc_url": "",
    "category": "Max Puliero Pies"
}

import bpy

if __package__:
    from ._registration import register_module, unregister_module
else:
    from MaxPuliero_Pies_5_1._registration import register_module, unregister_module

import os  # Added import statement for os module
from bpy.types import Operator, Menu


# Brushes

class PIE_OT_Paint_Mask(Operator):
    bl_idname = "paint.mask"
    bl_label = "Paint Mask"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.brush.asset_activate(
        asset_library_type='ESSENTIALS', 
        asset_library_identifier="", 
        relative_asset_identifier=f"brushes/essentials_brushes-mesh_sculpt.blend/Brush/Mask"
        )
        return {'FINISHED'}
    

class Box_Mask(Operator):
    bl_idname = "box.mask"
    bl_label = "Box Mask"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.wm.tool_set_by_id(name="builtin.box_mask")
        return {'FINISHED'}
    

class Lasso_Mask(Operator):
    bl_idname = "lasso.mask"
    bl_label = "Lasso Mask"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.wm.tool_set_by_id(name="builtin.lasso_mask")
        return {'FINISHED'}
    

class Line_Mask(Operator):
    bl_idname = "line.mask"
    bl_label = "Line Mask"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.wm.tool_set_by_id(name="builtin.line_mask")
        return {'FINISHED'}
    
    
class Line_Prj(Operator):
    bl_idname = "line.prj"
    bl_label = "Line Project"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.wm.tool_set_by_id(name="builtin.line_project")
        return {'FINISHED'}
    
    
class Box_Hide(Operator):
    bl_idname = "box.hide"
    bl_label = "Box Hide"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.wm.tool_set_by_id(name="builtin.box_hide")
        return {'FINISHED'}


# Operator to set the sculpt brush to Draw
class PIE_OT_Sculpt_Mask_Draw(Operator):
    bl_idname = "sculpt.sculpt_mask"
    bl_label = "Sculpt SculptDraw"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        # Set the sculpt brush to Draw
        context.tool_settings.sculpt.brush = bpy.data.brushes['Draw']
        return {'FINISHED'}


# Pie menu for sculpting
class PIE_MT_Sculpt_Mask(Menu):
    bl_idname = "PIE_MT_sculpt_Mask"
    bl_label = "Sculpt Mask"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        pie.scale_y = 1.0

        # 4 - LEFT
        pie.operator("box.mask", text="    Box Mask", icon_value=brush_icons["ops.sculpt.border_mask"])
        # 6 - RIGHT
        pie.operator("box.hide", text="    Box Hide", icon_value=brush_icons["ops.sculpt.border_hide"])
        # 2 - BOTTOM
        pie.operator("line.prj", text="    Line Project", icon_value=brush_icons["ops.sculpt.line_project"])
        # 8 - TOP
        pie.operator("paint.mask", text="    Paint Mask", icon_value=brush_icons["brush.sculpt.mask"])
        # 7 - TOP - LEFT
        pie.operator("lasso.mask", text="    Lasso Mask", icon_value=brush_icons["ops.sculpt.lasso_mask"])
        # 9 - TOP - RIGHT
        pie.separator()
        # 1 - BOTTOM - LEFT
        pie.operator("line.mask", text="    Line Mask", icon_value=brush_icons["ops.sculpt.line_mask"])
        # 3 - BOTTOM - RIGHT
        pie.separator()


brush_icons = {}

def create_icons():
    global brush_icons
    icons_directory = bpy.utils.system_resource('DATAFILES', path="icons")
    brushes = (
        "ops.sculpt.border_mask", "brush.sculpt.mask", 
        "ops.sculpt.lasso_mask", "ops.sculpt.line_mask", 
        "ops.sculpt.line_project", "ops.sculpt.border_hide", 
        "ops.sculpt.border_mask",
    
    )
    for brush in brushes:
        filename = os.path.join(icons_directory, f"{brush}.dat")
        icon_value = bpy.app.icons.new_triangles_from_file(filename)
        brush_icons[brush] = icon_value
              
        
Classes = (
    PIE_MT_Sculpt_Mask,
    PIE_OT_Paint_Mask,
    Box_Mask,
    Lasso_Mask,
    Line_Mask,
    Line_Prj,
    Box_Hide,
)


def release_icons():
    global brush_icons
    for value in brush_icons.values():
        bpy.app.icons.release(value)

classes = (PIE_OT_Paint_Mask, Box_Mask, Lasso_Mask, Line_Mask, Line_Prj, Box_Hide, PIE_OT_Sculpt_Mask_Draw, PIE_MT_Sculpt_Mask,)

shortcuts = ({'keymap': 'Sculpt',
  'space_type': 'EMPTY',
  'type': 'M',
  'value': 'PRESS',
  'menu': 'PIE_MT_sculpt_Mask'},)

def register():
    register_module("MaxPuliero_Pies_5_1.PM_Sculpt_Mask", classes, shortcuts)


def unregister():
    unregister_module("MaxPuliero_Pies_5_1.PM_Sculpt_Mask", classes, shortcuts)


if __name__ == "__main__":
    register()
