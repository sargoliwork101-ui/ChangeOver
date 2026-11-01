#!/usr/bin/env python3
"""
check_module_contract.py - read-only boundary guardian for Firmware/Modules.

[EN] Why this file exists
     The firmware modules are folders with their own header, their own tests and
     their own README. What they are NOT is sealed: the include path is flat, so
     any module can include any other module's header and reach inside it, and a
     module's internal functions are visible to the linker unless they are
     static. Nothing in the repository noticed either of those before this tool.
     Splitting the work over several contributors without a gate would let one
     person's change quietly take another person's module down.

     This tool reads the sources and answers, without building or modifying
     anything:
       1. which module includes which other module's header,
       2. which modules depend on each other in a cycle,
       3. which module reads another module's global variable directly,
       4. how large each module's public surface is, and how much of the module
          is visible to the linker without being declared in its header.

     Report mode (default) always exits 0: it only says what it sees.
     Strict mode (--strict) fails on a boundary a module is not allowed to
     cross. Only the modules named in the allowlist are enforced, so the gate
     can be switched on one module at a time and the rest of the project stays
     exactly as it is today.

Usage
    python3 tools/check_module_contract.py                  # report only
    python3 tools/check_module_contract.py --strict         # fail on a breach
    python3 tools/check_module_contract.py --json           # machine readable
    python3 tools/check_module_contract.py --allow FILE     # other allowlist
"""
import json
import os
import re
import sys
REPOSITORY_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIRMWARE = os.path.join(REPOSITORY_ROOT, "Firmware")
MODULES = os.path.join(FIRMWARE, "Modules")
DEFAULT_ALLOWLIST = os.path.join(REPOSITORY_ROOT, "tools", "module_boundaries.json")
SKIP_DIRECTORIES = {"Tester", "Simulator", "build", "__pycache__"}
SHARED_LAYERS = ("Bsp", "Config", "App", "Rtos")
# ==================== Source reading ====================
def strip_comments(text, keep_strings=False):
    """
    [EN] Blank out comments, keeping the newlines so line numbers stay right.
         String literals are blanked too unless keep_strings is set: a header
         name mentioned in a comment must not count as a dependency, but the
         header name inside #include "x.h" is a string and must survive.
    """
    out = []
    index = 0
    length = len(text)
    while index < length:
        char = text[index]
        if char == "/" and index + 1 < length and text[index + 1] == "/":
            while index < length and text[index] != "\n":
                out.append(" ")
                index += 1
            continue
        if char == "/" and index + 1 < length and text[index + 1] == "*":
            index += 2
            while index + 1 < length and not (text[index] == "*" and text[index + 1] == "/"):
                out.append("\n" if text[index] == "\n" else " ")
                index += 1
            out.append("  ")
            index += 2
            continue
        if (not keep_strings) and char in ('"', "'"):
            quote = char
            out.append(" ")
            index += 1
            while index < length:
                if text[index] == "\\":
                    out.append("  ")
                    index += 2
                    continue
                if text[index] == quote or text[index] == "\n":
                    index += 1
                    break
                out.append(" ")
                index += 1
            out.append(" ")
            continue
        out.append(char)
        index += 1
    return "".join(out)
def read_source(path, keep_strings=False):
    with open(path, encoding="utf-8", errors="replace") as handle:
        return strip_comments(handle.read(), keep_strings)
def walk_sources(root):
    for directory, subdirectories, files in os.walk(root):
        subdirectories[:] = [d for d in subdirectories if d not in SKIP_DIRECTORIES]
        for name in sorted(files):
            if name.endswith((".c", ".h")):
                yield os.path.join(directory, name)
# ==================== Ownership map ====================
def build_owners():
    """
    [EN] header file name -> module that owns it. Firmware/Modules/<name> is one
         module; Bsp, Config, App and Rtos are the shared lower layers, treated
         as modules too, because a dependency on them is still a dependency the
         contributor has to declare.
    """
    owners = {}
    duplicates = []
    for layer in SHARED_LAYERS:
        root = os.path.join(FIRMWARE, layer)
        if not os.path.isdir(root):
            continue
        for path in walk_sources(root):
            if path.endswith(".h"):
                name = os.path.basename(path)
                if name in owners and owners[name] != layer:
                    duplicates.append(name)
                owners[name] = layer
    for module in module_directories():
        for path in walk_sources(os.path.join(MODULES, module)):
            if path.endswith(".h"):
                name = os.path.basename(path)
                if name in owners and owners[name] != module:
                    duplicates.append(name)
                owners[name] = module
    return owners, sorted(set(duplicates))
def module_directories():
    found = []
    for name in sorted(os.listdir(MODULES)):
        directory = os.path.join(MODULES, name)
        if not os.path.isdir(directory) or name in SKIP_DIRECTORIES:
            continue
        if any(True for _ in walk_sources(directory)):
            found.append(name)
    return found
# ==================== Analysis ====================
DEFINITION_PATTERN = re.compile(
    r"^(?!\s*static\b)[A-Za-z_][\w \t\*]*?\b(func__[A-Za-z0-9_]+)\s*\(", re.MULTILINE)
def find_definitions(text):
    """
    [EN] Non-static function DEFINITIONS, not prototypes. A line that starts
         with a return type is a definition only when a body brace comes before
         the terminating semicolon; cal_lut.c declares two BSP functions at file
         scope instead of including bsp_flash.h, and those prototypes were being
         counted as definitions - which hid the real finding behind a fake one.
    """
    names = set()
    for match in DEFINITION_PATTERN.finditer(text):
        tail = text[match.end():match.end() + 400]
        brace = tail.find("{")
        semicolon = tail.find(";")
        if brace < 0:
            continue
        if 0 <= semicolon < brace:
            continue
        names.add(match.group(1))
    return names
DECLARATION_PATTERN = re.compile(r"\b(func__[A-Za-z0-9_]+)\s*\(")
EXTERN_PATTERN = re.compile(r"\bextern\b[^;\n]*?\b([A-Za-z_]\w*__G__\w+)\b")
def analyse():
    owners, duplicates = build_owners()
    modules = module_directories()
    sources = []
    definitions = {}
    declarations = {}
    global_exports = {}
    for module in modules:
        for path in walk_sources(os.path.join(MODULES, module)):
            sources.append({
                "module": module,
                "path": path,
                "relative": os.path.relpath(path, FIRMWARE),
                "includes": read_source(path, keep_strings=True),
                "code": read_source(path),
            })
    for source in sources:
        module = source["module"]
        text = source["code"]
        if source["path"].endswith(".h"):
            for name in DECLARATION_PATTERN.findall(text):
                declarations.setdefault(module, set()).add(name)
            for name in EXTERN_PATTERN.findall(text):
                global_exports.setdefault(name, set()).add(module)
        else:
            definitions.setdefault(module, set()).update(find_definitions(text))
    include_sites = []
    include_pairs = {}
    for source in sources:
        for match in re.finditer(r'^\s*#\s*include\s+"([^"]+)"', source["includes"], re.MULTILINE):
            provider = owners.get(os.path.basename(match.group(1)))
            if provider is None or provider == source["module"]:
                continue
            line = source["includes"].count("\n", 0, match.start()) + 1
            include_sites.append((source["module"], provider, source["relative"], line))
            include_pairs.setdefault((source["module"], provider), []).append(
                (source["relative"], line))
    # Names another module could legally call: declared in a header, or defined
    # without static (visible to the linker even when no header mentions it).
    callable_by_module = {}
    for module in modules:
        names = set(declarations.get(module, set())) | set(definitions.get(module, set()))
        callable_by_module[module] = names
    call_counts = {}
    global_counts = {}
    global_names = {}
    for source in sources:
        if not source["path"].endswith(".c"):
            continue
        text = source["code"]
        module = source["module"]
        for provider in modules:
            if provider == module:
                continue
            hits = 0
            for name in callable_by_module[provider]:
                hits += len(re.findall(r"\b%s\b" % re.escape(name), text))
            if hits:
                call_counts[(module, provider)] = hits
        for name, providers in global_exports.items():
            for provider in providers:
                if provider == module:
                    continue
                hits = len(re.findall(r"\b%s\b" % re.escape(name), text))
                if hits:
                    global_counts[(module, provider)] = global_counts.get((module, provider), 0) + hits
                    global_names.setdefault((module, provider), set()).add(name)
    graph = {module: set() for module in modules}
    for (module, provider) in list(include_pairs) + list(call_counts) + list(global_counts):
        graph[module].add(provider)
    return {
        "modules": modules,
        "duplicates": duplicates,
        "owners": owners,
        "include_sites": include_sites,
        "include_pairs": include_pairs,
        "call_counts": call_counts,
        "global_counts": global_counts,
        "global_names": global_names,
        "declarations": declarations,
        "definitions": definitions,
        "global_exports": global_exports,
        "graph": graph,
        "cycles": find_cycles(graph),
        "short_cycles": find_short_cycles(graph, 4),
    }
def find_cycles(graph):
    """
    [EN] Tarjan's strongly connected components: every component with more than
         one member is a group of modules that can only be changed together.
    """
    counter = [0]
    stack = []
    lowlink = {}
    index = {}
    on_stack = {}
    result = []
    def strongconnect(node):
        index[node] = counter[0]
        lowlink[node] = counter[0]
        counter[0] += 1
        stack.append(node)
        on_stack[node] = True
        for successor in sorted(graph.get(node, ())):
            if successor not in index:
                strongconnect(successor)
                lowlink[node] = min(lowlink[node], lowlink[successor])
            elif on_stack.get(successor):
                lowlink[node] = min(lowlink[node], index[successor])
        if lowlink[node] == index[node]:
            component = []
            while True:
                member = stack.pop()
                on_stack[member] = False
                component.append(member)
                if member == node:
                    break
            if len(component) > 1:
                result.append(sorted(component))
    sys.setrecursionlimit(10000)
    for node in sorted(graph):
        if node not in index:
            strongconnect(node)
    return sorted(result)
def find_short_cycles(graph, max_length):
    """
    [EN] Elementary cycles up to max_length. The big component above is what
         makes four modules inseparable; these small loops are the actual cuts
         that break it, so the report names both.
    """
    found = set()
    def walk(start, node, path):
        for successor in sorted(graph.get(node, ())):
            if successor == start and len(path) >= 2:
                found.add(tuple(path))
            elif successor not in path and len(path) < max_length:
                walk(start, successor, path + [successor])
    for node in sorted(graph):
        walk(node, node, [node])
    ordered = []
    for cycle in found:
        rotated = min(cycle[index:] + cycle[:index] for index in range(len(cycle)))
        ordered.append(rotated)
    return sorted(set(ordered), key=lambda c: (len(c), c))
# ==================== Reporting ====================
def public_surface(report, module):
    declared = report["declarations"].get(module, set())
    defined = report["definitions"].get(module, set())
    return len(declared), len(defined), len(defined - declared)
def print_report(report):
    line = "=" * 78
    print(line)
    print("MODULE BOUNDARY REPORT - read only, nothing was built, changed or run")
    print(line)
    print("modules with code            : %d" % len(report["modules"]))
    print("headers in the ownership map : %d" % len(report["owners"]))
    print("duplicate header names       : %s" %
          (", ".join(report["duplicates"]) if report["duplicates"] else "none"))
    print()
    print("[1] WHO INCLUDES WHOM (another module's public header)")
    print("    %-14s -> %-14s %5s" % ("module", "provider", "sites"))
    for (module, provider), sites in sorted(report["include_pairs"].items(),
                                            key=lambda kv: (-len(kv[1]), kv[0])):
        print("    %-14s -> %-14s %5d" % (module, provider, len(sites)))
    print("    total: %d include site(s) across %d module pair(s)"
          % (len(report["include_sites"]), len(report["include_pairs"])))
    print()
    print("[2] CYCLES - modules that can only be changed together")
    if not report["cycles"]:
        print("    none")
    for component in report["cycles"]:
        print("    inseparable group: { %s }" % ", ".join(component))
    for cycle in report["short_cycles"]:
        print("    closed loop      : %s" % " -> ".join(list(cycle) + [cycle[0]]))
    print("    total: %d group(s), %d loop(s) up to length 4"
          % (len(report["cycles"]), len(report["short_cycles"])))
    print()
    print("[3] DIRECT READS OF ANOTHER MODULE'S GLOBAL")
    if not report["global_counts"]:
        print("    none")
    for (module, provider), count in sorted(report["global_counts"].items(),
                                            key=lambda kv: -kv[1]):
        names = sorted(report["global_names"].get((module, provider), set()))
        print("    %-14s reads %2d global(s) of %-12s (%d uses): %s"
              % (module, len(names), provider, count,
                 ", ".join(names[:3]) + (" ..." if len(names) > 3 else "")))
    print("    total: %d use(s) across %d pair(s)"
          % (sum(report["global_counts"].values()), len(report["global_counts"])))
    print()
    print("[4] CROSS-MODULE CALLS (public surface actually in use)")
    for (module, provider), count in sorted(report["call_counts"].items(),
                                            key=lambda kv: -kv[1])[:10]:
        print("    %-14s -> %-14s %4d call site(s)" % (module, provider, count))
    print("    total: %d call site(s) across %d pair(s)"
          % (sum(report["call_counts"].values()), len(report["call_counts"])))
    print()
    print("[5] PUBLIC SURFACE / LEAKED INTERNALS")
    print("    %-14s %8s %10s %9s" % ("module", "declared", "defined", "leaked"))
    leaked_total = 0
    for module in report["modules"]:
        declared, defined, leaked = public_surface(report, module)
        leaked_total += leaked
        print("    %-14s %8d %10d %9d" % (module, declared, defined, leaked))
    print("    leaked = defined without static and absent from the module's own")
    print("    header, so the linker lets any other module call it. total: %d"
          % leaked_total)
    print(line)
    print("REPORT MODE: nothing was enforced. Everything above is today's state,")
    print("not a new error. Run with --strict to fail on a boundary breach.")
    print(line)
def run_strict(report, allowlist_path):
    if not os.path.isfile(allowlist_path):
        print("ERROR: --strict needs an allowlist file: %s" % allowlist_path)
        print("       Nothing was enforced. Create the file or run without --strict.")
        return 2
    with open(allowlist_path, encoding="utf-8") as handle:
        config = json.load(handle)
    enforced = config.get("enforced", [])
    allowed_includes = config.get("allow", {})
    allowed_globals = config.get("allow_globals", {})
    failures = []
    for module in enforced:
        if module not in report["modules"]:
            failures.append("enforced module '%s' does not exist" % module)
            continue
        permitted = set(allowed_includes.get(module, []))
        for (owner, provider), sites in sorted(report["include_pairs"].items()):
            if owner != module or provider in permitted:
                continue
            failures.append("%s includes %s (%s) - not in its allowlist"
                            % (module, provider,
                               ", ".join("%s:%d" % site for site in sites[:3])))
        permitted_globals = set(allowed_globals.get(module, []))
        for (owner, provider) in sorted(report["global_counts"]):
            if owner != module or provider in permitted_globals:
                continue
            failures.append("%s reads a global of %s - not in its allowlist"
                            % (module, provider))
    print()
    print("STRICT MODE: enforcing %d module(s): %s"
          % (len(enforced), ", ".join(enforced) if enforced else "(none declared)"))
    if failures:
        for failure in failures:
            print("  FAIL  %s" % failure)
        print("%d boundary breach(es)." % len(failures))
        return 1
    print("  OK    no enforced module crossed a boundary it may not cross")
    return 0
HELP_TEXT = """usage: python3 tools/check_module_contract.py [option]
  (no option)   print the full report, always exit 0
  --strict      fail with exit 1 if an enforced module crossed a boundary
  --json        print the measured numbers as JSON instead of a table
  --allow FILE  read the allowlist from FILE instead of tools/module_boundaries.json
  --help        print this text
Every mode only reads the sources. Nothing is built, changed or run."""
def main(argv):
    for flag in ("-h", "--help"):
        if flag in argv:
            print(HELP_TEXT)
            return 0
    unknown = [item for item in argv
               if item.startswith("-") and item not in ("--strict", "--json", "--allow")]
    if unknown:
        print("ERROR: unknown option: %s" % " ".join(unknown))
        print(HELP_TEXT)
        return 2
    allowlist_path = DEFAULT_ALLOWLIST
    if "--allow" in argv:
        index = argv.index("--allow") + 1
        if index >= len(argv) or argv[index].startswith("-"):
            print("ERROR: --allow needs a file name.")
            print(HELP_TEXT)
            return 2
        allowlist_path = argv[index]
    report = analyse()
    if "--json" in argv:
        printable = {
            "modules": report["modules"],
            "include_pairs": {"%s->%s" % key: len(value)
                              for key, value in report["include_pairs"].items()},
            "cycles": report["cycles"],
            "short_cycles": ["->".join(cycle) for cycle in report["short_cycles"]],
            "global_counts": {"%s->%s" % key: value
                              for key, value in report["global_counts"].items()},
            "call_counts": {"%s->%s" % key: value
                            for key, value in report["call_counts"].items()},
            "declared": {module: len(names) for module, names in report["declarations"].items()},
            "defined": {module: len(names) for module, names in report["definitions"].items()},
        }
        print(json.dumps(printable, indent=2, ensure_ascii=False))
    else:
        print_report(report)
    return run_strict(report, allowlist_path) if "--strict" in argv else 0
if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
