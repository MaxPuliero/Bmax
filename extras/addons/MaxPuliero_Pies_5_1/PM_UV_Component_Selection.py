import bpy

if __package__:
    from ._registration import register_module, unregister_module
else:
    from MaxPuliero_Pies_5_1._registration import register_module, unregister_module

from bpy.types import Menu

bl_info = {
    "name": "UV Component Selection // Hotkey: RMB",
    "description": "UV Component Selection PM (Sync Supported)",
    "author": "Max Puliero",
    "version": (1, 1),
    "blender": (5, 0, 0),
    "location": "3D View",
    "category": "Max Puliero Pies"
}


class UVCompSel(bpy.types.Menu):
    bl_label = "UV Component Selection"
    bl_idname = "UV_MT_uv_component_selection_pm"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        
        # Accesso diretto ai tool settings
        # In Blender 5.0 la path standard è context.tool_settings
        ts = context.tool_settings
        is_sync = ts.use_uv_select_sync

        # --- LEFT (Vertex/Points) ---
        if is_sync:
            # In Sync usa la selezione Mesh
            pie.operator("mesh.select_mode", text="Vert (Sync)", icon="VERTEXSEL").type = 'VERT'
        else:
            # In UV mode usa la selezione UV
            pie.operator("uv.select_mode", text="Points", icon="VERTEXSEL").type = 'VERTEX'

        # --- RIGHT (Select Linked) ---
        # Select Linked funziona generalmente in entrambi i casi
        pie.operator("uv.select_linked", text="Select Linked", icon="UV_ISLANDSEL")

        # --- BOTTOM (Face) ---
        if is_sync:
            pie.operator("mesh.select_mode", text="Face (Sync)", icon="FACESEL").type = 'FACE'
        else:
            pie.operator("uv.select_mode", text="Face", icon="FACESEL").type = 'FACE'

        # --- TOP (Edge) ---
        if is_sync:
            pie.operator("mesh.select_mode", text="Edge (Sync)", icon="EDGESEL").type = 'EDGE'
        else:
            pie.operator("uv.select_mode", text="Edge", icon="EDGESEL").type = 'EDGE'

        # --- TOP/LEFT (Separator) ---
        pie.separator()

        # --- TOP/RIGHT (Island) ---
        if is_sync:
            # La modalità 'ISLAND' non esiste per la mesh. 
            # Fallback su FACE o disabilita. Qui ho messo FACE.
            op = pie.operator("mesh.select_mode", text="Island (N/A in Sync)", icon="UV_ISLANDSEL")
            op.type = 'FACE' 
        else:
            pie.operator("uv.select_mode", text="Island", icon="UV_ISLANDSEL").type = 'ISLAND'

        # --- BOTTOM/LEFT (Search) ---
        pie.operator("wm.search_menu", text="Search", icon="VIEWZOOM")

        # --- BOTTOM/RIGHT (Context Menu) ---
        pie.operator("wm.call_menu", text="Context Menu", icon="ALIGN_JUSTIFY").name = 'IMAGE_MT_uvs_context_menu'

classes = (UVCompSel,)

shortcuts = ({'keymap': 'UV Editor',
  'space_type': 'EMPTY',
  'type': 'RIGHTMOUSE',
  'value': 'PRESS',
  'menu': 'UV_MT_uv_component_selection_pm'},)

def register():
    register_module("MaxPuliero_Pies_5_1.PM_UV_Component_Selection", classes, shortcuts)


def unregister():
    unregister_module("MaxPuliero_Pies_5_1.PM_UV_Component_Selection", classes, shortcuts)


if __name__ == "__main__":
    register()
    # bpy.ops.wm.call_menu_pie(name="UV_MT_uv_component_selection_pm")