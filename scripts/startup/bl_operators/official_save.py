# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Save a compatible copy in an isolated Blender process."""

import bpy
from bpy.props import BoolProperty, EnumProperty, StringProperty
from bpy.types import Operator
from bpy_extras.io_utils import ExportHelper


def instance_random_nodes():
    found = []
    seen = set()

    def visit(tree):
        if tree is None or tree.as_pointer() in seen:
            return
        seen.add(tree.as_pointer())
        for node in tree.nodes:
            if node.bl_idname == 'ShaderNodeObjectInfo' and node.use_instance_random:
                found.append(tree.name + ' / ' + node.name)
            if node.type == 'GROUP':
                visit(node.node_tree)

    for owner in (*bpy.data.materials, *bpy.data.worlds):
        visit(owner.node_tree)
    for tree in bpy.data.node_groups:
        visit(tree)
    return found


def instance_random_viewports():
    return sum(space.shading.use_instance_random and
               (space.shading.color_type == 'RANDOM' or space.shading.wireframe_color_type == 'RANDOM')
               for screen in bpy.data.screens for area in screen.areas for space in area.spaces
               if space.type == 'VIEW_3D')


def _bake_normals(mesh):
    import numpy as np
    if mesh.normal_weighting_mode == 'UNWEIGHTED':
        return False
    if mesh.loops and not mesh.has_custom_normals:
        normals = np.empty(len(mesh.loops) * 3, dtype=np.float32)
        mesh.corner_normals.foreach_get('vector', normals)
        mesh.normals_split_custom_set(normals.reshape((-1, 3)))
    mesh.normal_weighting_mode = 'UNWEIGHTED'
    mesh.normal_weight = 50
    mesh.normal_weight_threshold = 0.01
    mesh.update()
    return True


def _snapshot_images(directory):
    """Transfer unsaved pixels without packing or saving the live image datablocks."""
    from array import array
    from pathlib import Path
    records = []
    for image in bpy.data.images:
        if not image.is_dirty or image.source == 'VIEWER':
            continue
        if image.source not in {'FILE', 'GENERATED'} or image.is_multiview:
            raise RuntimeError('Save modified tiled, animated or multiview images first: ' + image.name)
        width, height = image.size
        if not width or not height:
            raise RuntimeError('Cannot read modified image: ' + image.name)
        pixels = array('f', [0.0]) * len(image.pixels)
        image.pixels.foreach_get(pixels)
        path = Path(directory) / ('pixels-%d.bin' % len(records))
        with path.open('wb') as handle:
            pixels.tofile(handle)
        records.append(dict(name=image.name, size=[width, height], pixels=str(path),
                            count=len(pixels), is_float=image.is_float,
                            colorspace=image.colorspace_settings.name, alpha_mode=image.alpha_mode))
    return records


def _restore_images(records):
    from array import array
    for record in records:
        image = bpy.data.images.get(record['name'])
        if image is None:
            raise RuntimeError('Modified image missing from snapshot: ' + record['name'])
        if list(image.size) != record['size'] or len(image.pixels) != record['count']:
            old = image
            image = bpy.data.images.new(record['name'] + '_official_pixels',
                                        width=record['size'][0], height=record['size'][1],
                                        alpha=True, float_buffer=record['is_float'])
            old.user_remap(image)
            bpy.data.images.remove(old)
            image.name = record['name']
        image.colorspace_settings.name = record['colorspace']
        # Blender float pixel buffers are associated-alpha scene data. Tag the
        # embedded EXR accordingly so reloading does not premultiply them again.
        image.alpha_mode = ('PREMUL' if record['is_float'] and
                            record['alpha_mode'] in {'STRAIGHT', 'PREMUL'} else record['alpha_mode'])
        pixels = array('f')
        with open(record['pixels'], 'rb') as handle:
            pixels.fromfile(handle, record['count'])
        image.pixels.foreach_set(pixels)
        image.update()
        image.file_format = 'OPEN_EXR' if record['is_float'] else 'PNG'
        image.pack()


def _localize_copy():
    # Native Make Local All also links unused library objects into the scene.
    # Remove only those new collection memberships again on this disposable copy.
    collections = list(bpy.data.collections) + [scene.collection for scene in bpy.data.scenes]
    membership = {collection.as_pointer(): set(collection.objects) for collection in collections}
    selection = [(layer, layer.objects.active,
                  [(obj, obj.select_get(view_layer=layer)) for obj in layer.objects])
                 for scene in bpy.data.scenes for layer in scene.view_layers]
    if bpy.data.libraries:
        bpy.ops.object.make_local(type='ALL')
        for collection in list(bpy.data.collections) + [scene.collection for scene in bpy.data.scenes]:
            old = membership.get(collection.as_pointer(), set())
            for obj in list(collection.objects):
                if obj not in old:
                    collection.objects.unlink(obj)
        for layer, active, objects in selection:
            for obj, selected in objects:
                if obj.name in layer.objects:
                    obj.select_set(selected, view_layer=layer)
            if active is not None and active.name in layer.objects:
                layer.objects.active = active
    for mesh in bpy.data.meshes:
        if mesh.library is not None and mesh.users:
            raise RuntimeError('Cannot localize linked mesh for conversion: ' + mesh.name)


def _leave_modes():
    for scene in bpy.data.scenes:
        for layer in scene.view_layers:
            for obj in list(layer.objects):
                if obj.mode != 'OBJECT':
                    with bpy.context.temp_override(scene=scene, view_layer=layer,
                                                   object=obj, active_object=obj):
                        bpy.ops.object.mode_set(mode='OBJECT')


def _color_meshes(level, bake_normals, cancel):
    """Create static evaluated meshes, retaining all color attributes and materials."""
    result = []
    preserved = set()
    seen = set()
    scenes = list(bpy.data.scenes)
    in_scenes = {obj.as_pointer() for scene in scenes for obj in scene.objects}
    detached = [obj for obj in bpy.data.objects if obj.as_pointer() not in in_scenes and
                obj.type == 'MESH' and obj.data.has_multires_color and
                any(mod.type == 'MULTIRES' for mod in obj.modifiers)]
    fallback = None
    if detached:
        fallback = bpy.data.scenes.new('Official Color Evaluation')
        source_scene = bpy.context.scene
        fallback.render.fps = source_scene.render.fps
        fallback.render.fps_base = source_scene.render.fps_base
        fallback.unit_settings.scale_length = source_scene.unit_settings.scale_length
        for obj in detached:
            fallback.collection.objects.link(obj)
        fallback.frame_set(source_scene.frame_current, subframe=source_scene.frame_subframe)
        scenes.append(fallback)
    for scene in scenes:
        layer = scene.view_layers[0]
        collection_flags = [(c, c.hide_viewport) for c in bpy.data.collections]
        object_flags = [(obj, obj.hide_viewport) for obj in scene.objects]
        layer_flags = []

        def reveal(item):
            layer_flags.append((item, item.exclude, item.hide_viewport))
            item.exclude = False
            item.hide_viewport = False
            for child in item.children:
                reveal(child)

        reveal(layer.layer_collection)
        for collection, _hidden in collection_flags:
            collection.hide_viewport = False
        for obj, _hidden in object_flags:
            obj.hide_viewport = False
        layer.update()
        try:
            for obj in list(scene.objects):
                if obj.as_pointer() in seen or obj.type != 'MESH' or not obj.data.has_multires_color:
                    continue
                multires = next((mod for mod in obj.modifiers if mod.type == 'MULTIRES'), None)
                if multires is None:
                    continue
                cancel()
                seen.add(obj.as_pointer())
                old_level, old_visible = multires.levels, multires.show_viewport
                old_hidden = obj.hide_get(view_layer=layer)
                try:
                    obj.hide_set(False, view_layer=layer)
                    multires.show_viewport = True
                    multires.levels = (multires.total_levels if level == 'HIGHEST' else
                                       multires.render_levels if level == 'RENDER' else old_level)
                    with bpy.context.temp_override(scene=scene, view_layer=layer):
                        layer.update()
                        depsgraph = bpy.context.evaluated_depsgraph_get()
                        evaluated = obj.evaluated_get(depsgraph)
                        mesh = bpy.data.meshes.new_from_object(
                            evaluated, preserve_all_data_layers=True, depsgraph=depsgraph)
                        matrix = evaluated.matrix_world.copy()
                    mesh.name = obj.data.name + '_OfficialColor'
                    if bake_normals:
                        _bake_normals(mesh)
                    mesh.prepare_official(bake_sculpt_layers=True, discard_multires_color=True)
                    copy = bpy.data.objects.new(obj.name + '_OfficialColor', mesh)
                    copy.matrix_world = matrix
                    result.append(copy)
                    preserved.add(obj.data.as_pointer())
                finally:
                    multires.levels, multires.show_viewport = old_level, old_visible
                    obj.hide_set(old_hidden, view_layer=layer)
        finally:
            for item, excluded, hidden in reversed(layer_flags):
                item.exclude, item.hide_viewport = excluded, hidden
            for collection, hidden in collection_flags:
                collection.hide_viewport = hidden
            for obj, hidden in object_flags:
                obj.hide_viewport = hidden
    if fallback is not None:
        bpy.data.scenes.remove(fallback)
    return result, preserved


def run_worker(job_path):
    import json
    import os
    import traceback
    from pathlib import Path
    job_path = Path(job_path)
    directory = job_path.parent
    job = json.loads(job_path.read_text(encoding='utf-8'))
    stages = []
    sidecar = None
    committed = False

    def cancel():
        if (directory / 'cancel').exists():
            raise InterruptedError('Save as Official cancelled')

    def stage(folder):
        import uuid
        path = Path(folder) / ('.bmax-official-' + job['token'] + '-' + uuid.uuid4().hex + '.blend')
        stages.append(path)
        (directory / 'stages.json').write_text(json.dumps([str(p) for p in stages]), encoding='utf-8')
        return path

    try:
        cancel()
        bpy.context.preferences.filepaths.save_version = 0
        bpy.data.use_autopack = False
        _leave_modes()
        _localize_copy()
        _restore_images(job['images'])
        warnings = []
        nodes = instance_random_nodes()
        if instance_random_viewports():
            warnings.append('Viewport Instances Random colors also use standard behavior in official Blender.')
        if nodes:
            warnings.append('INSTANCE RANDOM COLORS WILL CHANGE in official Blender. '
                            'Object Info > Random > Instances is not converted (%d nodes).' % len(nodes))
        if not job['bake_layers'] and any(mesh.sculpt_layers for mesh in bpy.data.meshes):
            warnings.append('Sculpt Layers were not converted and are not editable in official Blender.')
        # Validate/materialize layers before evaluating a high-resolution copy.
        # Pack standard displacement buffers when conversion needs their samples.
        for obj in bpy.data.objects:
            if obj.type != 'MESH':
                continue
            mod = next((m for m in obj.modifiers if m.type == 'MULTIRES'), None)
            if mod is not None and mod.is_external and (job['pack_resources'] or
                                                        (job['bake_layers'] and obj.data.sculpt_layers)):
                with bpy.context.temp_override(object=obj, active_object=obj):
                    bpy.ops.object.multires_external_pack()
        layers = 0
        if job['bake_layers']:
            for mesh in bpy.data.meshes:
                if mesh.library is None and mesh.sculpt_layers:
                    cancel()
                    mesh.prepare_official(bake_sculpt_layers=True, discard_multires_color=False)
                    layers += 1
        colored, preserved = (_color_meshes(job['color_level'], job['bake_normals'], cancel)
                              if job['color_copy'] else ([], set()))
        backup_meshes = {obj.data.as_pointer() for obj in colored}
        normals = grids = omitted_color = 0
        for mesh in list(bpy.data.meshes):
            if mesh.library or mesh.as_pointer() in backup_meshes:
                continue
            cancel()
            grids += int(mesh.has_multires_color)
            omitted_color += int(mesh.has_multires_color and mesh.users and mesh.as_pointer() not in preserved)
            mesh.prepare_official(bake_sculpt_layers=False, discard_multires_color=True)
            normals += int(job['bake_normals'] and _bake_normals(mesh))
        if omitted_color:
            warnings.append('High-resolution Multires color is not in the official copy; '
                            'only its base color attributes remain. The Bmax source is unchanged.')
        if not job['bake_normals'] and any(mesh.normal_weighting_mode != 'UNWEIGHTED' for mesh in bpy.data.meshes):
            warnings.append('Bmax normal weighting was not converted; shading can change in official Blender.')
        if job['pack_resources']:
            bpy.ops.file.pack_all()
        destination = Path(job['filepath'])
        destination.parent.mkdir(parents=True, exist_ok=True)
        backup_scene = None
        if colored:
            value = job['color_directory']
            if not value:
                folder = destination.parent / (destination.stem + '_multires_color')
            elif value.startswith('//'):
                folder = destination.parent / value[2:]
            else:
                folder = Path(value)
                if not folder.is_absolute():
                    folder = destination.parent / folder
            folder.mkdir(parents=True, exist_ok=True)
            sidecar = folder / (destination.stem + '_multires_color.blend')
            index = 1
            while sidecar.exists() or sidecar == destination:
                sidecar = folder / (destination.stem + '_multires_color_%03d.blend' % index)
                index += 1
            backup_scene = bpy.data.scenes.new('Official Multires Color')
            for obj in colored:
                backup_scene.collection.objects.link(obj)
            for view_layer in backup_scene.view_layers:
                with bpy.context.temp_override(scene=backup_scene, view_layer=view_layer):
                    view_layer.update()
            color_stage = stage(folder)
            bpy.data.libraries.write(str(color_stage), {backup_scene}, path_remap='RELATIVE_ALL', compress=True)
            # Partial library files have no active scene. Finalize in another
            # isolated process so opening the color .blend shows its mesh scene.
            import subprocess
            subprocess.run([bpy.app.binary_path, '--background', '--factory-startup',
                            '--disable-autoexec', '--threads', '2', '--python-exit-code', '1',
                            str(color_stage), '--python', str(Path(__file__).resolve()), '--',
                            '--finalize-color', backup_scene.name], check=True,
                           creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            cancel()
            # Keep the backup scene and geometry out of the main official file.
            bpy.data.scenes.remove(backup_scene)
            for obj in colored:
                mesh = obj.data
                bpy.data.objects.remove(obj, do_unlink=True)
                if not mesh.users:
                    bpy.data.meshes.remove(mesh)
        main_stage = stage(destination.parent)
        cancel()
        bpy.ops.wm.save_as_mainfile(filepath=str(main_stage), check_existing=False,
                                   relative_remap=True, compress=job['compress'])
        cancel()
        if sidecar:
            # Do not overwrite an existing backup, even if another job created it meanwhile.
            with sidecar.open('xb'):
                pass
            try:
                os.replace(color_stage, sidecar)
            except Exception:
                sidecar.unlink(missing_ok=True)
                raise
        try:
            os.replace(main_stage, destination)
        except Exception:
            if sidecar:
                sidecar.unlink(missing_ok=True)
            raise
        committed = True
        result = dict(status='ok', filepath=str(destination), color_file=str(sidecar) if sidecar else '',
                      normals=normals, layers=layers, color_meshes=len(backup_meshes), warnings=warnings)
    except Exception as error:
        result = dict(status='cancelled' if isinstance(error, InterruptedError) else 'error',
                      error=str(error), traceback=traceback.format_exc())
    finally:
        for path in stages:
            path.unlink(missing_ok=True)
    (directory / 'result.json').write_text(json.dumps(result, ensure_ascii=False), encoding='utf-8')
    if not committed and result['status'] != 'cancelled':
        raise RuntimeError(result['error'])


class WM_OT_save_as_official(Operator, ExportHelper):
    bl_idname = 'wm.save_as_official'
    bl_label = 'Save as Official'
    bl_description = 'Save a compatible .blend copy without changing the open Bmax project'
    bl_translation_context = '*'
    filename_ext = '.blend'

    filter_glob: StringProperty(default='*.blend', options={'HIDDEN'})
    bake_normals: BoolProperty(name='Bake Bmax Normals', default=True,
                               description='Store Bmax base-mesh normal weighting as standard custom normals')
    bake_layers: BoolProperty(name='Bake Sculpt Layers', default=True,
                              description='Merge the visible sculpt into native Multires geometry, preserving subdivision levels')
    color_copy: BoolProperty(name='Separate Multires Color Copy', default=True,
                             description='Save static high-resolution colored meshes in a separate .blend file')
    color_level: EnumProperty(name='Color Copy Level', default='HIGHEST', items=(
        ('HIGHEST', 'Highest Available', 'Preserve the finest stored detail'),
        ('RENDER', 'Render Level', 'Use each Multires modifier render level'),
        ('VIEWPORT', 'Viewport Level', 'Use each Multires modifier viewport level')))
    color_directory: StringProperty(name='Color Copy Folder', subtype='DIR_PATH',
                                    options={'PATH_SUPPORTS_BLEND_RELATIVE'},
                                    description='Empty uses a folder beside the official copy; relative paths use that copy as base')
    pack_resources: BoolProperty(name='Pack External Resources', default=True,
                                  description='Pack supported external resources into the copies')
    compress: BoolProperty(name='Compress', default=True)

    def invoke(self, context, _event):
        import os
        base = os.path.splitext(bpy.data.filepath)[0] if bpy.data.filepath else 'Untitled'
        self.filepath = base + '_official.blend'
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def draw(self, _context):
        layout = self.layout
        layout.prop(self, 'bake_normals')
        layout.prop(self, 'bake_layers')
        layout.prop(self, 'color_copy')
        sub = layout.column()
        sub.enabled = self.color_copy
        sub.prop(self, 'color_level', text='Level')
        sub.prop(self, 'color_directory', text='Folder')
        layout.prop(self, 'pack_resources')
        layout.prop(self, 'compress')
        if instance_random_nodes() or instance_random_viewports():
            box = layout.box()
            box.alert = True
            box.label(text='INSTANCE RANDOM WILL CHANGE', icon='ERROR')
            box.label(text='Official Blender will use its')
            box.label(text='standard Random colors.')
            box.label(text='Materials are not converted.')
        if self.color_copy:
            layout.label(text='Color copy: static evaluated meshes', icon='INFO')
        elif any(mesh.has_multires_color for mesh in bpy.data.meshes):
            layout.label(text='High-resolution Multires color will be omitted', icon='ERROR')
        if not self.bake_layers and any(mesh.sculpt_layers for mesh in bpy.data.meshes):
            layout.label(text='Sculpt Layers are unsupported in official Blender', icon='ERROR')
        if not self.bake_normals and any(mesh.normal_weighting_mode != 'UNWEIGHTED' for mesh in bpy.data.meshes):
            layout.label(text='Bmax normal weighting may look different', icon='ERROR')
        layout.label(text='Bmax project stays unchanged', icon='INFO')

    def execute(self, context):
        import json
        import os
        import secrets
        import subprocess
        import tempfile
        from pathlib import Path
        if getattr(WM_OT_save_as_official, '_busy', False):
            self.report({'ERROR'}, 'Another Save as Official operation is running')
            return {'CANCELLED'}
        destination = Path(bpy.path.ensure_ext(os.path.abspath(self.filepath), '.blend'))
        if not self.filepath or not destination.name:
            self.report({'ERROR'}, 'Choose an official copy filename')
            return {'CANCELLED'}
        if bpy.data.filepath and os.path.normcase(os.path.realpath(destination)) == os.path.normcase(os.path.realpath(bpy.data.filepath)):
            self.report({'ERROR'}, 'Choose a different filename to preserve the Bmax original')
            return {'CANCELLED'}
        self._directory = Path(tempfile.mkdtemp(prefix='bmax-official-save-'))
        self._timer = None
        self._process = None
        WM_OT_save_as_official._busy = True
        try:
            images = _snapshot_images(self._directory)
            snapshot = self._directory / 'source.blend'
            bpy.ops.wm.save_as_mainfile(filepath=str(snapshot), copy=True, check_existing=False,
                                       relative_remap=True, compress=False, bmax_snapshot=True)
            job = dict(filepath=str(destination), token=secrets.token_hex(12), images=images,
                       bake_normals=self.bake_normals, bake_layers=self.bake_layers,
                       color_copy=self.color_copy, color_level=self.color_level,
                       color_directory=self.color_directory, pack_resources=self.pack_resources,
                       compress=self.compress)
            (self._directory / 'job.json').write_text(json.dumps(job, ensure_ascii=False), encoding='utf-8')
            environment = os.environ.copy()
            for key, folder in [('BLENDER_USER_CONFIG', 'config'), ('BLENDER_USER_SCRIPTS', 'scripts'),
                                ('BLENDER_USER_EXTENSIONS', 'extensions')]:
                path = self._directory / folder
                path.mkdir()
                environment[key] = str(path)
            command = [bpy.app.binary_path, '--background', '--factory-startup', '--disable-autoexec',
                       '--threads', '2', '--python-exit-code', '1', str(snapshot), '--python',
                       str(Path(__file__).resolve()), '--', str(self._directory / 'job.json')]
            self._log = (self._directory / 'worker.log').open('wb')
            self._process = subprocess.Popen(command, env=environment, stdout=self._log,
                                              stderr=subprocess.STDOUT,
                                              creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            WM_OT_save_as_official._busy = True
            if bpy.app.background:
                self._process.wait()
                return self._finish(context)
            self._timer = context.window_manager.event_timer_add(0.3, window=context.window)
            context.window_manager.modal_handler_add(self)
            context.window_manager.progress_begin(0, 100)
            return {'RUNNING_MODAL'}
        except Exception as error:
            self.report({'ERROR'}, str(error))
            self._cleanup(context)
            return {'CANCELLED'}

    def modal(self, context, event):
        if event.type == 'ESC':
            (self._directory / 'cancel').touch()
            return {'RUNNING_MODAL'}
        if event.type == 'TIMER' and self._process.poll() is not None:
            return self._finish(context)
        return {'PASS_THROUGH'}

    def _finish(self, context):
        import json
        try:
            path = self._directory / 'result.json'
            if not path.exists():
                raise RuntimeError('Official conversion failed; the original project was not replaced')
            result = json.loads(path.read_text(encoding='utf-8'))
            if result['status'] == 'cancelled':
                self.report({'INFO'}, 'Save as Official cancelled')
                return {'CANCELLED'}
            if result['status'] != 'ok':
                raise RuntimeError(result.get('error', 'Official conversion failed'))
            self.report({'INFO'}, 'Saved official copy: %d normal conversions, %d sculpt stacks, %d color meshes' %
                        (result['normals'], result['layers'], result['color_meshes']))
            if result['color_file']:
                self.report({'INFO'}, 'Multires color copy: ' + result['color_file'])
            for warning in result['warnings']:
                self.report({'WARNING'}, warning)
            return {'FINISHED'}
        except Exception as error:
            self.report({'ERROR'}, str(error))
            return {'CANCELLED'}
        finally:
            self._cleanup(context)

    def _cleanup(self, context):
        import json
        import shutil
        if getattr(self, '_timer', None) is not None:
            context.window_manager.event_timer_remove(self._timer)
            self._timer = None
            context.window_manager.progress_end()
        if getattr(self, '_log', None) is not None:
            self._log.close()
        if getattr(self, '_directory', None) is not None:
            # A crashed worker may leave a staged output. Delete only files with
            # this job's random ownership prefix, never an existing user output.
            try:
                job = json.loads((self._directory / 'job.json').read_text(encoding='utf-8'))
                manifest = self._directory / 'stages.json'
                if manifest.exists():
                    from pathlib import Path
                    for value in json.loads(manifest.read_text(encoding='utf-8')):
                        path = Path(value)
                        if path.name.startswith('.bmax-official-' + job['token'] + '-') and path.suffix == '.blend':
                            path.unlink(missing_ok=True)
            except (OSError, ValueError, KeyError):
                pass
            try:
                shutil.rmtree(self._directory)
            except OSError:
                self.report({'WARNING'}, 'Could not remove temporary files: ' + str(self._directory))
            self._directory = None
        WM_OT_save_as_official._busy = False


classes = ((WM_OT_save_as_official,) if 'prepare_official' in bpy.types.Mesh.bl_rna.functions and
           'bmax_snapshot' in bpy.ops.wm.save_as_mainfile.get_rna_type().properties else ())

if __name__ == '__main__':
    import sys
    if '--finalize-color' in sys.argv:
        scene = bpy.data.scenes.get(sys.argv[-1])
        if scene is None or not bpy.context.window_manager.windows:
            raise RuntimeError('Cannot activate the Multires color scene')
        for window in bpy.context.window_manager.windows:
            window.scene = scene
        bpy.context.preferences.filepaths.save_version = 0
        bpy.data.use_autopack = False
        bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath, check_existing=False,
                                   relative_remap=False, compress=True)
    else:
        run_worker(sys.argv[-1])
