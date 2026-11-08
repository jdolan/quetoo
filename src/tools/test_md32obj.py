import contextlib
import io
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from md32obj import main, read_md3


def make_model(num_frames):
  header_size = struct.calcsize("<ii64siiiiiiiii")
  surface_size = struct.calcsize("<i64siiiiiiiiii")
  vertices = b"".join(struct.pack("<hhhh", 64 * (i + 1), 0, 0, i)
                      for i in range(num_frames))
  surface = struct.pack("<i64siiiiiiiiii", 0x33504449, b"test", 0,
                        num_frames, 0, 1, 0, surface_size, surface_size,
                        surface_size, surface_size + 8,
                        surface_size + 8 + len(vertices))
  header = struct.pack("<ii64siiiiiiiii", 0x33504449, 15, b"test", 0,
                       num_frames, 0, 1, 0, header_size, header_size,
                       header_size, header_size + len(surface) + 8 + len(vertices))
  return header + surface + struct.pack("<ff", 0.25, 0.75) + vertices


class Md32ObjTest(unittest.TestCase):
  def setUp(self):
    self.temp = tempfile.TemporaryDirectory()
    self.addCleanup(self.temp.cleanup)
    self.path = Path(self.temp.name) / "tris.md3"

  def test_static_default_and_selected_pose(self):
    self.path.write_bytes(make_model(1))
    self.assertEqual(read_md3(self.path)[0].vertices, [(1.0, 0.0, 0.0)])
    self.path.write_bytes(make_model(2))
    with self.assertRaisesRegex(ValueError, "select a pose"):
      read_md3(self.path)
    surfaces = read_md3(self.path, 1)
    self.assertEqual(surfaces[0].vertices, [(2.0, 0.0, 0.0)])
    self.assertEqual(surfaces[0].texcoords, [(0.25, 0.75)])
    self.assertNotEqual(surfaces[0].normals, read_md3(self.path, 0)[0].normals)

  def test_invalid_pose(self):
    self.path.write_bytes(make_model(2))
    for frame in (-1, 2):
      with self.subTest(frame=frame):
        with self.assertRaisesRegex(ValueError, "out of range"):
          read_md3(self.path, frame)

  def test_cli_reports_failed_conversion(self):
    self.path.write_bytes(make_model(2))
    output = self.path.with_suffix(".obj")
    with patch("sys.argv", ["md32obj", str(self.path), "-o", str(output)]):
      with contextlib.redirect_stderr(io.StringIO()):
        self.assertEqual(main(), 1)
    self.assertFalse(output.exists())
    with patch("sys.argv", ["md32obj", str(self.path), "--frame", "1",
                            "-o", str(output)]):
      with contextlib.redirect_stdout(io.StringIO()):
        self.assertEqual(main(), 0)
    self.assertIn("v 2 0 0", output.read_text())


if __name__ == "__main__":
  unittest.main()
