#!/usr/bin/env python3
"""Reproducible host analysis and report scoping; no production fixes.

Exit codes: 0 clean/known findings/report generated, 1 new analyzer findings,
2 infrastructure or invalid input. clang-tidy reports also require PyYAML.
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
    from quality_baseline import Diagnostics, compare, load_baseline

    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    status = "infrastructure_error"
    diagnostics = 0
    records = []
    invocations = []
    comparison = None
    baseline_path = getattr(args, "baseline", ROOT / "tools/quality-baseline.json")
    try:
        baseline = load_baseline(baseline_path)
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
        sources = [path.relative_to(ROOT).as_posix() for _, path in entries]
        if (set(sources) != PORTABLE_SOURCES | DEVICE_SOURCES
                or len(sources) != len(set(sources))):
            raise ValueError("Incomplete or duplicate host analysis source manifest")
        filtered = output / "compile_commands.json"
        filtered.write_text(json.dumps([entry for entry, _ in entries], indent=2),
                            encoding="utf-8")
        executable = args.executable or args.tool
        structured = Diagnostics(ROOT)
        with (output / "analysis.log").open("w", encoding="utf-8") as log:
            version = run([executable, "--version"], log)
            if version.returncode:
                raise RuntimeError("Tool version query failed")
            if args.tool == "clang-tidy":
                # A clean unit normally has no export-fixes file. Remove old
                # files first so a prior run can never supply its diagnostics.
                for index in range(len(entries)):
                    (output / f"fixes-{index}.yaml").unlink(missing_ok=True)
                for index, (_, source) in enumerate(entries):
                    fixes = output / f"fixes-{index}.yaml"
                    result = run([executable, str(source), "-p", str(filtered.parent),
                                  "--config-file=" + str(ROOT / ".clang-tidy"),
                                  "--export-fixes=" + str(fixes)], log)
                    count = len(re.findall(r": (?:warning|error|remark):", result.stdout))
                    diagnostics += count
                    invocations.append({"source": sources[index],
                                        "returncode": result.returncode,
                                        "displayed_diagnostics": count,
                                        "report": fixes.name if fixes.exists() else None})
                    if (result.returncode not in (0, 1)
                            or "[clang-diagnostic-error]" in result.stdout
                            or (result.returncode and not count)):
                        raise RuntimeError(f"clang-tidy failed/incomplete for {source}")
                    unit = structured.clang(fixes, source, count)
                    for record in unit:
                        record["translation_unit"] = sources[index]
                        record["report"] = fixes.name
                    records.extend(unit)
            else:
                result = subprocess.run(
                    [executable, "--project=" + str(filtered),
                     "--enable=warning,performance,portability", "--std=c++17",
                     "--error-exitcode=1", "--xml", "--xml-version=2"],
                    cwd=ROOT, text=True, encoding="utf-8", errors="replace",
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
                log.write(result.stdout + result.stderr)
                xml = result.stderr
                (output / "cppcheck.xml").write_text(xml, encoding="utf-8")
                invocations.append({"sources": sources, "returncode": result.returncode,
                                    "report": "cppcheck.xml"})
                if result.returncode not in (0, 1):
                    raise RuntimeError("cppcheck failed or crashed")
                records = structured.cppcheck(xml, result.stdout, len(entries))
                diagnostics = len(records)
                if result.returncode and not records:
                    raise RuntimeError("cppcheck failed without diagnostics")
        comparison = compare(args.tool, records, baseline)
        status = ("new_findings" if comparison["new"] else
                  "known_findings" if records else "clean")
    except (ImportError, OSError, ValueError, TypeError, KeyError, AttributeError,
            RuntimeError, ET.ParseError) as error:
        (output / "infrastructure-error.txt").write_text(str(error), encoding="utf-8")
        print(error, file=sys.stderr)
    report = {"tool": args.tool, "status": status, "diagnostics": diagnostics,
              "baseline_evaluated": comparison is not None, "baseline": comparison}
    (output / "result.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    (output / "diagnostics.json").write_text(json.dumps(records, indent=2), encoding="utf-8")
    (output / "invocations.json").write_text(json.dumps(invocations, indent=2), encoding="utf-8")
    details = "Baseline not evaluated: analysis was incomplete or invalid.\n"
    if comparison is not None:
        details = (f"Unique diagnostics: {comparison['unique_diagnostics']}; "
                   f"known: {len(comparison['known'])}; new: {len(comparison['new'])}; "
                   f"missing baseline entries: {len(comparison['missing'])}.\n")
        current = {record["id"]: record for record in records}
        for label in ("known", "new", "missing"):
            entries = ([current[key] for key in comparison["known"]]
                       if label == "known" else comparison[label])
            for entry in entries:
                identity = entry["identity"]
                details += (f"\n- **{label}** `{entry['id']}`: "
                            f"`{identity['path']}` / `{identity['check']}`: "
                            f"{identity['message']}\n")
    (output / "summary.md").write_text(
        f"### {args.tool}\n\nStatus: **{status}**; diagnostics: {diagnostics}.\n\n"
        + details + "\nAll raw diagnostics remain in the artifact. "
        "Known findings are tracked, not suppressed or fixed.\n",
        encoding="utf-8")
    console = {key: report[key] for key in ("tool", "status", "diagnostics", "baseline_evaluated")}
    if comparison is not None:
        console.update({key: len(comparison[key]) for key in ("known", "new", "missing")})
    print(json.dumps(console))
    return {"clean": 0, "known_findings": 0, "new_findings": 1,
            "infrastructure_error": 2}[status]


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
    analyzer.add_argument("--baseline", type=Path, default=ROOT / "tools/quality-baseline.json")
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
