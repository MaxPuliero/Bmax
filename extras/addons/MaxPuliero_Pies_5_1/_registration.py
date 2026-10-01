"""Shared registration for Max Puliero pie modules."""
import bpy

_MODULES = locals().get('_MODULES', {})
_KEY_DEFAULTS = {
    'any': False, 'shift': False, 'ctrl': False, 'alt': False, 'oskey': False,
    'key_modifier': 'NONE', 'direction': 'ANY', 'repeat': False,
}


def _matching_item(item, shortcut):
    return (item.idname == 'wm.call_menu_pie' and
            getattr(item.properties, 'name', '') == shortcut['menu'] and
            item.type == shortcut['type'] and item.value == shortcut['value'] and
            all(getattr(item, key) == shortcut.get(key, default)
                for key, default in _KEY_DEFAULTS.items()))


def _ensure_shortcuts(shortcuts):
    wm = bpy.context.window_manager
    kc = wm.keyconfigs.addon if wm else None
    if kc is None:
        return
    for shortcut in shortcuts:
        km = kc.keymaps.get(shortcut['keymap'])
        if km is None:
            km = kc.keymaps.new(name=shortcut['keymap'],
                                space_type=shortcut.get('space_type', 'EMPTY'))
        matching = [item for item in km.keymap_items if _matching_item(item, shortcut)]
        if matching:
            # These menu names belong to this package; collapse duplicate canonical bindings.
            for duplicate in matching[1:]:
                km.keymap_items.remove(duplicate)
            continue
        kwargs = {key: shortcut[key] for key in _KEY_DEFAULTS if key in shortcut}
        item = km.keymap_items.new('wm.call_menu_pie', shortcut['type'],
                                  shortcut['value'], **kwargs)
        item.properties.name = shortcut['menu']


def _remove_shortcuts(shortcuts):
    wm = bpy.context.window_manager
    kc = wm.keyconfigs.addon if wm else None
    if kc is None:
        return
    menus = {shortcut['menu'] for shortcut in shortcuts}
    for km in kc.keymaps:
        for item in list(km.keymap_items):
            if (item.idname == 'wm.call_menu_pie' and
                    getattr(item.properties, 'name', '') in menus):
                km.keymap_items.remove(item)


def register_module(name, classes, shortcuts):
    previous = _MODULES.get(name)
    if previous:
        old_classes, old_shortcuts = previous
        if tuple(classes) == old_classes and all(cls.is_registered for cls in classes):
            _ensure_shortcuts(shortcuts)
            return
        unregister_module(name, old_classes, old_shortcuts)
    registered = []
    _MODULES[name] = (tuple(registered), tuple(shortcuts))
    try:
        for cls in classes:
            if not cls.is_registered:
                bpy.utils.register_class(cls)
            registered.append(cls)
            _MODULES[name] = (tuple(registered), tuple(shortcuts))
        _ensure_shortcuts(shortcuts)
    except Exception:
        unregister_module(name, registered, shortcuts)
        raise


def unregister_module(name, classes, shortcuts):
    registered, registered_shortcuts = _MODULES.pop(name, (tuple(classes), tuple(shortcuts)))
    errors = []
    # Always remove our shortcuts before touching classes, so a class error cannot strand them.
    try:
        _remove_shortcuts(registered_shortcuts)
    except Exception as error:
        errors.append(error)
    for cls in reversed(registered):
        try:
            if cls.is_registered:
                bpy.utils.unregister_class(cls)
        except Exception as error:
            errors.append(error)
    if errors:
        for error in errors:
            print('Max Puliero Pies unregister error:', name, error)
        raise RuntimeError('Could not completely unregister ' + name) from errors[0]
