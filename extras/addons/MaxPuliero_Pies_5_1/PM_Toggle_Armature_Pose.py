import bpy

if __package__:
    from ._registration import register_module, unregister_module
else:
    from MaxPuliero_Pies_5_1._registration import register_module, unregister_module

from bpy.types import Menu

bl_info = {
    "name": "Pose/Rest Toggle // Hotkey: P",
    "description": "Pose/Rest PM",
    "author": "Max Puliero",
    "version": (1, 0),
    "blender": (4, 3, 0),
    "location": "3D View",
    "category": "Max Puliero Pies"
}



class ArmaturePose(bpy.types.Operator):
    bl_idname = "armature.pose"
    bl_label = "Pose Position"
    def execute(self, context): 
        bpy.context.object.data.pose_position = 'POSE'
        return {'FINISHED'}
    
class ArmatureRest(bpy.types.Operator):
    bl_idname = "armature.rest"
    bl_label = "Rest Position"
    def execute(self, context): 
        bpy.context.object.data.pose_position = 'REST'
        return {'FINISHED'}


class ArmaturePoseToggle(Menu):
    bl_label = "Pose"
    bl_idname = "VIEW3D_MT_pose_pm"

    def draw(self, context):
        layout = self.layout
        selected_objects = context.selected_objects
        armature_selected = any(obj.type == 'ARMATURE' for obj in selected_objects)

        if armature_selected:
            pie = layout.menu_pie()
            # left
            pie.operator("armature.rest", icon="OUTLINER_OB_ARMATURE")
            # right
            pie.operator("armature.pose", icon="ARMATURE_DATA")
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

classes = (ArmaturePose, ArmatureRest, ArmaturePoseToggle,)

shortcuts = ({'keymap': '3D View',
  'space_type': 'VIEW_3D',
  'type': 'P',
  'value': 'PRESS',
  'menu': 'VIEW3D_MT_pose_pm'},)

def register():
    register_module("MaxPuliero_Pies_5_1.PM_Toggle_Armature_Pose", classes, shortcuts)


def unregister():
    unregister_module("MaxPuliero_Pies_5_1.PM_Toggle_Armature_Pose", classes, shortcuts)


