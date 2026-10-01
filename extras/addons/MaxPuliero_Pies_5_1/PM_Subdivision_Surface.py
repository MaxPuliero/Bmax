import bpy

if __package__:
    from ._registration import register_module, unregister_module
else:
    from MaxPuliero_Pies_5_1._registration import register_module, unregister_module


bl_info = {
    "name": "Subdivision Surface Modifier // Hotkey: 1",
    "description": "Subdivision Surface (Multi-Object Support)",
    "author": "Max Puliero",
    "version": (1, 4),
    "blender": (4, 3, 0),
    "location": "3D View",
    "category": "Max Puliero Pies"
}

def get_last_subdiv_modifier(obj):
    last_subdiv = None
    if obj and getattr(obj, "type", "") == 'MESH':
        for mod in obj.modifiers:
            if mod.type == 'SUBSURF':
                last_subdiv = mod
    return last_subdiv


class AddSubdivMod(bpy.types.Operator):
    bl_idname = "object.add_subdiv_surf_mod"
    bl_label = "Add Subdivision Surface Modifier"

    def execute(self, context):
        for obj in context.selected_objects:
            if obj.type == 'MESH':
                subdiv_mod = get_last_subdiv_modifier(obj)
                if not subdiv_mod:
                    subdiv_mod = obj.modifiers.new(name="Subdivision", type='SUBSURF')
                    subdiv_mod.levels = 1
                subdiv_mod.use_limit_surface = False
        return {'FINISHED'}


class AddOneLv(bpy.types.Operator):
    bl_idname = "object.add_one_lv"
    bl_label = "Subdivision +1"

    def execute(self, context):
        for obj in context.selected_objects:
            if obj.type == 'MESH':
                subdiv_mod = get_last_subdiv_modifier(obj)
                if subdiv_mod:
                    subdiv_mod.levels += 1
                    subdiv_mod.use_limit_surface = False
                else:
                    subdiv_mod = obj.modifiers.new(name="Subdivision", type='SUBSURF')
                    subdiv_mod.levels = 1
                    subdiv_mod.use_limit_surface = False
        return {'FINISHED'}


class RemoveSubdiv(bpy.types.Operator):
    bl_idname = "object.remove_subdiv_surf"
    bl_label = "Remove Subdivision"

    def execute(self, context):
        for obj in context.selected_objects:
            if obj.type == 'MESH':
                subdiv_mod = get_last_subdiv_modifier(obj)
                if subdiv_mod:
                    obj.modifiers.remove(subdiv_mod)
        return {'FINISHED'}


class SubRelativeSubdiv(bpy.types.Operator):
    bl_idname = "object.sub_subdiv_surf"
    bl_label = "Subdivision -1"

    def execute(self, context):
        for obj in context.selected_objects:
            if obj.type == 'MESH':
                subdiv_mod = get_last_subdiv_modifier(obj)
                if subdiv_mod:
                    if subdiv_mod.levels > 0:
                        subdiv_mod.levels -= 1
                    subdiv_mod.use_limit_surface = False
        return {'FINISHED'}


class ZeroSubdiv(bpy.types.Operator):
    bl_idname = "object.zero_subdiv"
    bl_label = "Subdivision at 0"

    def execute(self, context):
        for obj in context.selected_objects:
            if obj.type == 'MESH':
                subdiv_mod = get_last_subdiv_modifier(obj)
                if subdiv_mod:
                    subdiv_mod.levels = 0
        return {'FINISHED'}


class Subdivsurf(bpy.types.Menu):
    bl_label = "Subdivision Surface"
    bl_idname = "VIEW3D_MT_subdivsurf_pm"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()

        pie.operator("object.sub_subdiv_surf")
        pie.operator("object.add_one_lv")
        pie.operator("object.remove_subdiv_surf")
        pie.operator("object.add_subdiv_surf_mod")
        pie.separator()
        pie.separator()
        pie.operator("object.zero_subdiv")
        pie.separator()

classes = (AddSubdivMod, RemoveSubdiv, SubRelativeSubdiv, ZeroSubdiv, Subdivsurf, AddOneLv,)

shortcuts = ({'keymap': 'Object Mode',
  'space_type': 'EMPTY',
  'type': 'ONE',
  'value': 'PRESS',
  'menu': 'VIEW3D_MT_subdivsurf_pm'},
 {'keymap': 'Mesh',
  'space_type': 'EMPTY',
  'type': 'ONE',
  'value': 'PRESS',
  'menu': 'VIEW3D_MT_subdivsurf_pm'})

def register():
    register_module("MaxPuliero_Pies_5_1.PM_Subdivision_Surface", classes, shortcuts)


def unregister():
    unregister_module("MaxPuliero_Pies_5_1.PM_Subdivision_Surface", classes, shortcuts)


if __name__ == "__main__":
    register()