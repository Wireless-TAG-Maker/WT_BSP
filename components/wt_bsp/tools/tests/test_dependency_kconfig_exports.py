"""Dependency conditions must survive ESP-IDF's actual JSON serialization."""

import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

try:
    import esp_kconfiglib
    from kconfgen.core import get_json_values
except ImportError:
    esp_kconfiglib = None

try:
    import yaml
    from idf_component_tools.debugger import KCONFIG_CONTEXT, SdkconfigContext
    from idf_component_tools.manifest.models import OptionalRequirement
except ImportError:
    OptionalRequirement = None


COMPONENT = Path(__file__).resolve().parents[2]
CAMERA_UVC = COMPONENT.parents[1] / "examples/common_components/wt_camera_uvc"
FEATURES = ("BUTTON", "RGB", "DSI", "TOUCH", "CSI", "CAMERA", "USB_DEVICE_UVC", "USB_DEVICE_CDC")
spec = importlib.util.spec_from_file_location(
    "wt_bsp_dependency_board_tools", COMPONENT / "tools/wt_bsp_set_board.py",
)
board_tools = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = board_tools
spec.loader.exec_module(board_tools)


@unittest.skipUnless(esp_kconfiglib, "run with the ESP-IDF Kconfig package")
class DependencyExportTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="wt-bsp-dependency-export-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.component = self.root / "components/wt_bsp"
        shutil.copytree(COMPONENT, self.component, ignore=shutil.ignore_patterns("__pycache__"))
        self.common_files = {
            path.relative_to(self.component): path.read_bytes()
            for path in self.component.rglob("Kconfig*")
            if path.is_file()
        }
        # Stand-ins for the vendor Kconfig definitions used by camera profiles.
        # The feature and board Kconfig files themselves are the real sources.
        prelude = []
        for target in sorted({board.target for board in board_tools.discover_boards(self.root).values()}):
            prelude.append("config IDF_TARGET_{}\n    bool\n    default {}\n".format(
                target.upper(), "y" if target == "esp32p4" else "n"))
        for name in ("SOC_LCDCAM_CAM_SUPPORTED", "SOC_JPEG_ENCODE_SUPPORTED"):
            prelude.append("config {}\n    bool\n    default y\n".format(name))
        for name in ("ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE", "ESP_VIDEO_ENABLE_HW_JPEG_ENC_VIDEO_DEVICE",
                     "TINYUSB_CDC_ENABLED", "CAMERA_GC2145", "CAMERA_OV3660",
                     "CAMERA_GC2145_DVP_RGB565_BE_1600X1200_13FPS",
                     "CAMERA_OV3660_DVP_JPEG_640X480_25FPS"):
            prelude.append("config {}\n    bool\n".format(name))
        for sensor in ("GC2145", "OV3660"):
            prelude.append("config CAMERA_{}_DVP_IF_FORMAT_INDEX_DEFAULT\n    int\n    default 0\n".format(sensor))
        for sensor, member in (("GC2145", "RGB565_BE_1600X1200_13FPS"),
                               ("OV3660", "JPEG_640X480_25FPS")):
            prelude.append(
                'choice CAMERA_{sensor}_DVP_DEFAULT_FMT\n'
                '    prompt "Fixture sensor format"\n'
                '    config CAMERA_{sensor}_DVP_DEFAULT_FMT_{member}\n'
                '        bool "Fixture mode"\n'
                'endchoice\n'.format(sensor=sensor, member=member)
            )
        self.kconfig_file = self.root / "Kconfig"
        self.kconfig_file.write_text("\n".join(prelude) + '\nsource "{}"\n'.format(
            self.component / "Kconfig.projbuild"))

    def add_board(self, suffix, capabilities=()):
        name = "WTEXPORT-" + suffix
        board_id = name.replace("-", "_")
        board = self.component / "boards" / name
        board.mkdir()
        board.joinpath("Kconfig.projbuild").write_text(
            'config WT_BSP_SELECTED_BOARD_{id}\n'
            '    bool\n\n'
            'choice WT_BSP_BOARD\n'
            '    default WT_BSP_BOARD_{id} if WT_BSP_SELECTED_BOARD_{id}\n'
            '    config WT_BSP_BOARD_{id}\n'
            '        bool "{name}"\n'
            '        depends on IDF_TARGET_ESP32P4\n'
            '        select WT_BSP_BOARD_HAS_BOARD\n'
            '{capabilities}'
            'endchoice\n'.format(
                id=board_id, name=name,
                capabilities="".join("        select WT_BSP_BOARD_HAS_{}\n".format(feature)
                                     for feature in capabilities),
            )
        )
        return name

    def load(self, name):
        board = board_tools.discover_boards(self.root)[name]
        selector = board_tools.generate_sdkconfig_board_kconfig(self.root, board)
        with patch.dict(os.environ, {"WT_BSP_BOARD_KCONFIG": str(selector)}):
            return esp_kconfiglib.Kconfig(str(self.kconfig_file), warn_to_stderr=False)

    def include_uvc_application(self):
        vendor_settings = []
        for name, value in (("UVC_CAM1_FRAMESIZE_WIDTH", 1280),
                            ("UVC_CAM1_FRAMESIZE_HEIGT", 720),
                            ("UVC_CAM1_FRAMERATE", 15)):
            vendor_settings.append('config {}\n    int "USB video setting"\n    default {}\n'.format(
                name, value))
        self.kconfig_file.write_text(
            self.kconfig_file.read_text() + "\n" + "\n".join(vendor_settings)
            + '\nsource "{}"\n'.format(CAMERA_UVC / "Kconfig.projbuild")
        )

    def export(self, model):
        # Exercise the serializer used by ESP-IDF, then round-trip its JSON.
        result = json.loads(json.dumps(get_json_values(model)))
        for feature in FEATURES:
            self.assertIn("WT_BSP_DEP_" + feature, result)
            self.assertIsInstance(result["WT_BSP_DEP_" + feature], str)
        self.assertIn("WT_BSP_DEP_TOUCH_GT911", result)
        self.assertIsInstance(result["WT_BSP_DEP_TOUCH_GT911"], str)
        return result

    def assert_common_unchanged(self):
        for relative, original in self.common_files.items():
            self.assertEqual((self.component / relative).read_bytes(), original, str(relative))

    def selected_dependencies(self, exported, target, idf_version):
        manifest = yaml.safe_load((self.component / "idf_component.yml").read_text())
        context = SdkconfigContext()
        context.sdkconfig = exported
        token = KCONFIG_CONTEXT.set(context)
        selected = {}
        try:
            with patch.dict(os.environ, {"IDF_TARGET": target}), patch(
                "idf_component_tools.manifest.if_parser.get_idf_version", return_value=idf_version,
            ):
                for name, dependency in manifest["dependencies"].items():
                    if not dependency.get("matches") and not dependency.get("rules"):
                        continue
                    requirement = OptionalRequirement(
                        matches=dependency.get("matches", []), rules=dependency.get("rules", []),
                    )
                    self.assertTrue(requirement.has_kconfig_option, name)
                    version_spec = requirement.version_spec_if_meet_conditions(dependency["version"])
                    if version_spec is not None:
                        selected[name] = version_spec
        finally:
            KCONFIG_CONTEXT.reset(token)
        return selected

    def test_hidden_unsupported_features_still_export_false_dependencies(self):
        model = self.load(self.add_board("NONE"))
        values = self.export(model)
        for feature in FEATURES:
            self.assertNotIn("WT_BSP_ENABLE_" + feature, values)
            self.assertEqual(values["WT_BSP_DEP_" + feature], "n")
        self.assert_common_unchanged()

    def test_camera_only_board_exports_camera_without_other_features(self):
        model = self.load(self.add_board("CAMERAONLY", ("CAMERA",)))
        values = self.export(model)
        self.assertIs(values["WT_BSP_ENABLE_CAMERA"], True)
        for feature in FEATURES:
            self.assertEqual(values["WT_BSP_DEP_" + feature], "y" if feature == "CAMERA" else "n")
        self.assert_common_unchanged()

    def test_each_dependency_tracks_user_enable_and_cut(self):
        for feature in FEATURES:
            with self.subTest(feature=feature):
                model = self.load(self.add_board(feature, (feature,)))
                enabled = model.syms["WT_BSP_ENABLE_" + feature]
                self.assertEqual(enabled.visibility, 2)
                for value in ("n", "y", "n"):
                    self.assertTrue(enabled.set_value(value))
                    self.assertEqual(enabled.str_value, value)
                    exported = self.export(model)
                    for other in FEATURES:
                        self.assertEqual(exported["WT_BSP_DEP_" + other], value if other == feature else "n")
        self.assert_common_unchanged()

    def test_saved_camera_cut_keeps_dependency_false_after_reload(self):
        name = self.add_board("SAVEDCAMERA", ("CAMERA",))
        model = self.load(name)
        model.syms["WT_BSP_ENABLE_CAMERA"].set_value("n")
        sdkconfig = self.root / "sdkconfig"
        model.write_config(str(sdkconfig))
        fresh = self.load(name)
        fresh.load_config(str(sdkconfig))
        self.assertEqual(fresh.syms["WT_BSP_ENABLE_CAMERA"].str_value, "n")
        self.assertEqual(self.export(fresh)["WT_BSP_DEP_CAMERA"], "n")
        self.assert_common_unchanged()

    def test_p4_uvc_application_matches_csi_and_enables_jpeg(self):
        self.include_uvc_application()
        for board in ("WT9932P4-TINY", "WT9932P4C61-TINY", "WT9932P4X-TINY"):
            with self.subTest(board=board):
                model = self.load(board)
                model.syms["WT_BSP_ENABLE_USB_DEVICE_UVC"].set_value("y")
                # Saved vendor defaults must not override the application's CSI mode.
                for name, value in (("UVC_CAM1_FRAMESIZE_WIDTH", 1280),
                                    ("UVC_CAM1_FRAMESIZE_HEIGT", 720),
                                    ("UVC_CAM1_FRAMERATE", 15)):
                    model.syms[name].set_value(value)
                values = self.export(model)
                self.assertIs(values.get("ESP_VIDEO_ENABLE_HW_JPEG_ENC_VIDEO_DEVICE", False), True)
                self.assertEqual(values["UVC_CAM1_FRAMESIZE_WIDTH"], 1024)
                self.assertEqual(values["UVC_CAM1_FRAMESIZE_HEIGT"], 600)
                self.assertEqual(values["UVC_CAM1_FRAMERATE"], 30)
        self.assert_common_unchanged()

    def test_disabling_uvc_releases_the_p4_application_profile(self):
        self.include_uvc_application()
        model = self.load("WT9932P4-TINY")
        uvc = model.syms["WT_BSP_ENABLE_USB_DEVICE_UVC"]
        for enabled in ("y", "n", "y"):
            uvc.set_value(enabled)
            values = self.export(model)
            self.assertIs(values["WT_BSP_ENABLE_CSI"], True)
            self.assertEqual(values["UVC_CAM1_FRAMESIZE_WIDTH"], 1024 if enabled == "y" else 1280)
            self.assertEqual(values["UVC_CAM1_FRAMESIZE_HEIGT"], 600 if enabled == "y" else 720)
            self.assertEqual(values["UVC_CAM1_FRAMERATE"], 30 if enabled == "y" else 15)
            self.assertEqual(values.get("ESP_VIDEO_ENABLE_HW_JPEG_ENC_VIDEO_DEVICE", False), enabled == "y")
        self.assert_common_unchanged()

    def test_uvc_only_application_does_not_enable_a_camera(self):
        self.include_uvc_application()
        model = self.load(self.add_board("UVCONLYAPP", ("USB_DEVICE_UVC",)))
        model.syms["WT_BSP_ENABLE_USB_DEVICE_UVC"].set_value("y")
        values = self.export(model)
        self.assertEqual(values["WT_BSP_DEP_CSI"], "n")
        self.assertEqual(values["WT_BSP_DEP_CAMERA"], "n")
        self.assertEqual(values["WT_BSP_DEP_RGB"], "n")
        self.assertIs(values.get("ESP_VIDEO_ENABLE_HW_JPEG_ENC_VIDEO_DEVICE", False), False)
        self.assertEqual(values["UVC_CAM1_FRAMESIZE_WIDTH"], 1280)
        self.assertEqual(values["UVC_CAM1_FRAMESIZE_HEIGT"], 720)
        self.assert_common_unchanged()

    def test_s31_uvc_application_keeps_its_sensor_profiles(self):
        self.include_uvc_application()
        text = self.kconfig_file.read_text()
        text = text.replace("config IDF_TARGET_ESP32P4\n    bool\n    default y\n",
                            "config IDF_TARGET_ESP32P4\n    bool\n    default n\n")
        text = text.replace("config IDF_TARGET_ESP32S31\n    bool\n    default n\n",
                            "config IDF_TARGET_ESP32S31\n    bool\n    default y\n")
        self.kconfig_file.write_text(text)
        model = self.load("WT9932S31-TINY")
        model.syms["WT_BSP_ENABLE_USB_DEVICE_UVC"].set_value("y")
        for sensor, width, height, rate in (("GC2145", 1600, 1200, 8), ("OV3660", 640, 480, 25)):
            with self.subTest(sensor=sensor):
                model.syms["WT_BSP_S31_CAMERA_SENSOR_" + sensor].set_value("y")
                values = self.export(model)
                self.assertEqual(values["UVC_CAM1_FRAMESIZE_WIDTH"], width)
                self.assertEqual(values["UVC_CAM1_FRAMESIZE_HEIGT"], height)
                self.assertEqual(values["UVC_CAM1_FRAMERATE"], rate)
        self.assert_common_unchanged()

    @unittest.skipUnless(OptionalRequirement, "run with ESP-IDF Component Manager")
    def test_gt911_dependency_tracks_lcd_adapter_and_touch_cut(self):
        for adapter in ("WT_BSP_DSI_LCD_ADAPTER_WLK7002_MIPI_15P",
                        "WT_BSP_DSI_LCD_ADAPTER_WTDKP4_LCD_ADAPTER"):
            model = self.load("WT9932P4C61-TINY")
            self.assertTrue(model.syms[adapter].set_value("y"))
            self.assertEqual(model.syms[adapter].str_value, "y")
            for enabled in ("y", "n"):
                with self.subTest(adapter=adapter, touch=enabled):
                    model.syms["WT_BSP_ENABLE_TOUCH"].set_value(enabled)
                    values = self.export(model)
                    self.assertEqual(values["WT_BSP_DEP_TOUCH_GT911"], enabled)
                    dependencies = self.selected_dependencies(values, "esp32p4", "6.1.0")
                    if enabled == "y":
                        self.assertEqual(dependencies["espressif/esp_lcd_touch_gt911"], "1.2.1")
                    else:
                        self.assertNotIn("espressif/esp_lcd_touch_gt911", dependencies)
            model.syms["WT_BSP_ENABLE_TOUCH"].set_value("y")
            model.syms["WT_BSP_DSI_LCD_ADAPTER_DEFAULT"].set_value("y")
            values = self.export(model)
            self.assertEqual(values["WT_BSP_DEP_TOUCH_GT911"], "n")
            self.assertNotIn("espressif/esp_lcd_touch_gt911",
                             self.selected_dependencies(values, "esp32p4", "6.1.0"))
        self.assert_common_unchanged()

    @unittest.skipUnless(OptionalRequirement, "run with ESP-IDF Component Manager")
    def test_manifest_parser_routes_exported_feature_dependencies(self):
        expected = {
            "BUTTON": {"button"},
            "RGB": {"led_strip"},
            "DSI": {"esp_lcd_ek79007", "espressif/esp_lcd_ili9881c", "kevincoooool/esp_lcd_st7102",
                    "esp_lvgl_port", "lvgl/lvgl"},
            "TOUCH": {"espressif/esp_lcd_touch", "espressif/esp_lcd_touch_st7123",
                      "esp_lvgl_port", "lvgl/lvgl"},
            "CSI": {"espressif/esp_video", "espressif/esp_cam_sensor",
                    "espressif/esp_sccb_intf", "espressif/esp_ipa"},
            "CAMERA": {"espressif/esp_video", "espressif/esp_cam_sensor"},
            "USB_DEVICE_UVC": {"espressif/usb_device_uvc"},
            "USB_DEVICE_CDC": {"espressif/esp_tinyusb"},
        }
        targets = ("esp32p4", "esp32s31", "esp32c3")
        versions = ("6.0.0", "6.1.0")
        # The Kconfig fixture supplies each capability independently. The
        # resolver target is varied separately to verify the manifest guards.
        for feature in FEATURES:
            model = self.load(self.add_board("RESOLVE_" + feature, (feature,)))
            enabled = model.syms["WT_BSP_ENABLE_" + feature]
            for value in ("n", "y"):
                self.assertTrue(enabled.set_value(value))
                self.assertEqual(enabled.str_value, value)
                exported = self.export(model)
                for target in targets:
                    for version in versions:
                        with self.subTest(feature=feature, value=value, target=target, idf_version=version):
                            wanted = set()
                            if value == "y":
                                if feature in ("BUTTON", "RGB") or target == "esp32p4":
                                    wanted = expected[feature].copy()
                                elif target == "esp32s31" and feature in ("CAMERA", "USB_DEVICE_UVC", "USB_DEVICE_CDC"):
                                    wanted = expected[feature].copy()
                                if target == "esp32s31" and feature == "USB_DEVICE_UVC":
                                    wanted.add("espressif/tinyusb")
                                if version == "6.0.0" and target == "esp32p4" and feature in ("DSI", "CSI", "CAMERA"):
                                    wanted.add("espressif/esp_h264")
                            selected = self.selected_dependencies(exported, target, version)
                            self.assertEqual(set(selected), wanted)
                            if "esp_lvgl_port" in selected:
                                self.assertEqual(selected["esp_lvgl_port"], "2.8.*" if version == "6.0.0" else "^2")
                            if "espressif/esp_h264" in selected:
                                self.assertEqual(selected["espressif/esp_h264"], "1.3.0")
        unsupported = self.export(self.load(self.add_board("RESOLVE_NONE")))
        for target in targets:
            for version in versions:
                with self.subTest(unsupported_board=True, target=target, idf_version=version):
                    self.assertEqual(self.selected_dependencies(unsupported, target, version), {})
        self.assert_common_unchanged()


if __name__ == "__main__":
    unittest.main()
