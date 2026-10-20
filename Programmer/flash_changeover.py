#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
[EN] Flash ChangeOver STM32F103 with STM32CubeProgrammer CLI, without losing the
     parameter bank or the calibration table. This is the script for EVERY
     flash after the board has been provisioned once; the very first flash of a
     bare board uses first_flash_changeover.py instead.

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
     بدون از دست دادن بانک پارامترها و جدول کالیبراسیون. این اسکریپت برای
     «همهٔ فلش‌های بعد از اولین بار» است؛ اولین فلش برد نو با
     first_flash_changeover.py انجام می‌شود.

     چرا لازم است: وقتی قطعه قفل خواندن (RDP سطح ۱) دارد، تنها راه برنامه‌ریزی
     برداشتن قفل است و در STM32F1 برداشتن قفل یعنی پاک‌کردن کامل تراشه. آن
     پاک‌کردن فقط فرم‌ور را نمی‌برد؛ دو صفحهٔ دادهٔ بالای تصویر را هم می‌برد.
     پس ترتیب کار: خواندن آن ۸ کیلوبایت، پاک‌کردن، نوشتن برنامه، برگرداندن آن
     ۸ کیلوبایت، و بررسی.

     قواعد ایمنی: بدون پشتیبان سالم هیچ چیز پاک نمی‌شود، پشتیبان بعد از برگشت
     دوباره بررسی می‌شود، پشتیبان کهنه رد می‌شود، و --dry-run فقط دستورها را
     چاپ می‌کند.

Usage / نحوهٔ استفاده:
    python3 Programmer/flash_changeover.py --image build/ChangeOver.hex \
        --backup Programmer/backups/params_v1.90_board7.bin

    python3 Programmer/flash_changeover.py --image ... --dry-run
    python3 Programmer/flash_changeover.py --restore-only --backup ...
    python3 Programmer/flash_changeover.py --image ... --lock

See Programmer/SCRIPTS.md for the step-by-step explanation / توضیح گام‌به‌گام در
همان فایل کنار اسکریپت‌ها است.
"""

import argparse
import hashlib
import pathlib
import sys

import stm32_cli


def describe_backup(backup_path):
    """[EN] Print a short fingerprint so the operator can see WHICH backup.
       [FA] چاپ یک اثر انگشت کوتاه تا کاربر ببیند کدام پشتیبان است."""
    digest = hashlib.sha256(backup_path.read_bytes()).hexdigest()[:16]
    print("  backup: %s (%d bytes, sha256:%s)"
          % (backup_path, stm32_cli.DATA_SIZE_BYTES, digest))


def step_backup(cli, backup_path, dry_run):
    """[EN] Step 1: the data area must be on disk before anything is erased.
       [FA] گام ۱: ناحیهٔ داده باید قبل از هر پاک‌کردنی روی دیسک باشد."""
    print("[1/5] backup of the parameter + calibration area / پشتیبان ناحیهٔ داده")
    stm32_cli.read_data_area(cli, backup_path, dry_run)
    if dry_run:
        return
    stm32_cli.check_backup_file(backup_path)
    describe_backup(backup_path)


def step_flash(cli, image_path, dry_run):
    """[EN] Step 2: erase, program, verify. / [FA] گام ۲: پاک‌کردن، نوشتن، بررسی."""
    print("[2/5] full chip erase + program + verify / پاک‌کردن و نوشتن برنامه")
    if not dry_run and not pathlib.Path(image_path).exists():
        raise stm32_cli.FlashError("image not found: %s" % image_path)
    stm32_cli.flash_image(cli, image_path, dry_run)


def step_restore(cli, backup_path, dry_run):
    """[EN] Step 3: put the data area back and verify it.
       [FA] گام ۳: برگرداندن ناحیهٔ داده و بررسی آن."""
    print("[3/5] restore the data area + verify / برگرداندن ناحیهٔ داده")
    stm32_cli.restore_data_area(cli, backup_path, dry_run)


def step_report(cli, dry_run):
    """[EN] Step 4: reset and show the protection state.
       [FA] گام ۴: ریست و نمایش وضعیت قفل."""
    print("[4/5] reset and read back the protection state / ریست و خواندن وضعیت قفل")
    stm32_cli.reset_board(cli, dry_run)
    stm32_cli.show_protection_state(cli, dry_run)


def step_lock(cli, lock_after, dry_run):
    """[EN] Step 5: optional RDP level 1. / [FA] گام ۵: قفل خواندن اختیاری."""
    print("[5/5] read protection / قفل خواندن")
    if not lock_after:
        print("  skipped (pass --lock to enable RDP level 1) / رد شد (برای قفل --lock بدهید)")
        return
    stm32_cli.enable_read_protection(cli, dry_run)


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
        cli = stm32_cli.find_cli(arguments.cli)

        if arguments.restore_only:
            print("[restore] writing the backup back / برگرداندن پشتیبان")
            if not arguments.dry_run:
                stm32_cli.check_backup_file(arguments.backup)
            stm32_cli.restore_data_area(cli, arguments.backup, arguments.dry_run)
            stm32_cli.reset_board(cli, arguments.dry_run)
            return 0

        if not arguments.image:
            raise stm32_cli.FlashError("--image is required unless --restore-only is used")

        step_backup(cli, arguments.backup, arguments.dry_run)
        step_flash(cli, arguments.image, arguments.dry_run)
        step_restore(cli, arguments.backup, arguments.dry_run)
        step_report(cli, arguments.dry_run)
        step_lock(cli, arguments.lock, arguments.dry_run)
        return 0
    except stm32_cli.FlashError as error:
        print("\nSTOPPED / متوقف شد:")
        print("  " + str(error).replace("\n", "\n  "))
        return 2


if __name__ == "__main__":
    sys.exit(main())
