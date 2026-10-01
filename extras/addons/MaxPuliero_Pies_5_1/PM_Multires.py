import bpy

if __package__:
    from ._registration import register_module, unregister_module
else:
    from MaxPuliero_Pies_5_1._registration import register_module, unregister_module


bl_info = {
    "name": "Multiresolution Modifier // Hotkey: 2",
    "description": "Multiresolution",
    "author": "Max Puliero",
    "version": (1, 0),
    "blender": (4, 3, 0),
    "location": "3D View",
    "category": "Max Puliero Pies"
}

class AddRelativeMultires(bpy.types.Operator):
    bl_idname = "add.subdiv_surf"
    bl_label = "Add Multiresolution"

    def execute(self, context):
        if context.active_object and context.active_object.type == 'MESH':
            has_multires_modifier = False
            for modifier in context.active_object.modifiers:
                if modifier.type == 'MULTIRES':
                    has_multires_modifier = True
                    break

            # Add or subdivide MultiRes modifier based on its presence
            if has_multires_modifier:
                bpy.context.object.modifiers["Multires"].sculpt_levels += 1
                bpy.context.object.modifiers["Multires"].render_levels += 1
                bpy.context.object.modifiers["Multires"].levels += 1
            else:
                bpy.ops.object.modifier_add(type='MULTIRES')
                bpy.ops.object.multires_subdivide(modifier="Multires", mode='CATMULL_CLARK')
        return {'FINISHED'}

class SubdivMultires(bpy.types.Operator):
    bl_idname = "multires.subdiv"
    bl_label = "Subdivide"

    def execute(self, context):
        bpy.ops.object.multires_subdivide(modifier="Multires", mode='CATMULL_CLARK')
        return {'FINISHED'}
    
class DeleteMultires(bpy.types.Operator):
    bl_idname = "multires.delete"
    bl_label = "Delete Multires"

    def execute(self, context):
        bpy.ops.object.modifier_remove(modifier="Multires")
        return {'FINISHED'}

class SubRelativeMultires(bpy.types.Operator):
    bl_idname = "mp_pies.multires_level_decrease"
    bl_label = "Multires -1"

    def execute(self, context):
        if context.active_object and context.active_object.type == 'MESH':
            for modifier in context.active_object.modifiers:
                if modifier.type == 'MULTIRES':
                    modifier.levels -= 1
                    modifier.sculpt_levels -= 1
                    break
        return {'FINISHED'}

class ZeroMultires(bpy.types.Operator):
    bl_idname = "zero.multires"
    bl_label = "Multires at 0"

    def execute(self, context):
        bpy.context.object.modifiers["Multires"].levels = 0
        bpy.context.object.modifiers["Multires"].sculpt_levels = 0
        return {'FINISHED'}

class Multires(bpy.types.Menu):
    bl_label = "Multiresolution"
    bl_idname = "VIEW3D_MT_multires_pm"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()

        # left
        pie.operator("mp_pies.multires_level_decrease")
        # right
        pie.operator("multires.subdiv")
        # bottom
        pie.operator("multires.delete")
        # top
        pie.operator("add.subdiv_surf")
        # top/left
        pie.separator()
        # top/right
        pie.separator()
        # bottom/left
        pie.operator("zero.multires")
        # bottom/right
        pie.separator()

classes = (AddRelativeMultires, SubdivMultires, DeleteMultires, SubRelativeMultires, ZeroMultires, Multires,)

shortcuts = ({'keymap': 'Object Mode',
  'space_type': 'EMPTY',
  'type': 'TWO',
  'value': 'PRESS',
  'menu': 'VIEW3D_MT_multires_pm'},
 {'keymap': 'Sculpt',
  'space_type': 'EMPTY',
  'type': 'TWO',
  'value': 'PRESS',
  'menu': 'VIEW3D_MT_multires_pm'})

def register():
    register_module("MaxPuliero_Pies_5_1.PM_Multires", classes, shortcuts)


def unregister():
    unregister_module("MaxPuliero_Pies_5_1.PM_Multires", classes, shortcuts)


if __name__ == "__main__":
    register()