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
 *      آخر ادامه می‌یابد. همراه: تاپ مقسم پک = 68K+1.2K روی 6.8K
 *      (شماتیک؛ اصلاح ۲۰۲۶-۰۹-۲۹، عدد جعلی ۶۶۲۰۰ حذف شد).
 * ============================================================================ */

#define CAL_CURRENT1_LUT_ENABLE 1u

#if (CAL_CURRENT1_LUT_ENABLE != 0u)
/* [EN] USER-ORDERED CORRECTION 2026-09-29 - refitted because the PACK
 *      voltage divider was corrected to the real schematic value.
 *      This table is battery POWER, and power is voltage x current. It had
 *      been fitted while the pack divider was falsified, so it silently
 *      absorbed that error: the firmware then computed I = P_lut / V with
 *      BOTH terms wrong by the same factor, the error cancelled, and the
 *      current looked right while the power and the voltage were not.
 *      Correcting the divider without refitting here would have broken the
 *      current by ~8 percent. Refitted directly from the 2026-09-27 SOLO1
 *      DMM currents at the corrected voltage; replay error improved from
 *      4 mA to 1 mA.
 * [FA] اصلاح به دستور کاربر ۲۰۲۶-۰۹-۲۹ - چون مقسم ولتاژ پک به مقدار واقعی
 *      شماتیک اصلاح شد، این جدول دوباره برازش شد. این جدول توانِ باتری است و
 *      توان یعنی ولتاژ ضرب در جریان؛ چون زمانی برازش شده بود که مقسم پک جعلی
 *      بود، همان خطا را در خود جذب کرده بود: فرم‌ور I = P/V را با هر دو جملهٔ
 *      غلط حساب می‌کرد، خطا حذف می‌شد و جریان درست به نظر می‌رسید در حالی که
 *      توان و ولتاژ غلط بودند. اصلاح مقسم بدون برازش دوباره، جریان را حدود
 *      ۸٪ خراب می‌کرد. خطای بازپخش از ۴ به ۱ میلی‌آمپر بهتر شد. */
static const uint32_t CAL_Current1LutChainMa[] =
    { 0u, 5u, 11u, 31u, 54u, 81u, 114u, 148u, 189u, 231u, 277u, 330u, 382u, 444u, 504u, 567u, 640u };
static const uint32_t CAL_Current1LutBatteryMw[] =
    { 0u, 0u, 140u, 459u, 820u, 1095u, 1723u, 2259u, 2865u, 3496u, 4130u, 4862u, 5636u, 6488u, 7364u, 8290u, 9345u };
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
/* [EN] MISLABELLED UNTIL 2026-09-29: this was called a "static divider
 *      error", which it is not. The V12 divider is exactly the schematic
 *      value (R48 33K + R15 1.2K over R16 6.8K) and is correct. 150 mV on
 *      12.2 V is 1.23 percent - the SAME common-mode error the input channel
 *      shows (+1.19 percent), i.e. this is this channel's share of the global
 *      ADC-reference error documented at BSP_MEASUREMENT_VREF_MV, patched
 *      locally. Kept for now because removing it without correcting VREF
 *      would make V12 read 1.2 percent low, but it must NOT be treated as a
 *      divider fix and no further per-channel patch may be added for a
 *      shared error.
 * [FA] تا ۲۰۲۶-۰۹-۲۹ برچسب غلط داشت: «خطای ثابت مقسم» نامیده می‌شد که نیست.
 *      مقسم ۱۲ولت دقیقاً مقدار شماتیک است و درست است. ۱۵۰ میلی‌ولت روی ۱۲٫۲
 *      ولت یعنی ۱٫۲۳٪ - همان خطای مشترکی که کانال ورودی هم نشان می‌دهد
 *      (۱٫۱۹٪)؛ یعنی سهم این کانال از خطای سراسری مرجع ADC که در
 *      BSP_MEASUREMENT_VREF_MV مستند شده، به‌صورت محلی وصله شده است. فعلاً
 *      می‌ماند چون حذفش بدون اصلاح VREF باعث می‌شود ۱۲ولت ۱٫۲٪ کم بخواند،
 *      ولی نباید آن را «اصلاح مقسم» دانست و وصلهٔ تک‌کاناله جدید برای خطای
 *      مشترک مجاز نیست. */
#define CAL_BATTERY12_BENCH_STATIC_MV  150u   /* [EN] share of the GLOBAL VREF error, mV / سهم خطای سراسری VREF، mV */
#define CAL_BATTERY12_BENCH_PATH_MOHM   470u   /* [EN] charge-path resistance, mOhm / مقاومت مسیر شارژ، mOhm */
#endif

#endif /* CALIBRATION_H */
