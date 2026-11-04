# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Manage the native preferences lists without executing their entries."""

import bpy
from bpy.props import EnumProperty, IntProperty, PointerProperty, StringProperty
from bpy.types import Operator, PropertyGroup

_EDITOR_ITEMS = tuple(
    (item.identifier, item.name, item.description, item.icon, item.value)
    for item in bpy.types.UserMenu.bl_rna.properties["space_type"].enum_items
    if item.identifier and item.identifier != 'EMPTY'
)
# Native saved context keys follow CTX_data_mode_string, not bpy.context.mode.
_NATIVE_CONTEXTS = (
    'mesh_edit', 'curve_edit', 'surface_edit', 'text_edit', 'armature_edit',
    'mball_edit', 'lattice_edit', 'curves_edit', 'grease_pencil_edit', 'pointcloud_edit',
    'posemode', 'sculpt_mode', 'weightpaint', 'vertexpaint', 'imagepaint', 'particlemode',
    'objectmode', 'greasepencil_paint', 'greasepencil_edit', 'greasepencil_sculpt',
    'greasepencil_weight', 'greasepencil_vertex', 'curves_sculpt', 'grease_pencil_paint',
    'grease_pencil_sculpt', 'grease_pencil_weight', 'grease_pencil_vertex',
)
_MODE_NAMES = {item.value: item.name for item in bpy.types.Context.bl_rna.properties["mode"].enum_items
               if item.identifier}
_CONTEXT_LABELS = {key: _MODE_NAMES.get(index, key.replace('_', ' ').title())
                   for index, key in enumerate(_NATIVE_CONTEXTS)}
_CONTEXT_LABELS.update({
    'ShaderNodeTree': "Shader Editor",
    'GeometryNodeTree': "Geometry Nodes",
    'CompositorNodeTree': "Compositor",
    'TextureNodeTree': "Texture Nodes",
})
# Dynamic enum strings must outlive RNA's references to them.
_context_numbers = {}
_context_items_cache = {}


def _context_number(value):
    return _context_numbers.setdefault(value, len(_context_numbers) + 1)


def _enum_items(values):
    key = tuple(sorted(values))
    if key not in _context_items_cache:
        _context_items_cache[key] = tuple(
            (value, _CONTEXT_LABELS.get(value, value.replace('_', ' ').title()),
             "", _context_number(value))
            for value in key
        )
    return _context_items_cache[key]


def _context_items(self, context):
    values = {self.get("context_key", "objectmode")}
    if context:
        editors = {self.editor}
        if self.editor == 'VIEW_3D':
            editors.update({'TOPBAR', 'PROPERTIES'})
        values.update(menu.context for menu in context.preferences.user_menus
                      if menu.space_type in editors)
    return _enum_items(values)


def _shared_items(self, context):
    values = {self.get("shared_context_key", "objectmode")}
    if context:
        values.update(menu.context for menu in context.preferences.user_menus
                      if menu.space_type == 'TOPBAR')
    return _enum_items(values)


def _context_get(self):
    return _context_number(self.get("context_key", "objectmode"))


def _shared_get(self):
    return _context_number(self.get("shared_context_key", "objectmode"))


def _set_key(self, value, key):
    for identifier, number in _context_numbers.items():
        if number == value:
            self[key] = identifier
            self.active_index = self.topbar_index = self.properties_index = 0
            break


def _context_set(self, value):
    _set_key(self, value, "context_key")


def _shared_set(self, value):
    _set_key(self, value, "shared_context_key")


def _editor_update(self, context):
    contexts = sorted(menu.context for menu in context.preferences.user_menus
                      if menu.space_type == self.editor) if context else []
    default = "ShaderNodeTree" if self.editor == 'NODE_EDITOR' else "objectmode"
    self["context_key"] = default if default in contexts or not contexts else contexts[0]
    self.active_index = self.topbar_index = self.properties_index = 0


class QuickFavoritesState(PropertyGroup):
    """Transient panel selection; native favorites remain in userpref.blend."""

    editor: EnumProperty(name="Editor", items=_EDITOR_ITEMS, default='VIEW_3D',
                         update=_editor_update, options={'SKIP_SAVE'})
    menu_context: EnumProperty(name="Context", items=_context_items,
                               get=_context_get, set=_context_set, options={'SKIP_SAVE'})
    shared_context: EnumProperty(name="Shared Favorites Mode", items=_shared_items,
                                 get=_shared_get, set=_shared_set, options={'SKIP_SAVE'})
    active_index: IntProperty(options={'SKIP_SAVE'})
    topbar_index: IntProperty(options={'SKIP_SAVE'})
    properties_index: IntProperty(options={'SKIP_SAVE'})

    @classmethod
    def register(cls):
        bpy.types.WindowManager.quick_favorites = PointerProperty(
            type=cls, options={'SKIP_SAVE'})

    @classmethod
    def unregister(cls):
        del bpy.types.WindowManager.quick_favorites


def find_menu(context, editor, menu_context):
    return next((menu for menu in context.preferences.user_menus
                 if menu.space_type == editor and menu.context == menu_context), None)


def menu_groups(context):
    state = context.window_manager.quick_favorites
    key = state.get("context_key", "objectmode")
    groups = [(state.editor, key, "active_index")]
    if state.editor != 'TOPBAR':
        shared_key = state.get("shared_context_key", "objectmode") if state.editor == 'NODE_EDITOR' else key
        groups.append(('TOPBAR', shared_key, "topbar_index"))
    if state.editor == 'VIEW_3D':
        groups.append(('PROPERTIES', key, "properties_index"))
    return groups


class PREFERENCES_OT_quick_favorites_show(Operator):
    bl_idname = "preferences.quick_favorites_show"
    bl_label = "Manage Quick Favorites"
    bl_description = "Reorder or remove favorites for this editor and context"
    bl_options = {'INTERNAL'}
    bl_translation_context = '*'

    editor: StringProperty(default='VIEW_3D', options={'HIDDEN', 'SKIP_SAVE'})
    menu_context: StringProperty(default='objectmode', options={'HIDDEN', 'SKIP_SAVE'})
    mode: StringProperty(default='objectmode', options={'HIDDEN', 'SKIP_SAVE'})

    def execute(self, context):
        state = context.window_manager.quick_favorites
        if self.editor in {item[0] for item in _EDITOR_ITEMS}:
            state.editor = self.editor
        state["context_key"] = self.menu_context
        state["shared_context_key"] = self.mode
        state.active_index = state.topbar_index = state.properties_index = 0
        return bpy.ops.screen.userpref_show('INVOKE_DEFAULT', section='INTERFACE')


class QuickFavoriteItemOperator:
    editor: StringProperty(options={'HIDDEN'})
    menu_context: StringProperty(options={'HIDDEN'})
    index: IntProperty(min=0, options={'HIDDEN'})
    active_property: StringProperty(default="active_index", options={'HIDDEN'})

    def target_menu(self, context):
        return find_menu(context, self.editor, self.menu_context)


class PREFERENCES_OT_quick_favorite_move(QuickFavoriteItemOperator, Operator):
    bl_idname = "preferences.quick_favorite_move"
    bl_label = "Move Quick Favorite"
    bl_description = "Change the order of favorites in this list"
    bl_options = {'INTERNAL'}
    bl_translation_context = '*'

    direction: EnumProperty(items=(('UP', "Up", ""), ('DOWN', "Down", "")))

    def execute(self, context):
        menu = self.target_menu(context)
        target = self.index + (-1 if self.direction == 'UP' else 1)
        if menu is None or not (0 <= self.index < len(menu.entries) and 0 <= target < len(menu.entries)):
            return {'CANCELLED'}
        menu.entries.move(self.index, target)
        if self.active_property in {"active_index", "topbar_index", "properties_index"}:
            setattr(context.window_manager.quick_favorites, self.active_property, target)
        return {'FINISHED'}


class PREFERENCES_OT_quick_favorite_remove(QuickFavoriteItemOperator, Operator):
    bl_idname = "preferences.quick_favorite_remove"
    bl_label = "Remove Quick Favorite"
    bl_description = "Remove this entry from Quick Favorites"
    bl_options = {'INTERNAL'}
    bl_translation_context = '*'

    def execute(self, context):
        menu = self.target_menu(context)
        if menu is None or not (0 <= self.index < len(menu.entries)):
            return {'CANCELLED'}
        menu.entries.remove(menu.entries[self.index])
        if self.active_property in {"active_index", "topbar_index", "properties_index"}:
            setattr(context.window_manager.quick_favorites, self.active_property,
                    max(0, min(self.index, len(menu.entries) - 1)))
        return {'FINISHED'}


classes = (
    QuickFavoritesState,
    PREFERENCES_OT_quick_favorites_show,
    PREFERENCES_OT_quick_favorite_move,
    PREFERENCES_OT_quick_favorite_remove,
)
