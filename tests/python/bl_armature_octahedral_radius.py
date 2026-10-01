# SPDX-License-Identifier: GPL-2.0-or-later
"""Run with the custom Blender build in background mode using --factory-startup."""

import tempfile
import unittest
from pathlib import Path

import bpy


class OctahedralRadiusTest(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        self.armature = bpy.data.armatures.new("RadiusTest")
        self.obj = bpy.data.objects.new("RadiusTest", self.armature)
        bpy.context.collection.objects.link(self.obj)
        bpy.context.view_layer.objects.active = self.obj
        self.obj.select_set(True)
        bpy.ops.object.mode_set(mode='EDIT')
        for name, length in (("Short", 0.2), ("Long", 2.0)):
            bone = self.armature.edit_bones.new(name)
            bone.head = (0.0, 0.0, 0.0)
            bone.tail = (0.0, 0.0, length)
            bone.octahedral_radius = 0.04
        bpy.ops.object.mode_set(mode='POSE')

    def assertMatrixEqual(self, a, b):
        for row_a, row_b in zip(a, b):
            for value_a, value_b in zip(row_a, row_b):
                self.assertAlmostEqual(value_a, value_b, places=6)

    def test_radius_does_not_change_rest_or_evaluated_pose(self):
        pose = self.obj.pose.bones["Long"]
        pose.rotation_mode = 'XYZ'
        pose.rotation_euler = (0.1, 0.2, 0.3)
        constraint = pose.constraints.new('LIMIT_ROTATION')
        constraint.use_limit_x = True
        constraint.min_x = -0.4
        constraint.max_x = 0.4
        constraint.owner_space = 'LOCAL'
        bpy.context.view_layer.update()
        rest = self.armature.bones["Long"].matrix_local.copy()
        evaluated = self.obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
        original_pose = evaluated.pose.bones["Long"].matrix.copy()
        self.armature.bones["Long"].octahedral_radius = 0.3
        bpy.context.view_layer.update()
        evaluated = self.obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
        self.assertMatrixEqual(rest, self.armature.bones["Long"].matrix_local)
        self.assertMatrixEqual(original_pose, evaluated.pose.bones["Long"].matrix)
        self.assertAlmostEqual(self.armature.bones["Long"].length, 2.0, places=6)

    def test_edit_pose_roundtrip_and_length_independence(self):
        self.armature.bones["Short"].octahedral_radius = 0.075
        bpy.ops.object.mode_set(mode='EDIT')
        short = self.armature.edit_bones["Short"]
        self.assertAlmostEqual(short.octahedral_radius, 0.075, places=6)
        short.tail.z = 0.9
        self.assertAlmostEqual(short.octahedral_radius, 0.075, places=6)
        short.octahedral_radius = 0.025
        bpy.ops.object.mode_set(mode='POSE')
        self.assertAlmostEqual(self.armature.bones["Short"].octahedral_radius, 0.025, places=6)
        self.assertAlmostEqual(self.armature.bones["Short"].length, 0.9, places=6)
        self.assertAlmostEqual(self.armature.bones["Long"].octahedral_radius, 0.04, places=6)

    def test_pose_hide_and_reveal_transfer_to_edit(self):
        short = self.obj.pose.bones["Short"]
        short.select = True
        short.hide = True
        bpy.ops.object.mode_set(mode='EDIT')
        edit_bone = self.armature.edit_bones["Short"]
        self.assertTrue(edit_bone.hide)
        self.assertFalse(edit_bone.select)
        self.assertFalse(edit_bone.select_head)
        self.assertFalse(edit_bone.select_tail)
        bpy.ops.armature.reveal(select=False)
        bpy.ops.object.mode_set(mode='POSE')
        self.assertFalse(self.obj.pose.bones["Short"].hide)
        self.assertFalse(self.obj.pose.bones["Short"].select)

    def test_edit_hide_and_pose_reveal_roundtrip(self):
        bpy.ops.object.mode_set(mode='EDIT')
        for bone in self.armature.edit_bones:
            bone.select = bone.select_head = bone.select_tail = False
        short = self.armature.edit_bones["Short"]
        short.select = short.select_head = short.select_tail = True
        bpy.ops.armature.hide(unselected=False)
        self.assertTrue(short.hide)
        bpy.ops.object.mode_set(mode='POSE')
        self.assertTrue(self.obj.pose.bones["Short"].hide)
        self.assertFalse(self.obj.pose.bones["Short"].select)
        self.assertFalse(self.obj.pose.bones["Long"].hide)
        bpy.ops.pose.reveal(select=False)
        bpy.ops.object.mode_set(mode='EDIT')
        self.assertFalse(self.armature.edit_bones["Short"].hide)
        self.assertFalse(self.armature.edit_bones["Short"].select)

    def test_duplicate_keeps_radius(self):
        bpy.ops.object.mode_set(mode='EDIT')
        for bone in self.armature.edit_bones:
            bone.select = bone.select_head = bone.select_tail = False
        short = self.armature.edit_bones["Short"]
        short.select = short.select_head = short.select_tail = True
        self.armature.edit_bones.active = short
        bpy.ops.armature.duplicate()
        duplicate = self.armature.edit_bones["Short.001"]
        self.assertAlmostEqual(duplicate.octahedral_radius, 0.04, places=6)
        bpy.ops.object.mode_set(mode='POSE')
        self.assertAlmostEqual(self.armature.bones["Short.001"].octahedral_radius, 0.04, places=6)

    def test_standard_file_migrates_existing_width(self):
        fixture = Path(__file__).resolve().parents[1] / "files/animation/armature_join_with_action_constraints.blend"
        if not fixture.exists() or fixture.read_bytes().startswith(b"version https://git-lfs"):
            self.skipTest("Standard Blender fixture has not been downloaded")
        bpy.ops.wm.open_mainfile(filepath=str(fixture))
        bones_checked = 0
        for armature in bpy.data.armatures:
            for bone in armature.bones:
                expected = bone.length * 0.1 if bone.length > 0.00001 else 0.000001
                self.assertAlmostEqual(bone.octahedral_radius, expected, places=6)
                bones_checked += 1
        self.assertGreater(bones_checked, 0)

    def test_saved_radius_survives_reopening(self):
        self.armature.bones["Short"].octahedral_radius = 0.123
        with tempfile.TemporaryDirectory() as directory:
            path = str(Path(directory) / "radius.blend")
            bpy.ops.wm.save_as_mainfile(filepath=path)
            bpy.ops.wm.open_mainfile(filepath=path)
            armature = bpy.data.armatures["RadiusTest"]
            self.assertAlmostEqual(armature.bones["Short"].octahedral_radius, 0.123, places=6)
            self.assertAlmostEqual(armature.bones["Long"].octahedral_radius, 0.04, places=6)


if __name__ == "__main__":
    result = unittest.TextTestRunner(verbosity=2).run(
        unittest.defaultTestLoader.loadTestsFromTestCase(OctahedralRadiusTest))
    if not result.wasSuccessful():
        raise SystemExit(1)
