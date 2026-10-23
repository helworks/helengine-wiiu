"""Exercise the real Make policy without shader compilation or a game build.

Run with the SDK's GNU Make (4.3) and CC/CXX host compiler, selecting a fixed
HELENGINE_WIIU_SHADER_TEST_ROOT. macOS's bundled Make 3.81 cannot validate the
rapid cache transitions because it compares file timestamps at whole seconds.
"""

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

    def test_native_cache_invalidates_in_both_inventory_directions(self):
        """Use the production configuration stamp with a real host-compiled fixture object."""
        repository = Path(__file__).resolve().parents[2]
        build = self.Root / "build"
        build.mkdir(exist_ok=True)
        rules = self.Root / "devkitpro/wut/share/wut_rules"
        rules.parent.mkdir(parents=True, exist_ok=True)
        rules.write_text(
            "%.o: $(WIIU_CACHE_TEST_SOURCE)\n"
            "\t$(CXX) $(CXXFLAGS) -c $< -o $@\n"
            "\t@printf 'compiled\\n' >> $(WIIU_CACHE_TEST_LOG)\n"
        )
        source = self.Root / "cache-fixture.cpp"
        source.write_text("int GetAvailability() { return HELENGINE_WIIU_HAS_STANDARD_SHADERS; }\n")
        log = self.Root / "cache-compilations.log"
        log.write_text("")
        arguments = [
            "make", "--no-print-directory", "-f", str(repository / "Makefile"),
            "WiiUGx2Presenter.o", f"TOPDIR={repository}", f"DEVKITPRO={self.Root / 'devkitpro'}",
            f"CC={os.environ.get('CC', 'clang')}", f"CXX={os.environ.get('CXX', 'clang++')}",
            "OFILES=WiiUGx2Presenter.o", "OFILES_SRC=WiiUGx2Presenter.o",
            f"WIIU_CACHE_TEST_SOURCE={source}", f"WIIU_CACHE_TEST_LOG={log}",
            f"WIIU_STANDARD_SHADER_SOURCES={self.Shaders}", f"CONTENT={self.Content}",
        ]
        self.AssertCompilationCount(arguments, build, log, 1)
        self.AssertCompilationCount(arguments, build, log, 1)
        for variant in self.Variants:
            for stage in ("vs", "ps"):
                (self.Shaders / f"{variant}.{stage}").write_text("inventory fixture only\n")
        self.AssertCompilationCount(arguments, build, log, 2)
        self.AssertCompilationCount(arguments, build, log, 2)
        for variant in self.Variants:
            for stage in ("vs", "ps"):
                (self.Shaders / f"{variant}.{stage}").unlink()
        self.AssertCompilationCount(arguments, build, log, 3)
        self.AssertCompilationCount(arguments, build, log, 3)

    def AssertCompilationCount(self, arguments, build, log, count):
        """Check actual compiler invocations without deleting the fixture's cached object."""
        result = subprocess.run(arguments, cwd=build, text=True, capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(len(log.read_text().splitlines()), count, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
