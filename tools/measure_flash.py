#!/usr/bin/env python3
"""
[EN] Measure the STM32 image size on the host, so "does it still fit in 62K"
     stops being a question only the Windows IDE can answer.

     There is no arm-none-eabi-gcc in this sandbox, so the sources are built
     for Thumb/Cortex-M3 with the LLVM toolchain inside the ziglang wheel.
     The absolute total will NOT equal what CubeIDE reports - different
     compiler, different libc, no startup or vector table here. What this
     tool is for is the DELTA: compile the tree twice, once before a change
     and once after, and the difference in Thumb code size is trustworthy.
     That is exactly what a "FLASH overflowed by 780 bytes" decision needs,
     and guessing at it from source is how people remove the wrong thing.

[FA] اندازه‌گیری حجم ایمیج STM32 روی هاست، تا «هنوز در ۶۲ کیلوبایت جا می‌شود
     یا نه» سؤالی نباشد که فقط IDE ویندوز جوابش را بداند.

     در این سندباکس arm-none-eabi-gcc نیست، پس سورس‌ها با توولچین LLVM داخل
     بستهٔ ziglang برای Thumb/Cortex-M3 ساخته می‌شوند. عدد مطلق با گزارش
     CubeIDE یکی نخواهد بود - کامپایلر فرق دارد، libc فرق دارد، و startup و
     جدول بردار اینجا نیست. کار این ابزار «اختلاف» است: درخت را دو بار بساز،
     یک‌بار پیش از تغییر و یک‌بار پس از آن؛ اختلاف حجم کد Thumb قابل‌اعتماد
     است. تصمیم «۷۸۰ بایت سرریز» دقیقاً به همین نیاز دارد، و حدس‌زدن از روی
     سورس همان کاری است که باعث می‌شود آدم چیز اشتباه را حذف کند.
"""
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TARGET = ["-target", "thumb-linux-musleabi", "-mcpu=cortex_m3"]

INCLUDES = [
    "CubeIDE/Core/Inc",
    "CubeIDE/Drivers/CMSIS/Include",
    "CubeIDE/Drivers/CMSIS/Device/ST/STM32F1xx/Include",
    "CubeIDE/Drivers/STM32F1xx_HAL_Driver/Inc",
    "CubeIDE/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy",
    "CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/include",
    "CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2",
    "CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3",
    "Firmware/App/Inc", "Firmware/Bsp/Inc", "Firmware/Config/Inc", "Firmware/Rtos/Inc",
    "Firmware/Modules/Ui", "Firmware/Modules/Measurement", "Firmware/Modules/Protection",
    "Firmware/Modules/Changeover", "Firmware/Modules/Charger", "Firmware/Modules/Jitter",
    "Firmware/Modules/Fault", "Firmware/Modules/EspLink", "Firmware/Modules/McuPowerPath",
    # [EN] v1.53: the Imbalance module was missing, so five translation units
    #      failed and the "total" silently excluded them.
    # [FA] ماژول Imbalance جا افتاده بود و پنج فایل بی‌صدا از مجموع می‌افتادند.
    "Firmware/Modules/Imbalance",
]


def sources():
    """[EN] The same set CubeIDE compiles. / [FA] همان مجموعه‌ای که CubeIDE می‌سازد."""
    out = []
    out += sorted((ROOT / "CubeIDE/Core/Src").glob("*.c"))
    out += sorted((ROOT / "CubeIDE/Drivers/STM32F1xx_HAL_Driver/Src").glob("*.c"))
    frt = ROOT / "CubeIDE/Middlewares/Third_Party/FreeRTOS/Source"
    out += sorted(frt.glob("*.c"))
    out += sorted((frt / "CMSIS_RTOS_V2").glob("*.c"))
    out += [frt / "portable/GCC/ARM_CM3/port.c"]  # static allocation only: no heap_4
    out += sorted(f for f in (ROOT / "Firmware").rglob("*.c")
                  if "Tester" not in f.parts)   # [EN] host tests are not flashed
                                                # [FA] تست‌های هاست روی برد نمی‌روند
    return [p for p in out if p.is_file()]


def section_sizes(obj):
    """[EN] Flash-resident sections only. / [FA] فقط بخش‌هایی که روی فلش می‌نشینند."""
    from elftools.elf.elffile import ELFFile
    text = rodata = data = 0
    with open(obj, "rb") as fh:
        for sec in ELFFile(fh).iter_sections():
            name, size = sec.name, sec["sh_size"]
            if not (sec["sh_flags"] & 0x2):          # SHF_ALLOC
                continue
            if name.startswith(".text"):
                text += size
            elif name.startswith(".rodata") or name.startswith(".srodata"):
                rodata += size
            elif name.startswith(".data") and ".rel" not in name:
                data += size
    return text, rodata, data


def build(outdir):
    outdir = Path(outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    rows, failures = [], []
    for src in sources():
        rel = src.relative_to(ROOT)
        obj = outdir / (str(rel).replace("/", "_") + ".o")
        cmd = (["python3", "-m", "ziglang", "cc"] + TARGET +
               ["-c", "-Os", "-ffunction-sections", "-fdata-sections", "-fno-common",
                "-w", "-DSTM32F103xB", "-DUSE_HAL_DRIVER"] +
               [f"-I{ROOT / i}" for i in INCLUDES] +
               [str(src), "-o", str(obj)])
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0:
            failures.append((str(rel), res.stderr.strip().splitlines()[:1]))
            continue
        t, r, d = section_sizes(obj)
        rows.append({"file": str(rel), "text": t, "rodata": r, "data": d, "total": t + r + d})
    return rows, failures



def lld_linker_script(outdir):
    """[EN] The project's own script, with two things lld will not take: a
       MEMORY block referenced before it is defined, and the GCC-11 READONLY
       attribute. Hoisted and stripped here, never in the project file.
       [FA] اسکریپت خود پروژه، با دو چیزی که lld نمی‌پذیرد: بلوک MEMORY که پیش
       از تعریف ارجاع داده شده و صفت READONLY مخصوص GCC 11. اینجا جابه‌جا و
       حذف می‌شوند، نه در فایل پروژه."""
    import re as _re
    src = (ROOT / "CubeIDE/STM32CubeIDE/STM32F103C8TX_FLASH.ld").read_text()
    mem = _re.search(r"MEMORY\s*\{.*?\n\}", src, _re.S).group(0)
    out = src.replace(mem, "/* hoisted for lld */")
    at = out.rfind("\n", 0, out.index("_estack")) + 1
    out = out[:at] + mem + "\n\n" + out[at:]
    out = out.replace(" (READONLY) :", " :")
    path = Path(outdir) / "measure.ld"
    path.write_text(out)
    return path


def link(outdir):
    """[EN] Link the objects the way the board links them, with --gc-sections,
       and report the flash-resident total. Newlib is absent here, so
       tools/measure_support/libc_stubs.c stands in - identical in every run,
       so it cancels out of any comparison.
       [FA] لینک آبجکت‌ها همان‌طور که روی برد لینک می‌شوند، با gc-sections، و
       گزارش مجموع بخش‌های ساکن فلش. newlib اینجا نیست و استاب جایش می‌نشیند -
       در هر اجرا یکسان است، پس در مقایسه حذف می‌شود."""
    outdir = Path(outdir)
    startup = ROOT / "CubeIDE/STM32CubeIDE/Application/User/Startup/startup_stm32f103c8tx.s"
    subprocess.run(["python3", "-m", "ziglang", "cc"] + TARGET +
                   ["-c", "-x", "assembler-with-cpp", str(startup),
                    "-o", str(outdir / "zz_startup.o")], capture_output=True)
    subprocess.run(["python3", "-m", "ziglang", "cc"] + TARGET +
                   ["-c", "-Os", "-ffunction-sections", "-fdata-sections", "-w",
                    str(ROOT / "tools/measure_support/libc_stubs.c"),
                    "-o", str(outdir / "zz_libc.o")], capture_output=True)
    script = lld_linker_script(outdir)
    elf = outdir / "image.elf"
    res = subprocess.run(["python3", "-m", "ziglang", "ld.lld", "-T", str(script),
                          "--gc-sections", f"-Map={outdir / 'image.map'}",
                          "-o", str(elf)] + [str(o) for o in sorted(outdir.glob("*.o"))],
                         capture_output=True, text=True)
    if res.returncode != 0:
        return None, res.stderr
    from elftools.elf.elffile import ELFFile
    total = 0
    with open(elf, "rb") as fh:
        for sec in ELFFile(fh).iter_sections():
            if (sec["sh_flags"] & 0x2) and sec["sh_type"] != "SHT_NOBITS":
                total += sec["sh_size"]
    return total, ""


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "/tmp/flashmeasure"
    save = sys.argv[2] if len(sys.argv) > 2 else None
    rows, failures = build(outdir)

    text = sum(r["text"] for r in rows)
    rodata = sum(r["rodata"] for r in rows)
    data = sum(r["data"] for r in rows)
    total = text + rodata + data

    print(f"objects built    : {len(rows)}")
    if failures:
        print(f"COMPILE FAILURES : {len(failures)}")
        for f, why in failures[:8]:
            print(f"   {f}: {why[0] if why else ''}")
    print(f".text            : {text:>7}")
    print(f".rodata          : {rodata:>7}")
    print(f".data            : {data:>7}")
    print(f"FLASH TOTAL      : {total:>7} bytes (Thumb, zig/LLVM - compare deltas, not absolutes)")

    print("\nlargest contributors:")
    for r in sorted(rows, key=lambda x: -x["total"])[:12]:
        print(f"  {r['total']:>6}  {r['file']}")

    total_linked, err = link(outdir)
    if total_linked is None:
        print("\nLINK FAILED (this is a real finding, not a tooling glitch):")
        print("   " + "\n   ".join(err.strip().splitlines()[:6]))
        return 2
    print(f"\nLINKED IMAGE     : {total_linked} bytes of flash-resident sections")
    print("   (zig/LLVM + stub libc: compare this number against another run,")
    print("    not against what CubeIDE prints)")

    if save:
        Path(save).write_text(json.dumps(
            {"rows": rows, "linked_total": total_linked}, indent=1), encoding="utf-8")
        print(f"saved: {save}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
