/* ============================================================================
 * @file    calibration.h
 * ChangeOver bench calibration tables / جداول کالیبراسیون بنچ ChangeOver
 * ----------------------------------------------------------------------------
 * [EN] All bench calibration tables in ONE place (included ONLY by
 *      measurement.c; the arrays are static). HOW TO EXTEND (all three
 *      tables): 1) append the anchor to BOTH arrays (chain AND battery -
 *      equal lengths, host-test enforced); 2) keep each axis strictly
 *      increasing (the point count derives itself via sizeof); 3) tables
 *      are keyed on MEASURED chain current, never duty.
 * [FA] همهٔ جدول‌های کالیبراسیون بنچ یک‌جا (فقط measurement.c اینکلود
 *      می‌کند؛ آرایه‌ها static). راهنمای بزرگ‌کردن (هر سه جدول): ۱) لنگر
 *      را به «هر دو» آرایه اضافه کن (طول برابر - تست هاست قفل می‌کند)؛
 *      ۲) محورها اکیداً صعودی بمانند (تعداد نقاط از sizeof)؛ ۳) کلید
 *      جدول «جریان زنجیرهٔ اندازه‌گیری‌شده» است، هرگز دیوتی.
 * ============================================================================ */

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdint.h>

/* ============================================================================
 * TABLE 1 - channel-1 LUT (chain mA -> battery-1 POWER mW)
 * جدول ۱ - LUT کانال ۱ (mA زنجیره ← توان باتری ۱ بر حسب mW)
 * ----------------------------------------------------------------------------
 * [EN] Output is battery-1 POWER in mW, not current: in DCM the mid-ON
 *      chain sample tracks the energy per cycle (battery-voltage
 *      independent); measurement.c divides by the LIVE vhigh (clamped
 *      8.0..15.0 V) to get mA. Anchored on the 2026-09-27 SOLO1 sweep with
 *      off1=8 / gain1=1046 - if those params change the table MUST be
 *      rebuilt (the chain axis rescales). Gate (5,0): chain <= 5 is
 *      switching noise and reads exactly 0. The D7 dip (81,1028) is the
 *      real curve, kept as an anchor. D5's DMM voltage (12200 mV) is a
 *      +150 mV outlier vs its neighbours but harmless (36 mA x 150 mV =
 *      5 mW). Above the last anchor the last slope (13.14 mW per
 *      chain-mA) extends. COMPANION: V24 pack divider
 *      top = 66200 ohms.
 * [FA] خروجی «توان باتری ۱» است نه جریان: در DCM نمونهٔ وسط-ON زنجیره
 *      انرژیِ هر سایکل را دنبال می‌کند (مستقل از ولتاژ باتری) و
 *      measurement.c آن را به vhigh زنده (گیرهٔ ۸..۱۵V) تقسیم می‌کند.
 *      لنگرها با off1=8 / gain1=1046 ثبت شده‌اند - با تغییر آنها جدول
 *      باید بازسازی شود (محور زنجیره جابه‌جا می‌شود). گیت (۵٫۰):
 *      زنجیره ≤۵ نویز سوییچینگ است و دقیقاً صفر می‌خواند. گودی D7
 *      (81,1028) منحنی واقعی است و لنگر ماند. بالای آخرین لنگر، شیب
 *      آخر (۱۳٫۱۴ mW به‌ازای هر mA زنجیره) ادامه می‌یابد. همراه: تاپ
 *      مقسم پک = ۶۶۲۰۰ اهم.
 * ============================================================================ */

#define CAL_CURRENT1_LUT_ENABLE 1u

#if (CAL_CURRENT1_LUT_ENABLE != 0u)
static const uint32_t CAL_Current1LutChainMa[] =
    { 0u, 5u, 11u, 31u, 54u, 81u, 114u, 148u, 189u, 231u, 277u, 330u, 382u, 444u, 504u, 567u, 640u };
static const uint32_t CAL_Current1LutBatteryMw[] =
    { 0u, 0u, 132u, 440u, 772u, 1028u, 1615u, 2111u, 2674u, 3261u, 3857u, 4553u, 5288u, 6074u, 6854u, 7686u, 8645u };
#define CAL_CURRENT1_LUT_POINTS \
    ((uint32_t)(sizeof(CAL_Current1LutChainMa) / \
                sizeof(CAL_Current1LutChainMa[0u])))
#endif

/* ============================================================================
 * TABLE 2 - channel-2 LUT (chain mA -> battery-2 POWER mW)
 * جدول ۲ - LUT کانال ۲ (mA زنجیره ← توان باتری ۲ بر حسب mW)
 * ----------------------------------------------------------------------------
 * [EN] v1.13 (user order 2026-09-25, "voltages are fixed but the currents
 *      you read are wrong"): the table OUTPUT is the battery-2 POWER in
 *      mW, not current - the DCM energy per cycle is battery-voltage
 *      independent (current = P/Vbat); measurement.c divides by the LIVE
 *      battery-2 voltage (clamped 8.0..15.0 V) to get mA. Anchors are
 *      fitted end-to-end against the exact integer firmware pipeline
 *      (2026-09-27 SOLO2 sweep; off2=8 / gain2=1303 - if those params
 *      change the table MUST be rebuilt). Gate (20,0): chain <= 20 is
 *      switching noise at 1..3% duty (the true backfeed there is
 *      invisible to the single-supply shunt chain) and reads exactly 0;
 *      the 2%-duty zener discharge also floors to 0 (error <= 13 mA at
 *      the very bottom). EXCLUDED: D9 (unsettled-sample outlier) and the
 *      D16 DMM VOLTAGE (12650 mV is a typo; power recomputed with the
 *      firmware 13617 mV). Above the last anchor the last slope
 *      (12.37 mW per chain-mA) extends. Methodology floor: +-1 count =
 *      +-3 mA (slope ~30 mW/chain-mA on the steep band).
 * [FA] همان معماری توان جدول ۱ (ناوردای DCM: جریان = P/Vbat با تقسیم
 *      زمان اجرا بر ولتاژ زندهٔ باتری ۲، گیرهٔ ۸..۱۵V). لنگرها
 *      سرتاسری روی خط‌لولهٔ صحیحِ خود فرم‌ور فیت شده‌اند (سوییپ SOLO2
 *      ۲۰۲۶-۰۹-۲۷؛ off2=8 / gain2=1303 - با تغییر آنها جدول باید
 *      بازسازی شود). گیت (۲۰٫۰): زنجیره ≤۲۰ نویز سوییچینگ دیوتی ۱..۳٪
 *      است (برگشت جریان واقعی آنجا برای زنجیرهٔ شنت تک‌تغذیه نامرئی
 *      است) و دقیقاً صفر می‌خواند؛ تخلیهٔ زنرِ دیوتی ۲٪ هم کف ۰ می‌گیرد
 *      (خطا ≤ ۱۳mA فقط در کف). بالای آخرین لنگر شیب آخر (۱۲٫۳۷ mW
 *      به‌ازای هر mA زنجیره) ادامه می‌یابد. کف روش: ‎±1 شمارش = ‎±3mA.
 * ============================================================================ */

#define CAL_CURRENT2_LUT_ENABLE 1u

#if (CAL_CURRENT2_LUT_ENABLE != 0u)
static const uint32_t CAL_Current2LutChainMa[] =
    { 0u, 20u, 37u, 106u, 189u, 236u, 253u, 283u, 312u, 353u, 390u, 441u, 557u, 707u };
static const uint32_t CAL_Current2LutBatteryMw[] =
    { 0u, 0u, 109u, 751u, 1581u, 2685u, 3260u, 3925u, 4550u, 5355u, 6035u, 6817u, 8573u, 10429u };
#define CAL_CURRENT2_LUT_POINTS \
    ((uint32_t)(sizeof(CAL_Current2LutChainMa) / \
                sizeof(CAL_Current2LutChainMa[0u])))
#endif

/* ============================================================================
 * TABLE 3 - battery-2 (V12 / Vlow) voltage compensation
 * جدول ۳ - جبران ولتاژ باتری ۲ (V12 / Vlow)
 * ----------------------------------------------------------------------------
 * [EN] The V12 sense reads above the battery-2 terminals: static divider
 *      error + charge-path wire drop proportional to the current. LSQ fit
 *      (10 DMM points, 0..764 mA): error = 150 mV + 0.47 ohm x I2
 *      (residual +/-28 mV = 0.23%). measurement.c subtracts
 *      STATIC + I2 x PATH/1000 AFTER the runtime V12 offset, using the
 *      post-LUT channel-2 current, BEFORE Vlow/Vhigh are derived. With
 *      more bench points this two-term model can grow into a full anchor table
 *      exactly like tables 1/2.
 * [FA] سنس V12 بالاتر از ترمینال باتری ۲ می‌خواند: خطای ثابت مقسم + افت
 *      مسیر شارژ متناسب با جریان. برازش LSQ (۱۰ نقطهٔ DMM، 0..764mA):
 *      خطا = 150mV + ۰٫۴۷ اهم × I2 (خطای باقی‌مانده ±۲۸mV = ۰٫۲۳٪).
 *      measurement.c آن را بعد از آفست زمان اجرا و با جریان پس از
 *      جدول، قبل از مشتق‌گیری Vlow/Vhigh کم می‌کند. با نقاط بیشتر این
 *      مدل دو جمله‌ای می‌تواند مثل جدول‌های ۱/۲ جدول انکری کامل شود.
 * ============================================================================ */

#define CAL_BATTERY12_BENCH_COMP_ENABLE 1u

#if (CAL_BATTERY12_BENCH_COMP_ENABLE != 0u)
#define CAL_BATTERY12_BENCH_STATIC_MV  150u   /* [EN] static divider error, mV / خطای ثابت مقسم، mV */
#define CAL_BATTERY12_BENCH_PATH_MOHM   470u   /* [EN] charge-path resistance, mOhm / مقاومت مسیر شارژ، mOhm */
#endif

#endif /* CALIBRATION_H */
