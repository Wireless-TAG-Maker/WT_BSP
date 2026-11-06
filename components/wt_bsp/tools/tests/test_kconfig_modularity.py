"""New boards must join selection and discovery without changing common files."""

import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

try:
    import esp_kconfiglib
except ImportError:
    esp_kconfiglib = None

try:
    from esp_menuconfig.model import MenuConfigState
except ImportError:
    MenuConfigState = None


COMPONENT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "wt_bsp_modularity_board_tools", COMPONENT / "tools/wt_bsp_set_board.py",
)
board_tools = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = board_tools
spec.loader.exec_module(board_tools)


@unittest.skipUnless(esp_kconfiglib, "run with the ESP-IDF Kconfig package")
class KconfigModularityTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="wt-bsp-modularity-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.component = self.root / "components" / "wt_bsp"
        shutil.copytree(COMPONENT, self.component, ignore=shutil.ignore_patterns("__pycache__"))
        self.public_files = {
            path.relative_to(self.component): path.read_bytes()
            for path in self.component.rglob("Kconfig*")
            if path.is_file() and path.parent.parent != self.component / "boards"
        }
        collector = Path("boards/CMakeLists.txt")
        self.public_files[collector] = (self.component / collector).read_bytes()

    def add_board(self, name, target, fallback=False, capabilities=()):
        board_id = name.replace("-", "_")
        directory = self.component / "boards" / name
        directory.mkdir()
        fallback_line = (
            "    default WT_BSP_BOARD_{} if IDF_TARGET_{} && !WT_BSP_BOARD_SELECTED_BY_TOOL\n".format(
                board_id, target.upper()) if fallback else ""
        )
        (directory / "Kconfig.projbuild").write_text(
            "config WT_BSP_SELECTED_BOARD_{id}\n"
            "    bool\n\n"
            "choice WT_BSP_BOARD\n"
            "    default WT_BSP_BOARD_{id} if WT_BSP_SELECTED_BOARD_{id}\n"
            "{fallback}\n"
            "    config WT_BSP_BOARD_{id}\n"
            "        bool \"{name}\"\n"
            "        depends on IDF_TARGET_{target}\n"
            "        select WT_BSP_BOARD_HAS_BOARD\n"
            "{capabilities}"
            "endchoice\n".format(
                id=board_id, name=name, target=target.upper(), fallback=fallback_line,
                capabilities="".join("        select WT_BSP_BOARD_HAS_{}\n".format(name)
                                     for name in capabilities),
            )
        )
        (directory / "CMakeLists.txt").write_text(
            "if(CONFIG_WT_BSP_BOARD_{id})\n"
            "    list(APPEND WT_BSP_BOARD_FEATURES fixture_board)\n"
            "    list(APPEND WT_BSP_BOARD_SRCS \"${{CMAKE_CURRENT_LIST_DIR}}/board.c\")\n"
            "endif()\n".format(id=board_id)
        )
        (directory / "board.c").write_text("/* Synthetic board discovery fixture. */\n")
        return board_id, directory

    def load(self, target, selected=None):
        root = self.root / "Kconfig"
        boards = board_tools.discover_boards(self.root)
        targets = sorted({board.target for board in boards.values()} | {target})
        root.write_text("\n".join(
            'config IDF_TARGET_{}\n    bool\n    default {}\n'.format(
                name.upper(), "y" if name == target else "n")
            for name in targets
        ) + '\nsource "{}"\n'.format(self.component / "Kconfig.projbuild"))
        environment = dict(os.environ)
        environment.pop("WT_BSP_BOARD_KCONFIG", None)
        if selected:
            board = boards[selected]
            environment["WT_BSP_BOARD_KCONFIG"] = str(
                board_tools.generate_sdkconfig_board_kconfig(self.root, board)
            )
        with patch.dict(os.environ, environment, clear=True):
            kconfig = esp_kconfiglib.Kconfig(str(root), warn_to_stderr=False)
        self.assertEqual(
            {symbol.name for symbol in kconfig.named_choices["WT_BSP_BOARD"].syms},
            {board.config[len("CONFIG_"):] for board in boards.values()},
        )
        return kconfig

    def assert_common_files_unchanged(self):
        for relative, original in self.public_files.items():
            self.assertEqual((self.component / relative).read_bytes(), original, str(relative))

    def test_new_same_target_board_is_discovered_and_tool_selection_wins(self):
        board_id, _ = self.add_board("WTZZP4-TINY", "esp32p4")
        self.assertEqual(self.load("esp32p4").named_choices["WT_BSP_BOARD"].selection.name,
                         "WT_BSP_BOARD_WT9932P4_TINY")
        kconfig = self.load("esp32p4", selected="WTZZP4-TINY")
        choice = kconfig.named_choices["WT_BSP_BOARD"]
        self.assertEqual(choice.selection.name, "WT_BSP_BOARD_" + board_id)
        self.assertEqual(sum(symbol.str_value == "y" for symbol in choice.syms), 1)
        # A persisted native selection still takes precedence over a tool default.
        saved = self.root / "sdkconfig"
        saved.write_text("CONFIG_WT_BSP_BOARD_WT9932P4C61_TINY=y\n")
        kconfig.load_config(str(saved))
        self.assertEqual(choice.selection.name, "WT_BSP_BOARD_WT9932P4C61_TINY")
        self.assert_common_files_unchanged()

    def test_new_target_board_joins_without_a_public_target_allowlist(self):
        board_id, _ = self.add_board("WTZZFUTURE-TINY", "esp32future", fallback=True)
        kconfig = self.load("esp32future")
        choice = kconfig.named_choices["WT_BSP_BOARD"]
        self.assertEqual(choice.selection.name, "WT_BSP_BOARD_" + board_id)
        self.assertEqual(sum(symbol.str_value == "y" for symbol in choice.syms), 1)
        discovered = board_tools.discover_boards(self.root)["WTZZFUTURE-TINY"]
        self.assertEqual(discovered.target, "esp32future")
        self.assertEqual(discovered.config, "CONFIG_WT_BSP_BOARD_" + board_id)
        self.assert_common_files_unchanged()

    def test_new_board_can_enable_uvc_through_its_capabilities(self):
        self.add_board("WTZZP4-TINY", "esp32p4", capabilities=("USB_DEVICE_UVC", "CSI", "RGB"))
        kconfig = self.load("esp32p4", selected="WTZZP4-TINY")
        uvc = kconfig.syms["WT_BSP_ENABLE_USB_DEVICE_UVC"]
        for name in ("CSI", "RGB"):
            feature = kconfig.syms["WT_BSP_ENABLE_" + name]
            self.assertTrue(feature.set_value("n"))
            self.assertEqual(feature.str_value, "n")
        self.assertEqual(uvc.visibility, 2)
        self.assertTrue(uvc.set_value("y"))
        self.assertEqual(uvc.str_value, "y")
        self.assertEqual(kconfig.syms["WT_BSP_ENABLE_CSI"].str_value, "n")
        self.assertEqual(kconfig.syms["WT_BSP_ENABLE_RGB"].str_value, "n")
        self.assertEqual(self.load("esp32c2").syms["WT_BSP_ENABLE_USB_DEVICE_UVC"].visibility, 0)
        self.assert_common_files_unchanged()

    def test_uvc_can_run_without_camera_or_rgb_capabilities(self):
        self.add_board("WTZZUVCONLY-TINY", "esp32p4", capabilities=("USB_DEVICE_UVC",))
        kconfig = self.load("esp32p4", selected="WTZZUVCONLY-TINY")
        uvc = kconfig.syms["WT_BSP_ENABLE_USB_DEVICE_UVC"]
        self.assertEqual(uvc.visibility, 2)
        self.assertTrue(uvc.set_value("y"))
        self.assertEqual(uvc.str_value, "y")
        for name in ("CAMERA", "CSI", "RGB"):
            self.assertEqual(kconfig.syms["WT_BSP_BOARD_HAS_" + name].str_value, "n")
            self.assertEqual(kconfig.syms["WT_BSP_ENABLE_" + name].str_value, "n")
        self.assert_common_files_unchanged()

    @unittest.skipUnless(MenuConfigState, "the ESP-IDF menuconfig model is needed")
    def test_menu_shows_matching_boards_and_keeps_board_options_outside_choice(self):
        board_id, directory = self.add_board("WTZZP4-TINY", "esp32p4")
        (directory / "Kconfig.options").write_text(
            "if WT_BSP_BOARD_{}\n"
            "config WT_BSP_FIXTURE_OPTION\n"
            "    int \"Fixture board option\"\n"
            "    default 42\n\n"
            "choice WT_BSP_FIXTURE_SENSOR\n"
            "    prompt \"Fixture sensor\"\n"
            "    default WT_BSP_FIXTURE_SENSOR_A\n"
            "    config WT_BSP_FIXTURE_SENSOR_A\n"
            "        bool \"Sensor A\"\n"
            "    config WT_BSP_FIXTURE_SENSOR_B\n"
            "        bool \"Sensor B\"\n"
            "endchoice\n"
            "endif\n".format(board_id)
        )
        kconfig = self.load("esp32p4", selected="WTZZP4-TINY")
        choice = kconfig.named_choices["WT_BSP_BOARD"]
        state = MenuConfigState(kconfig, "", "", False)
        self.assertTrue(state.enter_menu(choice.nodes[0]))
        boards = board_tools.discover_boards(self.root)
        self.assertEqual(
            {node.item.name for node in state.shown},
            {board.config[len("CONFIG_"):] for board in boards.values()
             if board.target == "esp32p4"},
        )
        self.assertEqual(kconfig.syms["WT_BSP_FIXTURE_OPTION"].str_value, "42")
        sensor = kconfig.named_choices["WT_BSP_FIXTURE_SENSOR"]
        self.assertEqual(sensor.selection.name, "WT_BSP_FIXTURE_SENSOR_A")
        self.assertTrue(set(sensor.syms).isdisjoint(choice.syms))
        self.assertNotIn(kconfig.syms["WT_BSP_FIXTURE_OPTION"], choice.syms)

        c2 = self.load("esp32c2")
        state = MenuConfigState(c2, "", "", False)
        self.assertTrue(state.enter_menu(c2.named_choices["WT_BSP_BOARD"].nodes[0]))
        self.assertEqual(
            {node.item.name for node in state.shown},
            {board.config[len("CONFIG_"):] for board in boards.values()
             if board.target == "esp32c2"},
        )
        self.assert_common_files_unchanged()

    @unittest.skipUnless(shutil.which("cmake"), "CMake is needed for board source discovery")
    def test_new_board_sources_join_cmake_without_changing_the_collector(self):
        board_id, directory = self.add_board("WTZZP4-TINY", "esp32p4")
        result = self.root / "cmake-result.txt"
        script = self.root / "configure.cmake"
        script.write_text(
            'set(CONFIG_WT_BSP_BOARD_{} y)\n'
            'include("{}")\n'
            'file(WRITE "{}" "${{WT_BSP_BOARD_FEATURES}}\\n${{WT_BSP_BOARD_SRCS}}\\n")\n'.format(
                board_id, self.component / "boards/CMakeLists.txt", result,
            )
        )
        subprocess.run([shutil.which("cmake"), "-P", str(script)],
                       check=True, capture_output=True, text=True)
        self.assertEqual(result.read_text().splitlines(), [
            "fixture_board", str(directory / "board.c"),
        ])
        self.assert_common_files_unchanged()


if __name__ == "__main__":
    unittest.main()
