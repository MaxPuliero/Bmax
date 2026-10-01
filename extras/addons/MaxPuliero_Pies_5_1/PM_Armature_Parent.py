import bpy

if __package__:
    from ._registration import register_module, unregister_module
else:
    from MaxPuliero_Pies_5_1._registration import register_module, unregister_module

from bpy.types import Menu

bl_info = {
    "name": "Armature Parent // Hotkey: P",
    "description": "Armature Edit Mode Parent PM",
    "author": "Max Puliero",
    "version": (1, 0),
    "blender": (5, 1, 0),
    "location": "3D View > Armature Edit Mode",
    "category": "Max Puliero Pies"
}


class ArmatureParentSetConnected(bpy.types.Operator):
    bl_idname = "armature.parent_set_connected_pm"
    bl_label = "Make Parent - Connected"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_ARMATURE'

    def execute(self, context):
        bpy.ops.armature.parent_set(type='CONNECTED')
        return {'FINISHED'}


class ArmatureParentSetOffset(bpy.types.Operator):
    bl_idname = "armature.parent_set_offset_pm"
    bl_label = "Make Parent - Keep Offset"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_ARMATURE'

    def execute(self, context):
        bpy.ops.armature.parent_set(type='OFFSET')
        return {'FINISHED'}


class ArmatureParentClear(bpy.types.Operator):
    bl_idname = "armature.parent_clear_pm"
    bl_label = "Clear Parent"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_ARMATURE'

    def execute(self, context):
        bpy.ops.armature.parent_clear(type='CLEAR')
        return {'FINISHED'}


class ArmatureBoneDisconnect(bpy.types.Operator):
    bl_idname = "armature.bone_disconnect_pm"
    bl_label = "Disconnect Bone"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_ARMATURE'

    def execute(self, context):
        bpy.ops.armature.parent_clear(type='DISCONNECT')
        return {'FINISHED'}


class ArmatureSeparateBone(bpy.types.Operator):
    bl_idname = "armature.separate_bone_pm"
    bl_label = "Separate Bone Into New Armature"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_ARMATURE'

    def execute(self, context):
        bpy.ops.armature.separate()
        return {'FINISHED'}


class ArmatureParent(Menu):
    bl_label = "Armature Parent"
    bl_idname = "VIEW3D_MT_armature_parent_pm"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        # left
        pie.operator("armature.parent_set_offset_pm", icon="CON_CHILDOF")
        # right
        pie.operator("armature.parent_set_connected_pm", icon="LINKED")
        # bottom
        pie.operator("armature.bone_disconnect_pm", icon="UNLINKED")
        # top
        pie.operator("armature.parent_clear_pm", icon="X")
        # top/left
        pie.operator("armature.separate_bone_pm", icon="ARMATURE_DATA")
        # top/right
        pie.separator()
        # bottom/left
        pie.separator()
        # bottom/right
        pie.separator()

classes = (ArmatureParentSetConnected, ArmatureParentSetOffset, ArmatureParentClear, ArmatureBoneDisconnect, ArmatureSeparateBone, ArmatureParent,)

shortcuts = ({'keymap': 'Armature',
  'space_type': 'EMPTY',
  'type': 'P',
  'value': 'PRESS',
  'menu': 'VIEW3D_MT_armature_parent_pm'},)

def register():
    register_module("MaxPuliero_Pies_5_1.PM_Armature_Parent", classes, shortcuts)


def unregister():
    unregister_module("MaxPuliero_Pies_5_1.PM_Armature_Parent", classes, shortcuts)


if __name__ == "__main__":
    register()

