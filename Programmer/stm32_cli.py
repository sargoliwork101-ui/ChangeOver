#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
[EN] Shared STM32CubeProgrammer CLI plumbing for the two ChangeOver scripts in
     this folder (first_flash_changeover.py and flash_changeover.py). One place
     for: finding the CLI, running one step and failing loudly, and the two
     constants that describe the board's data area. No board logic lives here.

[FA] لایهٔ مشترک خط فرمان STM32CubeProgrammer برای دو اسکریپت این پوشه
     (first_flash_changeover.py و flash_changeover.py). تنها یک جا برای: پیدا
     کردن ابزار، اجرای یک گام با خطای صریح، و دو ثابتی که ناحیهٔ دادهٔ برد را
     توصیف می‌کنند. هیچ منطق برد اینجا نیست.

[EN] Why one file: both scripts ask the same questions of the same tool, and a
     second copy of "how do I find STM32_Programmer_CLI" is exactly the kind of
     duplicate that drifts apart and then lies to the operator.
[FA] چرا یک فایل: هر دو اسکریپت یک سؤال را از یک ابزار می‌پرسند، و کپی دوم
     «ابزار را چطور پیدا کنم» همان تکراری است که از هم جدا می‌افتد و بعد به
     کاربر دروغ می‌گوید.
"""

import os
import pathlib
import shutil
import subprocess

# [EN] Data area above the application image: parameter bank (4 KiB) +
#      calibration table (2 KiB) + spare (2 KiB). Same window the firmware
#      driver is allowed to erase (BSP_FLASH_STORAGE_BASE_ADDR..END_ADDR).
# [FA] ناحیهٔ دادهٔ بالای تصویر برنامه: بانک پارامترها (۴KB) + جدول
#      کالیبراسیون (۲KB) + یدک (۲KB). همان پنجره‌ای که درایور فرم‌ور اجازهٔ
#      پاک‌کردنش را دارد.
DATA_BASE_ADDRESS = 0x0800E000
DATA_SIZE_BYTES = 0x2000

# [EN] RDP byte value for level 1 on STM32F1 (OB_RDP_LEVEL_1 in the vendor HAL).
# [FA] مقدار بایت RDP برای سطح ۱ روی STM32F1 (همان OB_RDP_LEVEL_1 در HAL).
RDP_LEVEL1_VALUE = "0xA5"

# [EN] The CLI is not on PATH on most Windows installs; also accept an override.
# [FA] ابزار در بیشتر نصب‌های ویندوز روی PATH نیست؛ اجازهٔ تعیین دستی هم داده می‌شود.
CLI_ENV_VARIABLE = "STM32_PROGRAMMER_CLI"

# [EN] SWD is the only port wired on this board (CON4: SWDIO/SWCLK/NRST/3V3/GND).
# [FA] تنها پورت سیم‌کشی‌شده روی این برد SWD است (CON4).
CONNECT = ["-c", "port=SWD", "mode=UR"]


class FlashError(RuntimeError):
    """[EN] A step that must stop the whole run. / [FA] گامی که باید کل اجرا را متوقف کند."""


def find_cli(explicit_path=None):
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
        "  or set STM32_PROGRAMMER_CLI to the full path of the executable,\n"
        "  or pass --cli <path>.\n"
        "  ابزار پیدا نشد: پوشهٔ bin را روی PATH بگذارید، یا متغیر\n"
        "  STM32_PROGRAMMER_CLI را ست کنید، یا --cli بدهید."
    )


def run(cli, arguments, dry_run=False):
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


def read_device_info(cli, dry_run=False):
    """[EN] Connect and return the tool's device banner (empty in dry-run).
       [FA] اتصال و برگرداندن بنر دستگاه از ابزار (در حالت آزمایشی خالی)."""
    return run(cli, CONNECT, dry_run)


def is_blank_data_area(path):
    """[EN] True when a data-area file is entirely erased (0xFF).
       [FA] true وقتی فایل ناحیهٔ داده کامل پاک‌شده (0xFF) باشد."""
    content = pathlib.Path(path).read_bytes()
    return content.count(0xFF) == len(content)


def show_protection_state(cli, dry_run=False):
    """[EN] Print the option-byte state, so the result is seen and not assumed.
       [FA] چاپ وضعیت option byte تا نتیجه دیده شود، نه فرض."""
    print("  --- option bytes / وضعیت قفل ---")
    output = run(cli, CONNECT + ["-ob", "displ"], dry_run)
    if output:
        print(output.rstrip())


def flash_image(cli, image_path, dry_run=False):
    """[EN] Full chip erase + program the application image + verify + reset.
       [FA] پاک‌کردن کامل تراشه + نوشتن تصویر برنامه + بررسی + ریست."""
    image_path = pathlib.Path(image_path)
    if not image_path.exists() and not dry_run:
        raise FlashError("image not found: %s" % image_path)

    suffix = image_path.suffix.lower()
    run(cli, CONNECT + ["-e", "all"], dry_run)

    if suffix == ".hex":
        run(cli, CONNECT + ["-w", str(image_path), "-v", "-rst"], dry_run)
        return

    if suffix == ".bin":
        run(cli, CONNECT + ["-w", str(image_path), "0x08000000", "-v", "-rst"], dry_run)
        return

    raise FlashError("unsupported image type %s (use .hex or .bin)" % image_path.suffix)


def read_data_area(cli, backup_path, dry_run=False):
    """[EN] Read the 8 KiB data area into a file.
       [FA] خواندن ۸ کیلوبایت ناحیهٔ داده در یک فایل."""
    backup_path = pathlib.Path(backup_path)
    backup_path.parent.mkdir(parents=True, exist_ok=True)
    run(
        cli,
        CONNECT + [
            "-r8", "0x%08X" % DATA_BASE_ADDRESS, "0x%X" % DATA_SIZE_BYTES,
            str(backup_path),
        ],
        dry_run,
    )


def check_backup_file(backup_path, accept_blank=False):
    """[EN] Validate an existing data-area file before it is trusted.
       [FA] بررسی فایل ناحیهٔ داده پیش از اعتماد به آن."""
    backup_path = pathlib.Path(backup_path)
    if not backup_path.exists():
        raise FlashError("backup file not found: %s" % backup_path)

    size = backup_path.stat().st_size
    if size != DATA_SIZE_BYTES:
        raise FlashError(
            "backup %s has %d bytes, expected %d"
            % (backup_path, size, DATA_SIZE_BYTES)
        )

    content = backup_path.read_bytes()
    if not accept_blank and content.count(0xFF) == len(content):
        raise FlashError(
            "backup is entirely 0xFF - the board looks blank/unreadable; "
            "nothing was erased. Check that the correct board is connected."
        )


def restore_data_area(cli, backup_path, dry_run=False):
    """[EN] Write the data area back and verify it against flash.
       [FA] برگرداندن ناحیهٔ داده و بررسی آن با محتوای فلش."""
    run(
        cli,
        CONNECT + ["-w", str(backup_path), "0x%08X" % DATA_BASE_ADDRESS, "-v"],
        dry_run,
    )


def enable_read_protection(cli, dry_run=False):
    """[EN] Set RDP level 1 (last step only).
       [FA] روشن‌کردن RDP سطح ۱ (فقط آخرین گام)."""
    run(cli, CONNECT + ["-ob", "RDP=%s" % RDP_LEVEL1_VALUE], dry_run)


def reset_board(cli, dry_run=False):
    """[EN] Reset through the debugger. / [FA] ریست از راه دیباگر."""
    run(cli, CONNECT + ["-rst"], dry_run)
