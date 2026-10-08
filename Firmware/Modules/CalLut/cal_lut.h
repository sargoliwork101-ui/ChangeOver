/**
 * @file    cal_lut.h
 * @brief   [EN] Runtime bench LUT storage in its OWN flash block (v1.66).
 *          [FA] نگهداری جدول بنچ در «بلوک فلش مخصوص خودش» (v1.66).
 *
 * @note    [EN] User order 2026-10-05: the calibration table must be
 *              pushable straight into the micro instead of being pasted
 *              into calibration.h and rebuilt - and because it moves a lot
 *              of data at once, its storage path must be SEPARATE from the
 *              ordinary parameter NVM. This module is that separate path:
 *              two dedicated 1 KiB pages (0x0800F000 / 0x0800F400) that sit
 *              ABOVE the parameter NVM (0x0800E000..0x0800EFFF since v1.80),
 *              the same
 *              power-cut-safe ping-pong, its own magic/version/CRC32, and
 *              NOTHING in common with esp_link_nvm.c except the BSP flash
 *              driver. A LUT push can therefore never disturb a parameter
 *              record and vice versa.
 *
 *              The compile-time tables in calibration.h STAY and remain the
 *              fallback (user order: "keep today's capability too"): the
 *              measurement path uses the flash table only when a valid
 *              record is present, and falls back to the compiled table on a
 *              fresh board, a corrupt record or a version bump.
 *
 *              Handshake: the panel stages the table (LUT_BEGIN +
 *              LUT_CHUNK), then asks for a commit (LUT_COMMIT carrying the
 *              CRC32 the panel computed). The board validates, writes,
 *              re-reads and answers LUT_ACK with its OWN CRC32 of what is
 *              now in flash. Only when the two match may the panel ask for
 *              the reboot (LUT_RESET) that lets every module start from the
 *              new table.
 *
 *          [FA] دستور کاربر ۲۰۲۶-۱۰-۰۵: جدول کالیبراسیون باید مستقیم به
 *              میکرو فرستاده شود، نه اینکه در calibration.h چسبانده و
 *              دوباره بیلد شود - و چون داده زیادی جابه‌جا می‌شود، مسیر
 *              ذخیره‌سازی‌اش باید از NVM پارامترها «جدا» باشد. این ماژول
 *              همان مسیر جداست: دو صفحهٔ اختصاصی ۱KB پایین‌تر از صفحه‌های
 *              پارامتر، همان پینگ‌پنگ امن در برابر قطع برق، مجیک/نسخه/CRC
 *              مخصوص خودش و هیچ اشتراکی با esp_link_nvm.c جز درایور فلش.
 *              پس فرستادن جدول هرگز رکورد پارامترها را به هم نمی‌زند.
 *              جدول‌های کامپایل‌تایم در calibration.h می‌مانند و پشتیبان
 *              هستند (دستور کاربر: «روش فعلی هم بماند»).
 *              دست‌دادن: پنل جدول را می‌چیند، بعد commit با CRC32 خودش را
 *              می‌فرستد؛ برد اعتبارسنجی و نوشتن و بازخوانی می‌کند و CRC32
 *              «آنچه حالا در فلش است» را در LUT_ACK برمی‌گرداند. فقط اگر
 *              این دو برابر بودند، پنل اجازهٔ ریست را می‌دهد.
 */

#ifndef CAL_LUT_H
#define CAL_LUT_H

#include <stdint.h>
#include <stdbool.h>

/* ==================== CalLut constants / ثابت‌ها ==================== */

/* [EN] The two dedicated pages. Guard-defined so the host test can redirect
 *      them into a RAM emulation and compile this exact state machine.
 * [FA] دو صفحهٔ اختصاصی. ماکروها گارد دارند تا تست هاست آنها را به
 *      شبیه‌سازی RAM ببرد و دقیقاً همین ماشین حالت را کامپایل کند. */
#ifndef CAL_LUT_PAGE_A_ADDR
#define CAL_LUT_PAGE_A_ADDR             0x0800F000u
#endif
#ifndef CAL_LUT_PAGE_B_ADDR
#define CAL_LUT_PAGE_B_ADDR             0x0800F400u
#endif

/* [EN] "CLUT" + format version; a bump invalidates old records and the
 *      compiled tables take over again (never a silent half-upgrade).
 * [FA] «CLUT» + نسخهٔ قالب؛ تغییر نسخه رکورد قدیمی را نامعتبر می‌کند و
 *      جدول کامپایل‌شده دوباره سر کار می‌آید. */
#define CAL_LUT_MAGIC                   0x434C5554u
#define CAL_LUT_VERSION                 1u

/* [EN] Per-channel point cap. 24 points x 2 axes x 2 channels x 4 B = 384 B
 *      of record (and the same in RAM). The bench wizard runs 17..21 duty
 *      steps today, so 24 leaves room without another flash page. The
 *      _Static_assert in the .c proves the record still fits the page.
 * [FA] سقف نقاط هر کانال. ۲۴ نقطه × ۲ محور × ۲ کانال × ۴ بایت = ۳۸۴ بایت
 *      رکورد (و همان‌قدر RAM). ویزارد امروز ۱۷..۲۱ پله دارد، پس ۲۴ جا دارد.
 *      گزارهٔ داخل فایل .c جاشدن رکورد در صفحه را اثبات می‌کند. */
#define CAL_LUT_POINTS_MAX              24u
#define CAL_LUT_POINTS_MIN              2u

/* [EN] Channel ids as they travel on the link (1-based, like the panel and
 *      the schematic name the batteries).
 * [FA] شناسهٔ کانال روی لینک (از ۱، مثل نام‌گذاری پنل و شماتیک). */
#define CAL_LUT_CHANNEL_1               1u
#define CAL_LUT_CHANNEL_2               2u

/* [EN] Commit/stage status codes, echoed to the panel in LUT_ACK so a
 *      failure says WHY instead of just "nothing happened".
 * [FA] کدهای وضعیت که در LUT_ACK به پنل برمی‌گردند تا خطا «دلیل» داشته
 *      باشد نه فقط «هیچ اتفاقی نیفتاد». */
#define CAL_LUT_ST_OK                   0u
#define CAL_LUT_ST_NO_STAGE             1u  /* commit without a begin / کامیت بدون شروع */
#define CAL_LUT_ST_COUNT                2u  /* point count out of range / تعداد نقاط نامعتبر */
#define CAL_LUT_ST_MISSING              3u  /* a chunk never arrived / تکه‌ای نرسید */
#define CAL_LUT_ST_CHAIN                4u  /* chain axis not strictly increasing / محور زنجیره صعودی نیست */
#define CAL_LUT_ST_POWER                5u  /* power axis dips / محور توان افت دارد */
#define CAL_LUT_ST_CRC                  6u  /* panel CRC != staged CRC / CRC پنل با CRC چیده‌شده فرق دارد */
#define CAL_LUT_ST_FLASH                7u  /* erase/program/verify failed / نوشتن فلش شکست */

/* ==================== CalLut types / نوع‌ها ==================== */

/**
 * @brief  [EN] One channel's table inside the flash record.
 *         [FA] جدول یک کانال داخل رکورد فلش.
 */
typedef struct
{
    uint32_t uint32_t__points;                              /* [EN] 0 = channel not overridden / ۰ = این کانال جایگزین نشده */
    uint32_t UINT32_T__A__ChainMa[CAL_LUT_POINTS_MAX];      /* [EN] ADC chain mA / mA زنجیره */
    uint32_t UINT32_T__A__PowerMw[CAL_LUT_POINTS_MAX];      /* [EN] Battery power mW / توان باتری */
} cal_lut_channel_t;

/**
 * @brief  [EN] The flash record: header + both channels + CRC32 over every
 *              preceding byte (same CRC the panel computes, so the ACK is a
 *              real end-to-end handshake and not a length check).
 *         [FA] رکورد فلش: سربرگ + هر دو کانال + CRC32 روی همهٔ بایت‌های
 *              قبل از خودش (همان CRC که پنل حساب می‌کند، پس ACK یک دست‌دادن
 *              واقعی سرتاسری است نه بررسی طول).
 */
typedef struct
{
    uint32_t uint32_t__magic;
    uint16_t uint16_t__version;
    uint16_t uint16_t__seq;
    cal_lut_channel_t CAL_LUT_CHANNEL_T__A__Channel[2];
    uint32_t uint32_t__crc32;
} cal_lut_record_t;

/* ==================== CalLut public functions ==================== */

/* [EN] Pure helpers - public because the host test compiles this exact code
 *      against a RAM-emulated flash.
 * [FA] توابع خالص - عمومی چون تست هاست همین کد را روی فلش شبیه‌سازی‌شده
 *      کامپایل می‌کند. */

/**
 * @brief  [EN] Reflected CRC32 (poly 0xEDB88320) - the standard zip CRC the
 *              panel's JavaScript also implements.
 *         [FA] CRC32 بازتابیده (استاندارد zip) که جاوااسکریپت پنل هم دارد.
 */
uint32_t func__CalLut_Crc32(const uint8_t *uint8_t__A__Data,
                            uint32_t uint32_t__length);

/**
 * @brief  [EN] Validate a candidate record: magic, version, point counts,
 *              strictly increasing chain axis, non-decreasing power axis
 *              (an unsigned dip wraps the interpolation to ~4e9 mW), CRC32.
 *         [FA] اعتبارسنجی رکورد: مجیک، نسخه، تعداد نقاط، محور زنجیرهٔ
 *              اکیداً صعودی، محور توان بدون افت (افت در حساب بدون‌علامت به
 *              ~۴e۹ می‌پیچد) و CRC32.
 */
bool func__CalLut_RecordValidate(const cal_lut_record_t *cal_lut_record_t__record);

/**
 * @brief  [EN] Boot load: newest valid record wins, otherwise the compiled
 *              tables in calibration.h stay in charge.
 *         [FA] بارگذاری بوت: تازه‌ترین رکورد معتبر؛ وگرنه جدول کامپایل‌شده.
 */
void func__CalLut_Init(void);

/**
 * @brief  [EN] Is a flash table in charge for this channel (1 or 2)?
 *         [FA] آیا برای این کانال جدول فلش فعال است؟
 */
bool func__CalLut_Active(uint8_t uint8_t__channel);

/**
 * @brief  [EN] Active point count / chain axis / power axis of a channel.
 *              Only valid while func__CalLut_Active() is true.
 *         [FA] تعداد نقاط / محور زنجیره / محور توانِ کانال فعال.
 */
uint32_t func__CalLut_Points(uint8_t uint8_t__channel);
const uint32_t *func__CalLut_ChainMa(uint8_t uint8_t__channel);
const uint32_t *func__CalLut_PowerMw(uint8_t uint8_t__channel);

/**
 * @brief  [EN] Start staging a new table (LUT_BEGIN). Clears the staging
 *              buffer; the ACTIVE table keeps working until the commit
 *              succeeds, so an interrupted push changes nothing. A zero
 *              count is legal and a successful commit removes that channel's
 *              previous flash override.
 *         [FA] شروع چیدن جدول جدید؛ جدول «فعال» تا موفقیت کامیت سر کار
 *              می‌ماند، پس ارسال نیمه‌کاره هیچ‌چیز را خراب نمی‌کند. تعداد
 *              صفر مجاز است و کامیت موفق override فلش قبلی همان کانال را حذف
 *              می‌کند.
 * @return bool [EN] false = counts out of range‎ / تعداد نقاط نامعتبر
 */
bool func__CalLut_StageBegin(uint32_t uint32_t__points1,
                             uint32_t uint32_t__points2);

/**
 * @brief  [EN] Store one staged point (LUT_CHUNK).
 *         [FA] ذخیرهٔ یک نقطه در ناحیهٔ چیدن.
 */
bool func__CalLut_StagePoint(uint8_t uint8_t__channel,
                             uint32_t uint32_t__index,
                             uint32_t uint32_t__chainMa,
                             uint32_t uint32_t__powerMw);

/**
 * @brief  [EN] Validate + write the staged table (LUT_COMMIT). On success
 *              the RAM copy is reloaded FROM FLASH, so what the measurement
 *              path uses is exactly what survived the write.
 *         [FA] اعتبارسنجی و نوشتن جدول چیده‌شده. در موفقیت، نسخهٔ RAM از
 *              «خود فلش» دوباره خوانده می‌شود.
 * @param  uint32_t__panelCrc32 [EN] CRC32 the panel computed / CRC32‎ پنل
 * @param  uint32_t__ptr_boardCrc32 [EN] Out: CRC32 now in flash / خروجی
 * @return uint8_t [EN] CAL_LUT_ST_* / کد وضعیت
 */
uint8_t func__CalLut_Commit(uint32_t uint32_t__panelCrc32,
                            uint32_t *uint32_t__ptr_boardCrc32);

/**
 * @brief  [EN] CRC32 of the record currently in flash (0 when none).
 *         [FA] CRC32 رکورد فعلی فلش (۰ اگر نباشد).
 */
uint32_t func__CalLut_ActiveCrc32(void);

/**
 * @brief  [EN] Arm the post-handshake reboot (LUT_RESET): the reset is NOT
 *              taken inside the link handler - the caller finishes its
 *              frame, the charger is idled, and func__CalLut_Tick() pulls
 *              the trigger a few comm runs later.
 *         [FA] مسلح‌کردن ریست بعد از دست‌دادن: ریست داخل هندلر لینک گرفته
 *              نمی‌شود؛ فریم تمام می‌شود، شارژر بیکار می‌شود و چند اجرا
 *              بعد func__CalLut_Tick() ماشه را می‌کشد.
 */
void func__CalLut_RequestReset(void);

/**
 * @brief  [EN] Comm-task housekeeping: performs the armed reboot.
 *         [FA] نگهداری تسک ارتباط: ریست مسلح‌شده را انجام می‌دهد.
 */
void func__CalLut_Tick(void);

#endif /* CAL_LUT_H */
