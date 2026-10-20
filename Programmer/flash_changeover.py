#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
[EN] Flash ChangeOver STM32F103 with STM32CubeProgrammer CLI, without losing the
     parameter bank or the calibration table.

     Why this exists: when the part is read-protected (RDP level 1) the ONLY way
     to program it is to remove the protection first, and on STM32F1 removing it
     is a full chip erase. That erase does not just remove the firmware - it also
     wipes the two data pages that sit above the image:

         0x0800E000 .. 0x0800EFFF   parameter bank (NVM, 4 KiB)
         0x0800F000 .. 0x0800F7FF   calibration table (LUT, 2 KiB)
         0x0800F800 .. 0x0800FFFF   spare (2 KiB)

     So the order is: read those 8 KiB out, erase, flash the image, write the
     8 KiB back, verify. The backup is a file on the laptop, keyed by the build
     it was taken from.

     Safety rules baked in:
       * nothing is erased unless a readable backup already exists,
       * the backup is verified against flash after it is written back,
       * a stale backup (wrong size, all 0xFF) is refused,
       * --dry-run prints every command without running it.

[FA] فلش‌کردن STM32F103 برد ChangeOver با ابزار خط فرمان STM32CubeProgrammer،
     بدون از دست دادن بانک پارامترها و جدول کالیبراسیون.

     چرا لازم است: وقتی قطعه قفل خواندن (RDP سطح ۱) دارد، تنها راه برنامه‌ریزی
     برداشتن قفل است و در STM32F1 برداشتن قفل یعنی پاک‌کردن کامل تراشه. آن
     پاک‌کردن فقط فرم‌ور را نمی‌برد؛ دو صفحهٔ دادهٔ بالای تصویر را هم می‌برد.
     پس ترتیب کار: خواندن آن ۸ کیلوبایت، پاک‌کردن، نوشتن برنامه، برگرداندن آن
     ۸ کیلوبایت، و بررسی. پشتیبان یک فایل روی لپ‌تاپ است، نام‌گذاری‌شده با
     نسخهٔ بیلدی که از آن گرفته شده.

     قواعد ایمنی داخل خود اسکریپت: بدون پشتیبان سالم هیچ چیز پاک نمی‌شود،
     پشتیبان بعد از برگشت دوباره بررسی می‌شود، پشتیبان کهنه رد می‌شود، و
     --dry-run فقط دستورها را چاپ می‌کند.

Usage / نحوهٔ استفاده:
    python3 Programmer/flash_changeover.py --image build/ChangeOver.hex \
        --backup Programmer/backups/params_v1.90_board7.bin

    python3 Programmer/flash_changeover.py --image ... --dry-run
    python3 Programmer/flash_changeover.py --restore-only --backup ...
    python3 Programmer/flash_changeover.py --image ... --lock
"""

import argparse
import hashlib
import os
import pathlib
import shutil
import subprocess
import sys

# [EN] Data area above the application image; same window the firmware driver
#      refuses to leave (BSP_FLASH_STORAGE_BASE_ADDR / END_ADDR).
# [FA] ناحیهٔ دادهٔ بالای تصویر برنامه؛ همان پنجره‌ای که درایور فرم‌ور اجازهٔ
#      بیرون‌رفتن از آن را ندارد.
DATA_BASE_ADDRESS = 0x0800E000
DATA_SIZE_BYTES = 0x2000

# [EN] RDP byte value for level 1 on STM32F1 (OB_RDP_LEVEL_1 in the vendor HAL).
# [FA] مقدار بایت RDP برای سطح ۱ روی STM32F1 (همان OB_RDP_LEVEL_1 در HAL).
RDP_LEVEL1_VALUE = "0xA5"

# [EN] The CLI is not on PATH on most Windows installs; also accept an override.
# [FA] ابزار خط فرمان در بیشتر نصب‌های ویندوز روی PATH نیست؛ اجازهٔ تعیین دستی هم
#      داده می‌شود.
CLI_ENV_VARIABLE = "STM32_PROGRAMMER_CLI"


class FlashError(RuntimeError):
    """[EN] A step that must stop the whole run. / [FA] گامی که باید کل اجرا را متوقف کند."""


def find_cli(explicit_path):
    """[EN] Locate STM32_Programmer_CLI, or fail with instructions.
       [FA] پیدا کردن STM32_Programmer_CLI یا شکست با راهنما."""
    candidates = []
    if explicit_path:
        candidates.append(explicit_path)
    if os.environ.get(CLI_ENV_VARIABLE):
        candidates.append(os.environ[CLI_ENV_VARIABLE])
    for name in ("STM32_Programmer_CLI", "STM32_Programmer_CLI.exe"):
        found = shutil.which(name)
        if found:
            candidates.append(found)
    for path in candidates:
        if pathlib.Path(path).exists() or shutil.which(path):
            return path
    raise FlashError(
        "STM32_Programmer_CLI not found.\n"
        "  Install STM32CubeProgrammer, then either put its bin folder on PATH\n"
        "  or set STM32_PROGRAMMER_CLI to the full path of the executable.\n"
        "  ابزار پیدا نشد: مسیر پوشهٔ bin را روی PATH بگذارید یا متغیر\n"
        "  STM32_PROGRAMMER_CLI را به مسیر کامل فایل اجرایی بدهید."
    )


def run(cli, arguments, dry_run):
    """[EN] Run one CLI step (or print it in dry-run) and fail loudly.
       [FA] اجرای یک گام ابزار (یا چاپ آن در حالت آزمایشی) با خطای صریح."""
    command = [cli] + arguments
    printable = " ".join(command)
    print("  $ " + printable)
    if dry_run:
        return ""

    completed = subprocess.run(command, capture_output=True, text=True)
    output = completed.stdout + completed.stderr
    if completed.returncode != 0:
        raise FlashError(
            "command failed (rc=%d): %s\n%s" % (completed.returncode, printable, output)
        )
    return output


def expected_backup_size():
    """[EN] Exact byte count of the data area. / [FA] تعداد دقیق بایت ناحیهٔ داده."""
    return DATA_SIZE_BYTES


def read_data_area(cli, backup_path, dry_run):
    """[EN] Read the 8 KiB data area into the backup file.
       [FA] خواندن ۸ کیلوبایت ناحیهٔ داده در فایل پشتیبان."""
    backup_path.parent.mkdir(parents=True, exist_ok=True)
    run(
        cli,
        [
            "-c", "port=SWD", "mode=UR",
            "-r8", "0x%08X" % DATA_BASE_ADDRESS, "0x%X" % DATA_SIZE_BYTES,
            str(backup_path),
        ],
        dry_run,
    )
    if dry_run:
        return

    size = backup_path.stat().st_size
    if size != expected_backup_size():
        raise FlashError(
            "backup has %d bytes, expected %d - refusing to erase anything"
            % (size, expected_backup_size())
        )

    content = backup_path.read_bytes()
    if content.count(0xFF) == len(content):
        raise FlashError(
            "backup is entirely 0xFF - the board looks blank/unreadable; "
            "nothing was erased. Check that the correct board is connected."
        )


def check_backup_file(backup_path):
    """[EN] Validate an existing backup before it is trusted.
       [FA] بررسی پشتیبان موجود پیش از اعتماد به آن."""
    if not backup_path.exists():
        raise FlashError("backup file not found: %s" % backup_path)
    size = backup_path.stat().st_size
    if size != expected_backup_size():
        raise FlashError(
            "backup %s has %d bytes, expected %d"
            % (backup_path, size, expected_backup_size())
        )


def describe_backup(backup_path):
    """[EN] Print a short fingerprint so the operator can see WHICH backup.
       [FA] چاپ یک اثر انگشت کوتاه تا کاربر ببیند کدام پشتیبان است."""
    digest = hashlib.sha256(backup_path.read_bytes()).hexdigest()[:16]
    print("  backup: %s (%d bytes, sha256:%s)" % (backup_path, expected_backup_size(), digest))


def flash_image(cli, image_path, dry_run):
    """[EN] Full chip erase + program the application image with verify.
       [FA] پاک‌کردن کامل تراشه + نوشتن تصویر برنامه همراه با بررسی."""
    if not image_path.exists():
        raise FlashError("image not found: %s" % image_path)

    suffix = image_path.suffix.lower()
    run(cli, ["-c", "port=SWD", "mode=UR", "-e", "all"], dry_run)
    if suffix == ".hex":
        run(cli, ["-c", "port=SWD", "mode=UR", "-w", str(image_path), "-v", "-rst"], dry_run)
        return

    if suffix == ".bin":
        run(
            cli,
            [
                "-c", "port=SWD", "mode=UR",
                "-w", str(image_path), "0x08000000", "-v", "-rst",
            ],
            dry_run,
        )
        return

    raise FlashError("unsupported image type %s (use .hex or .bin)" % image_path.suffix)


def restore_data_area(cli, backup_path, dry_run):
    """[EN] Write the data area back and verify it against flash.
       [FA] برگرداندن ناحیهٔ داده و بررسی آن با محتوای فلش."""
    run(
        cli,
        [
            "-c", "port=SWD", "mode=UR",
            "-w", str(backup_path), "0x%08X" % DATA_BASE_ADDRESS, "-v",
        ],
        dry_run,
    )


def enable_read_protection(cli, dry_run):
    """[EN] Set RDP level 1 (last step only).
       [FA] روشن‌کردن RDP سطح ۱ (فقط آخرین گام)."""
    run(cli, ["-c", "port=SWD", "mode=UR", "-ob", "RDP=%s" % RDP_LEVEL1_VALUE], dry_run)


def show_protection_state(cli, dry_run):
    """[EN] Print the option-byte state so the result is visible, not assumed.
       [FA] چاپ وضعیت option byte تا نتیجه دیده شود، نه فرض."""
    print("  --- option bytes / وضعیت قفل ---")
    output = run(cli, ["-c", "port=SWD", "mode=UR", "-ob", "displ"], dry_run)
    if output:
        print(output.rstrip())


def full_run(cli, image_path, backup_path, dry_run, lock_after):
    """[EN] The one supported order: backup, erase+flash, restore, optional lock.
       [FA] تنها ترتیب پشتیبانی‌شده: پشتیبان، پاک و نوشتن، برگرداندن، قفل اختیاری."""
    print("[1/5] backup of the parameter + calibration area / پشتیبان ناحیهٔ داده")
    read_data_area(cli, backup_path, dry_run)
    if not dry_run:
        describe_backup(backup_path)

    print("[2/5] full chip erase + program + verify / پاک‌کردن و نوشتن برنامه")
    flash_image(cli, image_path, dry_run)

    print("[3/5] restore the data area + verify / برگرداندن ناحیهٔ داده")
    restore_data_area(cli, backup_path, dry_run)

    print("[4/5] reset and read back the protection state / ریست و خواندن وضعیت قفل")
    run(cli, ["-c", "port=SWD", "mode=UR", "-rst"], dry_run)
    show_protection_state(cli, dry_run)

    print("[5/5] read protection / قفل خواندن")
    if lock_after:
        enable_read_protection(cli, dry_run)
    else:
        print("  skipped (pass --lock to enable RDP level 1) / رد شد (برای قفل --lock بدهید)")


def parse_arguments():
    """[EN] Command line of the script. / [FA] خط فرمان اسکریپت."""
    parser = argparse.ArgumentParser(
        description="Flash ChangeOver (STM32F103) while keeping the data area."
    )
    parser.add_argument("--image", type=pathlib.Path, help="firmware .hex or .bin")
    parser.add_argument("--backup", type=pathlib.Path, required=True,
                        help="file that holds the 8 KiB data area")
    parser.add_argument("--restore-only", action="store_true",
                        help="write the backup back without flashing")
    parser.add_argument("--lock", action="store_true",
                        help="enable RDP level 1 as the last step (one-way on F1)")
    parser.add_argument("--dry-run", action="store_true",
                        help="print the commands, change nothing")
    parser.add_argument("--cli", help="full path to STM32_Programmer_CLI")
    return parser.parse_args()


def main():
    """[EN] Entry point: pick the requested flow and report clearly.
       [FA] نقطهٔ ورود: انتخاب مسیر درخواستی و گزارش روشن."""
    arguments = parse_arguments()
    try:
        cli = find_cli(arguments.cli)

        if arguments.restore_only:
            print("[restore] writing the backup back / برگرداندن پشتیبان")
            check_backup_file(arguments.backup)
            restore_data_area(cli, arguments.backup, arguments.dry_run)
            run(cli, ["-c", "port=SWD", "mode=UR", "-rst"], arguments.dry_run)
            return 0

        if not arguments.image:
            raise FlashError("--image is required unless --restore-only is used")
        full_run(cli, arguments.image, arguments.backup, arguments.dry_run, arguments.lock)
        return 0
    except FlashError as error:
        print("\nSTOPPED / متوقف شد:")
        print("  " + str(error).replace("\n", "\n  "))
        return 2


if __name__ == "__main__":
    sys.exit(main())
