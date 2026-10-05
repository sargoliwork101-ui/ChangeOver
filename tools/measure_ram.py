#!/usr/bin/env python3
"""
[EN] Measure the STM32 RAM budget AND the worst-case stack depth of every RTOS
     task and interrupt handler, from the linked image itself.

     Why this exists: "it fits" was a hand-wavy claim in this repository for a
     long time. The failure mode nobody notices on a bench is a task stack that
     is deep enough 99 % of the time: a 520-byte frame three calls below a
     1 KiB stack does not announce itself, it corrupts the neighbouring task
     control block and the board reboots "randomly" days later. measure_flash.py
     answers "does the code fit the flash"; this answers "does the data fit the
     RAM, and can any call chain run off the end of a stack".

     Method: link the same objects measure_flash.py links, then disassemble
     .text (Thumb) and, per function, read the prologue the compiler actually
     emitted - push/vpush register lists and `sub sp, #imm` - for the frame
     size, and every `bl`/`b` that leaves the function for the call graph. The
     deepest chain from each root is then reported. Honest limits, stated
     rather than hidden:
       - indirect calls (`blx <reg>`) cannot be followed; the functions that
         make them are listed so the reader can judge them by hand;
       - recursion is reported, never silently cut;
       - clang/-Os frames track but do not equal arm-none-eabi-gcc frames,
         so compare deltas between runs, exactly like measure_flash.py.

[FA] سنجش بودجهٔ RAM و بدترین عمق پشتهٔ هر تسک و هر وقفه، از روی خودِ ایمیج
     لینک‌شده. چرا: «جا می‌شود» مدت‌ها ادعای دستی بود، و خرابی‌ای که روی میز
     دیده نمی‌شود پشته‌ای است که ۹۹٪ مواقع کافی است. روش: همان آبجکت‌هایی که
     measure_flash لینک می‌کند دیس‌اسمبل می‌شوند و از پرولوگ واقعی هر تابع
     (push و sub sp) اندازهٔ قاب و از bl/b گراف فراخوانی ساخته می‌شود.
     محدودیت‌ها صریح‌اند: فراخوانی غیرمستقیم دنبال نمی‌شود (فهرستشان چاپ
     می‌شود)، بازگشتی گزارش می‌شود و اعداد clang هستند نه gcc بازوی واقعی.
"""

import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import measure_flash as MF  # noqa: E402  (same sources, same includes, one source of truth)

LD = (ROOT / "CubeIDE/STM32CubeIDE/STM32F103C8TX_FLASH.ld").read_text()
RTOS_APP_C = (ROOT / "Firmware/Rtos/Src/rtos_app.c").read_text()
RTOS_CONFIG_H = (ROOT / "Firmware/Config/Inc/rtos_config.h").read_text()

# [EN] A context switch pushes the callee-saved registers the hardware did not:
#      8 hardware-stacked words + 8 software-stacked words = 64 B on top of the
#      task's own deepest frame. [FA] تعویض کانتکست ۶۴ بایت روی پشتهٔ تسک.
CONTEXT_BYTES = 64


def ram_length_bytes():
    m = re.search(r"RAM\s+\(xrw\)\s*: ORIGIN = 0x[0-9A-Fa-f]+,\s*LENGTH = (\d+)K", LD)
    return int(m.group(1)) * 1024


def linker_reserves():
    heap = int(re.search(r"_Min_Heap_Size\s*=\s*(0x[0-9A-Fa-f]+)", LD).group(1), 16)
    stack = int(re.search(r"_Min_Stack_Size\s*=\s*(0x[0-9A-Fa-f]+)", LD).group(1), 16)
    return heap, stack


def task_roots():
    """[EN] entry function -> (task name, stack bytes). / [FA] ریشهٔ هر تسک."""
    sizes = {m.group(1): int(m.group(2))
             for m in re.finditer(r"#define\s+(TASK_STACK_\w+)\s+(\d+)u", RTOS_CONFIG_H)}
    stack_of = {m.group(1): sizes[m.group(2)] * 4
                for m in re.finditer(r"rtos_stack_word_t\s+(\w+)\[(TASK_STACK_\w+)\]", RTOS_APP_C)}
    attr = {}
    for m in re.finditer(r"OS_THREAD_ATTR_T__G__(\w+)\s*=\s*\{(.*?)\n\};", RTOS_APP_C, re.S):
        body = m.group(2)
        attr[m.group(1)] = (re.search(r'\.name\s*=\s*"([^"]+)"', body).group(1),
                            stack_of[re.search(r"\.stack_mem\s*=\s*(\w+)", body).group(1)])
    return {m.group(1): attr[m.group(2)]
            for m in re.finditer(r"osThreadNew\((\w+),\s*NULL,\s*&OS_THREAD_ATTR_T__G__(\w+)\)",
                                 RTOS_APP_C)}


def isr_roots():
    it_c = (ROOT / "CubeIDE/Core/Src/stm32f1xx_it.c").read_text()
    return sorted(set(re.findall(r"^void\s+(\w+_(?:IRQ)?Handler)\(void\)", it_c, re.M)))


def analyse_image(elf_path):
    """[EN] frames{}, calls{}, indirect[] from the linked Thumb image.
       [FA] قاب‌ها، گراف فراخوانی و فراخوانی‌های غیرمستقیم از ایمیج."""
    from elftools.elf.elffile import ELFFile
    import capstone

    with open(elf_path, "rb") as fh:
        elf = ELFFile(fh)
        funcs = []           # (addr, size, name)
        for sec in elf.iter_sections():
            if sec.header["sh_type"] != "SHT_SYMTAB":
                continue
            for sym in sec.iter_symbols():
                if sym["st_info"]["type"] != "STT_FUNC" or sym["st_size"] == 0:
                    continue
                funcs.append((sym["st_value"] & ~1, sym["st_size"], sym.name))
        blobs = []
        for sec in elf.iter_sections():
            if sec.name.startswith(".text") and sec["sh_type"] == "SHT_PROGBITS":
                blobs.append((sec["sh_addr"], sec.data()))

    funcs.sort()
    by_addr = {a: n for a, _, n in funcs}
    md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
    md.detail = False

    frames, calls, indirect = {}, {}, set()
    for addr, size, name in funcs:
        code = None
        for base, data in blobs:
            if base <= addr < base + len(data):
                off = addr - base
                code = data[off:off + size]
                break
        if code is None:
            continue
        frame = 0
        callees = set()
        for ins in md.disasm(code, addr):
            mnem, ops = ins.mnemonic, ins.op_str
            if mnem.startswith("push"):
                frame += 4 * (ops.count(",") + 1 if ops.strip() else 0)
            elif mnem.startswith("vpush"):
                frame += 8 * (ops.count(",") + 1)
            elif mnem.startswith("sub") and ops.replace(" ", "").startswith("sp,"):
                # [EN] capstone prints immediates in hex OR decimal ("#0x208"
                #      and "#24" both occur). A naive r"#(\d+)" matched the
                #      leading 0 of "#0x208" and reported a 520-byte frame as
                #      0 - the exact kind of quiet undercount this tool exists
                #      to prevent, found while writing it.
                # [FA] ایمدیِیت گاهی هگز و گاهی دهدهی چاپ می‌شود؛ الگوی ساده،
                #      قاب ۵۲۰ بایتی را صفر می‌خواند - همان کم‌شماری خاموشی که
                #      این ابزار برای جلوگیری از آن نوشته شده.
                m = re.search(r"#(0x[0-9a-fA-F]+|\d+)", ops)
                if m:
                    frame += int(m.group(1), 0)
            elif mnem in ("bl", "blx"):
                m = re.match(r"#0x([0-9a-f]+)", ops)
                if m:
                    callees.add(by_addr.get(int(m.group(1), 16) & ~1, "?"))
                else:
                    indirect.add(name)
            elif mnem in ("b", "b.w", "bx") and ops.startswith("#0x"):
                target = int(ops[1:], 16) & ~1
                if not (addr <= target < addr + size):       # tail call
                    callees.add(by_addr.get(target, "?"))
            elif mnem == "bx":
                pass
        frames[name] = max(frames.get(name, 0), frame)
        calls[name] = callees - {"?", None}
    return frames, calls, sorted(indirect)


def deepest(root, frames, calls, seen=()):
    if root in seen:
        return 0, [root + " <recursion>"], True
    frame = frames.get(root, 0)
    best, best_chain, rec = 0, [], False
    for callee in sorted(calls.get(root, ())):
        size, chain, r = deepest(callee, frames, calls, seen + (root,))
        rec = rec or r
        if size > best:
            best, best_chain = size, chain
    return frame + best, [f"{root}({frame})"] + best_chain, rec


def main():
    try:
        import capstone  # noqa: F401
    except ImportError:
        print("SKIP: python3 -m pip install capstone - the stack walk needs a "
              "Thumb disassembler (RAM totals still require the link below)")
        return 0

    ram_total = ram_length_bytes()
    heap, main_stack = linker_reserves()
    with tempfile.TemporaryDirectory(prefix="ram_measure_") as tmp:
        objdir = Path(tmp) / "obj"
        MF.build(objdir)
        MF.link(objdir)
        elf = objdir / "image.elf"
        if not elf.exists():
            print("LINK FAILED - cannot measure RAM")
            return 1

        from elftools.elf.elffile import ELFFile
        bss = data = 0
        with open(elf, "rb") as fh:
            for sec in ELFFile(fh).iter_sections():
                if not (sec["sh_flags"] & 0x2):
                    continue
                if sec.name.startswith(".bss"):
                    bss += sec["sh_size"]
                elif sec.name.startswith(".data") and ".rel" not in sec.name:
                    data += sec["sh_size"]

        frames, calls, indirect = analyse_image(elf)

        print("RAM budget / بودجهٔ RAM")
        print("=" * 74)
        print(f"device RAM                                   : {ram_total:7d} B")
        print(f".data (initialised)                          : {data:7d} B")
        print(f".bss  (includes every task stack - static)   : {bss:7d} B")
        print(f"linker heap reserve                          : {heap:7d} B")
        print(f"linker main/ISR stack reserve                : {main_stack:7d} B")
        free = ram_total - data - bss - heap - main_stack
        print(f"FREE                                         : {free:7d} B")
        print()

        print("worst-case stack depth per task / بدترین عمق پشتهٔ هر تسک")
        print("=" * 74)
        tight = []
        for entry, (name, size) in sorted(task_roots().items(), key=lambda kv: kv[1][0]):
            depth, chain, rec = deepest(entry, frames, calls)
            depth += CONTEXT_BYTES
            use = 100.0 * depth / size
            flag = "OK   " if use < 70.0 else ("TIGHT" if use < 90.0 else "OVER ")
            if use >= 70.0:
                tight.append((name, depth, size))
            print(f"{flag} {name:5s} {depth:5d} / {size:5d} B ({use:5.1f}%)"
                  + ("  [recursion]" if rec else ""))
            print("        " + " -> ".join(chain[:9]))
        print()

        print("worst-case interrupt depth / بدترین عمق وقفه")
        print("=" * 74)
        isr_worst, isr_chain, isr_name = 0, [], "-"
        for handler in isr_roots():
            depth, chain, _ = deepest(handler, frames, calls)
            if depth > isr_worst:
                isr_worst, isr_chain, isr_name = depth, chain, handler
        print(f"deepest handler: {isr_name} = {isr_worst} B (runs on the stack of "
              f"whatever it preempts - add it to every row above)")
        print("        " + " -> ".join(isr_chain[:9]))
        print()

        if indirect:
            print("functions making INDIRECT calls (not followed by this tool, "
                  "judge by hand):")
            print("  " + ", ".join(indirect[:20]) + (" ..." if len(indirect) > 20 else ""))
            print()

        if free < 0:
            print("RAM OVERFLOW")
            return 1
        if tight:
            print("STACKS AT OR ABOVE 70 % (raise them in rtos_config.h):")
            for name, depth, size in tight:
                print(f"  {name}: {depth} B of {size} B")
            return 1
        print("RAM AND STACK BUDGET OK / بودجهٔ RAM و پشته سالم است")
        return 0


if __name__ == "__main__":
    sys.exit(main())
