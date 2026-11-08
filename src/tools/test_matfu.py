import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

import numpy as np
from PIL import Image

import matfu


class MatfuBatchTest(unittest.TestCase):
  def test_diffuse_height_filters_match_gui(self):
    with tempfile.TemporaryDirectory() as directory:
      parent = Path(directory)
      y, x = np.indices((16, 16))
      diffuse = np.stack([(x * 17), (y * 17), ((x + y) * 8)], axis=2).astype(np.uint8)
      path = parent / "skin.png"
      Image.fromarray(diffuse).save(path)
      params = {"h_strength": 0.4, "h_freq_low_gain": 0.7, "h_freq_high_gain": 1.3}
      normal, height = matfu.normals_from_diffuse(diffuse, 1.0, 0.5, 1.0, 1.0)
      for enabled in (True, False):
        with self.subTest(generate_height=enabled):
          preset = parent / "preset.json"
          preset.write_text(json.dumps({"params": params, "generate_height": enabled,
                                       "height_convention": "Heightmap"}))
          args = SimpleNamespace(preset=str(preset), save="normal,height,spec",
                                 directory=None, files=[str(path)], filter=None,
                                 from_diffuse=True, dry_run=False)
          with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(matfu.cmd_batch(args), 0)
          output = parent / ("skin" + matfu.NORMAL_SUFFIX + matfu.NORMAL_EXT)
          with Image.open(output) as image:
            pixels = np.asarray(image)
          filtered = matfu.apply_height_filters(height, params, as_heightmap=True) if enabled else None
          if enabled:
            self.assertFalse(np.array_equal(filtered, height))
            expected_pixels = matfu.fix_tiling_borders_rgba(np.dstack([normal, filtered]))
            np.testing.assert_array_equal(pixels, expected_pixels)
          else:
            self.assertEqual(pixels.shape[2], 3)
            np.testing.assert_array_equal(pixels, normal)
          expected = parent / "expected.jpg"
          matfu._save_spec_jpg(expected, matfu.process_spec(diffuse, filtered, params))
          actual = parent / ("skin" + matfu.SPEC_SUFFIX + matfu.SPEC_EXT)
          self.assertEqual(actual.read_bytes(), expected.read_bytes())


if __name__ == "__main__":
  unittest.main()
