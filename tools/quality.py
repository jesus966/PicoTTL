#!/usr/bin/env python3
"""Reproducible host analysis and report scoping; no production fixes.

Exit codes: 0 clean/report generated, 1 analyzer findings, 2 infrastructure
or invalid input. Requires only Python's standard library and the named tool.
"""
# Copyright (c) 2026 Jesús Fernández Gamito
# SPDX-License-Identifier: MIT

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
PORTABLE_SOURCES = {
    "src/AnsiTerminal.cpp", "src/DisplaySurface.cpp", "src/Graphics.cpp",
    "src/Terminal.cpp", "src/Text.cpp", "src/host/HostDisplayDevice.cpp",
}
DEVICE_SOURCES = {
    f"src/rp2350/{family}DisplayDevice.cpp" for family in ("Mda", "Cga", "Ega")
}


def own_host_code(path, root=ROOT):
    """Allow executed production code, not tests, SDK, generated fonts or PIO."""
    candidate = Path(path)
    if not candidate.is_absolute():
        candidate = root / candidate
    try:
        relative = candidate.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return False
    if relative in PORTABLE_SOURCES | DEVICE_SOURCES:
        return True
    return (relative.startswith("include/picottl/")
            and relative.endswith(".hpp")
            and not relative.startswith("include/picottl/fonts/")
            and relative != "include/picottl/rp2350/PioScanlineEngine.hpp")


def run(command, log):
    result = subprocess.run(command, cwd=ROOT, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, check=False)
    log.write(result.stdout)
    return result


def static_analysis(args):
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    status = "infrastructure_error"
    diagnostics = 0
    try:
        database = json.loads((args.build_dir / "compile_commands.json")
                              .read_text(encoding="utf-8"))
        if not isinstance(database, list) or any(
                not isinstance(entry, dict)
                or not isinstance(entry.get("file"), str)
                or not isinstance(entry.get("directory"), str)
                or not (isinstance(entry.get("command"), str)
                        or isinstance(entry.get("arguments"), list))
                for entry in database):
            raise ValueError("Invalid compilation database entries")
        if any(Path(entry["file"]).name == "PioScanlineEngine.cpp"
               for entry in database):
            raise ValueError("Use the standalone tests host database, not the firmware database")
        entries = []
        for entry in database:
            path = Path(entry["file"])
            if not path.is_absolute():
                path = Path(entry["directory"]) / path
            if own_host_code(path) and path.suffix == ".cpp":
                entries.append((entry, path.resolve()))
        if not entries:
            raise ValueError("No own host production sources in compilation database")
        filtered = output / "compile_commands.json"
        filtered.write_text(json.dumps([entry for entry, _ in entries], indent=2),
                            encoding="utf-8")
        executable = args.executable or args.tool
        with (output / "analysis.log").open("w", encoding="utf-8") as log:
            version = run([executable, "--version"], log)
            if version.returncode:
                raise RuntimeError("Tool version query failed")
            if args.tool == "clang-tidy":
                infrastructure = False
                for index, (_, source) in enumerate(entries):
                    result = run([executable, str(source), "-p", str(filtered.parent),
                                  "--config-file=" + str(ROOT / ".clang-tidy"),
                                  "--export-fixes=" + str(output / f"fixes-{index}.yaml")], log)
                    count = len(re.findall(r": (?:warning|error):", result.stdout))
                    diagnostics += count
                    if (result.returncode not in (0, 1)
                            or "[clang-diagnostic-error]" in result.stdout
                            or (result.returncode and not count)):
                        infrastructure = True
                status = ("infrastructure_error" if infrastructure else
                          "findings" if diagnostics else "clean")
            else:
                result = subprocess.run(
                    [executable, "--project=" + str(filtered),
                     "--enable=warning,performance,portability", "--std=c++17",
                     "--error-exitcode=1", "--xml", "--xml-version=2"],
                    cwd=ROOT, text=True, encoding="utf-8", errors="replace",
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
                log.write(result.stdout + result.stderr)
                xml = result.stderr
                parsed = ET.fromstring(xml)
                if (parsed.tag != "results" or parsed.get("version") != "2"
                        or parsed.find("errors") is None):
                    raise RuntimeError("cppcheck did not produce a version 2 results report")
                errors = parsed.findall("./errors/error")
                diagnostics = len(errors)
                (output / "cppcheck.xml").write_text(xml, encoding="utf-8")
                infrastructure = any(e.get("id") in {
                    "internalError", "syntaxError", "preprocessorErrorDirective"
                } for e in errors)
                status = ("infrastructure_error" if infrastructure or result.returncode not in (0, 1)
                          or (result.returncode and not errors) else
                          "findings" if errors else "clean")
    except (OSError, ValueError, RuntimeError, ET.ParseError) as error:
        (output / "infrastructure-error.txt").write_text(str(error), encoding="utf-8")
        print(error, file=sys.stderr)
    report = {"tool": args.tool, "status": status, "diagnostics": diagnostics}
    (output / "result.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    (output / "summary.md").write_text(
        f"### {args.tool}\n\nStatus: **{status}**; diagnostics: {diagnostics}.\n\n"
        "See the artifact for the complete log. Findings require triage; this\n"
        "does not classify them as new defects or accepted baseline warnings.\n",
        encoding="utf-8")
    print(json.dumps(report))
    return {"clean": 0, "findings": 1, "infrastructure_error": 2}[status]


def coverage(args):
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    kept = []
    line_counts = {}
    functions = {}
    for record in args.input.read_text(encoding="utf-8").split("end_of_record"):
        lines = record.strip().splitlines()
        source = next((line[3:] for line in lines if line.startswith("SF:")), None)
        if source is None or not own_host_code(source):
            continue
        # gcov can spell the same header differently in different units.
        # Use the same canonical key in the summary and the emitted LCOV.
        source = (ROOT / source).resolve().as_posix()
        lines = ["SF:" + source if line.startswith("SF:") else line for line in lines]
        kept.append("\n".join(lines) + "\nend_of_record\n")
        for line in lines:
            if line.startswith("DA:"):
                number, count, *_ = line[3:].split(",")
                key = (source, number)
                line_counts[key] = line_counts.get(key, 0) + int(count)
            elif line.startswith("FNDA:"):
                count, name = line[5:].split(",", 1)
                key = (source, name)
                functions[key] = functions.get(key, 0) + int(count)
    if not line_counts:
        raise ValueError("Coverage contains no own executable host lines")
    (output / "lcov.info").write_text("".join(kept), encoding="utf-8")
    totals = {}
    for kind, values in (("lines", line_counts), ("functions", functions)):
        hit = sum(count > 0 for count in values.values())
        total = len(values)
        totals[kind] = {"hit": hit, "total": total,
                        "percent": 100 * hit / total if total else None}
    (output / "summary.json").write_text(json.dumps(totals, indent=2), encoding="utf-8")
    rows = ["### Host coverage", "", "| Metric | Covered | Total | Coverage |",
            "|---|---:|---:|---:|"]
    for name, value in totals.items():
        percent = f"{value['percent']:.2f}%" if value["percent"] is not None else "N/A"
        rows.append(f"| {name} | {value['hit']} | {value['total']} | {percent} |")
    rows += ["", "No coverage threshold is enforced. Scope: portable production code",
             "and host-executed MDA/CGA/EGA lifecycle logic. Generated fonts, tests,",
             "the recording engine, SDK and real PIO/DMA engine are excluded.", ""]
    (output / "summary.md").write_text("\n".join(rows), encoding="utf-8")
    print("\n".join(rows))
    return 0


def sarif(args):
    """Filter only result locations, preserving the complete original artifact."""
    from urllib.parse import unquote, urlparse
    document = json.loads(args.input.read_text(encoding="utf-8"))
    removed = 0
    for run_data in document.get("runs", []):
        kept = []
        for result in run_data.get("results", []):
            locations = result.get("locations", [])
            if not locations:
                kept.append(result)  # Do not silently lose locationless diagnostics.
                continue
            artifact = locations[0].get("physicalLocation", {}).get("artifactLocation", {})
            uri = artifact.get("uri", "")
            if not uri and "index" in artifact:
                uri = run_data.get("artifacts", [])[artifact["index"]].get("location", {}).get("uri", "")
            if not uri:
                kept.append(result)  # An unresolved location is not a scope exclusion.
                continue
            path = unquote(urlparse(uri).path) if uri.startswith("file:") else unquote(uri)
            if own_host_code(path):
                kept.append(result)
            else:
                removed += 1
        run_data["results"] = kept
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "host.sarif").write_text(json.dumps(document), encoding="utf-8")
    print(f"Excluded {removed} result(s) outside the documented host production scope")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    analyzer = commands.add_parser("static")
    analyzer.add_argument("tool", choices=("clang-tidy", "cppcheck"))
    analyzer.add_argument("--build-dir", type=Path, required=True)
    analyzer.add_argument("--executable")
    analyzer.set_defaults(function=static_analysis)
    for command, function in (("coverage", coverage), ("sarif", sarif)):
        sub = commands.add_parser(command)
        sub.add_argument("--input", type=Path, required=True)
        sub.add_argument("--output-dir", type=Path, required=True)
        sub.set_defaults(function=function)
    analyzer.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        return args.function(args)
    except (OSError, ValueError) as error:
        print(f"Infrastructure/report error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
