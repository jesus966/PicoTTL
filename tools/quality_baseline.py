"""Exact, source-anchored identities for structured static diagnostics."""
# Copyright (c) 2026 Jesús Fernández Gamito
# SPDX-License-Identifier: MIT

from bisect import bisect_right
import hashlib
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET


TOOLS = {"clang-tidy", "cppcheck"}
INFRASTRUCTURE_CHECKS = {
    "clang-diagnostic-error", "internalError", "syntaxError",
    "preprocessorErrorDirective", "missingInclude", "missingIncludeSystem",
    "toomanyconfigs",
}
# A lexer for source anchors, not a C++ semantic parser. Preserve literals and
# multi-character operators; ignore only comments and inter-token whitespace.
TOKENS = re.compile(
    r'(?P<comment>//[^\r\n]*|/\*[\s\S]*?\*/)'
    r'|(?:u8|u|U|L)?R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\([\s\S]*?\)(?P=delimiter)"'
    r'|(?:u8|u|U|L)?"(?:\\[\s\S]|[^"\\])*"'
    r"|(?:u8|u|U|L)?'(?:\\[\s\S]|[^'\\])*'"
    r'|[A-Za-z_]\w*|\d[\w\x27.]*'
    r'|<=>|>>=|<<=|->\*|\.\.\.|::|->|\+\+|--|&&|\|\||<=|>=|==|!='
    r'|\+=|-=|\*=|/=|%=|&=|\|=|\^=|<<|>>|##|[^\s]'
)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=True,
                                     separators=(",", ":")).encode()).hexdigest()


def relative_path(path, root):
    candidate = Path(path)
    if not candidate.is_absolute():
        candidate = root / candidate
    try:
        return candidate.resolve().relative_to(root.resolve()).as_posix()
    except ValueError as error:
        raise ValueError(f"Diagnostic location outside repository: {path}") from error


class SourceIndex:
    def __init__(self, path):
        self.raw = path.read_bytes()
        self.text = self.raw.decode("utf-8")
        matches = [m for m in TOKENS.finditer(self.text) if m.lastgroup != "comment"]
        self.starts = [m.start() for m in matches]
        self.ends = [m.end() for m in matches]
        values = [m.group() for m in matches]
        stack = []
        start = 0
        seen = {}
        self.anchors = []
        for index, token in enumerate(values):
            scope = [" ".join(part) for part in stack]
            context = " ".join(values[max(0, index - 8):index + 9])
            key = (tuple(scope), context)
            seen[key] = seen.get(key, 0) + 1
            self.anchors.append({"scope": scope, "context": context,
                                 "occurrence": seen[key]})
            if token == "{":
                stack.append(values[start:index])
                start = index + 1
            elif token == "}":
                if not stack:
                    raise ValueError(f"Cannot anchor unbalanced source: {path}")
                stack.pop()
                start = index + 1
            elif token == ";":
                start = index + 1
        if stack:
            raise ValueError(f"Cannot anchor unbalanced source: {path}")

    def anchor(self, *, byte_offset=None, line=None, column=None):
        if byte_offset is not None:
            if type(byte_offset) is not int or not 0 <= byte_offset < len(self.raw):
                raise ValueError("Invalid diagnostic byte offset")
            offset = len(self.raw[:byte_offset].decode("utf-8"))
        else:
            lines = self.text.splitlines(keepends=True)
            if (type(line) is not int or type(column) is not int
                    or not 1 <= line <= len(lines)
                    or not 1 <= column <= len(lines[line - 1].rstrip("\r\n"))):
                raise ValueError("Invalid diagnostic line/column")
            offset = sum(map(len, lines[:line - 1])) + column - 1
        index = bisect_right(self.starts, offset) - 1
        if index < 0 or offset >= self.ends[index]:
            raise ValueError("Diagnostic does not point to a source token")
        return dict(self.anchors[index], token_offset=offset - self.starts[index])


class Diagnostics:
    def __init__(self, root):
        self.root = root.resolve()
        self.sources = {}

    def location(self, path, **position):
        relative = relative_path(path, self.root)
        if relative not in self.sources:
            self.sources[relative] = SourceIndex(self.root / relative)
        return {"path": relative, "anchor": self.sources[relative].anchor(**position)}

    def diagnostic(self, tool, check, message, severity, primary, details):
        if not all(isinstance(v, str) and v for v in (check, message, severity)):
            raise ValueError("Diagnostic missing rule, message or severity")
        if check in INFRASTRUCTURE_CHECKS:
            raise ValueError(f"Infrastructure diagnostic: {check}")
        identity = dict(tool=tool, check=check, message=message, severity=severity,
                        **primary, details_sha256=digest(details))
        return {"id": digest(identity), "identity": identity}

    def clang(self, path, source, visible_count):
        if not path.exists():
            if visible_count:
                raise ValueError("Missing clang-tidy YAML for displayed diagnostics")
            return []  # clang-tidy does not write export-fixes for clean units.
        import yaml

        class UniqueLoader(yaml.SafeLoader):
            pass

        def mapping(loader, node):
            result = {}
            for key_node, value_node in node.value:
                key = loader.construct_object(key_node)
                if key in result:
                    raise ValueError(f"Duplicate YAML key: {key}")
                result[key] = loader.construct_object(value_node)
            return result

        UniqueLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, mapping)
        try:
            document = yaml.load(path.read_text(encoding="utf-8"), Loader=UniqueLoader)
        except yaml.YAMLError as error:
            raise ValueError("Invalid clang-tidy YAML") from error
        if (not isinstance(document, dict)
                or not isinstance(document.get("MainSourceFile"), str)
                or relative_path(document["MainSourceFile"], self.root)
                != relative_path(source, self.root)
                or not isinstance(document.get("Diagnostics"), list)):
            raise ValueError("Incomplete or wrong-unit clang-tidy report")
        records = []
        for diagnostic in document["Diagnostics"]:
            message = diagnostic["DiagnosticMessage"]
            primary = self.location(message["FilePath"], byte_offset=message["FileOffset"])
            notes = []
            for note in diagnostic.get("Notes", []):
                normalized = {"message": note["Message"]}
                if note.get("FilePath"):
                    normalized.update(self.location(note["FilePath"],
                                                    byte_offset=note["FileOffset"]))
                notes.append(normalized)
            records.append(self.diagnostic(
                "clang-tidy", diagnostic["DiagnosticName"], message["Message"],
                diagnostic["Level"].lower(), primary, notes))
        if len(records) != visible_count:
            raise ValueError("clang-tidy text/YAML diagnostic counts differ")
        return records

    def cppcheck(self, xml, progress, expected_files):
        parsed = ET.fromstring(xml)
        if (parsed.tag != "results" or parsed.get("version") != "2"
                or parsed.find("errors") is None or parsed.find("cppcheck") is None):
            raise ValueError("cppcheck did not produce a version 2 results report")
        completed = [(int(a), int(b)) for a, b in re.findall(
            r"\b(\d+)/(\d+) files checked\b", progress)]
        if (expected_files, expected_files) not in completed:
            raise ValueError("Incomplete cppcheck file progress")
        records = []
        for error in parsed.findall("./errors/error"):
            locations = error.findall("location")
            if not locations:
                raise ValueError("cppcheck diagnostic has no stable source location")
            normalized = [self.location(p.attrib["file"], line=int(p.attrib["line"]),
                                        column=int(p.attrib["column"])) for p in locations]
            details = {"locations": normalized, "symbols": [p.text for p in error.findall("symbol")],
                       "verbose": error.get("verbose", "")}
            records.append(self.diagnostic(
                "cppcheck", error.attrib["id"], error.attrib["msg"],
                error.attrib["severity"], normalized[0], details))
        return records


def load_baseline(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"Duplicate baseline JSON key: {key}")
            result[key] = value
        return result

    document = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique)
    if (not isinstance(document, dict) or type(document.get("schema_version")) is not int
            or document["schema_version"] != 1 or set(document.get("tools", {})) != TOOLS):
        raise ValueError("Invalid baseline schema or tool set")
    for tool, entries in document["tools"].items():
        if not isinstance(entries, list):
            raise ValueError("Baseline diagnostics must be a list")
        seen = set()
        for entry in entries:
            identity = entry["identity"]
            anchor = identity.get("anchor", {})
            if (set(identity) != {"tool", "check", "message", "severity", "path", "anchor", "details_sha256"}
                    or identity["tool"] != tool
                    or not all(isinstance(identity[key], str) and identity[key]
                               for key in ("check", "message", "severity", "path"))
                    or Path(identity["path"]).is_absolute()
                    or ".." in Path(identity["path"]).parts
                    or not re.fullmatch(r"[0-9a-f]{64}", identity["details_sha256"])
                    or set(anchor) != {"scope", "context", "occurrence", "token_offset"}
                    or not isinstance(anchor["scope"], list)
                    or not all(isinstance(part, str) for part in anchor["scope"])
                    or not isinstance(anchor["context"], str) or not anchor["context"]
                    or type(anchor["occurrence"]) is not int or anchor["occurrence"] < 1
                    or type(anchor["token_offset"]) is not int or anchor["token_offset"] < 0
                    or identity["check"] in INFRASTRUCTURE_CHECKS
                    or not isinstance(entry.get("reason"), str) or not entry["reason"].strip()
                    or entry["id"] != digest(identity) or entry["id"] in seen):
                raise ValueError("Invalid, duplicate or unreviewed baseline diagnostic")
            seen.add(entry["id"])
    return document


def compare(tool, diagnostics, baseline):
    current = {diagnostic["id"]: diagnostic for diagnostic in diagnostics}
    known = {entry["id"]: entry for entry in baseline["tools"][tool]}
    return {"known": sorted(current.keys() & known.keys()),
            "new": [current[key] for key in sorted(current.keys() - known.keys())],
            "missing": [known[key] for key in sorted(known.keys() - current.keys())],
            "unique_diagnostics": len(current), "occurrences": len(diagnostics)}
