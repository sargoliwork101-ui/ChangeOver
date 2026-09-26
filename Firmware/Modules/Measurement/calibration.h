/* ============================================================================
 * @file    calibration.h
 * ChangeOver calibration tables / جداول کالیبراسیون ChangeOver
 * ----------------------------------------------------------------------------
 * [EN] ONE file for every bench calibration table (user order 2026-09-25:
 *      "put the calibration tables in a separate file named after this
 *      calibration, next to the existing files, so we can amend them easily").
 *      THREE tables live here; missing points get appended later for higher
 *      accuracy - editing a table is a pure initializer edit.
 *      This header is included ONLY by measurement.c (the arrays are static;
 *      a second include would duplicate them harmlessly but pointlessly).
 * [FA] یک فایل برای همهٔ جدول‌های کالیبراسیون بنچ (دستور کاربر
 *      ۲۰۲۶-۰۹-۲۵: «جدول‌های کالیبراسیون داخل یک فایل جدا به اسم همین
 *      کالیبراسیون، کنار همون فایل‌ها، برای اصلاح راحت‌تر»). سه جدول همین‌جاست؛
 *      نقاطی که نداریم بعداً اضافه می‌شوند تا دقت بیشتر شود - ویرایش جدول فقط
 *      ویرایش مقداردهی است و هیچ چیز دیگری عوض نمی‌شود.
 *      این هدر فقط توسط measurement.c اینکلود می‌شود (آرایه‌ها static هستند).
 *
 * [EN] HOW TO EXTEND (all three tables):
 *      1. Append the new anchor to BOTH arrays (chain AND battery) - the two
 *         arrays MUST keep the same length; the host test enforces it.
 *      2. Keep each axis strictly increasing; the point count derives itself
 *         from the initializer (sizeof), nothing else to touch.
 *      3. Tables are keyed on MEASURED quantities (ADC chain current), never
 *         on duty - the same duty gives different currents as the battery
 *         fills (user order 2026-09-25).
 * [FA] راهنمای بزرگ‌کردن (هر سه جدول):
 *      ۱. لنگر جدید را به «هر دو» آرایه اضافه کن (زنجیره و باتری) - طول دو
 *         آرایه باید برابر بماند؛ تست هاست همین را قفل می‌کند.
 *      ۲. هر محور اکیداً صعودی بماند؛ تعداد نقاط از خود مقداردهی (sizeof)
 *         استخراج می‌شود و چیز دیگری دست نمی‌خورد.
 *      ۳. جدول‌ها روی کمیت «اندازه‌گیری‌شده» (جریان زنجیرهٔ ADC) بسته می‌شوند،
 *         نه دیوتی - همان دیوتی با پرشدن باتری جریان متفاوتی می‌دهد.
 * ============================================================================ */

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdint.h>

/* ============================================================================
 * TABLE 1 - channel-1 current LUT (chain mA -> battery-1 mA)
 * جدول ۱ - LUT جریان کانال ۱ (mA زنجیره ← mA باتری ۱)
 * ----------------------------------------------------------------------------
 * [EN] EMPTY until the SOLO1 bench run exists (dmm_i_bat1 was still blank in
 *      every run so far). Channel 1 keeps its linear gain conversion.
 *      WHEN THE DATA ARRIVES: fill the anchors below, set the enable to 1u,
 *      and mirror the channel-2 LUT path in measurement.c
 *      (func__Measurement_Current2BenchLut) for channel 1.
 * [FA] تا رسیدن دادهٔ ران SOLO1 خالی می‌ماند (dmm_i_bat1 تا حالا همیشه خالی
 *      بوده). کانال ۱ با تبدیل خطی گین خودش می‌ماند. وقتی داده آمد: لنگرها
 *      را پر کن، انیبل را 1u کن و مسیر LUT کانال ۲ در measurement.c را برای
 *      کانال ۱ هم قرینه کن.
 * ============================================================================ */
#define CAL_CURRENT1_LUT_ENABLE 0u

#if (CAL_CURRENT1_LUT_ENABLE != 0u)
static const uint32_t CAL_Current1LutChainMa[] =
    { 0u };
static const uint32_t CAL_Current1LutBatteryMa[] =
    { 0u };
#define CAL_CURRENT1_LUT_POINTS \
    ((uint32_t)(sizeof(CAL_Current1LutChainMa) / \
                sizeof(CAL_Current1LutChainMa[0u])))
#endif

/* ============================================================================
 * TABLE 2 - channel-2 LUT (chain mA -> battery-2 POWER mW)
 * جدول ۲ - LUT کانال ۲ (mA زنجیره ← توان باتری ۲ بر حسب mW)
 * ----------------------------------------------------------------------------
/* [EN] v1.13 (user order 2026-09-25, "voltages are fixed but the currents
 *      you read are wrong"): the table OUTPUT is the battery-2 POWER in mW,
 *      NOT the current. Physics: in DCM the mid-ON chain sample tracks the
 *      energy per cycle, which is battery-voltage independent, while the
 *      battery CURRENT is P/Vbat. The old chain->current table silently
 *      embedded the battery voltage of the calibration run (its battery rose
 *      12.0 -> 13.65 V), so it overread by roughly 7 percent per volt once
 *      the battery filled. measurement.c divides this table's output by the
 *      LIVE battery-2 terminal voltage (previous 1 ms pass, clamped
 *      8.0..15.0 V) to get the current.
 *      Anchors from the dense 2026-09-25T18:14 SOLO2 run (10 DMM points,
 *      duty 2..20% step 2, off2=8 / gain2=1303 - if those params change the
 *      table must be rebuilt): P = DMM_I2 x DMM_V2 at each point. The axis
 *      is still the ADC CHAIN CURRENT (raw - off2) x 0.8776 x gain2/1000,
 *      NEVER duty (user order 2026-09-25). Above the last anchor the last
 *      slope (12.37 mW per chain-mA) extends. The 2%-duty point measured a
 *      true battery current of -13 mA (discharge through the zener path) -
 *      power cannot go negative on this axis, so the table floors it to 0
 *      (error <= 13 mA only at the very bottom).
 * [FA] v1.13 (دستور کاربر ۲۰۲۶-۰۹-۲۵: «ولتاژها درست شد ولی جریان‌ها
 *      اشتباه»): خروجی جدول «توان باتری ۲» بر حسب mW است، نه جریان.
 *      فیزیک: در DCM نمونهٔ وسط-ON زنجیره انرژیِ هر سایکل را دنبال می‌کند
 *      که مستقل از ولتاژ باتری است، ولی «جریان» باتری = P/Vbat. جدول قدیمی
 *      جریان↔جریان ولتاژ باتریِ ران کالیبراسیون (۱۲٫۰→۱۳٫۶۵V) را در خود
 *      داشت و با پُر شدن باتری حدود ۷٪ به‌ازای هر ولت بیش‌خوانی می‌کرد.
 *      measurement.c خروجی این جدول را به ولتاژ زندهٔ ترمینال باتری ۲
 *      (پاس ۱ms قبل، گیرهٔ ۸..۱۵V) تقسیم می‌کند تا جریان به‌دست آید.
 *      لنگرها از اجرای متراکم 2026-09-25T18:14 (۱۰ نقطهٔ DMM، دیوتی
 *      ۲..۲۰٪ گام ۲، off2=8 / gain2=1303): P = جریان DMM × ولتاژ DMM در هر
 *      نقطه. محور همچنان «جریان زنجیرهٔ ADC» است، هرگز دیوتی. بالای
 *      آخرین لنگر شیب آخرین بازه (۱۲٫۳۷ mW به‌ازای هر mA زنجیره) ادامه
 *      می‌یابد. نقطهٔ دیوتی ۲٪ جریان واقعی 13−mA داشت (تخلیه زنر) -
 *      توان روی این محور منفی نمی‌شود و همان‌جا 0 می‌گیرد (خطا ≤ 13mA
 *      فقط در کف).
 * ============================================================================ */
#define CAL_CURRENT2_LUT_ENABLE 1u

#if (CAL_CURRENT2_LUT_ENABLE != 0u)
static const uint32_t CAL_Current2LutChainMa[] =
    { 0u, 5u, 37u, 106u, 189u, 236u, 283u, 353u, 441u, 557u, 707u };
static const uint32_t CAL_Current2LutBatteryMw[] =
    { 0u, 0u, 109u, 751u, 1581u, 2625u, 3807u, 5224u, 6817u, 8573u, 10429u };
#define CAL_CURRENT2_LUT_POINTS \
    ((uint32_t)(sizeof(CAL_Current2LutChainMa) / \
                sizeof(CAL_Current2LutChainMa[0u])))
#endif

/* ============================================================================
 * TABLE 3 - battery-2 (V12 / Vlow) voltage compensation
 * جدول ۳ - جبران ولتاژ باتری ۲ (V12 / Vlow)
 * ----------------------------------------------------------------------------
 * [EN] The board's V12 sense reads above the battery-2 terminals: a static
 *      divider error plus a current-proportional charge-path wire drop. The
 *      dense 2026-09-25T18:14 run (10 DMM points, 0..764 mA) gives the LSQ
 *      fit error = 150 mV + 0.47 ohm x I2 (149.8 mV + 472.5 mOhm, rounded;
 *      residual within +/-28 mV = 0.23 percent; the 20%-duty point rides a
 *      fast-rising battery, so its window average lags the submit-time DMM
 *      reading). measurement.c subtracts STATIC + I2 x PATH/1000 from V12
 *      AFTER the runtime V12 offset, using the post-LUT channel-2 current,
 *      BEFORE Vlow/Vhigh are derived. WHEN MORE POINTS ARRIVE this two-term
 *      model can grow into a full anchor table (chain mA -> drop mV) exactly
 *      like tables 1/2 - until then the two constants ARE the table.
 * [FA] سنس V12 برد بالاتر از ترمینال باتری ۲ می‌خواند: خطای ثابت مقسم +
 *      افت مسیر شارژ متناسب با جریان. اجرای متراکم 2026-09-25T18:14
 *      (۱۰ نقطهٔ DMM، 0..764mA) کمینهٔ مربعات را می‌دهد: خطا = 150mV +
 *      ۰٫۴۷ اهم × I2 (گردشدهٔ 149.8/472.5؛ خطای باقی‌مانده ±۲۸mV = ۰٫۲۳٪؛
 *      نقطهٔ دیوتی ۲۰٪ روی باتریِ سریع‌بالارونده است و میانگین پنجره‌اش از
 *      قرائت هم‌لحظهٔ ثبت DMM عقب می‌ماند). measurement.c این را بعد از آفست
 *      زمان اجرا و با جریان پس از جدول، قبل از مشتق‌گیری Vlow/Vhigh کم
 *      می‌کند. با آمدن نقاط بیشتر این مدل دو جمله‌ای می‌تواند مثل جدول‌های
 *      ۱ و ۲ به جدول انکری کامل (mA زنجیره ← mV افت) تبدیل شود - تا آن
 *      موقع همین دو ثابت خودِ جدول‌اند.
 * ============================================================================ */
#define CAL_BATTERY12_BENCH_COMP_ENABLE 1u

#if (CAL_BATTERY12_BENCH_COMP_ENABLE != 0u)
#define CAL_BATTERY12_BENCH_STATIC_MV  150u   /* [EN] static divider error, mV / خطای ثابت مقسم، mV */
#define CAL_BATTERY12_BENCH_PATH_MOHM   470u   /* [EN] charge-path resistance, mOhm / مقاومت مسیر شارژ، mOhm */
#endif

#endif /* CALIBRATION_H */
