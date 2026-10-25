"""Stable identities, strict report parsing and baseline gate regressions."""
# Copyright (c) 2026 Jesús Fernández Gamito
# SPDX-License-Identifier: MIT

import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import yaml
import quality
import quality_baseline as baseline


def document(records=()):
    return {"schema_version": 1, "tools": {
        "clang-tidy": [dict(record, reason="Reviewed test fixture") for record in records],
        "cppcheck": []}}


class BaselineIdentities(unittest.TestCase):
    def record(self, text, needle="value", message="known warning", note="trace"):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "unit.cpp"
            source.write_bytes(text.encode())
            diagnostics = baseline.Diagnostics(root)
            offset = text.encode().index(needle.encode())
            return diagnostics.diagnostic(
                "clang-tidy", "test-check", message, "error",
                diagnostics.location(source, byte_offset=offset), [note])

    def test_line_comment_whitespace_and_crlf_changes_preserve_id(self):
        first = self.record("int f() { return value; }\n")
        moved = self.record("// UTF-8: á\r\n\r\nint f ( ) {\r\n return  value ;\r\n}\r\n")
        self.assertEqual(first["id"], moved["id"])

    def test_same_code_in_different_functions_is_not_the_same_diagnostic(self):
        first = self.record("int f() { return value; }\n")
        other = self.record("int g() { return value; }\n")
        self.assertNotEqual(first["id"], other["id"])

    def test_identical_sites_in_one_scope_have_different_occurrences(self):
        text = "int f() { " + "read(value); " * 30 + "}"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "unit.cpp"
            path.write_text(text)
            source = baseline.SourceIndex(path)
            offsets = [m.start() for m in baseline.re.finditer("value", text)]
            a = source.anchor(byte_offset=offsets[10])
            b = source.anchor(byte_offset=offsets[11])
            self.assertEqual(a["context"], b["context"])
            self.assertNotEqual(a["occurrence"], b["occurrence"])

    def test_changed_message_or_trace_is_new(self):
        known = self.record("int f() { return value; }")
        changed = self.record("int f() { return value; }", message="different warning")
        trace = self.record("int f() { return value; }", note="different path")
        compared = baseline.compare("clang-tidy", [changed, trace], document([known]))
        self.assertEqual(len(compared["new"]), 2)
        self.assertEqual(len(compared["missing"]), 1)

    def test_duplicates_count_once_without_losing_occurrences(self):
        record = self.record("int f() { return value; }")
        compared = baseline.compare("clang-tidy", [record, record], document([record]))
        self.assertEqual(compared["known"], [record["id"]])
        self.assertEqual(compared["occurrences"], 2)
        self.assertEqual(compared["unique_diagnostics"], 1)
        self.assertFalse(compared["new"])

    def test_missing_diagnostic_is_reported(self):
        record = self.record("int f() { return value; }")
        compared = baseline.compare("clang-tidy", [], document([record]))
        self.assertEqual(compared["missing"][0]["id"], record["id"])
        self.assertFalse(compared["new"])

    def test_corrupt_or_duplicate_baseline_is_rejected(self):
        record = self.record("int f() { return value; }")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "baseline.json"
            for bad in ("{", '{"schema_version":1,"schema_version":1}',
                        json.dumps(document([record, record]))):
                path.write_text(bad)
                with self.assertRaises((ValueError, KeyError)):
                    baseline.load_baseline(path)
            broken = document([record])
            broken["tools"]["clang-tidy"][0]["identity"]["message"] = "tampered"
            path.write_text(json.dumps(broken))
            with self.assertRaises(ValueError):
                baseline.load_baseline(path)


class BaselineGate(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.output = Path(self.temporary.name)
        self.source = quality.ROOT / "src/Graphics.cpp"
        self.offset = self.source.read_bytes().index(b"void Graphics::clear")
        self.report = {"MainSourceFile": str(self.source), "Diagnostics": [{
            "DiagnosticName": "test-check", "Level": "Error",
            "DiagnosticMessage": {"Message": "known warning", "FilePath": str(self.source),
                                  "FileOffset": self.offset}, "Notes": []}]}
        (self.output / "compile_commands.json").write_text(json.dumps([{
            "file": str(self.source), "directory": str(quality.ROOT),
            "command": "c++ -c src/Graphics.cpp"}]))
        self.baseline_file = self.output / "baseline.json"

    def known(self):
        path = self.output / "fixture.yaml"
        path.write_text(yaml.safe_dump(self.report))
        return baseline.Diagnostics(quality.ROOT).clang(path, self.source, 1)[0]

    def invoke(self, known=(), report=True, text=True, returncode=1):
        self.baseline_file.write_text(json.dumps(document(known)))

        def run(command, log):
            if "--version" in command:
                return subprocess.CompletedProcess(command, 0, "LLVM version 18.1.3\n")
            if report:
                path = Path(next(c.split("=", 1)[1] for c in command if c.startswith("--export-fixes=")))
                path.write_text(yaml.safe_dump(self.report) if report is True else report)
            output = "Graphics.cpp:1:1: error: known warning [test-check]\n" if text else ""
            log.write(output)
            return subprocess.CompletedProcess(command, returncode, output)

        with patch.multiple(quality, PORTABLE_SOURCES={"src/Graphics.cpp"}, DEVICE_SOURCES=set()), \
                patch.object(quality, "run", side_effect=run):
            result = quality.static_analysis(SimpleNamespace(
                build_dir=self.output, output_dir=self.output / "report", tool="clang-tidy",
                executable="clang-tidy", baseline=self.baseline_file))
        metadata = json.loads((self.output / "report/result.json").read_text())
        return result, metadata

    def test_only_known_diagnostics_succeeds_and_keeps_raw_reports(self):
        result, metadata = self.invoke([self.known()])
        self.assertEqual(result, 0)
        self.assertEqual(metadata["status"], "known_findings")
        self.assertEqual(len(metadata["baseline"]["known"]), 1)
        self.assertIn("known warning", (self.output / "report/analysis.log").read_text())
        self.assertTrue((self.output / "report/fixes-0.yaml").exists())

    def test_new_diagnostic_fails(self):
        result, metadata = self.invoke()
        self.assertEqual(result, 1)
        self.assertEqual(len(metadata["baseline"]["new"]), 1)

    def test_removed_diagnostic_succeeds_and_is_reported(self):
        result, metadata = self.invoke([self.known()], report=False, text=False, returncode=0)
        self.assertEqual(result, 0)
        self.assertEqual(len(metadata["baseline"]["missing"]), 1)
        self.assertIn("**missing**", (self.output / "report/summary.md").read_text())

    def test_stale_yaml_cannot_make_a_clean_run_look_known(self):
        self.invoke([self.known()])
        result, metadata = self.invoke([self.known()], report=False, text=False, returncode=0)
        self.assertEqual(result, 0)
        self.assertEqual(metadata["diagnostics"], 0)
        self.assertEqual(len(metadata["baseline"]["missing"]), 1)

    def test_missing_truncated_or_inconsistent_report_is_infrastructure(self):
        for report in (False, "Diagnostics: [", yaml.safe_dump(dict(self.report, Diagnostics=[]))):
            with self.subTest(report=report):
                result, metadata = self.invoke([self.known()], report=report)
                self.assertEqual(result, 2)
                self.assertFalse(metadata["baseline_evaluated"])
                self.assertIsNone(metadata["baseline"])

    def test_crash_with_known_diagnostic_still_fails(self):
        result, metadata = self.invoke([self.known()], returncode=-11)
        self.assertEqual(result, 2)
        self.assertFalse(metadata["baseline_evaluated"])

    def test_wrong_translation_unit_is_infrastructure(self):
        self.report["MainSourceFile"] = str(quality.ROOT / "src/Text.cpp")
        result, metadata = self.invoke()
        self.assertEqual(result, 2)
        self.assertFalse(metadata["baseline_evaluated"])


class CppcheckReports(unittest.TestCase):
    def test_incomplete_progress_even_with_valid_xml_is_rejected(self):
        xml = '<results version="2"><cppcheck version="2.13.0"/><errors/></results>'
        with self.assertRaises(ValueError):
            baseline.Diagnostics(quality.ROOT).cppcheck(xml, "8/9 files checked", 9)

    def test_truncated_xml_is_rejected(self):
        with self.assertRaises(baseline.ET.ParseError):
            baseline.Diagnostics(quality.ROOT).cppcheck('<results version="2">', "9/9 files checked", 9)

    def test_locationless_diagnostic_is_not_allowed_by_a_broad_match(self):
        xml = ('<results version="2"><cppcheck version="2.13.0"/><errors>'
               '<error id="new-warning" msg="unknown" severity="warning"/>'
               '</errors></results>')
        with self.assertRaises(ValueError):
            baseline.Diagnostics(quality.ROOT).cppcheck(xml, "9/9 files checked", 9)


class CppcheckGate(unittest.TestCase):
    def invoke(self, mode):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            source = quality.ROOT / "src/Graphics.cpp"
            line = next(index + 1 for index, text in enumerate(source.read_text().splitlines())
                        if text.startswith("void Graphics::clear"))
            root = baseline.ET.Element("results", version="2")
            baseline.ET.SubElement(root, "cppcheck", version="2.13.0")
            errors = baseline.ET.SubElement(root, "errors")
            error = baseline.ET.SubElement(errors, "error", id="test-cppcheck", msg="known", severity="warning")
            baseline.ET.SubElement(error, "location", file=str(source), line=str(line), column="1")
            xml = baseline.ET.tostring(root, encoding="unicode")
            record = baseline.Diagnostics(quality.ROOT).cppcheck(xml, "1/1 files checked", 1)[0]
            accepted = document()
            accepted["tools"]["cppcheck"] = [dict(record, reason="Reviewed fixture")]
            if mode == "new":
                accepted["tools"]["cppcheck"] = []
            if mode == "removed":
                errors.remove(error)
                xml = baseline.ET.tostring(root, encoding="unicode")
            if mode == "corrupt":
                xml = '<results version="2">'
            if mode == "duplicate":
                errors.append(error)
                xml = baseline.ET.tostring(root, encoding="unicode")
            baseline_file = output / "baseline.json"
            baseline_file.write_text(json.dumps(accepted))
            (output / "compile_commands.json").write_text(json.dumps([{
                "file": str(source), "directory": str(quality.ROOT), "command": "c++ -c src/Graphics.cpp"}]))
            version = subprocess.CompletedProcess([], 0, "Cppcheck 2.13.0\n")
            progress = "0/1 files checked" if mode == "incomplete" else "1/1 files checked"
            code = -11 if mode == "crash" else 0 if mode == "removed" else 1
            result = subprocess.CompletedProcess([], code, progress, xml)
            with patch.multiple(quality, PORTABLE_SOURCES={"src/Graphics.cpp"}, DEVICE_SOURCES=set()), \
                    patch.object(quality.subprocess, "run", side_effect=[version, result]):
                status = quality.static_analysis(SimpleNamespace(
                    build_dir=output, output_dir=output / "report", tool="cppcheck",
                    executable="cppcheck", baseline=baseline_file))
            metadata = json.loads((output / "report/result.json").read_text())
            self.assertTrue((output / "report/cppcheck.xml").exists())
            return status, metadata

    def test_known_and_duplicate_cppcheck_diagnostics_succeed(self):
        for mode in ("known", "duplicate"):
            with self.subTest(mode=mode):
                status, report = self.invoke(mode)
                self.assertEqual(status, 0)
                self.assertEqual(len(report["baseline"]["known"]), 1)
                self.assertEqual(report["diagnostics"], 2 if mode == "duplicate" else 1)

    def test_new_cppcheck_diagnostic_fails(self):
        status, report = self.invoke("new")
        self.assertEqual(status, 1)
        self.assertEqual(len(report["baseline"]["new"]), 1)

    def test_removed_cppcheck_diagnostic_is_reported(self):
        status, report = self.invoke("removed")
        self.assertEqual(status, 0)
        self.assertEqual(len(report["baseline"]["missing"]), 1)

    def test_invalid_crashed_or_incomplete_cppcheck_always_fails(self):
        for mode in ("corrupt", "crash", "incomplete"):
            with self.subTest(mode=mode):
                status, report = self.invoke(mode)
                self.assertEqual(status, 2)
                self.assertFalse(report["baseline_evaluated"])


if __name__ == "__main__":
    unittest.main()
