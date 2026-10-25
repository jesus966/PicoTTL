"""Scope and failure-classification regressions for quality report tooling."""
# Copyright (c) 2026 Jesús Fernández Gamito
# SPDX-License-Identifier: MIT

import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

SPEC = importlib.util.spec_from_file_location(
    "quality", Path(__file__).resolve().parents[1] / "quality.py")
quality = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(quality)


class QualityReports(unittest.TestCase):
    def analyze(self, args):
        # Small mocked analyzer invocations still have a complete test scope.
        with patch.multiple(quality, PORTABLE_SOURCES={"src/Graphics.cpp"},
                            DEVICE_SOURCES=set()):
            return quality.static_analysis(args)

    def test_scope_excludes_unexecuted_hardware_and_generated_dependencies(self):
        for source in ("src/Graphics.cpp", "src/rp2350/MdaDisplayDevice.cpp",
                       "include/picottl/PackedFramebuffer.hpp"):
            self.assertTrue(quality.own_host_code(source), source)
        for source in ("src/rp2350/PioScanlineEngine.cpp",
                       "include/picottl/rp2350/PioScanlineEngine.hpp",
                       "src/fonts/IbmMda9x14.cpp", "tests/test_graphics.cpp",
                       "tests/support/RecordingScanlineEngine.cpp",
                       "pico-sdk/src/stdlib.cpp", "../src/Graphics.cpp"):
            self.assertFalse(quality.own_host_code(source), source)

    def test_coverage_excludes_tests_and_merges_repeated_header_lines(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            source = quality.ROOT / "include/picottl/PackedFramebuffer.hpp"
            raw = output / "raw.info"
            raw.write_text(
                f"SF:{source}\nFN:10,shared\nFNDA:1,shared\n"
                "DA:10,1\nDA:11,0\nend_of_record\n"
                "SF:include/picottl/../picottl/PackedFramebuffer.hpp\n"
                "FN:10,shared\nFNDA:0,shared\nDA:10,1\nDA:12,2\nend_of_record\n"
                "SF:tests/test_graphics.cpp\nDA:1,100\nend_of_record\n",
                encoding="utf-8")
            self.assertEqual(quality.coverage(SimpleNamespace(input=raw, output_dir=output)), 0)
            summary = json.loads((output / "summary.json").read_text())
            self.assertEqual(summary["lines"]["total"], 3)
            self.assertEqual(summary["lines"]["hit"], 2)
            self.assertEqual(summary["functions"]["total"], 1)
            self.assertEqual(summary["functions"]["hit"], 1)
            self.assertNotIn("tests/", (output / "lcov.info").read_text())

    def test_empty_coverage_is_an_error_not_zero_percent(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            raw = output / "raw.info"
            raw.write_text("SF:tests/test_graphics.cpp\nDA:1,1\nend_of_record\n")
            with self.assertRaises(ValueError):
                quality.coverage(SimpleNamespace(input=raw, output_dir=output))

    def test_missing_analyzer_is_infrastructure_not_clean(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            (output / "compile_commands.json").write_text(json.dumps([{
                "file": str(quality.ROOT / "src/Graphics.cpp"),
                "directory": str(quality.ROOT), "command": "c++ -c src/Graphics.cpp"
            }]))
            result = self.analyze(SimpleNamespace(
                build_dir=output, output_dir=output / "report", tool="cppcheck",
                executable=str(output / "missing-executable")))
            self.assertEqual(result, 2)
            report = json.loads((output / "report/result.json").read_text())
            self.assertEqual(report["status"], "infrastructure_error")

    def test_sarif_keeps_own_and_unresolved_results(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)

            def finding(path):
                return {"locations": [{"physicalLocation": {
                    "artifactLocation": {"uri": path}}}]}

            own = finding("src/Graphics.cpp")
            external = finding("src/fonts/IbmMda9x14.cpp")
            unresolved = {"message": {"text": "No location"}}
            raw = output / "raw.sarif"
            raw.write_text(json.dumps({"runs": [{"results": [own, external, unresolved]}]}))
            quality.sarif(SimpleNamespace(input=raw, output_dir=output))
            results = json.loads((output / "host.sarif").read_text())["runs"][0]["results"]
            self.assertEqual(results, [own, unresolved])

    def test_firmware_database_is_rejected_before_running_analyzer(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            (output / "compile_commands.json").write_text(json.dumps([{
                "file": str(quality.ROOT / "src/rp2350/PioScanlineEngine.cpp"),
                "directory": str(quality.ROOT), "command": "arm-none-eabi-g++ -c engine.cpp"
            }]))
            result = self.analyze(SimpleNamespace(
                build_dir=output, output_dir=output / "report", tool="cppcheck",
                executable="must-not-run"))
            self.assertEqual(result, 2)
            error = (output / "report/infrastructure-error.txt").read_text()
            self.assertIn("host database", error)

    def test_malformed_database_is_an_infrastructure_error(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            (output / "compile_commands.json").write_text('[{"file": 123}]')
            result = self.analyze(SimpleNamespace(
                build_dir=output, output_dir=output / "report", tool="clang-tidy",
                executable="must-not-run"))
            self.assertEqual(result, 2)
            report = json.loads((output / "report/result.json").read_text())
            self.assertEqual(report["status"], "infrastructure_error")

    def test_clang_crash_with_diagnostics_is_infrastructure(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            (output / "compile_commands.json").write_text(json.dumps([{
                "file": str(quality.ROOT / "src/Graphics.cpp"),
                "directory": str(quality.ROOT), "command": "c++ -c src/Graphics.cpp"
            }]))
            version = subprocess.CompletedProcess([], 0, "clang-tidy version\n")
            crash = subprocess.CompletedProcess([], -11, "Graphics.cpp:1: error: diagnostic\n")
            with patch.object(quality, "run", side_effect=[version, crash]):
                result = self.analyze(SimpleNamespace(
                    build_dir=output, output_dir=output / "report",
                    tool="clang-tidy", executable="clang-tidy"))
            self.assertEqual(result, 2)
            report = json.loads((output / "report/result.json").read_text())
            self.assertEqual(report["status"], "infrastructure_error")
            self.assertEqual(report["diagnostics"], 1)

    def test_unrelated_cppcheck_xml_is_not_a_clean_analysis(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            (output / "compile_commands.json").write_text(json.dumps([{
                "file": str(quality.ROOT / "src/Graphics.cpp"),
                "directory": str(quality.ROOT), "command": "c++ -c src/Graphics.cpp"
            }]))
            version = subprocess.CompletedProcess([], 0, "Cppcheck version\n")
            invalid = subprocess.CompletedProcess([], 0, "", "<not-a-cppcheck-report/>")
            with patch.object(quality.subprocess, "run", side_effect=[version, invalid]):
                result = self.analyze(SimpleNamespace(
                    build_dir=output, output_dir=output / "report",
                    tool="cppcheck", executable="cppcheck"))
            self.assertEqual(result, 2)
            report = json.loads((output / "report/result.json").read_text())
            self.assertEqual(report["status"], "infrastructure_error")


if __name__ == "__main__":
    unittest.main()
