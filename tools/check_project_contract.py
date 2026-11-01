#!/usr/bin/env python3
"""Read-only checker for board boundary declarations and root-level ownership."""
import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = os.path.join(ROOT, "tools", "project_boundaries.json")


def _safe_relative_path(path):
    return (isinstance(path, str) and bool(path) and not os.path.isabs(path)
            and all(part not in ("", ".", "..") for part in path.split("/")))


def read_json_contract(path):
    with open(path, encoding="utf-8") as handle:
        text = handle.read()
    match = re.search(r"```contract\s+json\s*\n(.*?)\n```", text, re.DOTALL)
    if not match:
        raise ValueError("missing ```contract json``` block")
    return json.loads(match.group(1))


def check(manifest):
    errors = []

    def fail(message):
        errors.append(message)
        print("FAIL " + message)

    projects = manifest.get("projects")
    if not isinstance(projects, dict) or not projects:
        print("FAIL manifest must contain a non-empty projects object")
        return 1

    # Validate that every immediate repository entry is explicitly assigned to
    # one project, declared shared/outside the board projects, or intentionally
    # excluded (currently only Git metadata). This does not claim ownership of
    # files below a project root individually.
    assignments = []
    for name, project in projects.items():
        roots = project.get("roots", [project.get("root")])
        roots = [item for item in roots if item]
        if not roots:
            fail(f"{name}: no owned root configured")
        for root in roots:
            if not _safe_relative_path(root):
                fail(f"{name}: unsafe root path {root!r}")
                continue
            full = os.path.join(ROOT, root)
            if not os.path.isdir(full):
                fail(f"{name}: missing root directory {root}")
            for previous, previous_name in assignments:
                if root == previous or root.startswith(previous + "/") or previous.startswith(root + "/"):
                    fail(f"overlapping ownership: {previous_name} ({previous}) and {name} ({root})")
            assignments.append((root, name))

    outside_dirs = manifest.get("outside_directories", [])
    outside_files = manifest.get("outside_files", [])
    ignored = manifest.get("ignored_root_entries", [])
    for label, paths in (("outside_directories", outside_dirs),
                         ("outside_files", outside_files),
                         ("ignored_root_entries", ignored)):
        if not isinstance(paths, list):
            fail(f"{label} must be a list")
            continue
        for path in paths:
            if not _safe_relative_path(path) or "/" in path:
                fail(f"{label}: expected safe top-level path, got {path!r}")
                continue
            full = os.path.join(ROOT, path)
            if label == "outside_directories" and not os.path.isdir(full):
                fail(f"{label}: missing directory {path}")
            if label == "outside_files" and not os.path.isfile(full):
                fail(f"{label}: missing file {path}")
            if label == "ignored_root_entries" and not os.path.exists(full):
                fail(f"{label}: missing path {path}")
            assignments.append((path, label))

    classified = [path for path, _owner in assignments]
    if len(classified) != len(set(classified)):
        fail("a top-level path is assigned more than once")
    actual_entries = set(os.listdir(ROOT))
    declared_entries = set(classified)
    for path in sorted(actual_entries - declared_entries):
        fail(f"unclassified top-level repository entry: {path}")
    for path in sorted(declared_entries - actual_entries):
        fail(f"manifest classifies a non-existent top-level entry: {path}")

    for name, project in projects.items():
        roots = project.get("roots", [project.get("root")])
        roots = [item for item in roots if item]
        contract_path = project.get("contract")
        if not _safe_relative_path(contract_path):
            fail(f"{name}: unsafe/missing contract path")
            continue
        if not any(contract_path == root or contract_path.startswith(root + "/") for root in roots):
            fail(f"{name}: contract must be inside an owned root")
        full_contract_path = os.path.join(ROOT, contract_path)
        if not os.path.isfile(full_contract_path):
            fail(f"{name}: missing contract {contract_path}")
            continue
        try:
            contract = read_json_contract(full_contract_path)
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            fail(f"{name}: invalid contract block: {exc}")
            continue
        if contract.get("project") != name:
            fail(f"{name}: contract project must be {name}")
        if contract.get("version") != manifest.get("version"):
            fail(f"{name}: contract version does not match manifest")
        if contract.get("root") and contract["root"] not in roots:
            fail(f"{name}: contract root is not owned by this project")
        if contract.get("roots") and contract["roots"] != roots:
            fail(f"{name}: contract roots do not match manifest")

        route_source = project.get("route_source")
        if route_source:
            if not _safe_relative_path(route_source):
                fail(f"{name}: unsafe route source path")
            elif not any(route_source.startswith(root + "/") for root in roots):
                fail(f"{name}: route source must be inside an owned root")
            elif not os.path.isfile(os.path.join(ROOT, route_source)):
                fail(f"{name}: missing route source {route_source}")
            else:
                with open(os.path.join(ROOT, route_source), encoding="utf-8", errors="replace") as handle:
                    route_text = handle.read()
                source_routes = sorted(set(
                    f"{method.replace('HTTP_', '')} {path}"
                    for path, method in re.findall(
                        r'\.on\("([^"]+)"\s*,\s*(HTTP_\w+)', route_text)))
                contract_routes = sorted(set(contract.get("http_routes", [])))
                if source_routes != contract_routes:
                    fail(f"{name}: HTTP route list in contract does not match {route_source}")

        for item in project.get("locked_symbols", []):
            source_path = item.get("file")
            if not _safe_relative_path(source_path):
                fail(f"{name}: unsafe interface source path {source_path!r}")
                continue
            if not any(source_path.startswith(root + "/") for root in roots):
                fail(f"{name}: interface source must be inside an owned root: {source_path}")
            full_source_path = os.path.join(ROOT, source_path)
            if not os.path.isfile(full_source_path):
                fail(f"{name}: missing interface source {source_path}")
                continue
            with open(full_source_path, encoding="utf-8", errors="replace") as handle:
                source = handle.read()
            for symbol, expected in item.get("defines", {}).items():
                pattern = r"^\s*#\s*define\s+" + re.escape(symbol) + r"\s+([^\s/]+)"
                found = re.findall(pattern, source, re.MULTILINE)
                if len(found) != 1:
                    fail(f"{name}: expected one definition of {symbol} in {source_path}, found {len(found)}")
                elif found[0] != expected:
                    fail(f"{name}: {symbol} in {source_path} is {found[0]}, contract requires {expected}")

    if errors:
        print(f"PROJECT CONTRACT CHECK FAILED: {len(errors)} error(s)")
        return 1
    print("PASS: root-level paths are classified; declared routes and locked constants match source")
    print("NOTE: This is a consistency check, not a protected PR gate or a substitute for owner review.")
    return 0


def main():
    parser = argparse.ArgumentParser(description="Check board contracts and declared source interfaces.")
    parser.add_argument("--json", action="store_true", help="print the project boundary manifest")
    args = parser.parse_args()
    try:
        with open(MANIFEST, encoding="utf-8") as handle:
            manifest = json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        print(f"ERROR: cannot read {MANIFEST}: {exc}", file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps(manifest, ensure_ascii=False, indent=2))
        return 0
    return check(manifest)


if __name__ == "__main__":
    sys.exit(main())
