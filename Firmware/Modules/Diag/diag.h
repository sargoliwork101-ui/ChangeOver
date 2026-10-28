/**
 * @file    diag.h
 * @brief   [EN] Diagnostic log: charge time, count, 10 last faults, power events, min/max.
 *               Persisted in dedicated flash pages 0x0800F800 / 0x0800FC00 (2KB ping-pong).
 *          [FA] لاگ تشخیصی: زمان شارژ، تعداد شارژ، ۱۰ خطای آخر، رویدادهای قطع برق، کمینه/بیشینه.
 *               در صفحات فلش جدا ‎0x0800F800 / 0x0800FC00‎ به صورت پینگ‌پنگ ماندگار می‌شود.
 */

#ifndef DIAG_H
#define DIAG_H

/* ==================== Includes ==================== */
#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"

/* ==================== Defines ==================== */
#define DIAG_MAX_ERRORS               10u
#define DIAG_MAX_POWER_EVENTS         20u

#define DIAG_PAGE_A_ADDR              0x0800F800u
#define DIAG_PAGE_B_ADDR              0x0800FC00u
#define DIAG_MAGIC                    0x44494147u /* 'DIAG' */
#define DIAG_VERSION                  1u

/* ==================== Types ==================== */

/**
 * @brief  [EN] One fault entry with timestamp and snapshot.
 *         [FA] یک ورودی خطا با زمان و نمونه ولتاژها.
 */
typedef struct
{
    uint32_t uint32_t__timestamp;   /* [EN] Unix sec from phone sync + uptime / زمان یونیکس */
    uint32_t uint32_t__faultMask;   /* [EN] fault_mask at that moment / ماسک خطا */
    uint32_t uint32_t__vInMv;       /* [EN] Input mV / ورودی */
    uint32_t uint32_t__vPackMv;     /* [EN] Pack mV / پک */
    uint32_t uint32_t__vLowMv;      /* [EN] Low half mV / پایین */
    uint32_t uint32_t__vHighMv;     /* [EN] High half mV / بالا */
    uint32_t uint32_t__state1;      /* [EN] Charger state ch1 / حالت شارژر۱ */
    uint32_t uint32_t__state2;      /* [EN] Charger state ch2 / حالت شارژر۲ */
} diag_error_t;

/**
 * @brief  [EN] One power off/on event.
 *         [FA] یک رویداد قطع و وصل برق شهر.
 */
typedef struct
{
    uint32_t uint32_t__offTime;     /* [EN] Unix sec when input lost / زمان قطع */
    uint32_t uint32_t__onTime;      /* [EN] Unix sec when input back / زمان وصل */
    uint32_t uint32_t__durationSec; /* [EN] Duration sec / مدت قطعی */
} diag_power_event_t;

/**
 * @brief  [EN] Full persisted record.
 *         [FA] رکورد کامل ماندگار.
 */
typedef struct
{
    uint32_t uint32_t__magic;
    uint16_t uint16_t__version;
    uint16_t uint16_t__seq;
    uint16_t uint16_t__count; /* not used, kept for alignment */
    uint16_t uint16_t__reserved;

    /* [EN] Basic counters / شمارنده‌های اصلی */
    uint32_t uint32_t__totalChargeSec;
    uint32_t uint32_t__chargeSecCh1;
    uint32_t uint32_t__chargeSecCh2;
    uint32_t uint32_t__chargeCount;
    uint32_t uint32_t__chargeCountCh1;
    uint32_t uint32_t__chargeCountCh2;
    uint32_t uint32_t__uptimeSec;
    uint32_t uint32_t__resetCount;
    uint32_t uint32_t__boardTimeBase; /* [EN] boardTime = base + uptime / پایه زمان */

    /* [‎EN] Min/max‎ / کمینه بیشینه */
    uint32_t uint32_t__maxVinMv;
    uint32_t uint32_t__minVinMv;
    uint32_t uint32_t__maxVpackMv;
    uint32_t uint32_t__minVpackMv;
    uint32_t uint32_t__maxVlowMv;
    uint32_t uint32_t__minVlowMv;
    uint32_t uint32_t__maxVhighMv;
    uint32_t uint32_t__minVhighMv;
    uint32_t uint32_t__maxImbMv;
    uint32_t uint32_t__maxIch1Ma;
    uint32_t uint32_t__maxIch2Ma;

    /* [EN] Event counters / شمارنده رویداد */
    uint32_t uint32_t__imbalanceCount;
    uint32_t uint32_t__lowBatteryCount;
    uint32_t uint32_t__batLostCount;
    uint32_t uint32_t__techFaultCount;
    uint32_t uint32_t__ovCount; /* [EN] FAULT_ADC count; legacy name kept for the flash record layout / شمارندهٔ ‎FAULT_ADC‎؛ نام قدیمی به‌خاطر چیدمان رکورد فلش مانده است */

    uint8_t uint8_t__errorCount;
    uint8_t uint8_t__powerEventCount;
    uint8_t uint8_t__reserved[2];

    diag_error_t diag_error_t__A__Errors[DIAG_MAX_ERRORS];
    diag_power_event_t diag_power_event_t__A__PowerEvents[DIAG_MAX_POWER_EVENTS];

    uint32_t uint32_t__crc32;
} diag_record_t;

/* ==================== Public Functions ==================== */

/**
 * @brief  [EN] Init diag from flash, pick newest valid record.
 *         [FA] مقداردهی دیاگ از فلش، انتخاب تازه‌ترین رکورد سالم.
 */
void func__Diag_Init(void);

/**
 * @brief  [EN] Periodic tick: takes the uptime from the kernel tick counter and
 *              runs the debounced flash save. Call about once a second from
 *              TaskControl; a late call costs nothing because uptime is read
 *              from the kernel, not counted here (v1.113).
 *         [FA] تیک دوره‌ای: آپتایم را از شمارندهٔ تیک کرنل می‌گیرد و ذخیرهٔ
 *              دیبانس‌شدهٔ فلش را می‌گرداند. حدود هر ثانیه از TaskControl صدا
 *              زده می‌شود؛ دیر رسیدنش هزینه‌ای ندارد چون آپتایم از کرنل خوانده
 *              می‌شود و این‌جا شمرده نمی‌شود (نسخهٔ ۱٫۱۱۳).
 */
void func__Diag_Tick(void);

/**
 * @brief  [EN] Set board time from phone (Unix sec).
 *         [FA] تنظیم ساعت برد از گوشی (یونیکس).
 * @param  uint32_t__unixSec [EN] Unix seconds / ثانیه یونیکس
 */
void func__Diag_SetBoardTime(uint32_t uint32_t__unixSec);

/**
 * @brief  [EN] Get current board time (Unix sec) = base + uptime.
 *         [FA] گرفتن ساعت فعلی برد = پایه + آپتایم.
 * @return uint32_t [EN] Unix sec or 0 if never synced / یونیکس یا صفر
 */
uint32_t func__Diag_GetBoardTime(void);

/**
 * @brief  [EN] Count the charge time this channel has run since the previous
 *              call. The seconds are taken from the real uptime delta, so they
 *              never depend on how often the caller comes (v1.113).
 *         [FA] زمان شارژی که این کانال از فراخوانی قبلی کار کرده را می‌شمارد.
 *              ثانیه‌ها از تفاضل آپتایم واقعی گرفته می‌شوند، پس هرگز به نرخ
 *              فراخوانی وابسته نیستند (نسخهٔ ۱٫۱۱۳).
 * @param  uint8_t__channel [EN] 0 or 1 / کانال
 */
void func__Diag_AddChargeSec(uint8_t uint8_t__channel);

/**
 * @brief  [EN] Record a complete charge cycle.
 *         [FA] ثبت یک سیکل شارژ کامل.
 * @param  uint8_t__channel [EN] Channel / کانال
 */
void func__Diag_RecordChargeComplete(uint8_t uint8_t__channel);

/**
 * @brief  [EN] Record a fault with current snapshot.
 *         [FA] ثبت خطا با نمونه فعلی.
 * @param  fault_mask_t__faults [EN] Fault mask / ماسک خطا
 * @param  measurement_snapshot_t__snap [EN] Snapshot / نمونه
 */
void func__Diag_RecordFault(fault_mask_t fault_mask_t__faults, const measurement_snapshot_t *measurement_snapshot_t__snap);

/**
 * @brief  [EN] Record input power off event start.
 *         [FA] ثبت شروع قطع برق شهر.
 */
void func__Diag_RecordPowerOff(void);

/**
 * @brief  [EN] Record input power on event end.
 *         [FA] ثبت پایان قطع و وصل برق.
 */
void func__Diag_RecordPowerOn(void);

/**
 * @brief  [EN] Clear stored diagnostic error and power event histories.
 *         [FA] پاک‌کردن تاریخچهٔ خطاها و رویدادهای برق در دیاگ.
 */
void func__Diag_ClearHistory(void);

/**
 * @brief  [EN] Update min/max voltages and currents from snapshot.
 *         [FA] به‌روزرسانی کمینه بیشینه از نمونه.
 * @param  measurement_snapshot_t__snap [EN] Snapshot / نمونه
 */
void func__Diag_UpdateMinMax(const measurement_snapshot_t *measurement_snapshot_t__snap);

/**
 * @brief  [EN] Get active record for ESP-Link transmission.
 *         [FA] گرفتن رکورد فعال برای ارسال به ESP.
 * @‎return const diag_record_t* [EN] Pointer‎ / اشاره‌گر
 */
const diag_record_t *func__Diag_GetRecord(void);

/**
 * @brief  [EN] Force immediate flash save (e.g. before reset).
 *         [FA] ذخیره فوری فلش (قبل از ریست).
 * @return bool [EN] true on success / موفقیت
 */
bool func__Diag_SaveNow(void);

/**
 * @brief  [EN] Lock the scheduler around a cross-task read of the active
 *              diagnostic record.
 *         [FA] قفل زمان‌بند هنگام خواندن رکورد فعال دیاگ از تسک دیگر.
 * @return int32_t [EN] Previous kernel lock state / وضعیت قبلی قفل کرنل
 */
int32_t func__Diag_LockRecord(void);

/**
 * @brief  [EN] Restore the scheduler lock after reading the active record.
 *         [FA] بازگردانی قفل زمان‌بند پس از خواندن رکورد فعال دیاگ.
 * @param  int32_t__savedLock [EN] Value returned by func__Diag_LockRecord / مقدار بازگشتی قفل
 */
void func__Diag_UnlockRecord(int32_t int32_t__savedLock);

#endif /* DIAG_H */
