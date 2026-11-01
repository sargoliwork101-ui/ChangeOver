# tools/README.md - who owns the checkers, and where the boundary data lives

One copy of every checker lives in this folder. None of them is copied into a
module folder, and none of them lives inside `Firmware/`. This file records the
reason, so the question "should the boundary checker sit in every module?" is
answered once instead of in every review.

## Why one central copy

A module folder owns one module. A boundary is a relationship between two
modules, so it belongs to neither of them. If `check_module_contract.py` were
copied into `Firmware/Modules/Charger/`, then the owner of Charger would own the
rule that says what Charger may reach - which is exactly the rule that has to
stay outside their reach to mean anything.

Three more practical reasons:

| If the checker is copied into 13 folders | If there is one copy here |
| --- | --- |
| 13 copies drift apart within weeks | one file, one behaviour |
| a bug means 13 fixes | a bug means one fix |
| each copy sees only its own folder, so it cannot see who depends on whom | it reads the whole tree, so it can see both sides of every edge |
| each module can weaken its own copy | no module can weaken the gate |

## What lives where

| Thing | Path | Owner | Why there |
| --- | --- | --- | --- |
| the checker itself | `tools/check_module_contract.py` | whoever integrates the tree | read-only, runs over all modules at once |
| the list of allowed boundaries | `tools/module_boundaries.json` | whoever integrates the tree | shared between modules, so it is one central file, never a per-module file |
| what a module promises its callers | `Firmware/Modules/<Module>/CONTRACT.md` | the module owner | it describes one module only, so it sits next to that module's `README.md` |
| tests of one module | `Firmware/Modules/<Module>/Tester/` | the module owner | unchanged, this is how it already works |

So: the tool is central, the rules about who may reach whom are central, and the
only per-module file is the contract sheet, which is data, not a second copy of
the tool.

## Current state

`tools/module_boundaries.json` ships with an empty `enforced` list. The report
mode runs over the whole tree; strict mode enforces nothing until a module is
added to that list on purpose. See
`python3 tools/check_module_contract.py --help` for the four ways to run it.
