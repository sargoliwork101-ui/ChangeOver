#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
[EN] FIRST flash of a bare ChangeOver board (STM32F103C8T6): prove the board is
     really new, erase it, program the firmware, and leave the data area in a
     deliberately blank state.

     Difference from flash_changeover.py, which is the script for every later
     flash:
       * there is nothing to preserve on a new board, so nothing is restored -
         after the chip erase the parameter bank and the calibration table are
         intentionally left erased (0xFF),
       * erased data area has a MEANING in this firmware: the parameter bank
         falls back to the compiled defaults and the calibration table is off,
         so current is reported on the gain/offset line. That is the honest
         "factory state" and it is verified by reading it back,
       * the script refuses to run on a board that already holds data, because
         "first flash" on a calibrated board silently destroys its calibration.

     What "blank" costs you afterwards: the board must be calibrated from the
     ESP panel, and only after that does a real backup exist to protect.

[FA] اولین فلش برد نو ChangeOver (STM32F103C8T6): بررسی اینکه برد واقعاً نو
     است، پاک‌کردن، نوشتن فرم‌ور، و گذاشتن ناحیهٔ داده در حالت عمداً پاک.

     تفاوت با flash_changeover.py که برای همهٔ فلش‌های بعدی است:
       * روی برد نو چیزی برای حفظ‌کردن نیست، پس هیچ چیز برگردانده نمی‌شود؛
         بانک پارامترها و جدول کالیبراسیون عمداً پاک (0xFF) می‌مانند،
       * ناحیهٔ دادهٔ پاک در این فرم‌ور معنا دارد: پارامترها به پیش‌فرض
         کامپایل برمی‌گردند و جدول کالیبراسیون خاموش است، پس جریان با خط
         gain/offset گزارش می‌شود. این همان «حالت کارخانه» صادقانه است و با
         خواندن دوباره بررسی می‌شود،
       * اسکریپت روی بردی که از قبل داده دارد اجرا نمی‌شود، چون «اولین فلش»
         روی برد کالیبره‌شده، کالیبراسیونش را بی‌صدا از بین می‌برد.

     هزینهٔ این حالت بعدش: برد باید از پنل ESP کالیبره شود، و تنها بعد از آن
     یک پشتیبان واقعی برای محافظت وجود دارد.

Usage / نحوهٔ استفاده:
    # ۱) اول فقط ببین چه دستورهایی اجرا می‌شود (هیچ تغییری روی برد نیست)
    python3 Programmer/first_flash_changeover.py --image build/ChangeOver.hex \\
        --label board7 --dry-run

    # ۲) اجرای واقعی
    python3 Programmer/first_flash_changeover.py --image build/ChangeOver.hex \\
        --label board7

    # اگر برد از قبل داده داشت و مطمئنید که باید پاک شود
    python3 Programmer/first_flash_changeover.py --image ... --label board7 \\
        --force-provisioned

Options / گزینه‌ها:
    --image             فایل .hex یا .bin برنامه (اجباری)
    --label             نام یا سریال برد؛ در نام فایل شاهدِ حالت اولیه می‌آید
    --backup-dir        پوشهٔ فایل‌های شاهد (پیش‌فرض Programmer/backups)
    --force-provisioned اجازهٔ اجرا روی بردی که از قبل داده دارد
    --lock              قفل خواندن را در پایان روشن کن (برای اولین بار توصیه نمی‌شود)
    --dry-run           فقط چاپ دستورها
    --cli               مسیر کامل STM32_Programmer_CLI

Step-by-step explanation / توضیح گام‌به‌گام: Programmer/SCRIPTS.md
"""

import argparse
import datetime
import pathlib
import re
import sys

import stm32_cli

# [EN] Device banner handling. The tool is not a stable API, so the rule is
#      deliberately one-sided: accept anything that says STM32F1 (all F1 parts
#      share the same protection mechanism), and REFUSE anything that clearly
#      names a different family. An unnamed device only earns a warning - the
#      next step still refuses to erase a board that holds data, so an unnamed
#      board cannot be destroyed silently.
#      Written this way after a test fixture ("STM32F10x") tripped a stricter
#      pattern and would have aborted a perfectly good board.
# [FA] برخورد با بنر دستگاه. ابزار یک API پایدار نیست، پس قاعده عمداً یک‌طرفه
#      است: هر چه بگوید STM32F1 قبول می‌شود (همهٔ قطعه‌های F1 یک سازوکار قفل
#      دارند) و هر چه خانوادهٔ دیگری را صریح نام ببرد رد می‌شود. دستگاه
#      بی‌نام فقط هشدار می‌گیرد - گام بعد هر بردی که داده دارد را پاک نمی‌کند،
#      پس برد بی‌نام هم بی‌صدا از بین نمی‌رود. این شکل بعد از آن نوشته شد که
#      یک نمونهٔ تست («STM32F10x») الگوی سخت‌گیرانه‌تر را رد کرد و برد سالم را
#      متوقف می‌کرد.
EXPECTED_FAMILY_PATTERN = re.compile(r"STM32F1")
OTHER_FAMILY_PATTERN = re.compile(r"STM32[FLGHUWC][0-9]")


def step_identify(cli, dry_run):
    """[EN] Step 1: connect and confirm the part before touching anything.
       [FA] گام ۱: اتصال و تأیید قطعه پیش از هر تغییری."""
    print("[1/6] connect and identify the part / اتصال و شناسایی قطعه")
    banner = stm32_cli.read_device_info(cli, dry_run)
    if dry_run:
        return
    if banner:
        print(banner.rstrip())

    if EXPECTED_FAMILY_PATTERN.search(banner):
        print("  device: an STM32F1 part answered / یک قطعهٔ STM32F1 پاسخ داد")
        return

    if OTHER_FAMILY_PATTERN.search(banner):
        raise stm32_cli.FlashError(
            "the connected part is not an STM32F1 - wrong board or wrong "
            "connector; nothing was erased"
        )

    print("  WARNING: the tool did not name the device; continuing because the "
          "next step still refuses to erase a board that holds data\n"
          "  هشدار: ابزار نام قطعه را نداد؛ ادامه می‌دهیم چون گام بعد هر بردی "
          "که داده دارد را پاک نمی‌کند")


def step_probe(cli, probe_path, dry_run, force_provisioned):
    """[EN] Step 2: read the data area and decide "new board" or "not new".
       [FA] گام ۲: خواندن ناحیهٔ داده و تصمیم «برد نو» یا «نو نیست»."""
    print("[2/6] probe: is this really a bare board? / بررسی: آیا برد واقعاً نو است؟")
    if dry_run:
        print("  (dry-run) would read 0x%08X..0x%08X into %s"
              % (stm32_cli.DATA_BASE_ADDRESS,
                 stm32_cli.DATA_BASE_ADDRESS + stm32_cli.DATA_SIZE_BYTES - 1,
                 probe_path))
        return

    try:
        stm32_cli.read_data_area(cli, probe_path, dry_run)
    except stm32_cli.FlashError as error:
        raise stm32_cli.FlashError(
            "the data area could not be read, which usually means the part is "
            "still read-protected from an earlier run.\n"
            "  A protected board cannot be checked, and removing the protection "
            "would erase it. Nothing was erased by this script.\n"
            "  ناحیهٔ داده خوانده نشد؛ معمولاً یعنی قطعه از قبل قفل است. برد "
            "قفل‌شده قابل بررسی نیست و برداشتن قفل آن را پاک می‌کند. این اسکریپت "
            "چیزی پاک نکرد.\n"
            "  original error: %s" % error
        )

    if stm32_cli.is_blank_data_area(probe_path):
        print("  data area is blank (0xFF): bare board, good to program / "
              "ناحیهٔ داده پاک است: برد نو")
        probe_path.unlink()
        return

    if force_provisioned:
        print("  data area holds values, but --force-provisioned was given / "
              "ناحیهٔ داده مقدار دارد، ولی --force-provisioned داده شده")
        print("  what is about to be overwritten is kept here: %s" % probe_path)
        return

    raise stm32_cli.FlashError(
        "this board already holds parameters or a calibration table.\n"
        "  Programming it as a NEW board would erase that data on purpose, so "
        "the script stopped before touching the flash.\n"
        "  - to keep the data, use flash_changeover.py (backup + restore)\n"
        "  - to wipe it deliberately, re-run with --force-provisioned\n"
        "  این برد از قبل پارامتر یا جدول کالیبراسیون دارد. فلش‌کردن به‌عنوان "
        "برد نو آن داده را عمداً پاک می‌کند، پس اسکریپت پیش از دست‌زدن به فلش "
        "متوقف شد. برای نگه‌داشتن داده از flash_changeover.py استفاده کنید؛ "
        "برای پاک‌کردن عمدی، --force-provisioned بدهید.\n"
        "  probe saved for inspection: %s" % probe_path
    )


def step_program(cli, image_path, dry_run):
    """[EN] Step 3: erase everything and program the firmware.
       [FA] گام ۳: پاک‌کردن همه و نوشتن فرم‌ور."""
    print("[3/6] full chip erase + program + verify / پاک‌کردن کامل و نوشتن برنامه")
    if not dry_run and not pathlib.Path(image_path).exists():
        raise stm32_cli.FlashError("image not found: %s" % image_path)
    stm32_cli.flash_image(cli, image_path, dry_run)


def step_confirm_factory_state(cli, baseline_path, dry_run):
    """[EN] Step 4: prove the data area really is erased after programming.
       [FA] گام ۴: اثبات اینکه ناحیهٔ داده بعد از نوشتن واقعاً پاک است."""
    print("[4/6] confirm the data area is blank (compiled defaults) / "
          "تأیید پاک‌بودن ناحیهٔ داده (پیش‌فرض کامپایل)")
    stm32_cli.read_data_area(cli, baseline_path, dry_run)
    if dry_run:
        return

    if not stm32_cli.is_blank_data_area(baseline_path):
        raise stm32_cli.FlashError(
            "the data area is NOT blank after programming; the board would "
            "start with unknown parameters.\n"
            "  read-back saved as: %s\n"
            "  ناحیهٔ داده بعد از نوشتن پاک نیست؛ برد با پارامترهای ناشناخته "
            "بالا می‌آمد." % baseline_path
        )
    print("  data area reads back as 0xFF / ناحیهٔ داده 0xFF خوانده شد")


def step_reset_and_report(cli, dry_run):
    """[EN] Step 5: reset and show the protection state of the fresh board.
       [FA] گام ۵: ریست و نمایش وضعیت قفل برد تازه."""
    print("[5/6] reset and show the protection state / ریست و نمایش وضعیت قفل")
    stm32_cli.reset_board(cli, dry_run)
    stm32_cli.show_protection_state(cli, dry_run)


def step_optional_lock(cli, lock_now, dry_run):
    """[EN] Step 6: optional lock, with the reason it is not the default.
       [FA] گام ۶: قفل اختیاری، با دلیل اینکه پیش‌فرض نیست."""
    print("[6/6] read protection / قفل خواندن")
    if not lock_now:
        print("  skipped, and this is the recommendation for a first flash / "
              "رد شد و برای اولین فلش همین توصیه می‌شود")
        print("  why: the board is not calibrated yet; locking now means every "
              "re-flash after this starts with a full chip erase")
        print("  دلیل: برد هنوز کالیبره نشده است؛ قفل‌کردن حالا یعنی هر فلش بعدی "
              "با پاک‌شدن کامل تراشه شروع شود")
        return
    stm32_cli.enable_read_protection(cli, dry_run)


def print_next_steps(label, baseline_path):
    """[EN] The checklist the operator needs right after a first flash.
       [FA] چک‌لیستی که کاربر بلافاصله بعد از اولین فلش لازم دارد."""
    print(
        "\nDONE. What to do now / انجام شد. کارهای بعدی:\n"
        "  1. power the board and watch the boot (LED self-test) / برد را روشن "
        "کنید و بالا آمدن و خودآزمایی چراغ‌ها را ببینید\n"
        "  2. open the ESP panel and check the board answers / پنل ESP را باز "
        "کنید و ببینید برد جواب می‌دهد\n"
        "  3. calibrate from the panel (the table is off right now, so current "
        "comes from the gain/offset line) / کالیبراسیون را از پنل انجام دهید "
        "(جدول خاموش است و جریان با خط gain/offset می‌آید)\n"
        "  4. take the FIRST real backup: / اولین پشتیبان واقعی را بگیرید:\n"
        "     python3 Programmer/flash_changeover.py --image <same image> \\\n"
        "         --backup Programmer/backups/params_%s_<version>.bin\n"
        "     (it also re-flashes, so it proves the backup/restore path works)\n"
        "  5. only after that, consider the lock: set MODULE_FLASH_LOCK=1 in "
        "Firmware/Config/Inc/modules_enable.h, or pass --lock\n"
        "  factory-state evidence saved as: %s\n"
        "  شاهد حالت اولیه ذخیره شد: همان فایل بالا\n"
        % (label, baseline_path)
    )


def parse_arguments():
    """[EN] Command line of the script. / [FA] خط فرمان اسکریپت."""
    parser = argparse.ArgumentParser(
        description="First flash of a bare ChangeOver board (STM32F103C8T6)."
    )
    parser.add_argument("--image", type=pathlib.Path, required=True,
                        help="firmware .hex or .bin")
    parser.add_argument("--label", default="board",
                        help="board name or serial, used in the evidence file name")
    parser.add_argument("--backup-dir", type=pathlib.Path,
                        default=pathlib.Path(__file__).resolve().parent / "backups",
                        help="where the factory-state evidence file is written")
    parser.add_argument("--force-provisioned", action="store_true",
                        help="allow a board that already holds parameters/calibration")
    parser.add_argument("--lock", action="store_true",
                        help="enable RDP level 1 as the last step (not recommended on first flash)")
    parser.add_argument("--dry-run", action="store_true",
                        help="print the commands, change nothing")
    parser.add_argument("--cli", help="full path to STM32_Programmer_CLI")
    return parser.parse_args()


def main():
    """[EN] Entry point: identify, probe, program, prove, report.
       [FA] نقطهٔ ورود: شناسایی، بررسی، نوشتن، اثبات، گزارش."""
    arguments = parse_arguments()
    stamp = datetime.date.today().isoformat()
    baseline_path = (arguments.backup_dir
                     / ("first_flash_%s_%s.bin" % (arguments.label, stamp)))
    probe_path = (arguments.backup_dir
                  / ("probe_%s_%s.bin" % (arguments.label, stamp)))

    try:
        # [EN] Check the cheap things first: a missing image must fail before the
        #      board is even contacted.
        # [FA] اول کارهای ارزان: فایل برنامهٔ نبود باید پیش از اتصال به برد
        #      شکست بخورد.
        if not arguments.dry_run and not pathlib.Path(arguments.image).exists():
            raise stm32_cli.FlashError("image not found: %s" % arguments.image)

        cli = stm32_cli.find_cli(arguments.cli)
        step_identify(cli, arguments.dry_run)
        step_probe(cli, probe_path, arguments.dry_run, arguments.force_provisioned)
        step_program(cli, arguments.image, arguments.dry_run)
        step_confirm_factory_state(cli, baseline_path, arguments.dry_run)
        step_reset_and_report(cli, arguments.dry_run)
        step_optional_lock(cli, arguments.lock, arguments.dry_run)
        print_next_steps(arguments.label, baseline_path)
        return 0
    except stm32_cli.FlashError as error:
        print("\nSTOPPED / متوقف شد:")
        print("  " + str(error).replace("\n", "\n  "))
        return 2


if __name__ == "__main__":
    sys.exit(main())
