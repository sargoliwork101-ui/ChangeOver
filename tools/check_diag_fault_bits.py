#!/usr/bin/env python3
"""
[EN] Guard for the Diag fault-counter bit mapping (audit item 6).

func__Diag_RecordFault() decodes three counters out of the fault mask:
  uint32_t__batLostCount    <- FAULT_CHARGER_BAT_LOST
  uint32_t__techFaultCount  <- FAULT_CHARGER_TECHNICAL
  uint32_t__ovCount         <- FAULT_ADC  (legacy field name, kept for
                              on-flash record layout compatibility)

This checker fails if diag.c ever decodes the mask with raw bit literals
again (e.g. (1u << 6)), if a counter is wired to the wrong FAULT_* name,
or if the expected FAULT_* constants disappear / collide in app_types.h.
Renumbering bits in app_types.h stays legal: both sides then move
together and this check keeps passing.

[FA] نگهبان نگاشت بیت‌های شمارنده‌های خطا در ‎Diag‎ (مورد ۶ ممیزی).
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / "Firmware/Config/Inc/app_types.h"
SOURCE = ROOT / "Firmware/Modules/Diag/diag.c"

EXPECTED = {
    "uint32_t__batLostCount": "FAULT_CHARGER_BAT_LOST",
    "uint32_t__techFaultCount": "FAULT_CHARGER_TECHNICAL",
    "uint32_t__ovCount": "FAULT_ADC",
}


def load_fault_bits(text):
    bits = {}
    for name, bit in re.findall(r"#define\s+(FAULT_\w+)\s+\(1u\s*<<\s*(\d+)\)", text):
        bits[name] = int(bit)
    return bits


def extract_function(text, signature):
    start = text.find(signature)
    if start < 0:
        return None
    brace = text.find("{", start)
    depth = 0
    for pos in range(brace, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[brace:pos + 1]
    return None


def main():
    problems = []
    header_text = HEADER.read_text(encoding="utf-8")
    source_text = SOURCE.read_text(encoding="utf-8")
    fault_bits = load_fault_bits(header_text)

    print("FAULT bit table from app_types.h:")
    for name in sorted(fault_bits, key=fault_bits.get):
        print("  bit {:>2}  {}".format(fault_bits[name], name))

    for const in EXPECTED.values():
        if const not in fault_bits:
            problems.append(const + " missing from app_types.h")
    used_bits = [fault_bits[c] for c in EXPECTED.values() if c in fault_bits]
    if len(set(used_bits)) != len(used_bits):
        problems.append("expected FAULT_* constants share a bit")

    body = extract_function(source_text, "void func__Diag_RecordFault")
    if body is None:
        problems.append("func__Diag_RecordFault not found in diag.c")
        body = ""

    for lineno, line in enumerate(body.splitlines(), start=1):
        if re.search(r"fault_mask_t__faults\s*&\s*\(?\s*1u", line):
            problems.append(
                "raw bit literal decodes the fault mask (RecordFault line "
                + str(lineno) + "): " + line.strip())

    for counter, const in EXPECTED.items():
        stmt = None
        for line in body.splitlines():
            if counter in line and "++" in line:
                stmt = line
                break
        if stmt is None:
            problems.append("counter " + counter + " is never incremented")
            continue
        found = re.findall(r"FAULT_\w+", stmt)
        if const not in found:
            problems.append(
                counter + " must test " + const + " but tests "
                + (", ".join(found) if found else "a raw literal"))

    if problems:
        print("\nDIAG FAULT BIT CHECK: FAIL")
        for problem in problems:
            print("  - " + problem)
        return 1
    print("\nDIAG FAULT BIT CHECK: PASS "
          "(counters decode named FAULT_* constants, mapping intact)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
