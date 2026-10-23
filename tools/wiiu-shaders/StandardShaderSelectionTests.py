"""Exercise the real Make inventory policy without a shader compiler or Wii U SDK."""

import os
from pathlib import Path
import subprocess
import unittest


class StandardShaderSelectionTests(unittest.TestCase):
    """Keep complete Standard groups available and reject incomplete cooked groups."""

    Variants = ("ForwardStandard", "ForwardStandardShadowed", "ShadowDepth")

    def setUp(self):
        """Prepare a fixed, explicitly selected platform temporary fixture."""
        self.Root = Path(os.environ["HELENGINE_WIIU_SHADER_TEST_ROOT"]).resolve()
        self.Shaders = self.Root / "shaders"
        self.Content = self.Root / "content"
        self.Shaders.mkdir(parents=True, exist_ok=True)
        self.Material = self.Content / "cooked/engine/materials/standard.hasset"
        self.Material.parent.mkdir(parents=True, exist_ok=True)
        self.Material.unlink(missing_ok=True)
        for variant in self.Variants:
            for stage in ("vs", "ps"):
                (self.Shaders / f"{variant}.{stage}").unlink(missing_ok=True)
        policy = Path(__file__).resolve().with_name("standard_shader_variants.mk")
        self.Probe = self.Root / "inventory.mk"
        self.Probe.write_text(
            f"include {policy.as_posix()}\n"
            ".PHONY: check\ncheck:\n"
            "\t@printf 'HAS=%s\\nVARIANTS=%s\\n' "
            "'$(HELENGINE_WIIU_HAS_STANDARD_SHADERS)' '$(STANDARD_SHADER_VARIANTS)'\n"
        )

    def RunInventory(self):
        """Evaluate the production Make policy with the fixture's actual inventory."""
        return subprocess.run(
            ["make", "--no-print-directory", "-f", str(self.Probe), "check",
             f"WIIU_STANDARD_SHADER_SOURCES={self.Shaders}", f"CONTENT={self.Content}"],
            text=True, capture_output=True, check=False,
        )

    def test_ui_without_standard_group_does_not_embed_standard(self):
        """A UI-only cook must not acquire a native Standard shader dependency."""
        result = self.RunInventory()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.splitlines(), ["HAS=0", "VARIANTS="])

    def test_complete_group_preserves_all_three_variants(self):
        """A complete cooked 3D group must remain embedded with the existing names."""
        for variant in self.Variants:
            for stage in ("vs", "ps"):
                (self.Shaders / f"{variant}.{stage}").write_text("inventory fixture only\n")
        result = self.RunInventory()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.splitlines(), ["HAS=1", "VARIANTS=" + " ".join(self.Variants)])

    def test_partial_group_fails_instead_of_becoming_ui_only(self):
        """One present source is evidence of a broken 3D group, not a UI-only cook."""
        (self.Shaders / "ForwardStandard.vs").write_text("inventory fixture only\n")
        result = self.RunInventory()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("incomplete Standard shader group", result.stderr)

    def test_declared_standard_material_requires_its_shader_group(self):
        """A packaged Standard material cannot silently lose its entire shader group."""
        self.Material.write_bytes(b"inventory marker only")
        result = self.RunInventory()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("incomplete Standard shader group", result.stderr)


if __name__ == "__main__":
    unittest.main()
