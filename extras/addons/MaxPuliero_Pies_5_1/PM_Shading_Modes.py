import bpy

if __package__:
    from ._registration import register_module, unregister_module
else:
    from MaxPuliero_Pies_5_1._registration import register_module, unregister_module

from bpy.types import Menu

bl_info = {
    "name": "Shading Modes // Hotkey: ALT+Z",
    "description": "Shading PM",
    "author": "Max Puliero",
    "version": (1, 1),
    "blender": (4, 3, 0),
    "location": "3D View",
    "category": "Max Puliero Pies"
}

class ShadeSolid(bpy.types.Operator):
    bl_idname = "shade.solid"
    bl_label = "Solid"
    def execute(self, context): 
        bpy.context.space_data.shading.type = 'SOLID'
        return {'FINISHED'}
    
class ShadeWire(bpy.types.Operator):
    bl_idname = "shade.wire"
    bl_label = "Wireframe"
    def execute(self, context): 
        bpy.context.space_data.shading.type = 'WIREFRAME'
        return {'FINISHED'}

class ShadeLookDev(bpy.types.Operator):
    bl_idname = "shade.lookdev"
    bl_label = "Look Dev"
    def execute(self, context): 
        bpy.context.space_data.shading.type = 'MATERIAL'
        return {'FINISHED'}

class Shaderender(bpy.types.Operator):
    bl_idname = "shade.render"
    bl_label = "Render"
    def execute(self, context): 
        bpy.context.space_data.shading.type = 'RENDERED'
        return {'FINISHED'}
    
class Wireontop(bpy.types.Operator):
    bl_idname = "wire.on_top"
    bl_label = "Wire on Top"
    def execute(self, context): 
        context.space_data.overlay.show_wireframes = not context.space_data.overlay.show_wireframes
        return {'FINISHED'}

class ShadeSmooth(bpy.types.Operator):
    bl_idname = "shade.smooth"
    bl_label = "Shade Smooth"
    def execute(self, context): 
        if bpy.context.object.mode == 'OBJECT':
            bpy.ops.object.shade_smooth()
        elif bpy.context.object.mode == 'EDIT':
            bpy.ops.mesh.faces_shade_smooth()
        return {'FINISHED'}

class Overlays(bpy.types.Operator):
    bl_idname = "overlay.toggle"
    bl_label = "Overlays"
    def execute(self, context): 
        bpy.context.space_data.overlay.show_overlays = not bpy.context.space_data.overlay.show_overlays
        return {'FINISHED'}

class Shading(Menu):
    bl_label = "Shading"
    bl_idname = "VIEW3D_MT_shading_pm"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        # left
        pie.operator("shade.wire", icon="SHADING_WIRE")
        # right
        pie.operator("shade.render", icon="SHADING_RENDERED")
        # bottom
        pie.operator("wire.on_top", icon="MESH_GRID")
        # top
        pie.operator("shade.solid", icon="SHADING_SOLID")
        # top/left
        pie.separator()
        # top/right
        pie.operator("shade.lookdev", icon="MATERIAL")
        # bottom/left
        pie.operator("shade.smooth", icon="ANTIALIASED")
        # bottom/right
        pie.operator("overlay.toggle", icon="OVERLAY")

classes = (ShadeSolid, ShadeWire, ShadeLookDev, Shaderender, Wireontop, ShadeSmooth, Overlays, Shading,)

shortcuts = ({'keymap': '3D View',
  'space_type': 'VIEW_3D',
  'type': 'Z',
  'value': 'PRESS',
  'alt': True,
  'menu': 'VIEW3D_MT_shading_pm'},)

def register():
    register_module("MaxPuliero_Pies_5_1.PM_Shading_Modes", classes, shortcuts)


def unregister():
    unregister_module("MaxPuliero_Pies_5_1.PM_Shading_Modes", classes, shortcuts)


if __name__ == "__main__":
    register()

    # bpy.ops.wm.call_menu_pie(name="VIEW3D_MT_shading_pm")
