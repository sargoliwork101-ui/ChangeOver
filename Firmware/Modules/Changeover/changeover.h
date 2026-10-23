/**
 * @file    changeover.h
 * @brief   [EN] Input vs battery path state machine - battery protect via BSP logical API only.
 *          [FA] ماشین حالت مسیر ورودی یا باتری - فقط با API منطقی BSP.
 *
 * @note    [EN] This module uses ONLY: snapshot.valid, snapshot.v_bat24_mv,
 *              snapshot.input_present, fault_mask, an internal low-battery latch
 *              and (MODULE_IMBALANCE) the func__Imbalance_GetOutputs() veto
 *              flag, plus (MODULE_CHARGER) func__Charger_DeadBlocksOutput()
 *              for the condemned-pack veto - both vetos assert the battery
 *              protect line and sit in SAFE. A recovered pack may reconnect
 *              with no mains after CHANGEOVER_PARAM_HEALTHY_HOLD_MS of
 *              unbroken healthy voltage, and owns the scenario-8 weak-battery
 *              verdict (ids 147..157) that guards it.
 *              Time conversion uses rtos_time.h only, tick=1ms
 *              assumption is forbidden.
 *              Only BSP_GPIO_PROTECT_BATTERY is allowed; PB5/PB7 are forbidden.
 *          [FA] این ماژول فقط از valid، v_bat24_mv، input_present، fault_mask،
 *              قفل داخلی باتری کم و (با ماژول عدم‌توازن) پرچم وتوی GetOutputs
 *              به‌علاوهٔ (با ماژول شارژر) وتوی باتری خرابِ DeadBlocksOutput
 *              استفاده می‌کند - هر دو وتو خط محافظ باتری را فعال و به SAFE
 *              می‌روند. باتریِ بازیابی‌شده هم می‌تواند بعد از یک پنجرهٔ سالمِ
 *              پیوسته بدون برق شهر وصل شود، با محافظ چرخه.
 */

#ifndef CHANGEOVER_H
#define CHANGEOVER_H

/* ==================== Includes ==================== */
#include "app_types.h"

/* ==================== Functions ==================== */

/**
 * @brief  [EN] Start protected in BOOT; wait for a valid, non-blocked sample
 *              at or above the existing 21200 mV reconnect threshold.
 *         [FA] در BOOT حفاظت فعال می‌ماند؛ تا نمونهٔ معتبر و بدون خطا با
 *              ولتاژ حداقل ۲۱۲۰۰ میلی‌ولت وصل نمی‌کند.
 */
void func__Changeover_Init(void);

/* ==================== Changeover_Evaluate Thresholds / آستانه‌های ارزیابی Changeover ==================== */

/**
 * @brief  [EN] Battery voltage below which the internal low-battery latch sets, in millivolts.
 *         Range 0..40000 mV; effect: v_bat24_mv < 21000 sets the latch; with the latch set for 3000 ms -> battery cut.
 *         [FA] ولتاژ باتری که پایین‌تر از آن قفل داخلی باتری کم بسته می‌شود، بر حسب میلی‌ولت.
 */
#define CHANGEOVER_BAT_LOW_ALARM_CUT_MV   21000u

/**
 * @brief  [EN] Battery voltage at or above which the internal low-battery latch clears, in millivolts.
 *         Range 0..40000 mV; effect: the latch set below 21000 mV stays set until v_bat24_mv >= 21200.
 *         v1.74: this window used to be UI link parameters 72/73; the user ordered it into
 *         Changeover as a fixed constant because this module is its only consumer and it must
 *         not be settable from the panel.
 *         [FA] ولتاژی که در آن یا بالاتر از آن قفلِ داخلیِ باتری کم باز می‌شود، بر حسب میلی‌ولت.
 *         این پنجره قبلاً پارامترهای ۷۲/۷۳ پنل بود و طبق دستور کاربر به‌صورت ثابت داخل همین
 *         ماژول آمد، چون تنها مصرف‌کننده‌اش همین‌جاست و نباید از پنل تنظیم شود.
 */
#define CHANGEOVER_BAT_LOW_ALARM_CLEAR_MV 21200u

/**
 * @brief  [EN] Battery voltage below which the independent (critical) cut triggers without waiting for the latch, in millivolts.
 *         Range 0..40000 mV; effect: v_bat24_mv < 20800 for 3000 ms -> battery cut.
 *         [FA] ولتاژ باتری که پایین‌تر از آن قطع بحرانی، بدون انتظار برای قفل، رخ می‌دهد، بر حسب میلی‌ولت.
 */
#define CHANGEOVER_BAT_CRITICAL_CUT_MV    20800u

/**
 * @brief  [EN] Battery voltage at or above which a reconnect is allowed, in millivolts.
 *         Range 0..40000 mV; effect: (input_present true with 30 s stable input, or a
 *         healthy hold of CHANGEOVER_PARAM_HEALTHY_HOLD_MS with mains absent) and
 *         v_bat24_mv >= 21200 for 3000 ms -> reconnect.
 *         [FA] ولتاژ باتری که در آن یا بالاتر از آن وصل مجدد مجاز است، بر حسب میلی‌ولت.
 */
#define CHANGEOVER_BAT_RECONNECT_MV       21200u

/**
 * @brief  [EN] Continuous duration required for cut or reconnect, in milliseconds.
 *         Range 1..60000 ms; effect: condition must be continuously true for this duration.
 *         [FA] مدت پیوسته مورد نیاز برای قطع یا وصل مجدد، بر حسب میلی‌ثانیه.
 */
#define CHANGEOVER_DURATION_MS            3000u

/**
 * @brief  [EN] Extra stable input time before reconnect allowed, in milliseconds.
 *         After a cut, input_present must stay true for this long in addition
 *         to the 3s voltage condition. Prevents immediate re-connect on a
 *         flickering input.
 *         [FA] زمان پایداری اضافه ورودی قبل از وصل مجدد، بر حسب میلی‌ثانیه.
 *         بعد از قطع، ورودی باید این مدت پایدار بماند علاوه بر شرط ۳ ثانیه ولتاژ.
 */
#define CHANGEOVER_INPUT_STABLE_MS        30000u

/* [EN] The healthy-hold window and the cycle-guard window are no longer
   compile-time constants: scenario 8 owns them as panel parameters
   (ids 150 and 149, defaults 10000 ms and 60000 ms) so a bench can tune the
   "several seconds in a stable state" test without a rebuild.
   [FA] پنجرهٔ ولتاژ سالم و پنجرهٔ محافظ چرخه دیگر ثابت کامپایل نیستند:
   سناریوی ۸ آن‌ها را به‌عنوان پارامتر پنل دارد (شناسه‌های ۱۵۰ و ۱۴۹،
   پیش‌فرض ۱۰۰۰۰ و ۶۰۰۰۰ میلی‌ثانیه) تا میز تست بتواند آزمون «چند ثانیه در
   وضعیت پایدار» را بدون بیلد دوباره تنظیم کند. */

/* ==================== Scenario 8: weak battery / سناریوی ۸: باتری ضعیف ==================== */

/* [EN] Scenario 8 (user order 2026-10-23): "the weak pack that empties fast and
 *      comes back when the load is removed must be detectable". The Changeover
 *      module is the only place that sees the mains-free reconnect and the cut
 *      that follows it, so the verdict lives here and the UI only draws it.
 *      A cut that happens inside CHANGEOVER_PARAM_CYCLE_GUARD_MS after a
 *      mains-free reconnect proves the pack cannot carry the load: the verdict
 *      latches, the battery path is held cut (id 157), red blink + beep are
 *      shown, and one retry is allowed after a quiet rest (id 148) with the
 *      pack resting above the reconnect threshold.
 *      Every id is independent, exactly like scenarios 5/6/7 - the LED shape
 *      and the beep shape never borrow from another scenario.
 *      [FA] سناریوی ۸ (دستور کاربر ۲۰۲۶-۱۰-۲۳): «باتری ضعیفی که زود خالی
 *      می‌شود و با برداشتن بار برمی‌گردد باید قابل تشخیص باشد». فقط همین ماژول
 *      وصلِ بدون برق شهر و قطعِ بعد از آن را می‌بیند، پس حکم اینجاست و UI فقط
 *      آن را نمایش می‌دهد. قطعی که داخل ‎CHANGEOVER_PARAM_CYCLE_GUARD_MS‎ بعد
 *      از وصلِ بدون برق شهر رخ دهد یعنی باتری بار را تحمل نمی‌کند: حکم قفل
 *      می‌شود، مسیر باتری بسته می‌ماند (شناسهٔ ۱۵۷)، قرمز چشمک‌زن + بوق نمایش
 *      داده می‌شود، و بعد از یک استراحتِ آرام (شناسهٔ ۱۴۸) با ولتاژ سالمِ پک
 *      یک بار تلاش مجدد مجاز است. هر شناسه مستقل است، دقیقاً مثل سناریوهای
 *      ۵/۶/۷ - شکل LED و شکل بوق از هیچ سناریوی دیگری قرض نمی‌گیرند. */

/** @brief [EN] 0/1, def 1: the scenario-8 verdict is enabled.
 *         [FA] ۰/۱، پیش‌فرض ۱: حکم سناریوی ۸ فعال است. */
#define CHANGEOVER_PARAM_WEAK_ENABLE          147u
/** @brief [EN] ms, def 600000 (10 min), 60000..3600000: quiet rest with a healthy pack before one retry.
 *         [FA] میلی‌ثانیه، پیش‌فرض ۶۰۰۰۰۰ (۱۰ دقیقه)، ۶۰۰۰۰..۳۶۰۰۰۰۰: استراحت آرام با پک سالم پیش از یک تلاش مجدد. */
#define CHANGEOVER_PARAM_WEAK_REST_MS         148u
/** @brief [EN] ms, def 60000, 1000..600000: a re-cut inside this window after a mains-free reconnect proves weakness.
 *         [FA] میلی‌ثانیه، پیش‌فرض ۶۰۰۰۰، ۱۰۰۰..۶۰۰۰۰۰: قطعِ دوباره داخل این پنجره بعد از وصل بدون برق شهر، ضعف را ثابت می‌کند. */
#define CHANGEOVER_PARAM_CYCLE_GUARD_MS       149u
/** @brief [EN] ms, def 10000, 1000..300000: unbroken healthy voltage needed before a mains-free reconnect.
 *         [FA] میلی‌ثانیه، پیش‌فرض ۱۰۰۰۰، ۱۰۰۰..۳۰۰۰۰۰: ولتاژ سالمِ بی‌وقفه لازم پیش از وصل بدون برق شهر. */
#define CHANGEOVER_PARAM_HEALTHY_HOLD_MS      150u
/** @brief [EN] ms, def 500, 0 = solid red else 100..10000: red blink period of scenario 8.
 *         [FA] میلی‌ثانیه، پیش‌فرض ۵۰۰، صفر = قرمز ثابت وگرنه ۱۰۰..۱۰۰۰۰: دورهٔ چشمک قرمز سناریوی ۸. */
#define CHANGEOVER_PARAM_WEAK_LED_PERIOD_MS   151u
/** @brief [EN] %, def 50, 5..95: red LED on-fraction of scenario 8.
 *         [FA] درصد، پیش‌فرض ۵۰، ۵..۹۵: سهم روشنی LED قرمز سناریوی ۸. */
#define CHANGEOVER_PARAM_WEAK_LED_DUTY_PCT    152u
/** @brief [EN] ms, def 3000, 0 = silent else 1000..600000: beep repeat period of scenario 8.
 *         [FA] میلی‌ثانیه، پیش‌فرض ۳۰۰۰، صفر = بی‌صدا وگرنه ۱۰۰۰..۶۰۰۰۰۰: دورهٔ تکرار بوق سناریوی ۸. */
#define CHANGEOVER_PARAM_WEAK_BEEP_PERIOD_MS  153u
/** @brief [EN] ms, def 200, 20..2000: length of one beep of scenario 8.
 *         [FA] میلی‌ثانیه، پیش‌فرض ۲۰۰، ۲۰..۲۰۰۰: طول یک بوق سناریوی ۸. */
#define CHANGEOVER_PARAM_WEAK_BEEP_LEN_MS     154u
/** @brief [EN] count, def 3, 0..10: beeps per pattern of scenario 8.
 *         [FA] تعداد، پیش‌فرض ۳، ۰..۱۰: تعداد بوق در هر الگوی سناریوی ۸. */
#define CHANGEOVER_PARAM_WEAK_BEEP_COUNT      155u
/** @brief [EN] ms, def 100, 0..5000: gap between beeps of scenario 8.
 *         [FA] میلی‌ثانیه، پیش‌فرض ۱۰۰، ۰..۵۰۰۰: گپ بین بوق‌های سناریوی ۸. */
#define CHANGEOVER_PARAM_WEAK_BEEP_GAP_MS     156u
/** @brief [EN] 0/1, def 1: while the verdict holds, keep the battery path cut (no load on a weak pack).
 *         [FA] ۰/۱، پیش‌فرض ۱: تا وقتی حکم برجاست مسیر باتری بسته بماند (بار روی باتری ضعیف نرود). */
#define CHANGEOVER_PARAM_WEAK_BLOCK_OUTPUT    157u

/** @brief [EN] mV, def 25000, 21200..29000: resting voltage the pack must hold before a
 *             mains-free reconnect for a re-cut to COUNT AS WEAKNESS. A pack that
 *             reconnects near-empty (rest 21.2..24.9 V) only proves it has no charge
 *             left, not that it is worn; a pack that rests at 25 V or more and still
 *             collapses under the load is the real scenario 8.
 *         [FA] میلی‌ولت، پیش‌فرض ۲۵۰۰۰، ۲۱۲۰۰..۲۹۰۰۰: ولتاژ استراحتی که پک باید پیش از
 *             وصلِ بدون برق شهر داشته باشد تا قطعِ دوباره «ضعف» حساب شود. پکی که
 *             نزدیک خالی (استراحت ۲۱٫۲..۲۴٫۹ ولت) وصل می‌شود فقط ثابت می‌کند شارژ
 *             ندارد، نه اینکه فرسوده است؛ پکی که ۲۵ ولت یا بیشتر استراحت می‌کند و باز
 *             زیر بار فرو می‌ریزد، همان سناریوی ۸ واقعی است. */
#define CHANGEOVER_PARAM_WEAK_REST_FLOOR_MV   165u

/* [EN] Retry budget (user decision 2026-10-23, second order): a mains-free
 *      reconnect that ends in a cut inside the cycle-guard window is one FAILED
 *      ATTEMPT. After three of them, with no mains in between, the battery path
 *      stays parked until the mains returns - an empty pack must not be put back
 *      on the load over and over.
 *      [FA] سقف تلاش (تصمیم کاربر ۲۰۲۶-۱۰-۲۳): وصلِ بدون برق شهری که داخل پنجرهٔ
 *      محافظ چرخه به قطع برسد یک «تلاش ناموفق» است. بعد از سه تلاش و بدون
 *      برگشت برق شهر، مسیر باتری تا برگشت برق شهر بسته می‌ماند - پک خالی نباید
 *      پشت سر هم روی بار گذاشته شود. */
#define CHANGEOVER_MAINS_FREE_RETRY_BUDGET    3u

/* [EN] Runtime command slot (not a user parameter): write 0 here through the
 *      link to drop the scenario-8 verdict right away, exactly like the
 *      scenario-5/6 reset keys. It is never persisted and never drawn as a
 *      parameter.
 *      [FA] اسلات فرمان زمان‌اجرا (پارامتر کاربر نیست): نوشتن صفر از راه لینک
 *      حکم سناریوی ۸ را فوری پاک می‌کند، دقیقاً مثل کلید ریست سناریو ۵/۶.
 *      نه ذخیره می‌شود و نه به‌عنوان پارامتر کشیده می‌شود.
 */
#define CHANGEOVER_SLOT_WEAK_RESET_ID         204u

/* [EN] 0xFF marks "not a scenario-8 parameter" inside the slot mapping.
 *      [FA] مقدار ۰xFF یعنی «شناسهٔ سناریوی ۸ نیست» در نگاشت جای جدول. */
#define CHANGEOVER_PARAM_INDEX_INVALID        0xFFu

#define CHANGEOVER_PARAM_FIRST_ID             CHANGEOVER_PARAM_WEAK_ENABLE
#define CHANGEOVER_PARAM_LAST_ID              CHANGEOVER_PARAM_WEAK_REST_FLOOR_MV
#define CHANGEOVER_PARAM_COUNT                12u

/* ==================== Scenario 8 defaults / پیش‌فرض‌های سناریوی ۸ ==================== */

#define CHANGEOVER_DEF_WEAK_ENABLE            1u
#define CHANGEOVER_DEF_WEAK_REST_MS           600000u
#define CHANGEOVER_DEF_CYCLE_GUARD_MS         60000u
#define CHANGEOVER_DEF_HEALTHY_HOLD_MS        10000u
#define CHANGEOVER_DEF_WEAK_LED_PERIOD_MS     500u
#define CHANGEOVER_DEF_WEAK_LED_DUTY_PCT      50u
#define CHANGEOVER_DEF_WEAK_BEEP_PERIOD_MS    3000u
#define CHANGEOVER_DEF_WEAK_BEEP_LEN_MS       200u
#define CHANGEOVER_DEF_WEAK_BEEP_COUNT        3u
#define CHANGEOVER_DEF_WEAK_BEEP_GAP_MS       100u
#define CHANGEOVER_DEF_WEAK_BLOCK_OUTPUT      1u
#define CHANGEOVER_DEF_WEAK_REST_FLOOR_MV     25000u

/* ==================== Scenario 8 live parameters / پارامترهای زندهٔ سناریوی ۸ ==================== */

/**
 * @brief  [EN] Write one scenario-8 parameter (147..157) through the clamp table
 *              and report the applied value.
 *         [FA] نوشتن یک پارامتر سناریوی ۸ (۱۴۷..۱۵۷) از مسیر گیره و گزارش
 *              مقدار اعمال‌شده.
 * @‎param  uint8_t__paramId [EN] 147..157‎ / شناسه
 * @param  uint32_t__value [EN] Requested value / مقدار درخواستی
 * @param  uint32_t__appliedValue [EN] Applied value out, NULL rejects / خروجی الزامی، NULL رد می‌کند
 * @return bool [EN] true when the id belongs to this module / شناسه متعلق به این ماژول بود
 */
bool func__Changeover_SetParam(uint8_t uint8_t__paramId,
                               uint32_t uint32_t__value,
                               uint32_t *uint32_t__appliedValue);

/**
 * @brief  [EN] Read one live scenario-8 parameter (147..157).
 *         [FA] خواندن یک پارامتر زندهٔ سناریوی ۸ (۱۴۷..۱۵۷).
 * @‎param  uint8_t__paramId [EN] 147..157‎ / شناسه
 * @param  uint32_t__value [EN] Value out / مقدار
 * @return bool [EN] true when the id belongs to this module / شناسه متعلق به این ماژول بود
 */
bool func__Changeover_GetParam(uint8_t uint8_t__paramId,
                               uint32_t *uint32_t__value);

/**
 * @brief  [EN] Is the scenario-8 weak-battery verdict latched right now?
 *         [FA] آیا حکم «باتری ضعیف» سناریوی ۸ همین حالا قفل است؟
 * @return bool [EN] true while latched / هنگام قفل‌بودن true
 */
bool func__Changeover_WeakVerdictActive(void);

/**
 * @brief  [EN] Is the mains-free reconnect parked because the retry budget is spent?
 *         [FA] آیا وصلِ بدون برق شهر به‌خاطر تمام‌شدن سقف تلاش پارک شده است؟
 * @return bool [EN] true while parked / هنگام پارک true
 */
bool func__Changeover_MainsFreeParked(void);

/**
 * @brief  [EN] Drop the scenario-8 verdict now (panel reset key, slot 204).
 *              The mains-free reconnect bookkeeping restarts too, so the
 *              single retry the user ordered becomes available again.
 *         [FA] حکم سناریوی ۸ را همین حالا پاک می‌کند (کلید ریست پنل، اسلات
 *              ۲۰۴). سابقهٔ وصلِ بدون برق شهر هم از نو شروع می‌شود تا همان
 *              تنها تلاش مجدد دوباره در دسترس باشد.
 * @return bool [EN] true when a verdict was actually latched / حکم واقعاً قفل بود
 */
bool func__Changeover_WeakVerdictReset(void);

/* ==================== Changeover_Evaluate / ارزیابی Changeover ==================== */

/**
 * @brief  [EN] Evaluate next system state from snapshot and faults and drive
 *              BSP_GPIO_PROTECT_BATTERY only. Uses rtos_time for 3000ms.
 *         [FA] حالت بعدی سیستم را از نمونه و خطا حساب و فقط پایه منطقی باتری را می‌زند.
 * @param  measurement_snapshot_t__snap [EN] Snapshot from Measurement, may be NULL / نمونه اندازه‌گیری
 * @param  fault_mask_t__faults [EN] Fault bits from Fault module / بیت‌های خطا
 * @return app_state_t [EN] Next system state / حالت بعدی
 */
app_state_t func__Changeover_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap, fault_mask_t fault_mask_t__faults);

#endif /* CHANGEOVER_H */
