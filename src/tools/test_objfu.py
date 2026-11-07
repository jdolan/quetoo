import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
import pygame

from md3 import Md3Model, Md3Surface
from md3fu import Animation
from objfu import (ConfigPreview, FlyCamera, ITEM_MAXS, ITEM_MINS, default_config, find_models, ground_config, item_layout,
                   load_muzzle, model_config, parse_model_config, replace_model_section, save_muzzle,
                   load_player, main, model_bounds, orbit_offset, pan_delta, parse_mesh_config, pick_box, resolve_obj_args, transform_vertices, view_coverage, view_guide, view_reference, view_reference_models, view_references, view_silhouette, view_transform, view_vertical_fov)


class ObjfuConfigTest(unittest.TestCase):
  def test_preview_deselect_keeps_document_and_process(self):
    preview = ConfigPreview.__new__(ConfigPreview)
    preview.index = 2
    preview.selected = True
    preview.running = True
    with patch.object(preview, "connection", create=True) as connection:
      preview.deselect()
      self.assertFalse(preview.selected)
      self.assertTrue(preview.running)
      self.assertEqual(preview.index, 2)
      connection.send.assert_not_called()
      preview.select(2)
      self.assertTrue(preview.selected)
      connection.send.assert_called_once_with(("select", 2))

  def test_fly_camera_movement(self):
    camera = FlyCamera((0, 0, 0), 0, 0, 100)
    camera.move(1, 0, .5)
    np.testing.assert_allclose(camera.position, (0, -20, 0), atol=1e-10)
    camera.move(0, 1, .25, fast=True)
    np.testing.assert_allclose(camera.position, (-240, -20, 0), atol=1e-10)
    start = camera.position.copy()
    camera.move(1, 1, 1)
    self.assertAlmostEqual(np.linalg.norm(camera.position - start), 240)
    other = FlyCamera((0, 0, 0), 0, 0, 100)
    for _ in range(10):
      other.move(1, 0, .05)
    np.testing.assert_allclose(other.position, (0, -20, 0), atol=1e-10)

  def test_fly_camera_fine_movement_on_large_grid(self):
    camera = FlyCamera((0, 0, 0), 0, 0, 2000)
    start = camera.position.copy()
    camera.move(1, 0, 1 / 60)
    self.assertAlmostEqual(np.linalg.norm(camera.position - start), 240 / 60)
    camera.event(pygame.event.Event(pygame.MOUSEWHEEL, y=-100))
    start = camera.position.copy()
    camera.move(1, 0, 1)
    self.assertAlmostEqual(np.linalg.norm(camera.position - start), 1)

  def test_fly_camera_right_button_and_focus_release(self):
    camera = FlyCamera((0, 0, 0), 0, 0, 100)
    with patch("pygame.event.set_grab") as grab, patch("pygame.mouse.set_visible") as visible, \
         patch("pygame.mouse.get_rel"):
      camera.event(pygame.event.Event(pygame.MOUSEBUTTONDOWN, button=1))
      self.assertFalse(camera.looking)
      camera.event(pygame.event.Event(pygame.MOUSEBUTTONDOWN, button=3))
      self.assertTrue(camera.looking)
      grab.assert_called_with(True)
      visible.assert_called_with(False)
      camera.event(pygame.event.Event(pygame.MOUSEMOTION, rel=(10, 1000)))
      self.assertEqual(camera.pitch, 85)
      camera.event(pygame.event.Event(pygame.WINDOWFOCUSLOST))
      self.assertFalse(camera.looking)
      grab.assert_called_with(False)
      visible.assert_called_with(True)
      camera.event(pygame.event.Event(pygame.MOUSEMOTION, rel=(10, -1000)))
      self.assertEqual(camera.pitch, 85)
      camera.event(pygame.event.Event(pygame.MOUSEBUTTONDOWN, button=3))
      camera.event(pygame.event.Event(pygame.MOUSEBUTTONUP, button=3))
      self.assertFalse(camera.looking)

  def test_fly_camera_keyboard_focus_and_save_shortcut(self):
    camera = FlyCamera((0, 0, 0), 0, 0, 100)
    keys = {pygame.K_w: True, pygame.K_s: False, pygame.K_a: False, pygame.K_d: False}
    with patch("pygame.key.get_focused", return_value=True) as focused, \
         patch("pygame.key.get_pressed", return_value=keys), \
         patch("pygame.key.get_mods", return_value=0) as mods:
      camera.update(.5)
      np.testing.assert_allclose(camera.position, (0, -20, 0), atol=1e-10)
      start = camera.position.copy()
      focused.return_value = False
      camera.update(.5)
      np.testing.assert_allclose(camera.position, start)
      focused.return_value = True
      mods.return_value = pygame.KMOD_CTRL
      camera.update(.5)
      np.testing.assert_allclose(camera.position, start)

  def test_fly_camera_mouse_turns_view_toward_motion(self):
    for yaw in (0, 35, 135, 270):
      for dx in (-10, 10):
        camera = FlyCamera((0, 0, 0), yaw, 15, 100)
        camera.looking = True
        forward = -orbit_offset(yaw, 15, 1)
        right = np.cross(forward, (0, 0, 1))
        camera.event(pygame.event.Event(pygame.MOUSEMOTION, rel=(dx, 0)))
        with patch("objfu.gluLookAt") as look:
          camera.apply()
        values = look.call_args.args
        direction = np.array(values[3:6]) - np.array(values[:3])
        self.assertGreater(np.dot(direction, right) * dx, 0)

  def test_model_config_shared_muzzle_and_isolated_update(self):
    text = ('// author notes\n{\n"type" "view"\n"scale" "2"\n"translate" "3 4 5"\n}\n'
            '{\n"type" "link"\n"translate" "6 7 8"\n}\n'
            '{\n"type" "muzzle"\n"origin" "20 0 20.5"\n}\n')
    configs = parse_model_config(text)
    self.assertEqual(configs["view"].muzzle, (20, 0, 20.5))
    self.assertEqual(configs["link"].muzzle, configs["view"].muzzle)
    np.testing.assert_allclose(transform_vertices(configs["view"].transform, [(0, 0, 0)]), [(6, 8, 10)])
    changed = replace_model_section(text, "world", '"scale" "0.8"')
    self.assertTrue(changed.startswith(text.rstrip()))
    with tempfile.TemporaryDirectory() as temp:
      root = Path(temp)
      (root / "model.cfg").write_text(text)
      save_muzzle(root, (21, 1, 20.5))
      self.assertEqual(load_muzzle(root), (21, 1, 20.5))
      result = (root / "model.cfg").read_text()
      self.assertEqual(result[:result.index('"type" "muzzle"')], text[:text.index('"type" "muzzle"')])
      self.assertEqual(list(root.iterdir()), [root / "model.cfg"])
    for invalid in ('{\n"type" "view"\n', '{ "type" "bad" }',
                    '{ "type" "view" }\n{ "type" "view" }',
                    '{ "type" "link" "muzzle" "1 2 3" }',
                    '{ "type" "muzzle" "origin" "1 2" }'):
      with self.subTest(invalid=invalid), self.assertRaises(ValueError):
        parse_model_config(invalid)

  def test_ordered_transforms_and_muzzle(self):
    config = parse_mesh_config('translate "10 20 30"\nrotate 0 0 90\nscale "2"\nmuzzle 1 0 0')
    np.testing.assert_allclose(transform_vertices(config.transform, [(1, 0, 0)]),
                               [(10, 22, 30)], atol=1e-12)
    self.assertEqual(config.muzzle, (1, 0, 0))
    reverse = parse_mesh_config("scale 2\nrotate 0 0 90\ntranslate 10 20 30")
    np.testing.assert_allclose(transform_vertices(reverse.transform, [(1, 0, 0)]),
                               [(-40, 22, 60)], atol=1e-12)

  def test_rotation_axes_and_sequence(self):
    for angles, vertex, expected in (
        ("90 0 0", (0, 1, 0), (0, 0, 1)),
        ("0 90 0", (0, 0, 1), (1, 0, 0)),
        ("0 0 90", (1, 0, 0), (0, 1, 0)),
        ("90 90 0", (0, 0, 1), (1, 0, 0))):
      with self.subTest(angles=angles):
        np.testing.assert_allclose(
          transform_vertices(parse_mesh_config(f"rotate {angles}").transform, [vertex]),
          [expected], atol=1e-12)

  def test_comments_and_invalid_edits(self):
    config = parse_mesh_config('// comment\ntranslate "1 2 3"\nscale 2 // end')
    np.testing.assert_allclose(transform_vertices(config.transform, [(1, 1, 1)]), [(3, 4, 5)])
    for text in ('translate "1 2', "rotate 90 0", "scale nan", "scale inf",
                 "translate 1 2\n3", "scale 1 2", "scalex 2",
                 "/* scale 2 */"):
      with self.subTest(text=text), self.assertRaises(ValueError):
        parse_mesh_config(text)
    np.testing.assert_array_equal(parse_mesh_config('scale "0.8 0.8 0.8"').transform,
                                  np.diag([0.8, 0.8, 0.8, 1]))
    self.assertEqual(parse_mesh_config('muzzle "1 2 3 4"').muzzle, (1, 2, 3))

  def test_integer_and_decimal_forms_are_equivalent(self):
    texts = (
      'translate "4 0 -24"\nscale ".85"\nmuzzle "24 0 20"',
      "translate 4.0 0.0 -24.0\nscale 0.85\nmuzzle 24.0 0.0 20.0",
      'translate "4. 0 -24.0"\nscale "0.850"\nmuzzle "24 0. 20.0"',
    )
    configs = [parse_mesh_config(text) for text in texts]
    for config in configs[1:]:
      np.testing.assert_array_equal(config.transform, configs[0].transform)
      self.assertEqual(config.muzzle, configs[0].muzzle)
    for text in ("scale 1", 'scale "1.0"', 'scale "1."'):
      np.testing.assert_array_equal(parse_mesh_config(text).transform, np.eye(4))

  def test_generated_positions_default_to_integers(self):
    verts = [(-2.2, -4.1, 3.25), (24.3, 4.0, 20.7)]
    self.assertEqual(default_config(verts, "world"), 'translate "0 0 0"\nscale "1"\n')
    self.assertTrue(default_config(verts, "view").endswith('muzzle "24 0 12"\n'))
    grounded = ground_config("scale .85\n", verts)
    self.assertTrue(grounded.startswith("translate 0 0 -19\n"))
    self.assertLessEqual(abs(16 + min(v[2] for v in transform_vertices(
      parse_mesh_config(grounded).transform, verts))), 0.5)

  def test_grounding_preserves_transform_and_text(self):
    text = "// retain comment\nrotate 90 0 0\nscale 2\ntranslate 1 2 3\nmuzzle 4 5 6\n"
    verts = [(0, -2, -3), (2, 4, 5)]
    before = transform_vertices(parse_mesh_config(text).transform, verts)
    grounded = ground_config(text, verts)
    self.assertTrue(grounded.endswith(text))
    config = parse_mesh_config(grounded)
    after = transform_vertices(config.transform, verts)
    self.assertAlmostEqual(min(v[2] for v in after), -16)
    np.testing.assert_allclose(np.asarray(after)[:, :2], np.asarray(before)[:, :2])
    self.assertEqual(config.muzzle, (4, 5, 6))

  def test_player_standing_frames_tag_hierarchy_and_floor(self):
    identity = ((0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1))
    torso_tag = ((0, 0, 10), *identity[1:])
    weapon_tag = ((5, 2, 3), (0, 1, 0), (-1, 0, 0), (0, 0, 1))
    surface = Md3Surface("skin", [(0, 1, 2)],
                         [[(0, 0, -2), (1, 0, -2), (0, 1, -2)]],
                         [(0, 0), (1, 0), (0, 1)])
    lower = Md3Model(1, [surface], [{"tag_torso": torso_tag}])
    upper = Md3Model(1, [surface], [{"tag_head": identity, "tag_weapon": weapon_tag}])
    head = Md3Model(1, [surface], [{}])
    animations = [Animation("LEGS_IDLE", 0, 1, 1, 1), Animation("TORSO_STAND1", 0, 1, 1, 1)]
    with tempfile.TemporaryDirectory() as temp:
      player = Path(temp)
      (player / "model.cfg").write_text(replace_model_section("", "world", "scale 2\n"))
      with patch("objfu.parse_animation_cfg", return_value=animations), \
           patch("objfu.parse_skin", return_value={"skin": "players/test/skin"}), \
           patch("objfu.read_md3", side_effect=[lower, upper, head]):
        parts, anchor = load_player(player)
    self.assertEqual(min(v[2] for verts, _, _ in parts for v in verts), 0)
    np.testing.assert_allclose(transform_vertices(anchor, [(1, 0, 0)]), [(10, 6, 30)])
    self.assertEqual(parts[0][2]["players/test/skin"], [[(0, 0), (1, 1), (2, 2)]])
    self.assertEqual(parts[0][1], [(0, 1), (1, 1), (0, 0)])

  def test_first_person_fov_matches_reference_aspect_rules(self):
    self.assertAlmostEqual(view_vertical_fov(90, 1), 90)
    self.assertAlmostEqual(view_vertical_fov(90, 2), view_vertical_fov(90, 16 / 9))

  def test_view_handedness_changes_only_lateral_translation(self):
    original = parse_mesh_config('translate "12 -8 -24"\nrotate 5 10 15\nscale .75').transform
    for hand, y in (("right", -8), ("left", 8), ("center", 0)):
      transformed = view_transform(original, hand)
      expected = original.copy()
      expected[1, 3] = y
      np.testing.assert_array_equal(transformed, expected)
    self.assertEqual(original[1, 3], -8)

  def test_cli_selects_config_and_sibling_data_root(self):
    with tempfile.TemporaryDirectory() as temp:
      model = Path(temp) / "tris.obj"
      model.write_text("v 0 0 0\n")
      with patch("sys.argv", ["objfu", str(model), "--config", "world"]), \
           patch("objfu.__file__", "/work/quetoo/src/tools/objfu.py"), \
           patch("objfu.edit_config") as editor:
        main()
      args, paths, roots = editor.call_args.args
      self.assertEqual(args.config, "world")
      self.assertEqual(paths, [model.resolve()])
      self.assertEqual(roots, [Path("/work/quetoo-data/target/default")])

  def test_model_globs_resolve_directories_across_roots(self):
    with tempfile.TemporaryDirectory() as temp:
      first, second = Path(temp) / "a", Path(temp) / "b"
      for root, name in ((first, "ammo/quake2_shells"), (second, "ammo/quake2_shells"),
                         (second, "armor/quake2_body"), (second, "ammo/shells")):
        (root / "models" / name).mkdir(parents=True)
        (root / "models" / name / "tris.obj").write_text("v 0 0 0\n")
      matches = find_models("*quake2*", [first, second])
      self.assertEqual(matches, [(first / "models/ammo/quake2_shells/tris.obj").resolve(),
                                 (second / "models/armor/quake2_body/tris.obj").resolve()])
      self.assertEqual(find_models("models/ammo/*", [second])[1].parent.name, "shells")
      paths = resolve_obj_args(["armor/quake2_body", "*quake2*"], [first, second])
      self.assertEqual([path.parent.name for path in paths], ["quake2_body", "quake2_shells"])
      with self.assertRaises(SystemExit):
        resolve_obj_args(["*missing*"], [first])
      with patch("sys.argv", ["objfu", "-p", str(second), "*quake2*"]), self.assertRaises(SystemExit):
        main()
      for mode in ("view", "link"):
        with patch("sys.argv", ["objfu", "-p", str(second), "*quake2*", "--config", mode]), \
             patch("objfu.edit_config") as editor:
          main()
        self.assertEqual([path.parent.name for path in editor.call_args.args[1]], ["quake2_shells", "quake2_body"])

  def test_item_layout_rests_origins_and_clears_models(self):
    origins, spacing, columns, rows = item_layout([[(-40, 0, 0), (40, 0, 0)]] * 5)
    self.assertEqual((columns, rows), (3, 2))
    self.assertGreaterEqual(spacing, 80)
    self.assertTrue(all(z == -ITEM_MINS[2] for _, _, z in origins))
    self.assertEqual(len(set(origins)), 5)

  def test_picking_selects_nearest_box_on_ray(self):
    boxes = [(np.add((100, 0, 16), ITEM_MINS), np.add((100, 0, 16), ITEM_MAXS)),
             (np.add((50, 0, 16), ITEM_MINS), np.add((50, 0, 16), ITEM_MAXS))]
    self.assertEqual(pick_box(boxes, (0, 0, 20), (1, 0, 0)), 1)
    self.assertEqual(pick_box(boxes, (200, 0, 20), (-1, 0, 0)), 0)
    self.assertIsNone(pick_box(boxes, (0, 0, 100), (1, 0, 0)))
    self.assertIsNone(pick_box(boxes, (0, 0, 20), (-1, 0, 0)))

  def test_model_bounds_follow_configuration(self):
    verts = [(-2, -4, 0), (6, 4, 10)]
    mins, maxs = model_bounds(parse_mesh_config("translate 0 0 -16\nscale 2").transform, verts)
    np.testing.assert_allclose(mins, (-4, -8, -16))
    np.testing.assert_allclose(maxs, (12, 8, 4))

  def test_pan_follows_cursor_in_screen_plane(self):
    yaw, pitch, distance, fov, height = 135, 15, 160, 50, 800
    eye = orbit_offset(yaw, pitch, distance)
    right = pan_delta(yaw, pitch, distance, fov, height, -100, 0)
    down = pan_delta(yaw, pitch, distance, fov, height, 0, 100)
    self.assertAlmostEqual(np.dot(right, eye), 0)
    self.assertAlmostEqual(np.dot(down, eye), 0)
    self.assertAlmostEqual(right[2], 0)
    self.assertGreater(down[2], 0)
    self.assertAlmostEqual(np.linalg.norm(right), 200 * distance * np.tan(np.radians(25)) / height)

  def test_view_reference_discovery_and_config_changes(self):
    with tempfile.TemporaryDirectory() as temp:
      root = Path(temp)
      for name in ("shotgun", "quake_shotgun", "quake2_shotgun"):
        weapon = root / "models" / "weapons" / name
        weapon.mkdir(parents=True)
        (weapon / "tris.obj").write_text("v 10 -2 -2\nv 10 2 -2\nv 10 0 2\nf 1 2 3\n")
        (weapon / "model.cfg").write_text(replace_model_section("", "view", "translate 5 0 0\n"))
      current = root / "models/weapons/quake2_shotgun/tris.obj"
      first = view_references([root, root], current)
      self.assertEqual(len(first), 1)
      self.assertEqual(first[0][0][0], [15, -2, -2])
      (root / "models/weapons/shotgun/model.cfg").write_text(replace_model_section("", "view", "translate 20 0 0\n"))
      second = view_references([root], current)
      self.assertEqual(second[0][0][0], [30, -2, -2])
      self.assertFalse(np.array_equal(view_silhouette(first, 100, 100, 90, "center"),
                                      view_silhouette(second, 100, 100, 90, "center")))
      with self.assertRaisesRegex(ValueError, "No Quetoo"):
        view_references([root], root / "models/weapons/shotgun/tris.obj")

  def test_view_guide_excludes_only_the_selected_reference(self):
    with tempfile.TemporaryDirectory() as temp:
      root = Path(temp)
      for name, y in (("shotgun", -5), ("machinegun", 5)):
        weapon = root / "models" / "weapons" / name
        weapon.mkdir(parents=True)
        (weapon / "tris.obj").write_text(f"v 10 {y - 2} -2\nv 10 {y + 2} -2\nv 10 {y} 2\nf 1 2 3\n")
      models = view_reference_models([root])
      coverages = {model.parent.name: view_coverage(view_reference(model), 100, 100, 90, "center")
                   for model in models}
      both = view_guide(list(coverages.values()))
      without = view_guide([coverages["machinegun"]])
      self.assertTrue(np.array_equal(both, view_silhouette(view_references([root], root / "x.obj"), 100, 100, 90,
                                                           "center")))
      self.assertTrue(np.array_equal(without, view_silhouette(
        view_references([root], root / "models/weapons/shotgun/tris.obj"), 100, 100, 90, "center")))
      self.assertGreater(without[:, :, 3].max(), both[:, :, 3].max())

  def test_silhouette_averages_weapons_not_triangle_density(self):
    verts = [(10, -2, -2), (10, 2, -2), (10, 0, 2)]
    empty = [(-10, -2, -2), (-10, 2, -2), (-10, 0, 2)]
    single = view_silhouette([(verts, [[0, 1, 2]])], 100, 100, 90, "center")
    duplicate = view_silhouette([(verts, [[0, 1, 2]] * 4)], 100, 100, 90, "center")
    np.testing.assert_array_equal(single, duplicate)
    self.assertEqual(single[:, :, 3].max(), 150)
    average = view_silhouette([(verts, [[0, 1, 2]]), (empty, [[0, 1, 2]])],
                              100, 100, 90, "center")
    self.assertEqual(average[:, :, 3].max(), 75)
    right = view_silhouette([(verts, [[0, 1, 2]])], 100, 100, 90, "right")
    left = view_silhouette([(verts, [[0, 1, 2]])], 100, 100, 90, "left")
    self.assertGreater(np.nonzero(right[:, :, 3])[1].mean(),
                       np.nonzero(left[:, :, 3])[1].mean())

  def test_silhouette_clips_triangles_crossing_camera_plane(self):
    pixels = view_silhouette([([(10, -1, -1), (10, 1, -1), (-1, 0, 1)], [[0, 1, 2]])],
                             100, 100, 90, "center")
    self.assertGreater(np.count_nonzero(pixels[:, :, 3]), 0)


if __name__ == "__main__":
  unittest.main()
