/**
 * @file    charger.c
 * @brief   [EN] Generic per-channel 12 V charger control. CHG_MASTER_ENABLE is
 *          the single master switch; when 0 the module stays in safe-idle
 *          (all PWM stopped, NC relay closed, JIT/relay policy inactive).
 *          CHG_TRANSFORMER_KNOWN is not bypassable; when 0 only the
 *          explicit limited bring-up test mode is allowed. The channels
 *          share implementation only - voltage, current, duty, state,
 *          retry counter and JIT sequence are per-channel.
 *          [FA] کنترل عمومی شارژرهای مستقل ۱۲ ولت. CHG_MASTER_ENABLE تنها
 *          کلید اصلی است؛ با ۰ ماژول در safe-idle می‌ماند (PWM متوقف، رلهٔ
 *          NC بسته، سیاست JIT/رله غیرفعال). CHG_TRANSFORMER_KNOWN bypass
 *          نمی‌شود؛ با ۰ فقط مود محدود bring-up مجاز است. فقط پیاده‌سازی
 *          مشترک است؛ ولتاژ/جریان/duty/state/retry/JIT هر کانال جداست.
 */

/* ==================== Includes / شامل‌ها ==================== */
#include "charger.h"
#include "app_config.h"
#include "bsp_gpio.h"
#include "bsp_pwm.h"
#include "cmsis_os2.h"
#include "modules_enable.h"
#include "rtos_time.h"

#include <stddef.h>

#if MODULE_JITTER
#include "jitter.h"
#endif

#if MODULE_FAULT
#include "fault.h"
#endif

/* ==================== Local types / نوع‌های داخلی ==================== */

typedef enum
{
    CHG_STATE_OFF = 0,
    CHG_STATE_BULK,
    CHG_STATE_ABSORB,
    CHG_STATE_FLOAT,
    CHG_STATE_BRINGUP,
    CHG_STATE_JIT_RETRY_WAIT,
    CHG_STATE_INPUT_WAIT,
    CHG_STATE_FINAL_FAULT,
    CHG_STATE_BAT_LOST,  /* [EN] battery wire cut during charge (fault bit latched) / باتری حین شارژ قطع شده */
    CHG_STATE_MANUAL     /* [EN] manual test mode owns this channel: duty driven from the panel, battery gates bypassed / مود تست دستی مالک کانال: duty از پنل، گیت‌های باتری رد شده‌اند */
} charger_state_t;

typedef struct
{
    bool bool__installed;
    uint16_t uint16_t__dutyPermille;
    uint16_t uint16_t__dutyBeforeTripPermille;
    uint8_t uint8_t__jitTripCount;
    charger_state_t charger_state_t__state;
    uint32_t uint32_t__absorbAccumTicks; /* [EN] accumulated time inside the 14.4..14.5 V window, ticks / زمان جمع‌شده داخل پنجره ۱۴٫۴ تا ۱۴٫۵ ولت */
    uint32_t uint32_t__absorbLastTick;   /* [EN] previous-pass tick while in ABSORB, for the accumulation delta / تیک پاس قبلی در ابزورب برای دلتای جمع */
    uint32_t uint32_t__retryDeadlineTick;
    uint32_t uint32_t__lastDutyStepTick;
    uint32_t uint32_t__stableFromTick; /* [EN] tick when installed+input+battery first looked valid; 0 = not present, gates bulk start (wire id CHG_LIMIT_PARAM_CONNECT_SETTLE_MS) / تیک اولین‌لحظه‌ای که اتصال معتبر دیده شد؛ صفر = باتری حاضر نیست؛ گیت شروع بالک */
    uint32_t uint32_t__taperSinceTick; /* [EN] first tick in this ABSORB episode that the tail current looked below CHG_TAPER_CURRENT_MA; 0 = not tapering / تیک اولین زیرجریان در ابزورب؛ صفر یعنی زیرجریان نیست */
    uint32_t uint32_t__absorbEnterTick; /* [EN] tick this ABSORB episode started; 0 = not in absorb; feeds the CHG_LIM(CHG_LIMIT_PARAM_ABSORB_MAX_MS) ceiling / تیک ورود به این ابزورب؛ صفر یعنی خارج؛ برای سقف یک‌ساعت */
    /* [EN] Tick the one-hour ceiling was ARMED, i.e. the first time the tail
       current fell below CHG_LIM(CHG_LIMIT_PARAM_ABSORB_MAX_ARM_MA) in this episode; 0 = not yet
       armed, and while it is 0 the ceiling does not count at all.
       [FA] تیکِ مسلح‌شدن سقف یک‌ساعته، یعنی اولین باری که جریان دنباله در این
       اپیزود زیر آستانه رفت؛ صفر یعنی هنوز مسلح نشده و تا وقتی صفر است سقف
       اصلاً نمی‌شمارد. */
    uint32_t uint32_t__absorbMaxArmTick;
    /* [EN] Two-loop CC/CV PID state (v1.22), per channel. The integral is in
       milli-permille (CHG_PID_DUTY_SCALE) and its remainder keeps the
       sub-unit fraction so a 0.01 permille/s creep survives integer math.
       pidDutyMilli holds the duty LAST APPLIED (not the fine demand), which
       is what the bumpless re-seed guard compares against the hardware.
       error/branch feed the derivative and reset it when the CC/CV winner
       changes.
       [FA] وضعیت PID دوحلقه‌ای (v1.22) برای هر کانال: انتگرال بر حسب
       میلی‌پرمیل است و باقی‌مانده‌اش کسر زیرواحدی را نگه می‌دارد تا خزش
       ۰٫۰۱ پرمیل بر ثانیه در ریاضی صحیح گم نشود. pidDutyMilli دیوتی
       «آخرین‌بار اعمال‌شده» را نگه می‌دارد نه تقاضای ریز را، و همین چیزی
       است که نگهبان بذرگیری بدون پرش با سخت‌افزار مقایسه می‌کند.
       خطا/شاخه مشتق را تغذیه و با عوض‌شدن برندهٔ CC/CV ریستش می‌کنند. */
    uint32_t uint32_t__pidDutyMilli;    /* [EN] last APPLIED duty, milli-permille / دیوتی آخرین‌بار اعمال‌شده */
    uint32_t uint32_t__pidVoltFilt;     /* [EN] N x filtered pack mV (see wire id CHG_LIMIT_PARAM_PID_VOLT_FILTER_N) / ولتاژ فیلترشدهٔ ضرب در N */
    int32_t  int32_t__pidIntegral;      /* [EN] integral term, milli-permille / جملهٔ انتگرالی */
    int32_t  int32_t__pidIntegralRem;   /* [EN] integral sub-unit remainder / باقی‌ماندهٔ انتگرال */
    int32_t  int32_t__pidLastError;     /* [EN] previous error of the winning branch / خطای قبلی شاخهٔ برنده */
    uint32_t uint32_t__pidLastTick;     /* [EN] tick of the last PID update / تیک آخرین به‌روزرسانی */
    bool     bool__pidSeeded;           /* [EN] false = re-seed from hardware duty / نادرست = بذرگیری دوباره */
    bool     bool__pidLastWasVoltage;   /* [EN] winning branch last update / شاخهٔ برندهٔ قبلی */
} charger_channel_state_t;

/* ==================== Static state / وضعیت داخلی ==================== */

static charger_channel_state_t CHARGER_CHANNEL_T__G__State[2];

/* [EN] Runtime charge profile, shared by both channels (user order
 *      2026-09-25: settable from the ESP panel tab, wire ids 20..26). Boot
 *      defaults = the old compile-time setpoints; flash-persisted since
 *      v1.14 like every other parameter. func__Charger_ClampProfile()
 *      keeps the set consistent.
 * [FA] پروفایل شارژ زمان اجرا، مشترک بین هر دو کانال (دستور کاربر
 *      ۲۰۲۶-۰۹-۲۵: تنظیم از تب پنل، شناسه‌های سیمی ۲۰..۲۶). پیش‌فرض بوت =
 *      ست‌پوینت‌های کامپایل‌تایم قبلی؛ مثل بقیهٔ پارامترها از نسخهٔ ۱.۱۴ روی فلش می‌ماند.
 *      func__Charger_ClampProfile() مجموعه را سازنده نگه می‌دارد. */
typedef struct
{
    uint32_t uint32_t__absorbMv;        /* [EN] absorb hold setpoint / تثبیت ابزورب */
    uint32_t uint32_t__absorbEnterMv;   /* [EN] absorb entry threshold / آستانهٔ ورود ابزورب */
    uint32_t uint32_t__absorbOverMv;    /* [EN] coarse down-step ceiling / سقف کاهش سریع */
    uint32_t uint32_t__floatMv;         /* [EN] float hold setpoint / تثبیت شناور */
    uint32_t uint32_t__reentryMv;       /* [EN] float->bulk reentry / بازگشت شناور به بالک */
    uint32_t uint32_t__bulkCurrentMaxMa;/* [EN] current-regulation top of band / سقف باند جریان */
    uint32_t uint32_t__taperCurrentMa;  /* [EN] float-entry tail current / زیرجریان ورود به شناور */
} charger_profile_t;

/* [EN] volatile: written by the EspLink task, read by the control task
   (full-program audit 2026-09-26). [FA] بین دو تسک بدون قفل پس volatile. */
static volatile charger_profile_t CHARGER_PROFILE_T__G__Profile =
{
    CHG_ABSORB_MV, CHG_ABSORB_ENTER_MV, CHG_ABSORB_OVER_MV,
    CHG_FLOAT_MV, CHG_REENTRY_MV, CHG_BULK_CURRENT_MAX_MA, CHG_TAPER_CURRENT_MA
};

/* [EN] Two-loop CC/CV PID settings (v1.22, user order 2026-09-28), shared by
 *      both channels, wire ids 83..92. The field ORDER is the wire-id order
 *      (asserts near the setter): enable, then three complete stage rows of
 *      Kp,Ki,Kd,up-rate,down-rate. Stage 1 is the CURRENT loop, stages 2
 *      and 3 are the VOLTAGE loop below / at the setpoint (see the
 *      regulator block in charger.h). Boot defaults = the CHG_PID_* macros;
 *      flash-persisted like every other parameter.
 * [FA] تنظیمات PID دوحلقه‌ای (v1.22، دستور کاربر ۲۰۲۶-۰۹-۲۸)، مشترک بین
 *      دو کانال، شناسه‌های سیمی ۸۳..۹۲. ترتیب فیلدها همان ترتیب شناسه‌هاست
 *      (اثبات‌های کنار ستر): فعال‌سازی، سپس سه ردیف کامل مرحله شامل Kp و Ki
 *      و Kd و نرخ صعود و نرخ نزول. مرحلهٔ ۱ حلقهٔ جریان است و مرحله‌های ۲ و ۳
 *      حلقهٔ ولتاژ زیر ست‌پوینت و روی ست‌پوینت (بلوک تنظیم‌کننده در
 *      charger.h). پیش‌فرض بوت = ماکروهای CHG_PID_*؛ مثل بقیهٔ پارامترها روی
 *      فلش می‌ماند. */
typedef struct
{
    uint32_t uint32_t__currentKp;       /* [EN] id 83, CC loop / حلقهٔ جریان */
    uint32_t uint32_t__currentKi;       /* [EN] id 84 */
    uint32_t uint32_t__currentKd;       /* [EN] id 85 */
    uint32_t uint32_t__currentUpRate;   /* [EN] id 86, milli-permille/s */
    uint32_t uint32_t__currentDownRate; /* [EN] id 87, milli-permille/s */
    uint32_t uint32_t__voltageKp;       /* [EN] id 88, CV loop / حلقهٔ ولتاژ */
    uint32_t uint32_t__voltageKi;       /* [EN] id 89 */
    uint32_t uint32_t__voltageKd;       /* [EN] id 90 */
    uint32_t uint32_t__voltageUpRate;   /* [EN] id 91, milli-permille/s */
    uint32_t uint32_t__voltageDownRate; /* [EN] id 92, milli-permille/s */
} charger_pid_t;

/* [EN] volatile for the same cross-task reason as the profile: the EspLink
   task writes, the control task reads. [FA] همان دلیل بین‌تسکی پروفایل. */
static volatile charger_pid_t CHARGER_PID_T__G__Pid =
{
    CHG_PID_CURRENT_KP,
    CHG_PID_CURRENT_KI,
    CHG_PID_CURRENT_KD,
    CHG_PID_CURRENT_UP_RATE,
    CHG_PID_CURRENT_DOWN_RATE,
    CHG_PID_VOLTAGE_KP,
    CHG_PID_VOLTAGE_KI,
    CHG_PID_VOLTAGE_KD,
    CHG_PID_VOLTAGE_UP_RATE,
    CHG_PID_VOLTAGE_DOWN_RATE
};

/* [EN] Runtime charger alarms (v1.15, wire ids 35..37, alarms tab). Boot
 *      defaults equal the old compile-time ceilings; Set clamps DOWN-ONLY
 *      (never above the compile maxima) and func__Charger_ClampAlarms()
 *      re-asserts clearance above the live profile band.
 * [FA] آلارم‌های زمان‌اجرا شارژر (v1.15، شناسه‌های ۳۵..۳۷، تب آلارم‌ها).
 *      پیش‌فرض بوت همان سقف‌های کامپایل‌تایم قبلی است؛ Set فقط پایین می‌برد
 *      (هرگز بالای سقف کامپایل) و ClampAlarms فاصله بالای باند زنده پروفایل را بازمی‌گرداند. */
/* [EN] Cross-task alarm ceilings (written by the comm task, read by the
   control task): volatile for visibility; the multi-field tear is closed
   by the scheduler lock in func__Charger_SetAlarmParam.
   [FA] سقف‌های آلارم بین‌تسکی: volatile برای دیده‌شدن؛ پارگی چندفیلدی با
   قفل زمان‌بند در ستر بسته می‌شود. */
static volatile uint32_t UINT32_T__G__ChargerHardFaultMa =
    CHG_CURRENT_HARD_FAULT_DEFAULT_MA;
static volatile uint32_t UINT32_T__G__ChargerOvCutoffMv = CHG_OV_CUTOFF_DEFAULT_MV;
static volatile uint32_t UINT32_T__G__ChargerValidFloorMv = CHG_MIN_VALID_BATTERY_MV;

/* [EN] NVM-save suspension flag (user order 2026-09-27): set by the comm
   task around a flash save, read by the control task. Single volatile
   bool, no lock needed.
   [FA] پرچم تعلیق برای ذخیرهٔ NVM: تسک ارتباط دور ذخیرهٔ فلش ست می‌کند و
   تسک کنترل می‌خواند. تک‌بولین volatile بدون قفل. */
static volatile bool BOOL__G__ChargerSuspended = false;

/* [EN] Live diag array - see the layout map in charger.h (user order
 *      2026-09-22: all charge-decision values visible in one Live
 *      Expressions entry).
 * [FA] آرایهٔ دیاگ زنده - نقشهٔ چیدمان در charger.h (دستور کاربر
 *      ۲۰۲۶-۰۹-۲۲: همهٔ مقادیر تصمیم شارژ در یک ورودی Live Expressions). */
volatile uint32_t UINT32_T__G__ChargerDiag[CHG_DIAG_COUNT] = {0u};

/* [EN] Calibration worksheet array - see the layout map in charger.h (user
 *      order 2026-09-22: one Live Expressions entry for bench calibration).
 * [FA] آرایهٔ برگهٔ کالیبراسیون - نقشهٔ چیدمان در charger.h (دستور کاربر
 *      ۲۰۲۶-۰۹-۲۲: یک ورودی Live Expressions برای کالیبراسیون بنچ). */
volatile uint32_t UINT32_T__G__ChargerCalib[CHG_CALIB_COUNT] = {0u};

/* [EN] Live per-channel output-current estimates (the decision values) -
 *      see charger.h. ch1 = upper/VHIGH, ch2 = lower/VLOW.
 * [FA] جریان‌های خروجی تخمینی زنده per channel (مقادیر تصمیم) - توضیح در
 *      charger.h. کانال ۱ = بالا/VHIGH، کانال ۲ = پایین/VLOW. */
volatile uint32_t UINT32_T__G__ChargerIest1Ma = 0u;
volatile uint32_t UINT32_T__G__ChargerIest2Ma = 0u;

/* [EN] Runtime per-channel conversion factors ETA1/ETA2 (protocol v1.3,
 *      user order 2026-09-24). 0 (compiled default) = identity: with the
 *      battery-calibrated gains the filtered reading already equals the
 *      battery current. Non-zero = live conversion iest = I x Vin x eta /
 *      (1000 x Vbat), set via ESP params 9/10 or computed by the panel
 *      CAL_REFERENCE command. Flash-persisted since v1.14 (NVM ids 9/10);
 *      written by the EspLink task, read in the control task; aligned
 *      32-bit values are atomic on Cortex-M3.
 * [FA] ضریب تبدیل زمان اجرای هر کانال (پروتکل v1.3، دستور ۲۰۲۶-۰۹-۲۴).
 *      صفر (پیش‌فرض) = همانی: با گین‌های کالیبره-باتری عدد فیلترشده خودش
 *      جریان باتری است. غیرصفر = تبدیل زندهٔ iest = I × Vin × η ÷ (۱۰۰۰ ×
 *      Vbat)؛ ست از پارامتر ۹/۱۰ یا فرمان CAL_REFERENCE. روی فلش از v1.14
 *      (شناسه ۹/۱۰)؛ نوشتن از تسک EspLink، خواندن در کنترل؛ u32 تراز اتمیک.
 */
static volatile uint32_t UINT32_T__G__ChargerEta1Permille =
    CHG_FLYBACK_ETA1_PERMILLE;
static volatile uint32_t UINT32_T__G__ChargerEta2Permille =
    CHG_FLYBACK_ETA2_PERMILLE;

/* [EN] Runtime per-channel enable gates for the ESP command panel (user
 *      order 2026-09-22: the ESP must be able to cut and reconnect each
 *      charger module). Default true = today's behavior. While false the
 *      channel's PWM is forced off and the state is held at OFF; a faulted
 *      channel (FINAL_FAULT) is never released by this gate.
 *      Flash-persisted since v1.14 (NVM ids 11/12).
 * [FA] گیت‌های فعال‌سازی هر کانال برای پنل فرمان ESP (دستور کاربر
 *      ۲۰۲۶-۰۹-۲۲: ESP باید بتواند هر ماژول شارژر را قطع/وصل کند).
 *      پیش‌فرض true = رفتار فعلی. تا وقتی false است PWM آن کانال قطع و
 *      وضعیت روی OFF نگه داشته می‌شود؛ کانال FINAL_FAULT هرگز با این
 *      گیت آزاد نمی‌شود. روی فلش می‌ماند از نسخهٔ ۱.۱۴ (شناسه‌های ۱۱/۱۲). */
static volatile bool BOOL__G__ChargerEspEnableCh[2] = {true, true};

/* [EN] Runtime per-channel PWM duty ceiling (user order 2026-09-22: the
 *      ESP panel sets the cap). Default CHG_DUTY_MAX_PERMILLE = today's
 *      behavior; ApplyDuty clamps EVERY requested duty (ramp, regulation,
 *      fixed mode) to min(compile max, this ceiling). Flash-persisted
 *      since v1.14 (NVM ids 13/14).
 * [FA] سقف duty ی PWM هر کانال در زمان اجرا (دستور کاربر ۲۰۲۶-۰۹-۲۲:
 *      پنل ESP سقف را تعیین می‌کند). پیش‌فرض CHG_DUTY_MAX_PERMILLE همان
 *      رفتار فعلی؛ ApplyDuty هر duty درخواستی (رمپ، تنظیم، مود فیکس)
 *      را به min(سقف کامپایل، این سقف) گیره می‌زند. روی فلش می‌ماند از
 *      نسخهٔ ۱.۱۴ (شناسه‌های ۱۳/۱۴). */
static volatile uint32_t UINT32_T__G__ChargerDutyCeilingPermille[2] =
    {CHG_DUTY_MAX_PERMILLE, CHG_DUTY_MAX_PERMILLE};

/* [EN] Runtime fixed-duty mode per channel (user order 2026-09-22: hold
 *      the PWM at one chosen number instead of the regulation loop) - with
 *      EXACTLY the safety wrapper of the compile-time bench-test mode:
 *      switching stops above CHG_ABSORB_MV, all JIT/input/battery/ESP-cut
 *      gates stay active. Default off. RAM only.
 * [FA] مود duty فیکس هر کانال (دستور ۲۰۲۶-۰۹-۲۲): اعمال duty ثابت به‌جای
 *      حلقهٔ تنظیم، با همان پوشش امنیتی مود بنچ کامپایل‌تایم: بالای
 *      CHG_ABSORB_MV توقف سوئیچینگ و همهٔ گیت‌های JIT/ورودی/باتری/ESP
 *      فعال. پیش‌فرض خاموش. فقط RAM. */
static volatile bool BOOL__G__ChargerDutyFixedEnable[2] = {false, false};
static volatile uint32_t UINT32_T__G__ChargerDutyFixedPermille[2] = {0u, 0u};

/* [EN] Manual test mode state (user order 2026-09-23, protocol v1.2
 *      param 19). Requested is written by the ESP link task; Active is
 *      owned by the charger task and flips in func__Charger_Evaluate
 *      (every enter/exit action runs in the charger context). The link
 *      stamp feeds the CHG_MANUAL_WATCHDOG_MS dead-man, refreshed by every
 *      valid ESP frame. RearmRequest = manual JIT re-arm: a fresh duty
 *      write while parked re-arms the channel (write in the ESP task,
 *      consumed by the charger task).
 * [FA] وضعیت مود تست دستی (دستور ۲۰۲۶-۰۹-۲۳، پارامتر ۱۹ v1.2). Requested
 *      را تسک ESP می‌نویسد؛ Active مالکش تسک شارژر است و در Evaluate
 *      برمی‌گردد (همهٔ عملیات ورود/خروج در زمینهٔ شارژر). مهر لینک ددمنِ
 *      CHG_MANUAL_WATCHDOG_MS را غذا می‌دهد؛ RearmRequest یعنی re-arm دستی
 *      JIT: نوشتن duty در حالت پارک کانال را مسلح می‌کند. */
static volatile bool BOOL__G__ChargerManualModeRequested = false;
static volatile bool BOOL__G__ChargerManualModeActive = false;
static volatile uint32_t UINT32_T__G__ManualLastLinkTick = 0u;
static volatile bool BOOL__G__ChargerManualRearmRequest[2] = {false, false};

static bool BOOL__G__ChargerInitialized;
static bool BOOL__G__RelayOpen;
static uint32_t UINT32_T__G__RelaySettleDeadline;

#define CHG_NO_CHANNEL 0xFFu
static uint8_t UINT8_T__G__RetryChannel;

/* ==================== Forward declarations / اعلان پیش‌موضع ==================== */
static bsp_pwm_channel_t func__Charger_PwmChannel(uint8_t uint8_t__channelIndex);
static void func__Charger_ClearAbsorbWindow(
    charger_channel_state_t *charger_channel_state_t__channel);
static void func__Charger_EnterManualTestMode(uint32_t uint32_t__nowTick);
static void func__Charger_ExitManualTestMode(void);
static void func__Charger_ManualDriveChannel(uint8_t uint8_t__channelIndex,
                                             const measurement_snapshot_t *measurement_snapshot_t__snap);
static void func__Charger_PidInvalidate(
    charger_channel_state_t *charger_channel_state_t__channel);
static uint32_t func__Charger_Limit(uint8_t uint8_t__index);

/* [EN] Read a panel-settable limit by its WIRE ID (ids 93..107, user order
 *      2026-10-03). Spelling the wire id at every use site keeps the call
 *      self-describing and means the table index exists in exactly one
 *      place - this file previously drifted by carrying the same number in
 *      several spots.
 * [FA] خواندن یک حدِ تنظیم‌شدنی از پنل با «شناسهٔ سیمی» (۹۳..۱۰۷، دستور
 *      کاربر). نوشتن شناسه در محل استفاده، فراخوانی را خودتوضیح نگه می‌دارد
 *      و نمایهٔ جدول فقط در یک نقطه وجود دارد - این فایل قبلاً از حمل یک عدد
 *      در چند جا دچار واگرایی شده بود. */
#define CHG_LIM(id) \
    func__Charger_Limit((uint8_t)((id) - CHG_LIMIT_PARAM_FIRST_ID))

/* ==================== Safe hardware policy / سیاست سخت‌افزاری امن ==================== */

/**
 * @brief  [EN] Keep every PWM output stopped, clear all per-channel duty, and
 *              keep the NC relay closed (coil off). This is the safe-idle
 *              state for master-disabled, low-input, transformer-unknown (no
 *              bring-up enabled) and non-final non-charging conditions. With
 *              CHG_MASTER_ENABLE=0 JIT/relay disconnect policy is inactive,
 *              so the NC contact is closed and no coil is energized.
 *         [FA] همه خروجی‌های PWM را متوقف می‌کند، duty هر کانال را صفر و رله
 *              NC را بسته (coil خاموش) نگه می‌دارد. این وضعیت safe-idle برای
 *              master غیرفعال، ورودی کم، ترانس ناشناخته (بدون bring-up فعال)
 *              و شرایط غیرنهایی بدون شارژ است. با CHG_MASTER_ENABLE=0 سیاست
 *              JIT/رله قطع‌کننده غیرفعال است، بنابراین کنتاکت NC بسته و کویل
 *              بدون انرژی است.
 */
/**
 * @brief  [EN] Clear the absorb-window bookkeeping of one channel (soak
 *              counter, taper stamp and both window stamps). Every restart
 *              or re-entry path calls this so the 10-minute soak always
 *              starts from zero. The entry-into-ABSORB path stamps
 *              absorbEnter/Last with "now" instead and stays inline.
 *         [FA] پاک‌کردن دفترچهٔ پنجرهٔ ابزورب یک کانال (شمارندهٔ شستشو، مهر
 *              تیپر و دو مهر پنجره). هر مسیر ری‌استارت/ورود مجدد این را
 *              صدا می‌زند تا شستشوی ۱۰ دقیقه همیشه از صفر شروع شود؛ مسیر
 *              ورود به ABSORB به‌جای صفر، مهر «اکنون» می‌زند و جدا می‌ماند.
 * @param  charger_channel_state_t__channel [EN] channel to clear / کانال هدف
 */
static void func__Charger_ClearAbsorbWindow(
    charger_channel_state_t *charger_channel_state_t__channel)
{
    charger_channel_state_t__channel->uint32_t__absorbAccumTicks = 0u;
    charger_channel_state_t__channel->uint32_t__taperSinceTick = 0u;
    charger_channel_state_t__channel->uint32_t__absorbEnterTick = 0u;
    charger_channel_state_t__channel->uint32_t__absorbMaxArmTick = 0u;
    charger_channel_state_t__channel->uint32_t__absorbLastTick = 0u;
}

/* [EN] Dip reset (v1.17b): like ClearAbsorbWindow EXCEPT the 1 h safety
 *      clock (absorbEnterTick) keeps running across dips below 14.3 V -
 *      the soak + taper restart (user directive) but the ceiling is now a
 *      wall clock from the FIRST absorb entry, so hunting around 14.3 V
 *      can no longer postpone FLOAT (and the end of the yellow blink)
 *      forever. Fresh cycles (reentry/OFF/JIT/init) still use the full
 *      ClearAbsorbWindow above.
 * [FA] ریست افت لحظه‌ای: مثل پاک‌کردن پنجره ابزورب ولی ساعت ۱ساعته نگه
 *      داشته می‌شود تا شکار دور ۱۴٫۳V نتواند FLOAT (و پایان چشمک زرد) را
 *      تا ابد عقب بیندازد؛ سیکل تازه هنوز ریست کامل می‌خواهد. */
static void func__Charger_DipResetAbsorbWindow(
    charger_channel_state_t *charger_channel_state_t__channel)
{
    charger_channel_state_t__channel->uint32_t__absorbAccumTicks = 0u;
    charger_channel_state_t__channel->uint32_t__taperSinceTick = 0u;
    charger_channel_state_t__channel->uint32_t__absorbLastTick = 0u;
}

static void func__Charger_SafeIdle(void)
{
    uint8_t uint8_t__channelIndex;

    func__BspPwm_StopAll();
    func__BspGpio_Write(BSP_GPIO_RELAY, false);
    BOOL__G__RelayOpen = false;
    UINT32_T__G__RelaySettleDeadline = 0u;
    UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyBeforeTripPermille = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__retryDeadlineTick = 0u;
        /* [EN] FINAL_FAULT and BAT_LOST survive SafeIdle so their latches and
           recovery logic are not wiped by fault-idle passes.
           [FA] حالت‌های قفل (خطای نهایی و قطع باتری) با SafeIdle پاک نمی‌شوند. */
        if ((CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state !=
             CHG_STATE_FINAL_FAULT) &&
            (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state !=
             CHG_STATE_BAT_LOST))
        {
            CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
                CHG_STATE_OFF;
            CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint8_t__jitTripCount = 0u;
            func__Charger_ClearAbsorbWindow(
                &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex]);
        }
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
    }
}

/**
 * @brief  [EN] Final fault disconnect: after the third same-channel JIT, both
 *              PWM outputs are already stopped; then the NC relay opens so
 *              the transformer input is physically removed.
 *         [FA] قطع نهایی fault: بعد از سومین JIT همان کانال، هر دو PWM قبلاً
 *              متوقف شده‌اند؛ سپس رله NC باز می‌شود تا ورودی ترانس واقعاً قطع
 *              شود.
 */
static void func__Charger_FinalDisconnect(void)
{
    uint8_t uint8_t__channelIndex;

    func__BspPwm_StopAll();
    func__BspGpio_Write(BSP_GPIO_RELAY, true);
    BOOL__G__RelayOpen = true;
    UINT32_T__G__RelaySettleDeadline = 0u;

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
    }
}

/**
 * @brief  [EN] Final-fault standby while the input is cut: keep the fault
 *              latched (BOOL__G__RelayOpen stays true) but release the relay
 *              coil. With the transformer input physically absent, energizing
 *              the NC relay disconnects nothing and only drains the battery
 *              (coil ~40 mA, plus ~7 mA board base = the observed ~50 mA).
 *              The coil re-energizes automatically through FinalDisconnect()
 *              the moment the input returns and the latch is still set.
 *         [FA] standby خطای نهایی وقتی ورودی قطع است: قفل خطا باقی می‌ماند
 *              ولی کویل رله رها می‌شود تا باتری را خالی نکند (~۵۰mA). با
 *              برگشت ورودی، کویل دوباره به‌صورت خودکار وصل می‌شود.
 */
static void func__Charger_FinalDisconnectIdle(void)
{
    uint8_t uint8_t__channelIndex;

    func__BspPwm_StopAll();
    func__BspGpio_Write(BSP_GPIO_RELAY, false);
    UINT32_T__G__RelaySettleDeadline = 0u;
    /* [EN] BOOL__G__RelayOpen intentionally NOT cleared: the final fault
       stays latched. / [FA] قفل خطا دست‌نخورده می‌ماند. */

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
    }
}

/* ==================== Channel helpers / توابع کمکی کانال ==================== */

static bool func__Charger_IsChannelInstalled(uint8_t uint8_t__channelIndex)
{
    uint32_t uint32_t__channelMask;

    if (uint8_t__channelIndex >= 2u)
    {
        return false;
    }

    uint32_t__channelMask = (1u << uint8_t__channelIndex);
    return ((CHG_INSTALLED_CHANNEL_MASK & uint32_t__channelMask) != 0u);
}

static bsp_pwm_channel_t func__Charger_PwmChannel(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return BSP_PWM_CHARGER_1;
    }

    return BSP_PWM_CHARGER_2;
}

static uint32_t func__Charger_ChannelVoltageMv(const measurement_snapshot_t *measurement_snapshot_t__snap,
                                                uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return measurement_snapshot_t__snap->v_bat_high_mv;
    }

    return measurement_snapshot_t__snap->v_bat_low_mv;
}

static uint32_t func__Charger_ChannelCurrentMa(const measurement_snapshot_t *measurement_snapshot_t__snap,
                                               uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return measurement_snapshot_t__snap->i_ch1_ma;
    }

    return measurement_snapshot_t__snap->i_ch2_ma;
}

/* ==================== Charger_OutputEstimateMa / تخمین جریان خروجی ==================== */

/**
 * @brief  [EN] Output (battery-side) current estimate - calibration
 *              architecture v1.3 (user order 2026-09-24). Two modes:
 *              (1) ETA = 0 (compiled default): identity - with the
 *              battery-calibrated gains the filtered reading already
 *              equals the battery current, so a reflash changes no number.
 *              (2) ETA != 0 (ESP param 9/10 or panel CAL_REFERENCE):
 *              iest = I x Vin x eta / (1000 x Vbat) with LIVE voltages,
 *              so the reading stays true while Vbat moves during a charge.
 *              Guards: below CHG_ETA_MIN_VIN_MV / CHG_ETA_MIN_VBAT_MV the
 *              estimate falls back to identity instead of dividing a
 *              garbage snapshot. Math ordered to stay inside 32 bits:
 *              (I*eta/1000) < ~1e7, product with Vin < ~3e8. (History of
 *              the deleted double-counting conversion: charger.h.)
 *         [FA] تخمین جریان خروجی (سمت باتری) - معماری کالیبراسیون v1.3
 *              (دستور کاربر ۲۰۲۶-۰۹-۲۴). (۱) η=۰ (پیش‌فرض): همانی - با
 *              گین‌های کالیبره-باتری عدد فیلترشده خودش جریان باتری است؛
 *              ریفلش هیچ عددی را عوض نمی‌کند. (۲) η≠۰ (پارامتر ۹/۱۰ یا
 *              CAL_REFERENCE پنل): iest = I × Vin × η ÷ (۱۰۰۰ × Vbat) با
 *              ولتاژهای زنده تا خوانش با حرکت Vbat درست بماند. گارد: زیر
 *              CHG_ETA_MIN_VIN_MV / CHG_ETA_MIN_VBAT_MV برگشت به همانی
 *              به‌جای تقسیم snapshot بی‌معنی. ترتیب ریاضی داخل ۳۲ بیت
 *              می‌ماند: (I×η÷۱۰۰۰) زیر ~1e7 و ضرب در Vin زیر ~3e8.
 *              (تاریخچهٔ تبدیل دوبارشمرِ حذف‌شده: charger.h.)
 * @param  measurement_snapshot_t__snap [EN] Live snapshot (Vin/Vbat) / snapshot زنده
 * @param  uint8_t__channelIndex [EN] 0 = ch1 (upper battery), 1 = ch2 / ۰=کانال۱، ۱=کانال۲
 * @param  uint32_t__primaryMa [EN] Filtered chain current, mA / جریان فیلترشدهٔ زنجیره، mA
 * @return uint32_t [EN] Battery-side current estimate, mA / تخمین جریان سمت باتری، mA
 */
static uint32_t func__Charger_OutputEstimateMa(const measurement_snapshot_t *measurement_snapshot_t__snap,
                                               uint8_t uint8_t__channelIndex,
                                               uint32_t uint32_t__primaryMa)
{
    uint32_t uint32_t__etaPermille =
        (uint8_t__channelIndex == 0u) ? UINT32_T__G__ChargerEta1Permille
                                      : UINT32_T__G__ChargerEta2Permille;

    if (uint32_t__etaPermille == 0u)
    {
        /* [EN] Identity (default): the reading already is the battery
           current. [FA] همانی (پیش‌فرض): عدد خودش جریان باتری است. */
        return uint32_t__primaryMa;
    }

    uint32_t uint32_t__vinMv = measurement_snapshot_t__snap->v_in_mv;
    uint32_t uint32_t__vbatMv =
        func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap,
                                       uint8_t__channelIndex);

    if ((uint32_t__vinMv < CHG_ETA_MIN_VIN_MV) ||
        (uint32_t__vbatMv < CHG_ETA_MIN_VBAT_MV))
    {
        /* [EN] Garbage voltages: fall back to the identity.
           [FA] ولتاژهای بی‌معنی: برگشت به همانی. */
        return uint32_t__primaryMa;
    }

    /* [EN] iest = I x Vin x eta / (1000 x Vbat), 32-bit-safe order.
       [FA] iest = I × Vin × η ÷ (۱۰۰۰ × Vbat)، ترتیب امن برای ۳۲ بیت. */
    return ((((uint32_t__primaryMa * uint32_t__etaPermille) / 1000u) * uint32_t__vinMv) /
            uint32_t__vbatMv);
}

static uint16_t func__Charger_MaxDutyPermille(void)
{
    if ((CHG_TRANSFORMER_KNOWN == 0u) && (CHG_BRINGUP_TEST_ENABLE != 0u))
    {
        return CHG_BRINGUP_TEST_MAX_DUTY_PERMILLE;
    }

    return CHG_DUTY_MAX_PERMILLE;
}

static uint32_t func__Charger_ActiveCurrentLimitMa(void)
{
    if ((CHG_TRANSFORMER_KNOWN == 0u) && (CHG_BRINGUP_TEST_ENABLE != 0u))
    {
        return CHG_BRINGUP_TEST_SOURCE_LIMIT_MA;
    }

    /* [EN] Derived from the profile band: limit = band + 25 mA (was the
       compile-time 675 over the 650 band). / سقف از باند پروفایل: حد = باند + ۲۵mA. */
    return (CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa + 25u);
}

static bool func__Charger_BatteryVoltageIsValid(uint32_t uint32_t__batteryMv)
{
    return ((uint32_t__batteryMv >= UINT32_T__G__ChargerValidFloorMv) &&
            (uint32_t__batteryMv <= UINT32_T__G__ChargerOvCutoffMv));
}

static void func__Charger_ApplyDuty(uint8_t uint8_t__channelIndex,
                                    uint16_t uint16_t__dutyPermille)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint8_t uint8_t__otherIndex;
    uint16_t uint16_t__maxDuty;
    uint16_t uint16_t__clampedDuty;

    if (uint8_t__channelIndex >= 2u)
    {
        return;
    }

    /* [EN] Suspension belt (see the Evaluate gate): any stray duty request
       while suspended forces the hardware to 0 WITHOUT touching the duty
       mirror, so the resume continues the ramp seamlessly.
       [FA] کمربند تعلیق: هر درخواست duty سرگردان در تعلیق، سخت‌افزار را صفر
       می‌کند بدون دست‌زدن به آینهٔ duty تا ادامهٔ رمپ یکپارچه باشد. */
    if (BOOL__G__ChargerSuspended != false)
    {
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
        return;
    }

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];

    if (charger_channel_state_t__channel->bool__installed == false)
    {
        charger_channel_state_t__channel->uint16_t__dutyPermille = 0u;
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
        return;
    }

    uint16_t__maxDuty = func__Charger_MaxDutyPermille();
    /* [EN] ESP runtime ceiling (user order 2026-09-22): every applied duty
       is clamped to the smaller of the compile max and the live per-channel
       ceiling - the single choke point for ramp, regulation and fixed mode.
       [FA] سقف زمان اجرای ESP (دستور کاربر ۲۰۲۶-۰۹-۲۲): هر duty اعمالی
       به کمینهٔ سقف کامپایل و سقف زندهٔ همان کانال گیره می‌خورد - تنها
       گلوگاه برای رمپ، تنظیم و مود فیکس. */
    if (UINT32_T__G__ChargerDutyCeilingPermille[uint8_t__channelIndex] <
        (uint32_t)uint16_t__maxDuty)
    {
        uint16_t__maxDuty =
            (uint16_t)UINT32_T__G__ChargerDutyCeilingPermille[uint8_t__channelIndex];
    }
    uint16_t__clampedDuty = uint16_t__dutyPermille;
    if (uint16_t__clampedDuty > uint16_t__maxDuty)
    {
        uint16_t__clampedDuty = uint16_t__maxDuty;
    }

    charger_channel_state_t__channel->uint16_t__dutyPermille = uint16_t__clampedDuty;
    func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex),
                                  uint16_t__clampedDuty);

    uint8_t__otherIndex = (uint8_t__channelIndex == 0u) ? 1u : 0u;
    if (CHARGER_CHANNEL_T__G__State[uint8_t__otherIndex].bool__installed == false)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__otherIndex].uint16_t__dutyPermille = 0u;
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__otherIndex), 0u);
    }
}

static void func__Charger_StopOneChannel(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex >= 2u)
    {
        return;
    }

    CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
    func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
}

/* ==================== Time helpers ==================== */

static uint32_t func__Charger_DurationTicks(uint32_t uint32_t__milliseconds)
{
    uint32_t uint32_t__ticks;

    uint32_t__ticks = func__Rtos_MillisecondsToTicks(uint32_t__milliseconds);
    if (uint32_t__ticks == 0u)
    {
        uint32_t__ticks = 1u;
    }

    return uint32_t__ticks;
}

static bool func__Charger_DeadlineElapsed(uint32_t uint32_t__nowTick,
                                          uint32_t uint32_t__deadlineTick)
{
    return ((int32_t)(uint32_t__nowTick - uint32_t__deadlineTick) >= 0);
}

/* ==================== Protection helpers ==================== */

static void func__Charger_ResetChannelToOff(uint8_t uint8_t__channelIndex)
{
    charger_channel_state_t *charger_channel_state_t__channel;

    if (uint8_t__channelIndex >= 2u)
    {
        return;
    }

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];
    charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_OFF;
    func__Charger_ClearAbsorbWindow(
        charger_channel_state_t__channel);
    charger_channel_state_t__channel->uint32_t__retryDeadlineTick = 0u;
    charger_channel_state_t__channel->uint32_t__lastDutyStepTick = 0u;
    charger_channel_state_t__channel->uint16_t__dutyBeforeTripPermille =
        charger_channel_state_t__channel->uint16_t__dutyPermille;
    /* [EN] A channel sent back to OFF starts its next charge from the soft
       start, not from the integral it had before the reset (v1.22).
       [FA] کانالی که به OFF برمی‌گردد شارژ بعدی را از شروع نرم آغاز می‌کند
       نه از انتگرالی که پیش از ریست داشت (v1.22). */
    func__Charger_PidInvalidate(charger_channel_state_t__channel);
}

static void func__Charger_LatchFinalFault(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex < 2u)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
            CHG_STATE_FINAL_FAULT;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint8_t__jitTripCount = 3u;
    }

    func__Charger_FinalDisconnect();
    UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
}

#if MODULE_JITTER

static void func__Charger_HandleJitTrip(uint8_t uint8_t__channelIndex,
                                        uint32_t uint32_t__nowTick)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint16_t uint16_t__dutyBeforeTripPermille;

    if (uint8_t__channelIndex >= 2u)
    {
        return;
    }

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FINAL_FAULT)
    {
        return;
    }

    charger_channel_state_t__channel->uint8_t__jitTripCount++;

    if (charger_channel_state_t__channel->uint8_t__jitTripCount >= 3u)
    {
        func__Charger_LatchFinalFault(uint8_t__channelIndex);
        return;
    }

    /* Capture before StopOneChannel(), which deliberately clears the live duty. */
    uint16_t__dutyBeforeTripPermille =
        charger_channel_state_t__channel->uint16_t__dutyPermille;
    func__Charger_StopOneChannel(uint8_t__channelIndex);
    charger_channel_state_t__channel->uint16_t__dutyBeforeTripPermille =
        uint16_t__dutyBeforeTripPermille;
    charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_JIT_RETRY_WAIT;
    if (BOOL__G__ChargerManualModeActive != false)
    {
        /* [EN] Manual test mode: the hardware cut stands, but there is NO
           auto-retry and no revive queue - the human re-sends the duty
           value (param 16/18) to re-arm the channel. The trip count still
           accumulates, so the 3rd trip latches FINAL_FAULT exactly like in
           the automatic mode.
           [FA] مود تست دستی: قطع سخت‌افزاری می‌ماند اما هیچ ریتری خودکار و
           صف احیایی نیست - کاربر برای مسلح‌کردن دوباره، مقدار duty
           (پارامتر ۱۶/۱۸) را دوباره می‌فرستد. شمارش تریپ جمع می‌شود، پس
           سومین تریپ دقیقاً مثل مود خودکار FINAL_FAULT را قفل می‌کند. */
        charger_channel_state_t__channel->uint32_t__retryDeadlineTick = 0u;
    }
    else
    {
        charger_channel_state_t__channel->uint32_t__retryDeadlineTick =
            uint32_t__nowTick + func__Charger_DurationTicks(CHG_LIM(CHG_LIMIT_PARAM_JIT_LOCKOUT_MS));
        /* [EN] Take the revive pointer only if it is free; while the rival is
           retrying this channel simply queues in JIT_RETRY_WAIT and the adopt-
           scan in ServiceRetry picks it up next - no pointer stomping, strict
           first-tripped-first-revived order.
           [FA] اشاره‌گر احیا فقط اگر آزاد است گرفته می‌شود؛ وگرنه کانال در صف
           می‌ماند تا اسکن ServiceRetry بردارد - ترتیب احیا حفظ می‌شود. */
        if (UINT8_T__G__RetryChannel == CHG_NO_CHANNEL)
        {
            UINT8_T__G__RetryChannel = uint8_t__channelIndex;
        }
    }
}

#endif

static uint16_t func__Charger_RetryDuty(uint8_t uint8_t__channelIndex)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint16_t uint16_t__retryDuty;
    uint16_t uint16_t__maxDuty;

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];
    uint16_t__retryDuty =
        (uint16_t)(charger_channel_state_t__channel->uint16_t__dutyBeforeTripPermille / 2u);

    if ((charger_channel_state_t__channel->uint8_t__jitTripCount > 1u) &&
        (uint16_t__retryDuty > CHG_DUTY_RETRY_SECOND_MAX))
    {
        uint16_t__retryDuty = CHG_DUTY_RETRY_SECOND_MAX;
    }

    if ((uint16_t__retryDuty == 0u) &&
        (charger_channel_state_t__channel->uint16_t__dutyBeforeTripPermille > 0u))
    {
        uint16_t__retryDuty = 1u;
    }

    uint16_t__maxDuty = func__Charger_MaxDutyPermille();
    if (uint16_t__retryDuty > uint16_t__maxDuty)
    {
        uint16_t__retryDuty = uint16_t__maxDuty;
    }

    return uint16_t__retryDuty;
}

static void func__Charger_ServiceRetry(uint32_t uint32_t__nowTick)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint16_t uint16_t__retryDuty;
    uint8_t uint8_t__retryChannel;

    uint8_t__retryChannel = UINT8_T__G__RetryChannel;

    /* [EN] Two-channel retry queue (audit 2026-09-20): a channel tripped
       while the rival was retrying is parked in JIT_RETRY_WAIT with its OWN
       per-channel deadline; when the global pointer frees up, adopt the
       first waiting channel so nothing starves. Revivals stay one-at-a-time
       (the pointer), preserving main's single-resume sequencing.
       [FA] صف ریتری دوکاناله (ممیزی): کانال تریپ‌شده در پنجرهٔ حریف با
       ددلاین خودش پارک می‌شود و با آزادشدن اشاره‌گر، اولین منتظر اخذ
       می‌شود تا قحطی نباشد؛ احیا همچنان تک‌تک. */
    if (uint8_t__retryChannel == CHG_NO_CHANNEL)
    {
        uint8_t uint8_t__waitIndex;

        for (uint8_t__waitIndex = 0u; uint8_t__waitIndex < 2u; uint8_t__waitIndex++)
        {
            if (CHARGER_CHANNEL_T__G__State[uint8_t__waitIndex].charger_state_t__state ==
                CHG_STATE_JIT_RETRY_WAIT)
            {
                uint8_t__retryChannel = uint8_t__waitIndex;
                UINT8_T__G__RetryChannel = uint8_t__retryChannel;
                break;
            }
        }
    }

    if (uint8_t__retryChannel == CHG_NO_CHANNEL)
    {
        return;
    }

    if (uint8_t__retryChannel >= 2u)
    {
        UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
        return;
    }

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__retryChannel];

    if (charger_channel_state_t__channel->charger_state_t__state != CHG_STATE_JIT_RETRY_WAIT)
    {
        UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
        return;
    }

    func__Charger_StopOneChannel(uint8_t__retryChannel);

    if (func__Charger_DeadlineElapsed(uint32_t__nowTick,
                                      charger_channel_state_t__channel->uint32_t__retryDeadlineTick) == false)
    {
        return;
    }

    uint16_t__retryDuty = func__Charger_RetryDuty(uint8_t__retryChannel);
    charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
    func__Charger_ClearAbsorbWindow(
        charger_channel_state_t__channel);
    charger_channel_state_t__channel->uint32_t__retryDeadlineTick = 0u;

#if MODULE_JITTER
    func__Jitter_ClearChannel((uint8_t)(uint8_t__retryChannel + 1u));
#endif

    func__Charger_ApplyDuty(uint8_t__retryChannel, uint16_t__retryDuty);
    /* [EN] Stamp the step timer so the resumed ramp waits a full up-interval
       before growing again (soft resume after a JIT trip).
       [FA] بعد از ری‌استارت JIT هم رمپ باید یک بازهٔ کامل صبر کند. */
    charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
    UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
}

/* ==================== Bring-up regulation ==================== */

static void func__Charger_BringupRegulateChannel(uint8_t uint8_t__channelIndex,
                                                 const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint32_t uint32_t__currentMa;
    uint32_t uint32_t__batteryMv;
    uint16_t uint16_t__nextDuty;
    uint16_t uint16_t__maxDuty;
    uint32_t uint32_t__increased;

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];
    if (charger_channel_state_t__channel->charger_state_t__state != CHG_STATE_BRINGUP)
    {
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BRINGUP;
        charger_channel_state_t__channel->uint16_t__dutyPermille = 0u;
        func__Charger_ApplyDuty(uint8_t__channelIndex, CHG_DUTY_START_PERMILLE);
        return;
    }

    uint32_t__batteryMv =
        func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap, uint8_t__channelIndex);
    uint32_t__currentMa =
        func__Charger_ChannelCurrentMa(measurement_snapshot_t__snap, uint8_t__channelIndex);

    if (func__Charger_BatteryVoltageIsValid(uint32_t__batteryMv) == false)
    {
        func__Charger_ResetChannelToOff(uint8_t__channelIndex);
        func__Charger_StopOneChannel(uint8_t__channelIndex);
        return;
    }

    if (uint32_t__currentMa > func__Charger_ActiveCurrentLimitMa())
    {
        func__Charger_ResetChannelToOff(uint8_t__channelIndex);
        func__Charger_StopOneChannel(uint8_t__channelIndex);
        return;
    }

    uint16_t__maxDuty = func__Charger_MaxDutyPermille();
    uint16_t__nextDuty = charger_channel_state_t__channel->uint16_t__dutyPermille;

    if (uint32_t__currentMa < func__Charger_ActiveCurrentLimitMa())
    {
        if (uint16_t__nextDuty < uint16_t__maxDuty)
        {
            uint32_t__increased = (uint32_t)uint16_t__nextDuty + CHG_DUTY_STEP_PERMILLE;
            if (uint32_t__increased > (uint32_t)uint16_t__maxDuty)
            {
                uint32_t__increased = (uint32_t)uint16_t__maxDuty;
            }
            uint16_t__nextDuty = (uint16_t)uint32_t__increased;
        }
    }
    else if (uint16_t__nextDuty > CHG_DUTY_STEP_PERMILLE)
    {
        uint16_t__nextDuty = (uint16_t)(uint16_t__nextDuty - CHG_DUTY_STEP_PERMILLE);
    }
    else
    {
        uint16_t__nextDuty = 0u;
    }

    func__Charger_ApplyDuty(uint8_t__channelIndex, uint16_t__nextDuty);
}

/* ==================== Regulation ==================== */

/**
 * @brief  [EN] True when the channel has seen installed + valid input +
 *         valid battery voltage continuously for at least
 *         CHG_CONNECT_SETTLE_MS (user directive 2026-09-19: let the
 *         connection settle 10..20 s, THEN start the charge). Used as the
 *         mandatory gate for every OFF -> BULK cold start; JIT resume and
 *         the 12.8 V reentry keep their already-live stamps so mid-cycle
 *         paths are not delayed.
 *         [FA] فقط وقتی اتصال به‌طور پیوسته حداقل ۱۵ ثانیه معتبر دیده شده
 *         اجازهٔ شروع بالک می‌دهد (دستور کاربر: اول ثبات، بعد شارژ).
 * @param  uint8_t__channelIndex [EN] channel 0 or 1 / کانال ۰ یا ۱
 * @param  uint32_t__nowTick     [EN] current kernel tick / تیک فعلی کرنل
 * @return bool [EN] true = settled, bulk may start / true = ثابت شده، بالک مجاز
 */
static bool func__Charger_BulkStartSettled(uint8_t uint8_t__channelIndex,
                                           uint32_t uint32_t__nowTick)
{
    uint32_t uint32_t__stableFromTick;

    uint32_t__stableFromTick =
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__stableFromTick;
    if (uint32_t__stableFromTick == 0u)
    {
        return false;
    }
    return ((uint32_t)(uint32_t__nowTick - uint32_t__stableFromTick) >=
            func__Charger_DurationTicks(CHG_LIM(CHG_LIMIT_PARAM_CONNECT_SETTLE_MS)));
}

/* ==================== Two-loop CC/CV PID core (v1.22) ====================
 * [EN] One positional PID whose gains are scheduled by the battery-voltage
 *      stage, feeding a per-stage slew limiter. Everything is integer math
 *      in milli-permille; see the charger.h block for the unit contract.
 * [FA] یک PID موقعیتی که ضرایبش با مرحلهٔ ولتاژ باتری زمان‌بندی می‌شود و به
 *      محدودکنندهٔ شیبِ همان مرحله می‌رسد. همهٔ ریاضی صحیح و بر حسب
 *      میلی‌پرمیل است؛ قرارداد واحدها در بلوک charger.h. */

/* [EN] Drop the PID's memory of the operating point: the next update
 *      re-seeds from the duty the hardware actually carries.
 * [FA] پاک‌کردن حافظهٔ نقطهٔ کار: به‌روزرسانی بعدی از دیوتی واقعی
 *      سخت‌افزار دوباره بذر می‌گیرد. */
static void func__Charger_PidInvalidate(charger_channel_state_t *charger_channel_state_t__channel)
{
    charger_channel_state_t__channel->bool__pidSeeded = false;
}

/* [EN] Live duty ceiling in milli-permille: the smaller of the compile-time
 *      DCM cap and the panel's per-channel ceiling - the same pair
 *      func__Charger_ApplyDuty enforces, so the integrator can never wind
 *      up against a limit the hardware will clip anyway.
 * [FA] سقف زندهٔ دیوتی بر حسب میلی‌پرمیل: کمینهٔ سقف کامپایل و سقف پنل برای
 *      همان کانال - همان جفتی که ApplyDuty اعمال می‌کند، پس انتگرال‌گیر
 *      هرگز پشت حدی که سخت‌افزار می‌برد وا نمی‌رود. */
static uint32_t func__Charger_PidCeilingMilli(uint8_t uint8_t__channelIndex)
{
    uint32_t uint32_t__ceilingPermille = (uint32_t)func__Charger_MaxDutyPermille();

    if (UINT32_T__G__ChargerDutyCeilingPermille[uint8_t__channelIndex] <
        uint32_t__ceilingPermille)
    {
        uint32_t__ceilingPermille =
            UINT32_T__G__ChargerDutyCeilingPermille[uint8_t__channelIndex];
    }

    return (uint32_t__ceilingPermille * CHG_PID_DUTY_SCALE);
}

/* [EN] Saturate one error before any gain touches it. / اشباع خطا پیش از ضریب. */
static int32_t func__Charger_PidClampError(int32_t int32_t__error)
{
    if (int32_t__error > CHG_PID_ERROR_CLAMP)
    {
        int32_t__error = CHG_PID_ERROR_CLAMP;
    }
    if (int32_t__error < -CHG_PID_ERROR_CLAMP)
    {
        int32_t__error = -CHG_PID_ERROR_CLAMP;
    }

    return int32_t__error;
}

/**
 * @brief  [EN] Advance the two-loop CC/CV PID for one channel and return the
 *              duty it wants, in permille. Called once per control pass;
 *              the math only advances every CHG_PID_PERIOD_MS, in between
 *              the last duty is repeated so ApplyDuty keeps its housekeeping.
 *         [FA] یک قدم PID دوحلقه‌ای برای یک کانال و بازگرداندن دیوتی
 *              خواسته‌شده بر حسب پرمیل. هر پاس کنترل صدا زده می‌شود ولی
 *              ریاضی فقط هر CHG_PID_PERIOD_MS جلو می‌رود؛ بین آن‌ها همان
 *              دیوتی قبلی تکرار می‌شود تا کارهای جانبی ApplyDuty بماند.
 * @param  uint8_t__channelIndex [EN] 0 or 1 / شمارهٔ کانال
 * @param  uint32_t__batteryMv [EN] This channel's battery voltage / ولتاژ باتری همین کانال
 * @param  uint32_t__currentMa [EN] Estimated battery current / جریان تخمینی باتری
 * @param  uint32_t__targetMv [EN] Live voltage setpoint / ست‌پوینت زندهٔ ولتاژ
 * @param  uint32_t__nowTick [EN] Kernel tick / تیک کرنل
 * @return uint16_t [EN] Requested duty in permille / دیوتی خواسته‌شده (پرمیل)
 */
static uint16_t func__Charger_PidStep(uint8_t uint8_t__channelIndex,
                                      uint32_t uint32_t__batteryMv,
                                      uint32_t uint32_t__currentMa,
                                      uint32_t uint32_t__targetMv,
                                      uint32_t uint32_t__nowTick)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint32_t uint32_t__ceilingMilli;
    uint32_t uint32_t__appliedMilli;
    uint32_t uint32_t__elapsedMs;
    uint32_t uint32_t__currentTargetMa;
    int32_t int32_t__voltageKp;
    int32_t int32_t__voltageKi;
    int32_t int32_t__voltageKd;
    int32_t int32_t__voltageUpRate;
    int32_t int32_t__voltageDownRate;
    int32_t int32_t__currentKp;
    int32_t int32_t__currentKi;
    int32_t int32_t__currentKd;
    int32_t int32_t__currentUpRate;
    int32_t int32_t__currentDownRate;
    int32_t int32_t__errorVoltage;
    int32_t int32_t__errorCurrent;
    int32_t int32_t__error;
    int32_t int32_t__gainI;
    int32_t int32_t__upRate;
    int32_t int32_t__downRate;
    int32_t int32_t__termPv;
    int32_t int32_t__termPi;
    int32_t int32_t__termDv;
    int32_t int32_t__termDi;
    int32_t int32_t__proportional;
    int32_t int32_t__derivative;
    int32_t int32_t__rate;
    int32_t int32_t__demand;
    int32_t int32_t__appliedPermille;
    int32_t int32_t__prevPermille;
    int32_t int32_t__deviation;
    int32_t int32_t__numerator;
    int32_t int32_t__step;
    bool bool__useVoltage;

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];
    uint32_t__ceilingMilli = func__Charger_PidCeilingMilli(uint8_t__channelIndex);
    uint32_t__appliedMilli =
        (uint32_t)charger_channel_state_t__channel->uint16_t__dutyPermille *
        CHG_PID_DUTY_SCALE;

    /* [EN] HARD BACKSTOPS (user order 2026-09-29): the 650 mA profile limit
       and the 14.8 V pack limit must be actively enforced so the batteries
       cannot be damaged - including by a future mis-tune of the gains,
       which is why they live here and not on the panel. Both shrink the
       duty ceiling in proportion to the excess instead of tripping: a
       fixed step would re-create the hunting this rewrite removed, and a
       latch would abort a healthy charge on one noisy sample. Because the
       integral is clamped to this same ceiling a few lines below, there is
       no windup left behind when the excess clears. See the block comment
       on CHG_LIM(CHG_LIMIT_PARAM_BACKSTOP_MV) in charger.h.
       [FA] پشتیبان‌های سخت (دستور کاربر ۲۰۲۶-۰۹-۲۹): حد ۶۵۰ میلی‌آمپرِ
       پروفایل و حد ۱۴٫۸ ولتِ باتری باید فعالانه اعمال شوند تا باتری‌ها
       آسیب نبینند - از جمله در برابر تنظیم بد آیندهٔ ضرایب، و دلیل اینکه
       اینجا هستند نه در پنل همین است. هر دو به‌جای قطع‌کردن، سقف دیوتی را
       به تناسب مقدار تجاوز جمع می‌کنند: پلهٔ ثابت همان بالا-پایین پریدن را
       برمی‌گرداند و قطع قفل‌شونده یک شارژ سالم را با یک نمونهٔ نویزی متوقف
       می‌کند. چون انتگرال چند خط پایین‌تر به همین سقف مقید می‌شود، وقتی
       تجاوز رفع شد چیزی برای باز شدن نمی‌ماند. */
    if (uint32_t__currentMa > CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa)
    {
        uint32_t uint32_t__cutMilli =
            (uint32_t__currentMa -
             CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa) *
            CHG_LIM(CHG_LIMIT_PARAM_BACKSTOP_GAIN_I);

        if (uint32_t__appliedMilli > uint32_t__cutMilli)
        {
            if ((uint32_t__appliedMilli - uint32_t__cutMilli) < uint32_t__ceilingMilli)
            {
                uint32_t__ceilingMilli = uint32_t__appliedMilli - uint32_t__cutMilli;
            }
        }
        else
        {
            uint32_t__ceilingMilli = 0u;
        }
    }
    if (uint32_t__batteryMv > CHG_LIM(CHG_LIMIT_PARAM_BACKSTOP_MV))
    {
        uint32_t uint32_t__cutMilli =
            (uint32_t__batteryMv - CHG_LIM(CHG_LIMIT_PARAM_BACKSTOP_MV)) * CHG_LIM(CHG_LIMIT_PARAM_BACKSTOP_GAIN_V);

        if (uint32_t__appliedMilli > uint32_t__cutMilli)
        {
            if ((uint32_t__appliedMilli - uint32_t__cutMilli) < uint32_t__ceilingMilli)
            {
                uint32_t__ceilingMilli = uint32_t__appliedMilli - uint32_t__cutMilli;
            }
        }
        else
        {
            uint32_t__ceilingMilli = 0u;
        }
    }

    /* [EN] Bumpless (re)seed. Any foreign writer - the BULK soft start, a
       halved JIT retry duty, manual/fixed mode, a lowered panel ceiling -
       leaves the hardware duty out of step with the integrator; adopt it
       instead of fighting it, and spend this pass settling.
       [FA] بذرگیری بدون پرش: هر نویسندهٔ بیرونی (شروع نرم بالک، نصف‌شدن
       دیوتی در ری‌تری JIT، مود دستی/فیکس، پایین‌آمدن سقف پنل) دیوتی
       سخت‌افزار را از انتگرال‌گیر جدا می‌کند؛ به‌جای جنگیدن، همان را
       می‌پذیریم و این پاس را صرف نشستن می‌کنیم. */
    if ((charger_channel_state_t__channel->bool__pidSeeded == false) ||
        (charger_channel_state_t__channel->uint32_t__pidDutyMilli >
         (uint32_t__appliedMilli +
          ((uint32_t)CHG_PID_RESEED_TOLERANCE_PERMILLE * CHG_PID_DUTY_SCALE))) ||
        (uint32_t__appliedMilli >
         (charger_channel_state_t__channel->uint32_t__pidDutyMilli +
          ((uint32_t)CHG_PID_RESEED_TOLERANCE_PERMILLE * CHG_PID_DUTY_SCALE))))
    {
        charger_channel_state_t__channel->uint32_t__pidDutyMilli = uint32_t__appliedMilli;
        charger_channel_state_t__channel->uint32_t__pidVoltFilt =
            uint32_t__batteryMv * CHG_LIM(CHG_LIMIT_PARAM_PID_VOLT_FILTER_N);
        charger_channel_state_t__channel->int32_t__pidIntegral = (int32_t)uint32_t__appliedMilli;
        charger_channel_state_t__channel->int32_t__pidIntegralRem = 0;
        charger_channel_state_t__channel->int32_t__pidLastError = 0;
        charger_channel_state_t__channel->bool__pidLastWasVoltage = true;
        charger_channel_state_t__channel->bool__pidSeeded = true;
        charger_channel_state_t__channel->uint32_t__pidLastTick = uint32_t__nowTick;
        return charger_channel_state_t__channel->uint16_t__dutyPermille;
    }

    /* [EN] Cadence gate: one update per CHG_PID_PERIOD_MS. / دروازهٔ ضرب‌آهنگ. */
    uint32_t__elapsedMs = func__Rtos_TicksToMilliseconds(
        (uint32_t)(uint32_t__nowTick -
                   charger_channel_state_t__channel->uint32_t__pidLastTick));
    if (uint32_t__elapsedMs < CHG_PID_PERIOD_MS)
    {
        return charger_channel_state_t__channel->uint16_t__dutyPermille;
    }
    if (uint32_t__elapsedMs > CHG_PID_DT_MAX_MS)
    {
        uint32_t__elapsedMs = CHG_PID_DT_MAX_MS;
    }
    if (uint32_t__elapsedMs < CHG_PID_DT_MIN_MS)
    {
        uint32_t__elapsedMs = CHG_PID_DT_MIN_MS;
    }
    charger_channel_state_t__channel->uint32_t__pidLastTick = uint32_t__nowTick;

    /* [EN] Voltage prefilter, advanced once per PID update so its time
       constant is CHG_LIM(CHG_LIMIT_PARAM_PID_VOLT_FILTER_N) x CHG_PID_PERIOD_MS = 1.6 s. The
       charge current arrives already filtered from measurement.c; the pack
       voltage does not, and ~7 mV of ADC step lands straight on the
       voltage loop's P term. Only the CONTROL path reads this - the hard
       backstops above deliberately used the raw sample.
       [FA] پیش‌فیلتر ولتاژ، یک‌بار در هر به‌روزرسانی PID جلو می‌رود پس ثابت
       زمانی‌اش CHG_LIM(CHG_LIMIT_PARAM_PID_VOLT_FILTER_N) ضرب در CHG_PID_PERIOD_MS = ۱٫۶ ثانیه
       است. جریان شارژ از measurement.c فیلترشده می‌آید ولی ولتاژ پک نه، و
       حدود ۷ میلی‌ولت پلهٔ ADC مستقیم روی جملهٔ P حلقهٔ ولتاژ می‌نشیند. فقط
       مسیر کنترل این را می‌خواند - پشتیبان‌های سخت بالا عمداً نمونهٔ خام را
       خواندند. */
    charger_channel_state_t__channel->uint32_t__pidVoltFilt =
        (charger_channel_state_t__channel->uint32_t__pidVoltFilt -
         (charger_channel_state_t__channel->uint32_t__pidVoltFilt /
          CHG_LIM(CHG_LIMIT_PARAM_PID_VOLT_FILTER_N))) +
        uint32_t__batteryMv;
    uint32_t__batteryMv =
        charger_channel_state_t__channel->uint32_t__pidVoltFilt / CHG_LIM(CHG_LIMIT_PARAM_PID_VOLT_FILTER_N);

    /* [EN] TWO gain rows, one per physical control loop - that is the whole
       regulator (user order 2026-09-29: "if we do it with one PID over the
       whole path does it not work? it does not matter if it is slow,
       because the battery itself is slow; I want maximum accuracy by the
       SIMPLEST method"). Measured answer: one row cannot do it, two rows
       are enough, three were one too many.
         - ONE shared row fails because a volt of voltage error and an amp
           of current error are different quantities. The min-select then
           compares millivolts against milliamps, there is no real CC->CV
           knee, and the duty hunts: 1408..84660 direction changes per 10 h
           against 4 here.
         - A single VOLTAGE PID with the 650 mA backstop as the only current
           limiter also fails, and it fails on the user's own safety
           requirement: a backstop is reactive, it can only answer AFTER the
           limit is crossed, so the current limit-cycles and peaks at 707 mA
           instead of sitting on 640.
         - The third row (a separate set for at/above the setpoint) was
           measured to buy nothing: with broadband sensor noise the two-row
           design is equal or QUIETER at every noise level, and identical on
           the clean plant. It was deleted.
       So: the current row always drives the CC branch and the voltage row
       always drives the CV branch, whatever the pack voltage happens to be.
       [FA] دو ردیف ضریب، هر کدام برای یک حلقهٔ کنترل فیزیکی - و کل
       تنظیم‌کننده همین است (دستور کاربر ۲۰۲۶-۰۹-۲۹: «اگر با یک PID در کل
       مسیر انجام بدهیم کار درنمی‌آید؟ مهم نیست کند باشد، چون باتری خودش
       کند است؛ با نهایت دقت ولی ساده‌ترین روش»). جواب اندازه‌گیری‌شده: یک
       ردیف نمی‌تواند، دو ردیف کافی است، سه ردیف یکی زیادی بود.
         - یک ردیف مشترک شکست می‌خورد چون یک ولت خطای ولتاژ و یک آمپر خطای
           جریان دو کمیت متفاوت‌اند. آن‌وقت کمینه‌گیری میلی‌ولت را با
           میلی‌آمپر مقایسه می‌کند، زانوی واقعی CC→CV وجود ندارد و دیوتی
           می‌لرزد: ۱۴۰۸ تا ۸۴۶۶۰ تغییر جهت در ۱۰ ساعت در برابر ۴ تای اینجا.
         - فقط یک PID ولتاژ با پشتیبان ۶۵۰ میلی‌آمپر به‌عنوان تنها
           محدودکنندهٔ جریان هم شکست می‌خورد، آن هم دقیقاً روی خواستهٔ ایمنی
           خود کاربر: پشتیبان واکنشی است و فقط بعد از رد شدن از حد جواب
           می‌دهد، پس جریان چرخهٔ حدی می‌زند و به‌جای نشستن روی ۶۴۰ تا ۷۰۷
           میلی‌آمپر بالا می‌رود.
         - ردیف سوم (دستهٔ جدا برای روی ست‌پوینت و بالاتر) اندازه‌گیری شد و
           هیچ سودی نداشت: با نویز پهن‌باند سنسور، طرح دوردیفی در هر سطح
           نویز مساوی یا آرام‌تر است و روی مدل تمیز دقیقاً یکی. حذف شد. */
    int32_t__currentKp = (int32_t)CHARGER_PID_T__G__Pid.uint32_t__currentKp;
    int32_t__currentKi = (int32_t)CHARGER_PID_T__G__Pid.uint32_t__currentKi;
    int32_t__currentKd = (int32_t)CHARGER_PID_T__G__Pid.uint32_t__currentKd;
    int32_t__currentUpRate = (int32_t)CHARGER_PID_T__G__Pid.uint32_t__currentUpRate;
    int32_t__currentDownRate = (int32_t)CHARGER_PID_T__G__Pid.uint32_t__currentDownRate;
    int32_t__voltageKp = (int32_t)CHARGER_PID_T__G__Pid.uint32_t__voltageKp;
    int32_t__voltageKi = (int32_t)CHARGER_PID_T__G__Pid.uint32_t__voltageKi;
    int32_t__voltageKd = (int32_t)CHARGER_PID_T__G__Pid.uint32_t__voltageKd;
    int32_t__voltageUpRate = (int32_t)CHARGER_PID_T__G__Pid.uint32_t__voltageUpRate;
    int32_t__voltageDownRate = (int32_t)CHARGER_PID_T__G__Pid.uint32_t__voltageDownRate;

    /* [EN] CC/CV min-select. The current branch aims at the middle of the
       old regulation band (top - CHG_LIM(CHG_LIMIT_PARAM_PID_CUR_MARGIN_MA)), so the
       familiar ~640 mA operating point is kept and the 950 mA hard fault
       keeps its clearance.
       [FA] کمینه‌گیری CC/CV: شاخهٔ جریان وسط باند قدیمی را هدف می‌گیرد
       (سقف منهای CHG_LIM(CHG_LIMIT_PARAM_PID_CUR_MARGIN_MA)) تا همان نقطهٔ کار آشنای
       ~۶۴۰mA بماند و خطای سخت ۹۵۰mA فاصله‌اش را حفظ کند. */
    uint32_t__currentTargetMa = CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa;
    if (uint32_t__currentTargetMa > (uint32_t)CHG_LIM(CHG_LIMIT_PARAM_PID_CUR_MARGIN_MA))
    {
        uint32_t__currentTargetMa -= (uint32_t)CHG_LIM(CHG_LIMIT_PARAM_PID_CUR_MARGIN_MA);
    }

    int32_t__errorVoltage =
        func__Charger_PidClampError((int32_t)uint32_t__targetMv - (int32_t)uint32_t__batteryMv);
    int32_t__errorCurrent =
        func__Charger_PidClampError((int32_t)uint32_t__currentTargetMa - (int32_t)uint32_t__currentMa);

    int32_t__termPv = (int32_t__voltageKp * int32_t__errorVoltage) / (int32_t)CHG_PID_KP_DIV;
    int32_t__termPi = (int32_t__currentKp * int32_t__errorCurrent) / (int32_t)CHG_PID_KP_DIV;
    /* [EN] The derivative only exists for the branch that also won the
       previous update; a fresh branch starts with no history (no kick).
       [FA] مشتق فقط برای شاخه‌ای معنی دارد که پاس قبل هم برنده بوده؛ شاخهٔ
       تازه بدون تاریخچه شروع می‌کند (بدون لگد). */
    int32_t__termDv = 0;
    int32_t__termDi = 0;
    if (charger_channel_state_t__channel->bool__pidLastWasVoltage != false)
    {
        int32_t__termDv =
            (int32_t__voltageKd *
             (int32_t__errorVoltage - charger_channel_state_t__channel->int32_t__pidLastError)) /
            (int32_t)CHG_PID_KD_DIV;
    }
    else
    {
        int32_t__termDi =
            (int32_t__currentKd *
             (int32_t__errorCurrent - charger_channel_state_t__channel->int32_t__pidLastError)) /
            (int32_t)CHG_PID_KD_DIV;
    }

    bool__useVoltage = ((int32_t__termPv + int32_t__termDv) <=
                        (int32_t__termPi + int32_t__termDi));
    if (bool__useVoltage != false)
    {
        int32_t__error = int32_t__errorVoltage;
        int32_t__gainI = int32_t__voltageKi;
        int32_t__upRate = int32_t__voltageUpRate;
        int32_t__downRate = int32_t__voltageDownRate;
        int32_t__proportional = int32_t__termPv;
        int32_t__derivative = int32_t__termDv;
    }
    else
    {
        int32_t__error = int32_t__errorCurrent;
        int32_t__gainI = int32_t__currentKi;
        int32_t__upRate = int32_t__currentUpRate;
        int32_t__downRate = int32_t__currentDownRate;
        int32_t__proportional = int32_t__termPi;
        int32_t__derivative = int32_t__termDi;
    }

    /* [EN] Integral advance, slew-limited. The rate limit is applied HERE,
       to the integral (the operating point the loop walks toward), and NOT
       to the finished output. The two look equivalent and are not: the P
       term ripples by a few milli-permille every time the applied duty
       quantises to the next whole permille, and an output limiter with a
       fast down-rate and a slow up-rate rectifies that ripple into a
       downward ratchet (simulated: the loop stalled at 268 mA and never
       reached the 640 mA bulk band). Limiting the integral leaves the
       ripple zero-mean and makes the ramp rate exactly the number the user
       dialled in. The sub-unit remainder is carried so a 0.01 permille/s
       creep is still exact.
       [FA] پیشروی انتگرال با سقف شیب. سقف شیب «اینجا» روی انتگرال (نقطهٔ
       کاری که حلقه به سمتش راه می‌رود) اعمال می‌شود نه روی خروجی نهایی.
       این دو شبیه هم به نظر می‌رسند ولی یکی نیستند: هر بار دیوتی اعمالی به
       پرمیل صحیح بعدی گرد می‌شود جملهٔ P چند میلی‌پرمیل ریپل می‌خورد و
       محدودکنندهٔ خروجی با نزول تند و صعود کند آن ریپل را یکسو و به جغجغهٔ
       رو به پایین تبدیل می‌کند (در شبیه‌سازی حلقه روی ۲۶۸mA گیر کرد و هرگز
       به باند ۶۴۰mA نرسید). با سقف روی انتگرال، ریپل میانگین‌صفر می‌ماند و
       نرخ رمپ دقیقاً همان عددی می‌شود که کاربر گذاشته. باقی‌ماندهٔ زیرواحدی
       حمل می‌شود تا خزش ۰٫۰۱ پرمیل بر ثانیه دقیق بماند. */
    int32_t__rate = (int32_t__gainI * int32_t__error) / (int32_t)CHG_PID_KI_DIV;
    if (int32_t__rate > int32_t__upRate)
    {
        int32_t__rate = int32_t__upRate;
    }
    if (int32_t__rate < -int32_t__downRate)
    {
        int32_t__rate = -int32_t__downRate;
    }

    int32_t__numerator = (int32_t__rate * (int32_t)uint32_t__elapsedMs) +
                         charger_channel_state_t__channel->int32_t__pidIntegralRem;
    int32_t__step = int32_t__numerator / 1000;
    charger_channel_state_t__channel->int32_t__pidIntegralRem =
        int32_t__numerator - (int32_t__step * 1000);
    charger_channel_state_t__channel->int32_t__pidIntegral += int32_t__step;

    /* [EN] Anti-windup: the integral lives in the SAME window as the duty
       it will become, so a clamp can never store energy the loop would
       have to unwind later.
       [FA] ضدِ وا رفتن: انتگرال در همان پنجره‌ای زندگی می‌کند که قرار است
       دیوتی شود، پس هیچ گیره‌ای انرژی‌ای ذخیره نمی‌کند که حلقه بعداً مجبور
       به بازکردنش شود. */
    if (charger_channel_state_t__channel->int32_t__pidIntegral < 0)
    {
        charger_channel_state_t__channel->int32_t__pidIntegral = 0;
        charger_channel_state_t__channel->int32_t__pidIntegralRem = 0;
    }
    if (charger_channel_state_t__channel->int32_t__pidIntegral >
        (int32_t)uint32_t__ceilingMilli)
    {
        charger_channel_state_t__channel->int32_t__pidIntegral =
            (int32_t)uint32_t__ceilingMilli;
        charger_channel_state_t__channel->int32_t__pidIntegralRem = 0;
    }

    int32_t__demand = charger_channel_state_t__channel->int32_t__pidIntegral +
                      int32_t__proportional + int32_t__derivative;
    if (int32_t__demand < 0)
    {
        int32_t__demand = 0;
    }
    if (int32_t__demand > (int32_t)uint32_t__ceilingMilli)
    {
        int32_t__demand = (int32_t)uint32_t__ceilingMilli;
    }

    /* [EN] Hysteretic commit. The hardware takes whole permille, so rounding
       the demand every pass makes the applied duty toggle between two
       neighbouring integers whenever the integral rests near a boundary -
       a 1 permille dither at up to 10 Hz. Electrically harmless, but it is
       the "duty keeps jumping around" the user complained about and it is
       what the readout shows. So the applied value only moves once the
       demand has drifted CHG_LIM(CHG_LIMIT_PARAM_PID_OUT_HYST_MILLI) away from what the
       hardware already carries. pidDutyMilli then stores the APPLIED duty
       (not the fine demand), which keeps the re-seed guard exactly as
       sensitive to foreign writers as it was before.
       [FA] ثبت با هیسترزیس. سخت‌افزار پرمیل صحیح می‌گیرد، پس اگر هر پاس
       تقاضا را گرد کنیم، هر وقت انتگرال نزدیک مرز بنشیند دیوتی اعمالی بین
       دو عدد صحیح همسایه بالا و پایین می‌پرد - لرزش یک پرمیلی تا ۱۰ هرتز.
       از نظر برقی بی‌ضرر است ولی همان «دیوتی مدام بالا-پایین می‌پرد» است که
       کاربر گفت و همان چیزی است که در نمایش دیده می‌شود. پس مقدار اعمالی
       فقط وقتی تکان می‌خورد که تقاضا به اندازهٔ CHG_LIM(CHG_LIMIT_PARAM_PID_OUT_HYST_MILLI) از
       آنچه روی سخت‌افزار است فاصله گرفته باشد. آنگاه pidDutyMilli دیوتی
       «اعمال‌شده» را نگه می‌دارد نه تقاضای ریز را، و همین باعث می‌شود
       نگهبان بذرگیری دقیقاً به همان اندازهٔ قبل به نویسندهٔ بیرونی حساس
       بماند. */
    int32_t__prevPermille = (int32_t)charger_channel_state_t__channel->uint16_t__dutyPermille;
    int32_t__appliedPermille = int32_t__prevPermille;
    int32_t__deviation = int32_t__demand -
                         (int32_t__appliedPermille * (int32_t)CHG_PID_DUTY_SCALE);
    if ((int32_t__deviation >= (int32_t)CHG_LIM(CHG_LIMIT_PARAM_PID_OUT_HYST_MILLI)) ||
        (int32_t__deviation <= -(int32_t)CHG_LIM(CHG_LIMIT_PARAM_PID_OUT_HYST_MILLI)))
    {
        int32_t__appliedPermille =
            (int32_t__demand + ((int32_t)CHG_PID_DUTY_SCALE / 2)) / (int32_t)CHG_PID_DUTY_SCALE;
    }

    /* [EN] Symmetric mis-tune cap. The integral is rate-limited but P is
       not, so a gain set pushed to the panel maximum could otherwise jump
       the duty across its whole range in one update - measured on the
       plant model, 4475 mA for 100 ms before the backstop could answer.
       Symmetric so it cannot rectify the P-term ripple the way the old
       asymmetric output limiter did, and wide enough (8 permille against a
       measured worst of 3) that normal charging never touches it. It sits
       BEFORE the hard ceiling below on purpose: a backstop must still be
       able to pull the duty down by any amount in a single pass.
       [FA] سقف متقارن مهار تنظیم اشتباه. انتگرال محدودِ نرخ است ولی P نه،
       پس دسته‌ضریبی که تا بیشینهٔ پنل بالا رفته می‌توانست دیوتی را در یک
       به‌روزرسانی در تمام بازه‌اش بپراند - روی مدل اندازه‌گیری شد: ۴۴۷۵
       میلی‌آمپر برای ۱۰۰ میلی‌ثانیه، پیش از آنکه پشتیبان بتواند جواب دهد.
       متقارن است تا نتواند ریپل جملهٔ P را مثل محدودکنندهٔ نامتقارن قدیمی
       یکسو کند، و آن‌قدر باز هست (۸ پرمیل در برابر بیشینهٔ اندازه‌گیری‌شدهٔ
       ۳) که شارژ عادی هرگز به آن نخورد. عمداً پیش از سقف سخت پایین
       می‌نشیند: پشتیبان باید بتواند دیوتی را در یک پاس هر قدر لازم است
       پایین بکشد. */
    if (int32_t__appliedPermille > (int32_t__prevPermille + (int32_t)CHG_LIM(CHG_LIMIT_PARAM_PID_MAX_STEP_PM)))
    {
        int32_t__appliedPermille = int32_t__prevPermille + (int32_t)CHG_LIM(CHG_LIMIT_PARAM_PID_MAX_STEP_PM);
    }
    if (int32_t__appliedPermille < (int32_t__prevPermille - (int32_t)CHG_LIM(CHG_LIMIT_PARAM_PID_MAX_STEP_PM)))
    {
        int32_t__appliedPermille = int32_t__prevPermille - (int32_t)CHG_LIM(CHG_LIMIT_PARAM_PID_MAX_STEP_PM);
    }

    /* [EN] The ceiling is an ABSOLUTE cap, applied after the hysteresis so
       the hysteresis can never hold the duty above a limit. This is what
       makes the 650 mA / 14.8 V backstops a guarantee rather than a
       tendency: without it a sub-hysteresis cut (under 0.7 permille) would
       simply be ignored and the pack would sit a few mA over the limit.
       Truncating division, never rounding - rounding up could re-cross it.
       [FA] سقف یک حد مطلق است و بعد از هیسترزیس اعمال می‌شود تا هیسترزیس
       هرگز نتواند دیوتی را بالای یک حد نگه دارد. همین است که پشتیبان‌های
       ۶۵۰ میلی‌آمپر و ۱۴٫۸ ولت را از «تمایل» به «تضمین» تبدیل می‌کند:
       بدون آن، کاهشی کوچک‌تر از هیسترزیس (زیر ۰٫۷ پرمیل) نادیده می‌رفت و
       باتری چند میلی‌آمپر بالای حد می‌ماند. تقسیم با قطع اعشار، نه گرد
       کردن - گرد کردن به بالا می‌تواند دوباره از حد رد شود. */
    if (int32_t__appliedPermille >
        (int32_t)(uint32_t__ceilingMilli / CHG_PID_DUTY_SCALE))
    {
        int32_t__appliedPermille = (int32_t)(uint32_t__ceilingMilli / CHG_PID_DUTY_SCALE);
    }

    charger_channel_state_t__channel->uint32_t__pidDutyMilli =
        (uint32_t)int32_t__appliedPermille * CHG_PID_DUTY_SCALE;
    charger_channel_state_t__channel->int32_t__pidLastError = int32_t__error;
    charger_channel_state_t__channel->bool__pidLastWasVoltage = bool__useVoltage;

    return (uint16_t)int32_t__appliedPermille;
}

static void func__Charger_RegulateChannel(uint8_t uint8_t__channelIndex,
                                          const measurement_snapshot_t *measurement_snapshot_t__snap,
                                          uint32_t uint32_t__nowTick)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint32_t uint32_t__batteryMv;
    uint32_t uint32_t__currentMa;
    uint32_t uint32_t__targetMv;
    uint32_t uint32_t__absorbTicks;
    uint16_t uint16_t__nextDuty;
    uint32_t uint32_t__downIntervalTicks;

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FINAL_FAULT)
    {
        return;
    }

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_JIT_RETRY_WAIT)
    {
        return;
    }

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_INPUT_WAIT)
    {
        return;
    }

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_BRINGUP)
    {
        return;
    }

    /* [EN] Battery-lost: the central Fault module owns detection and clear
       timing; Charger_Evaluate mirrors the bit into this state and back to
       OFF. As long as the state is BAT_LOST, never regulate.
       [FA] مالکیت تشخیص و پاک‌سازی قطع باتری با ماژول Fault است؛ تا وقتی
       وضعیت BAT_LOST است هیچ تنظیمی انجام نشود. */
    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_BAT_LOST)
    {
        return;
    }

    /* [EN] ESP enable gate (user order 2026-09-22: the ESP command panel
       must be able to cut and reconnect each charger module). Placed AFTER
       the fault/special states so a FINAL_FAULT latch is never released by
       this gate; while disabled the PWM is forced off and the state is held
       at OFF, so re-enabling soft-restarts BULK from 1% duty.
       [FA] گیت فعال‌سازی ESP (دستور کاربر ۲۰۲۶-۰۹-۲۲: پنل فرمان ESP باید
       بتواند هر ماژول شارژر را قطع/وصل کند). بعد از حالت‌های خطا/ویژه
       قرار گرفته تا قفل FINAL_FAULT هرگز با این گیت آزاد نشود؛ تا زمان
       غیرفعالی PWM قطع و وضعیت روی OFF نگه داشته می‌شود و با وصل دوباره
       BULK از duty ۱٪ نرم شروع می‌شود. */
    if (BOOL__G__ChargerEspEnableCh[uint8_t__channelIndex] == false)
    {
        func__Charger_StopOneChannel(uint8_t__channelIndex);
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_OFF;
        return;
    }

    /* [EN] Runtime fixed-duty mode (user order 2026-09-22): mirrors the
       compile-time bench-test block below - switching stops above
       CHG_ABSORB_MV (no overcharge with regulation off), state shows BULK,
       every protection cut above (JIT/input/battery/ESP) stays active.
       ApplyDuty clamps to min(compile max, runtime ceiling).
       [FA] مود duty فیکس زمان اجرا (دستور کاربر): آینهٔ بلوک بنچ
       کامپایل‌تایم پایین - بالای CHG_ABSORB_MV توقف سوئیچینگ، وضعیت BULK،
       همهٔ حفاظت‌های بالادست فعال. ApplyDuty به کمینهٔ سقف کامپایل و سقف
       زمان اجرا گیره می‌زند. */
    if (BOOL__G__ChargerDutyFixedEnable[uint8_t__channelIndex] != false)
    {
        uint32_t__batteryMv =
            func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap,
                                           uint8_t__channelIndex);
        /* [EN] Same overcharge guard as the bench-test mode: with the
           regulation loop off, stop switching at the absorb voltage.
           [FA] همان محافظ بیش‌شارژ مود تست بنچ: با خاموش‌بودن حلقهٔ
           تنظیم، سوئیچینگ در ولتاژ ابزورب متوقف می‌شود. */

        if (uint32_t__batteryMv >= CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv)
        {
            func__Charger_ApplyDuty(uint8_t__channelIndex, 0u);
            return;
        }

        if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_OFF)
        {
            charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
            func__Charger_ClearAbsorbWindow(
                charger_channel_state_t__channel);
        }
        charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
        func__Charger_ApplyDuty(uint8_t__channelIndex,
                                (uint16_t)UINT32_T__G__ChargerDutyFixedPermille[uint8_t__channelIndex]);
        return;
    }

    uint32_t__batteryMv =
        func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap, uint8_t__channelIndex);
    uint32_t__currentMa =
        func__Charger_ChannelCurrentMa(measurement_snapshot_t__snap, uint8_t__channelIndex);

    /* [EN] The snapshot current is primary-side; Bulk/Absorb/Float limits are
       output (battery) currents, so this normal-charge path decides with the
       converted value. The bring-up regulator above keeps primary mA.
       [FA] جریان snapshot سمت اولیه است؛ حدهای شارژ خروجی‌اند، پس مسیر نرمال با
       مقدار تبدیل‌شده تصمیم می‌گیرد. */
    uint32_t__currentMa =
        func__Charger_OutputEstimateMa(measurement_snapshot_t__snap,
                                       uint8_t__channelIndex,
                                       uint32_t__currentMa);

    /* [EN] Battery-disconnect detection used to live here; it moved to the
       central Fault module (fault.c, FAULT_BAT_*), which latches
       FAULT_CHARGER_BAT_LOST - see the mirror block in Charger_Evaluate.
       [FA] تشخیص قطع باتری به ماژول Fault منتقل شد؛ بلوک آینه در Evaluate. */
    if (func__Charger_BatteryVoltageIsValid(uint32_t__batteryMv) == false)
    {
        func__Charger_ResetChannelToOff(uint8_t__channelIndex);
        func__Charger_StopOneChannel(uint8_t__channelIndex);
        return;
    }

    if (uint32_t__currentMa > UINT32_T__G__ChargerHardFaultMa)
    {
        /* [EN] Only a hard over-current fault resets the channel; normal
           over-target is handled by the duty band below (no cut/restart).
           The value already passed Measurement's median-3 + moving-average
           chain (user order 2026-09-22), so this software cut reacts within
           one average window (~10 ms) - the hardware JIT comparator remains
           the fast over-current protection.
           [FA] فقط خطای سخت اضافه‌جریان کانال را ریست می‌کند؛ اضافهٔ عادی در
           باند دیوتی پایین‌تر مدیریت می‌شود. مقدار از زنجیرهٔ مدین-۳ +
           میانگین متحرک Measurement گذشته (دستور کاربر ۲۰۲۶-۰۹-۲۲) پس این
           قطع نرم‌افزاری حداکثر در حد یک پنجرهٔ میانگین (~۱۰ms) واکنش می‌دهد -
           حفاظت سریع اضافه‌جریان همچنان JIT سخت‌افزاری است. */
        func__Charger_ResetChannelToOff(uint8_t__channelIndex);
        func__Charger_StopOneChannel(uint8_t__channelIndex);
        return;
    }

    /* [EN] The charger adds no filter of its own on the current (user
       order 2026-09-22): the snapshot current is a clean PWM mid-ON
       synchronized sample, already filtered by Measurement's switchable
       median-3 / average-10 chain. The regulation band and the >950 mA
       hard fault both decide on that value; duty rate limits (one step
       per 500/1000 ms) prevent hunting; the hardware JIT comparator is
       the fast over-current protection.
       [FA] شارژر فیلتر خودش را روی جریان اضافه نمی‌کند (دستور کاربر):
       جریان snapshot نمونهٔ سنکرون وسط ON است که از زنجیرهٔ کلیددار
       مدین-۳ / میانگین-۱۰ Measurement گذشته. باند تنظیم و خطای سخت ۹۵۰mA
       با همین تصمیم می‌گیرند؛ محدودیت نرخ duty جلوی hunting را می‌گیرد و
       JIT سخت‌افزاری حفاظت سریع اضافه‌جریان است. */
#if (CHG_FIXED_DUTY_TEST_ENABLE != 0u)
    /* [EN] Bench diagnostic: fixed duty, no ramp/band/voltage regulation.
       Switching stops at the absorb voltage so the battery cannot be
       overcharged with regulation off; all protection cuts above stay active.
       [FA] حالت تست بنچ: دیوتی ثابت ۱۵٪؛ بالای ۱۴٫۴V سوئیچینگ متوقف؛ همهٔ
       حفاظت‌ها فعال‌اند. */
    if (uint32_t__batteryMv >= CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv)
    {
        func__Charger_ApplyDuty(uint8_t__channelIndex, 0u);
        return;
    }

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_OFF)
    {
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
        func__Charger_ClearAbsorbWindow(
            charger_channel_state_t__channel);
    }
    charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
    func__Charger_ApplyDuty(uint8_t__channelIndex, CHG_FIXED_DUTY_TEST_DUTY_PERMILLE);
    return;
#endif

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_OFF)
    {
        if (func__Charger_BulkStartSettled(uint8_t__channelIndex,
                                           uint32_t__nowTick) == false)
        {
            /* [EN] Connection not stable long enough yet (user directive
               2026-09-19: 10..20 s of "the battery is really connected"
               before any charge - 15 s): stay OFF at zero duty. This is also
               the flap-killer for battery-lost: after the fault bit clears
               (30 s), a still-missing cable keeps the voltage invalid, the
               stamp stays 0 and BULK never re-arms - no more re-pump /
               re-alarm ping-pong and no more yellow blink flashing inside
               the battery-lost buzzer window.
               [FA] اتصال هنوز ۱۵ ثانیه پایدار نیست (دستور کاربر: اول ثبات،
               بعد شارژ): OFF با دیوتی صفر می‌ماند. همین گیت چرخهٔ پینگ‌پنگ
               آلارم/بازتهیج آلارم و چشمک زرد وسط بوق قطع باتری را می‌کشد. */
            func__Charger_ApplyDuty(uint8_t__channelIndex, 0u);
            return;
        }
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
        func__Charger_ClearAbsorbWindow(
            charger_channel_state_t__channel);
        func__Charger_ApplyDuty(uint8_t__channelIndex, CHG_DUTY_START_PERMILLE);
        charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
        return;
    }

    uint32_t__targetMv = CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv;

    if ((charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FLOAT) &&
        (uint32_t__batteryMv < CHARGER_PROFILE_T__G__Profile.uint32_t__reentryMv))
    {
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
        func__Charger_ClearAbsorbWindow(
            charger_channel_state_t__channel);
        uint32_t__targetMv = CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv;
    }

    /* [EN] Absorb is a VOLTAGE-HOLD window now (user directive 2026-09-19):
       entered at 14.3 V, duty steps shrink to 0.1% so the 14.4 V setpoint
       holds steady without the old boundary hunting, the 10-minute soak
       counts only while 14.4..14.5 V, dipping below 14.3 V returns to BULK
       and RESETS the soak (his requirement until the offset case is solved),
       overshoot above 14.6 V gets coarse 0.5% down-steps to come back fast.
       [FA] ابزورب = پنجره تثبیت ولتاژ: ورود ۱۴٫۳V، پله ۰٫۱٪ برای نگه‌داشت
       ۱۴٫۴V بدون تلاطم مرز؛ شستشوی ۱۰ دقیقه فقط در ۱۴٫۴..۱۴٫۵V جمع می‌شود؛
       افت زیر ۱۴٫۳V برگشت به بالک + ریست شستشو؛ عبور از ۱۴٫۶V کاهش سریع
       ۰٫۵٪. */
    if (uint32_t__batteryMv >= CHARGER_PROFILE_T__G__Profile.uint32_t__absorbEnterMv)
    {
        if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_BULK)
        {
            charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_ABSORB;
            charger_channel_state_t__channel->uint32_t__absorbAccumTicks = 0u;
            charger_channel_state_t__channel->uint32_t__taperSinceTick = 0u;
            /* [EN] v1.17b: stamp the 1 h safety clock once - a dip-kept
               stamp survives (dip path), a fresh cycle stamps anew (its
               Clear zeroed the field). [FA] ساعت ۱ساعته فقط یک‌بار مهر
               می‌خورد: مهرِ نگه‌داشته‌شدهٔ افت می‌ماند، سیکل تازه مهر نو. */
            if (charger_channel_state_t__channel->uint32_t__absorbEnterTick == 0u)
            {
                charger_channel_state_t__channel->uint32_t__absorbEnterTick = uint32_t__nowTick;
            }
            charger_channel_state_t__channel->uint32_t__absorbLastTick = uint32_t__nowTick;
        }

        if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FLOAT)
        {
            uint32_t__targetMv = CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv;
        }
        else
        {
            uint32_t__targetMv = CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv;
        }

        if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_ABSORB)
        {
            uint32_t uint32_t__absorbDeltaTicks;

            uint32_t__absorbTicks = func__Charger_DurationTicks(CHG_LIM(CHG_LIMIT_PARAM_ABSORB_HOLD_MS));

            /* [EN] The soak counts during the WHOLE ABSORB stay (user
               directive 2026-09-19), from the 14.3 V entry through every
               overshoot episode - no sub-window. It resets only when the
               voltage falls below 14.3 V (else-branch), so the offset-affected
               equilibrium right under 14.4 V can no longer stall the soak.
               [FA] شستشو کل مدتِ حالت ابزورب جمع می‌شود (دستور کاربر)، بدون
               پنجره‌باریک؛ فقط زیر ۱۴٫۳V ریست می‌شود تا آفست تعادلِ مرز،
               شستشو را گیر نیندازد. */
            uint32_t__absorbDeltaTicks =
                (uint32_t)(uint32_t__nowTick -
                           charger_channel_state_t__channel->uint32_t__absorbLastTick);
            charger_channel_state_t__channel->uint32_t__absorbLastTick = uint32_t__nowTick;

            charger_channel_state_t__channel->uint32_t__absorbAccumTicks +=
                uint32_t__absorbDeltaTicks;
            if ((uint32_t__absorbTicks != 0u) &&
                (charger_channel_state_t__channel->uint32_t__absorbAccumTicks >
                 uint32_t__absorbTicks))
            {
                /* [EN] Clamp against long-soak overflow. / سقف برای اضافه‌سرریز. */
                charger_channel_state_t__channel->uint32_t__absorbAccumTicks =
                    uint32_t__absorbTicks;
            }

            bool bool__taperNow;
            bool bool__taperDone;
            bool bool__soakDone;
            bool bool__absorbTimedOut;
            uint32_t uint32_t__taperSustainTicks;
            uint32_t uint32_t__absorbMaxTicks;

            /* [EN] Absorb completion (user formula 2026-09-20, bench pack
               4.5 Ah): FLOAT begins when the minimum soak has passed AND the
               tail current stays below CHG_TAPER_CURRENT_MA (50 mA ~ C/90)
               steadily for CHG_LIM(CHG_LIMIT_PARAM_TAPER_SUSTAIN_MS) (60 s; the sense chain
               wobbles +/-10..20 mA so single dipping frames must not complete
               the charge). The CHG_LIM(CHG_LIMIT_PARAM_ABSORB_MAX_MS) = 1 hour ceiling ends
               absorb into FLOAT anyway, so a battery that never tapers
               cannot keep the pump awake forever.
               [FA] پایان ابزورب (فرمول کاربر، پک ۴٫۵ آمپرساعت): وقتی حداقل
               شستشو گذشته باشد **و** زیرجریان <۵۰mA به‌مدت پایدار ۶۰s مانده
               باشد FLOAT آغاز می‌شود؛ سقف امن یک‌ساعت در هرحال به FLOAT
               می‌فرستد تا باتریِ هرگز-تیپر‌نشده پمپ را بیدار نگه ندارد. */
            uint32_t__taperSustainTicks = func__Charger_DurationTicks(CHG_LIM(CHG_LIMIT_PARAM_TAPER_SUSTAIN_MS));
            uint32_t__absorbMaxTicks = func__Charger_DurationTicks(CHG_LIM(CHG_LIMIT_PARAM_ABSORB_MAX_MS));

            /* [EN] ARM the one-hour ceiling on CURRENT (user order 2026-09-29).
               Until the tail has actually come down below
               CHG_LIM(CHG_LIMIT_PARAM_ABSORB_MAX_ARM_MA) this episode, the ceiling does not count
               at all - a pack still pulling hundreds of milliamps is not
               finished, and ending its charge on a voltage-started clock is
               what made it sag past the 12.8 V reentry and begin again.
               Latched for the episode on purpose: the sense chain wobbles
               +/-10..20 mA, so re-arming on every frame that pops back above
               the threshold would let noise defeat the ceiling entirely.
               [FA] مسلح‌کردن سقف یک‌ساعته با «جریان» (دستور کاربر). تا وقتی
               جریان دنباله در این اپیزود واقعاً زیر آستانه نرفته باشد، سقف
               اصلاً نمی‌شمارد: پکی که هنوز صدها میلی‌آمپر می‌کشد تمام نشده، و
               پایان‌دادن شارژش با ساعتی که از روی ولتاژ شروع شده همان چیزی بود
               که باعث می‌شد تا زیر ۱۲٫۸ بیفتد و از نو شروع کند. عمداً برای کل
               اپیزود قفل می‌شود، چون زنجیرهٔ حس ±۱۰ تا ۲۰ میلی‌آمپر نوسان دارد. */
            if ((charger_channel_state_t__channel->uint32_t__absorbMaxArmTick == 0u) &&
                (uint32_t__currentMa < (uint32_t)CHG_LIM(CHG_LIMIT_PARAM_ABSORB_MAX_ARM_MA)))
            {
                charger_channel_state_t__channel->uint32_t__absorbMaxArmTick =
                    uint32_t__nowTick;
            }

            bool__taperNow = (uint32_t__currentMa < CHARGER_PROFILE_T__G__Profile.uint32_t__taperCurrentMa);
            if (bool__taperNow == false)
            {
                charger_channel_state_t__channel->uint32_t__taperSinceTick = 0u;
            }
            else if (charger_channel_state_t__channel->uint32_t__taperSinceTick == 0u)
            {
                charger_channel_state_t__channel->uint32_t__taperSinceTick = uint32_t__nowTick;
            }
            else
            {
                /* [EN] Taper already running. / زیرجریان قبلاً شروع شده. */
            }

            bool__taperDone =
                ((charger_channel_state_t__channel->uint32_t__taperSinceTick != 0u) &&
                 ((uint32_t)(uint32_t__nowTick -
                             charger_channel_state_t__channel->uint32_t__taperSinceTick) >=
                  uint32_t__taperSustainTicks));
            bool__soakDone =
                ((uint32_t__absorbTicks != 0u) &&
                 (charger_channel_state_t__channel->uint32_t__absorbAccumTicks >=
                  uint32_t__absorbTicks));
            bool__absorbTimedOut =
                ((charger_channel_state_t__channel->uint32_t__absorbMaxArmTick != 0u) &&
                 ((uint32_t)(uint32_t__nowTick -
                             charger_channel_state_t__channel->uint32_t__absorbMaxArmTick) >=
                  uint32_t__absorbMaxTicks));

            if (((bool__soakDone == true) && (bool__taperDone == true)) ||
                (bool__absorbTimedOut == true))
            {
                /* [EN] Episode over: drop the episode timers so the next
                   absorb starts from a clean sheet.
                   [FA] این سفرقسمت تمام شد؛ تایمرهای اپیزود صفر تا ابزورب بعدی
                   از صفحهٔ تمیز آغاز شود. */
                charger_channel_state_t__channel->uint32_t__taperSinceTick = 0u;
                charger_channel_state_t__channel->uint32_t__absorbEnterTick = 0u;
                charger_channel_state_t__channel->uint32_t__absorbMaxArmTick = 0u;
                charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_FLOAT;
                uint32_t__targetMv = CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv;
            }
        }
    }
    else
    {
        if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FLOAT)
        {
            /* [EN] FLOAT descending through and below the absorb window is
               the whole point of floating: hold 13.5 V, do NOT touch the
               soak bookkeeping and do NOT fall back to BULK (2026-09-19 bug
               caught on bench: the unguarded else->BULK restarted a fresh
               10-minute soak right after every soak completed, because the
               float descent crossed the window bottom).
               [FA] نزول فلوت به زیر پنجره جذب تعریف خود فلوت است: نگه‌داشت
               ۱۳٫۵V؛ نه شستشو دست می‌خورد نه به بالک برمی‌گردیم - باگ بنچ:
               else بی‌قید قبلی بلافاصله پس از اتمام شستشو به بالک پس می‌زد
               و شستشو را از صفر راه می‌انداخت. */
            uint32_t__targetMv = CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv;
        }
        else
        {
            /* [EN] Only while in ABSORB does a dip below the 14.3 V window
               bottom mean "the voltage hold failed": back to
               current-regulated BULK and RESET the soak (user directive).
               An already-BULK channel just stays BULK with a zeroed soak.
               v1.17b: the dip keeps the 1 h safety clock running
               (DipReset, not Clear), so repeated dips cannot postpone
               the forced FLOAT forever.
               [FA] فقط در حالت ابزورب افت زیر ۱۴٫۳V یعنی تثبیت شکست خورد:
               برگشت به بالک و ریست شستشو (دستور کاربر) ولی ساعت ۱ساعته
               نگه داشته می‌شود تا افت‌های پیاپی FLOAT اجباری را تا ابد
               عقب نیندازند. */
            charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
            uint32_t__targetMv = CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv;
            func__Charger_DipResetAbsorbWindow(
                charger_channel_state_t__channel);
        }
    }

    uint16_t__nextDuty = charger_channel_state_t__channel->uint16_t__dutyPermille;
    uint32_t__downIntervalTicks = func__Charger_DurationTicks(CHG_LIM(CHG_LIMIT_PARAM_RAMP_DOWN_INT_MS));

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FLOAT)
    {
        /* [EN] End of the charge cycle = PARK THE PUMP AT ZERO (user
           directive 2026-09-19: "why is the charger not off, why is duty
           still 4-5%"): step duty down to 0 on the coarse cadence and
           keep it parked; only the <12.8 V reentry (above) wakes BULK
           again.
           [FA] اتمام سیکل = پارک پمپ روی صفر (دستور کاربر: «چرا شارژر
           خاموش نیست، دیوتی هنوز ۴-۵٪ است؟»): دیوتی با ضرب‌آهنگ زبری تا
           صفر پایین می‌آید و پارک می‌ماند؛ فقط reentry زیر ۱۲٫۸V دوباره
           بالک را بیدار می‌کند. */
        if ((uint16_t__nextDuty != 0u) &&
            ((uint32_t)(uint32_t__nowTick -
                        charger_channel_state_t__channel->uint32_t__lastDutyStepTick) >=
             uint32_t__downIntervalTicks))
        {
            if (uint16_t__nextDuty > CHG_DUTY_STEP_PERMILLE)
            {
                uint16_t__nextDuty = (uint16_t)(uint16_t__nextDuty - CHG_DUTY_STEP_PERMILLE);
            }
            else
            {
                uint16_t__nextDuty = 0u;
            }
            charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
        }
        /* [EN] FLOAT is parked, not regulated (user directive 2026-09-19):
           keep the PID out of it and make the next reentry start from the
           real duty instead of a stale integral.
           [FA] فلوت پارک است نه تنظیم‌شده: PID را از آن بیرون نگه می‌داریم
           و بازگشت بعدی از دیوتی واقعی شروع می‌شود نه انتگرال کهنه. */
        func__Charger_PidInvalidate(charger_channel_state_t__channel);
    }
    else
    {
        /* [EN] v1.23 (user order 2026-09-29): the two-loop CC/CV PID is now the
           ONLY duty regulator - the fixed-step chain that used to live here
           is deleted, not merely bypassed. The state machine above already
           chose the setpoint and did the soak/taper/dip bookkeeping;
           everything the old chain did - coarse/fine steps, the
           500/1000/2000 ms cadences, the over-voltage escape, the current
           band with its 20 mA hysteresis - is now the scheduled gains, the
           per-stage slew limit and the two non-tunable backstops inside
           PidStep. There is no fallback path and no enable flag any more.
           [FA] v1.23 (دستور کاربر ۲۰۲۶-۰۹-۲۹): PID دوحلقه‌ای حالا تنها
           تنظیم‌کنندهٔ دیوتی است - زنجیرهٔ پله‌ثابتی که اینجا بود حذف شده،
           نه فقط دور زده. ماشین حالت بالا ست‌پوینت را انتخاب و دفترداری
           شستشو/تیپر/افت را انجام داده؛ هرچه زنجیرهٔ قدیمی می‌کرد - پله‌های
           زبر و ریز، ضرب‌آهنگ‌های ۵۰۰/۱۰۰۰/۲۰۰۰ms، فرار اضافه‌ولتاژ، باند
           جریان با هیسترزیس ۲۰mA - حالا همان ضرایب زمان‌بندی‌شده و سقف شیب
           هر مرحله و دو پشتیبان غیرقابل‌تنظیم داخل PidStep است. دیگر نه
           مسیر جایگزینی هست نه پرچم فعال‌سازی. */
        uint16_t__nextDuty = func__Charger_PidStep(uint8_t__channelIndex,
                                                   uint32_t__batteryMv,
                                                   uint32_t__currentMa,
                                                   uint32_t__targetMv,
                                                   uint32_t__nowTick);
        charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
    }

    func__Charger_ApplyDuty(uint8_t__channelIndex, uint16_t__nextDuty);
}

/* ==================== Charger_Init ==================== */

void func__Charger_Init(void)
{
    uint8_t uint8_t__channelIndex;

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__installed =
            func__Charger_IsChannelInstalled(uint8_t__channelIndex);
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyBeforeTripPermille = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint8_t__jitTripCount = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state = CHG_STATE_OFF;
        func__Charger_ClearAbsorbWindow(
            &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex]);
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__retryDeadlineTick = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__lastDutyStepTick = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__stableFromTick = 0u;
        /* [EN] v1.22: clear the PID working set; the gains themselves are
           settable statics and are NOT reset here (same rule as the
           profile - module Inits never undo a panel/flash value).
           [FA] v1.22: پاک‌کردن مجموعهٔ کاری PID؛ خود ضرایب استاتیک‌های
           قابل‌تنظیم‌اند و اینجا ریست نمی‌شوند (همان قانون پروفایل: Init
           ماژول هیچ‌وقت مقدار پنل/فلش را برنمی‌گرداند). */
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__pidDutyMilli = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].int32_t__pidIntegral = 0;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].int32_t__pidIntegralRem = 0;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].int32_t__pidLastError = 0;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__pidLastTick = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__pidSeeded = false;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__pidLastWasVoltage = true;
    }

    BOOL__G__ChargerInitialized = true;
    BOOL__G__RelayOpen = false;
    UINT32_T__G__RelaySettleDeadline = 0u;
    UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;

    func__Charger_SafeIdle();
}

/* ==================== Manual test mode helpers / هلپرهای مود تست دستی ==================== */

/**
 * @brief  [EN] Adopt the manual test mode: stamp the link dead-man and
 *              clear the battery-lost fault bit so its alarm cannot fire
 *              while bench testing (fault.c freezes the detection while
 *              the mode is active). The channel drive happens in the same
 *              Evaluate pass below.
 *         [FA] مود تست دستی را برمی‌دارد: مهر ددمنِ لینک و پاک‌کردن بیت
 *              خطای قطع باتری تا آلارمش حین تست بنچ نیفتد (fault.c تا
 *              وقتی مود فعال است تشخیص را فریز می‌کند). درایو کانال در
 *              همان پاس Evaluate پایین‌تر انجام می‌شود.
 * @param  uint32_t__nowTick [EN] Current kernel tick / تیک فعلی هسته
 */
static void func__Charger_EnterManualTestMode(uint32_t uint32_t__nowTick)
{
    BOOL__G__ChargerManualModeActive = true;
    UINT32_T__G__ManualLastLinkTick = uint32_t__nowTick;

#if MODULE_FAULT
    func__Fault_Clear(FAULT_CHARGER_BAT_LOST);
#endif
}

/**
 * @brief  [EN] Leave the manual test mode (panel off-switch or link
 *              dead-man): both duties drop to 0 and every channel
 *              restarts the AUTONOMOUS charger from OFF with fresh
 *              bookkeeping (the connection settle applies again). A
 *              latched FINAL_FAULT is never released here; the JIT trip
 *              latches of parked channels are cleared so the comparator
 *              can fire again.
 *         [FA] خروج از مود تست دستی (کلید خاموش پنل یا ددمن لینک): هر
 *              دو duty صفر و هر کانال شارژر خودکار را از OFF با دفترچهٔ
 *              تمیز ری‌استارت می‌کند (ثبات اتصال دوباره اعمال می‌شود).
 *              قفل FINAL_FAULT هرگز اینجا آزاد نمی‌شود؛ لچ‌های JIT
 *              کانال‌های پارک‌شده پاک می‌شوند تا کمپریتور دوباره شلیک کند.
 */
static void func__Charger_ExitManualTestMode(void)
{
    uint8_t uint8_t__channelIndex;
    charger_channel_state_t *charger_channel_state_t__channel;

    BOOL__G__ChargerManualModeRequested = false;
    BOOL__G__ChargerManualModeActive = false;

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        charger_channel_state_t__channel =
            &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];

#if MODULE_JITTER
        if (charger_channel_state_t__channel->charger_state_t__state ==
            CHG_STATE_JIT_RETRY_WAIT)
        {
            func__Jitter_ClearChannel((uint8_t)(uint8_t__channelIndex + 1u));
        }
#endif

        if (charger_channel_state_t__channel->charger_state_t__state !=
            CHG_STATE_FINAL_FAULT)
        {
            charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_OFF;
        }

        func__Charger_StopOneChannel(uint8_t__channelIndex);
        func__Charger_ClearAbsorbWindow(charger_channel_state_t__channel);
        charger_channel_state_t__channel->uint32_t__retryDeadlineTick = 0u;
        charger_channel_state_t__channel->uint32_t__lastDutyStepTick = 0u;
        charger_channel_state_t__channel->uint32_t__stableFromTick = 0u;
        charger_channel_state_t__channel->uint8_t__jitTripCount = 0u;
        charger_channel_state_t__channel->uint16_t__dutyBeforeTripPermille = 0u;
        BOOL__G__ChargerManualRearmRequest[uint8_t__channelIndex] = false;
    }

    UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
}

/**
 * @brief  [EN] Drive one channel in manual test mode: the commanded duty
 *              (the stored fixed-duty value, param 16/18) is applied
 *              directly - no ramp, no regulation loop, no battery gates.
 *              The hardware floor stays: the ESP channel cut forces 0,
 *              the 15.0 V hard overvoltage cutoff (CHG_MAX_VALID_BATTERY_MV)
 *              forces 0 until the voltage falls back, and ApplyDuty clamps
 *              to min(compile max, runtime ceiling). State shows MANUAL.
 *         [FA] درایو یک کانال در مود تست دستی: duty فرمان‌شده (مقدار فیکس
 *              ذخیره‌شده، پارامتر ۱۶/۱۸) مستقیم اعمال می‌شود - بدون رمپ،
 *              بدون حلقهٔ تنظیم، بدون گیت باتری. کف سخت‌افزاری می‌ماند:
 *              قطع ESP کانال → صفر، قطع سخت ۱۵٫۰V (CHG_MAX_VALID_BATTERY_MV)
 *              → صفر تا افت ولتاژ، و ApplyDuty به کمینهٔ سقف کامپایل و سقف
 *              زمان اجرا گیره می‌زند. وضعیت MANUAL نشان داده می‌شود.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  measurement_snapshot_t__snap [EN] Snapshot / نمونه
 */
static void func__Charger_ManualDriveChannel(uint8_t uint8_t__channelIndex,
                                             const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    charger_channel_state_t *charger_channel_state_t__channel;

    charger_channel_state_t__channel =
        &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];
    charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_MANUAL;

    /* [EN] ESP channel cut (param 11/12): duty 0, the state stays MANUAL.
       [FA] قطع ESP کانال (پارامتر ۱۱/۱۲): duty صفر، وضعیت MANUAL می‌ماند. */
    if (BOOL__G__ChargerEspEnableCh[uint8_t__channelIndex] == false)
    {
        func__Charger_ApplyDuty(uint8_t__channelIndex, 0u);
        return;
    }

    /* [EN] Hard overvoltage floor: with regulation off and no battery
       clamping the output, stop switching at 15.0 V and resume by itself
       once the voltage is back below it (protocol v1.2, section 5.2).
       [FA] کف سخت اضافه‌ولتاژ: با تنظیمِ خاموش و باتری‌ای که ولتاژ را
       نگه ندارد، سوئیچینگ در ۱۵٫۰V متوقف و با افت زیرش خودکار ادامه
       می‌یابد (پروتکل v1.2، بخش 5.2). */
    if (func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap,
                                       uint8_t__channelIndex) >= UINT32_T__G__ChargerOvCutoffMv)
    {
        func__Charger_ApplyDuty(uint8_t__channelIndex, 0u);
        return;
    }

    func__Charger_ApplyDuty(
        uint8_t__channelIndex,
        (uint16_t)UINT32_T__G__ChargerDutyFixedPermille[uint8_t__channelIndex]);
}

/* ==================== Charger suspension API ==================== */

/**
 * @brief  [EN] Set the NVM-save suspension flag (see header contract).
 *         [FA] ست‌کردن پرچم تعلیق ذخیرهٔ NVM (قرارداد هدر).
 * @param  bool__suspended [EN] true = hold gates at 0 / گیت‌ها صفر نگه داشته شوند
 */
void func__Charger_SetSuspended(bool bool__suspended)
{
    BOOL__G__ChargerSuspended = bool__suspended;
}

/**
 * @brief  [EN] Read the NVM-save suspension flag.
 *         [FA] خواندن پرچم تعلیق ذخیرهٔ NVM.
 * @return bool [EN] true = suspension active / تعلیق فعال است
 */
bool func__Charger_IsSuspended(void)
{
    return BOOL__G__ChargerSuspended;
}

/* ==================== Charger_Evaluate ==================== */
/**
 * @brief  [EN] Refresh the live diag array and the calibration worksheet
 *         array with every decision value.
 *         [FA] آرایهٔ دیاگ زنده و آرایهٔ برگهٔ کالیبراسیون را با همهٔ
 *         مقادیر تصمیم به‌روز می‌کند.
 * @param  measurement_snapshot_t__snap [EN] Current snapshot (may be NULL) /
 *         snapshot فعلی (می‌تواند NULL باشد)
 */
static void func__Charger_CaptureDiag(const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    uint8_t uint8_t__channelIndex;
    uint32_t uint32_t__base;
    uint32_t uint32_t__calibBase;
    uint32_t uint32_t__primaryMa;
    bool bool__snapValid;

    bool__snapValid =
        ((measurement_snapshot_t__snap != NULL) &&
         (measurement_snapshot_t__snap->valid == true));

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        uint32_t__base =
            (uint32_t)uint8_t__channelIndex * CHG_DIAG_CHANNEL_STRIDE;
        uint32_t__calibBase =
            CHG_CALIB_CH_UP_BASE +
            ((uint32_t)uint8_t__channelIndex * CHG_CALIB_CH_STRIDE);

        UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_STATE] =
            (uint32_t)CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state;
        UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_DUTY] =
            (uint32_t)CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille;
        UINT32_T__G__ChargerCalib[uint32_t__calibBase + CHG_CALIB_OFF_STATE] =
            (uint32_t)CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state;
        UINT32_T__G__ChargerCalib[uint32_t__calibBase + CHG_CALIB_OFF_DUTY] =
            (uint32_t)CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille;

        if (bool__snapValid == true)
        {
            UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_VBAT] =
                func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap,
                                               uint8_t__channelIndex);
            uint32_t__primaryMa =
                func__Charger_ChannelCurrentMa(measurement_snapshot_t__snap,
                                               uint8_t__channelIndex);
            UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_IPRI] =
                uint32_t__primaryMa;
            UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_IEST] =
                func__Charger_OutputEstimateMa(measurement_snapshot_t__snap,
                                               uint8_t__channelIndex,
                                               uint32_t__primaryMa);
            UINT32_T__G__ChargerCalib[uint32_t__calibBase + CHG_CALIB_OFF_VBAT] =
                UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_VBAT];
            UINT32_T__G__ChargerCalib[uint32_t__calibBase + CHG_CALIB_OFF_IPRI] =
                uint32_t__primaryMa;
            UINT32_T__G__ChargerCalib[uint32_t__calibBase + CHG_CALIB_OFF_IEST] =
                UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_IEST];
            if (uint8_t__channelIndex == 0u)
            {
                UINT32_T__G__ChargerIest1Ma =
                    UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_IEST];
            }
            else
            {
                UINT32_T__G__ChargerIest2Ma =
                    UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_IEST];
            }
        }
        else
        {
            UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_VBAT] = 0u;
            UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_IPRI] = 0u;
            UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_IEST] = 0u;
            UINT32_T__G__ChargerCalib[uint32_t__calibBase + CHG_CALIB_OFF_VBAT] = 0u;
            UINT32_T__G__ChargerCalib[uint32_t__calibBase + CHG_CALIB_OFF_IPRI] = 0u;
            UINT32_T__G__ChargerCalib[uint32_t__calibBase + CHG_CALIB_OFF_IEST] = 0u;
            if (uint8_t__channelIndex == 0u)
            {
                UINT32_T__G__ChargerIest1Ma = 0u;
            }
            else
            {
                UINT32_T__G__ChargerIest2Ma = 0u;
            }
        }
    }

    if (bool__snapValid == true)
    {
        UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_VIN] =
            measurement_snapshot_t__snap->v_in_mv;
        UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_V24] =
            measurement_snapshot_t__snap->v_bat24_mv;
        UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_V12] =
            measurement_snapshot_t__snap->v_bat12_mv;
        UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_VHIGH] =
            measurement_snapshot_t__snap->v_bat_high_mv;
        UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_VIN] =
            measurement_snapshot_t__snap->v_in_mv;
        UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_V24] =
            measurement_snapshot_t__snap->v_bat24_mv;
        UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_V12] =
            measurement_snapshot_t__snap->v_bat12_mv;
        UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_VHIGH] =
            measurement_snapshot_t__snap->v_bat_high_mv;
    }
    else
    {
        UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_VIN] = 0u;
        UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_V24] = 0u;
        UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_V12] = 0u;
        UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_VHIGH] = 0u;
        UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_VIN] = 0u;
        UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_V24] = 0u;
        UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_V12] = 0u;
        UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_VHIGH] = 0u;
    }

#if MODULE_FAULT
    UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_FAULT] = (uint32_t)func__Fault_Get();
#else
    UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_FAULT] = 0u;
#endif

    UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_VALID] =
        ((bool__snapValid == true) ? 1u : 0u);
    UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_VALID] =
        ((bool__snapValid == true) ? 1u : 0u);
}


void func__Charger_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap,
                            app_state_t app_state_t__state)
{
    uint32_t uint32_t__nowTick;
    uint8_t uint8_t__channelIndex;
    bool bool__anyFinalFault;
    bool bool__inputAdcValid;
    bool bool__controlAllowed;

    (void)app_state_t__state;

    if (BOOL__G__ChargerInitialized == false)
    {
        func__Charger_Init();
    }

    /* [EN] NVM-save suspension (user order 2026-09-27): hold both gates at
       0 and skip the pass. State, soak and settle are untouched, so the
       resume continues seamlessly. Manual/dead-man/fault-mirror bookkeeping
       below pauses for the ~0.1 s save - all their time constants are
       seconds (the dead-man is 3 s).
       [FA] تعلیق ذخیرهٔ NVM: هر دو گیت صفر و پاس رد می‌شود. حالت و شستشو
       دست نمی‌خورند پس ادامه یکپارچه است. */
    if (BOOL__G__ChargerSuspended != false)
    {
        func__BspPwm_StopAll();
        return;
    }

    uint32_t__nowTick = osKernelGetTickCount();

    /* [EN] Manual test mode transitions (user order 2026-09-23, protocol
       v1.2 param 19): the ESP link task only writes the REQUEST; every
       enter/exit action runs here in the charger context, so no PWM or
       state write ever races between tasks. Entering also clears the
       battery-lost bit BEFORE the mirror below so a pre-latched alarm
       cannot hold the channels hostage during a bench session (fault.c
       freezes the detection while the mode is active).
       [FA] گذارهای مود تست دستی (دستور کاربر ۲۰۲۶-۰۹-۲۳، پارامتر ۱۹
       v1.2): تسک ESP فقط «درخواست» را می‌نویسد؛ همهٔ عملیات ورود/خروج
       اینجا در زمینهٔ شارژر اجرا می‌شود تا هیچ نوشتنِ PWM یا وضعیتی بین
       تسک‌ها مسابقه نکند. ورود، بیت قطع باتری را قبل از آینهٔ پایین پاک
       می‌کند تا آلارمِ از قبل قفل‌شده کانال‌ها را گروگان نگیرد (fault.c
       تا وقتی مود فعال است تشخیص را فریز می‌کند). */
    if (BOOL__G__ChargerManualModeRequested != BOOL__G__ChargerManualModeActive)
    {
        if (BOOL__G__ChargerManualModeRequested != false)
        {
            func__Charger_EnterManualTestMode(uint32_t__nowTick);
        }
        else
        {
            func__Charger_ExitManualTestMode();
        }
    }

    /* [EN] Link dead-man: while the mode is active the panel must keep the
       link alive; 3 s of silence (CHG_LIM(CHG_LIMIT_PARAM_MANUAL_WATCHDOG_MS)) drops both duties
       to 0 and returns the charger to autonomous operation.
       [FA] ددمنِ لینک: تا وقتی مود فعال است پنل باید لینک را زنده نگه
       دارد؛ ۳ ثانیه سکوت (CHG_LIM(CHG_LIMIT_PARAM_MANUAL_WATCHDOG_MS)) هر دو duty را صفر و
       شارژر را به حالت خودکار برمی‌گرداند. */
    if ((BOOL__G__ChargerManualModeActive != false) &&
        (func__Charger_DeadlineElapsed(
             uint32_t__nowTick,
             UINT32_T__G__ManualLastLinkTick +
                 func__Charger_DurationTicks(CHG_LIM(CHG_LIMIT_PARAM_MANUAL_WATCHDOG_MS))) != false))
    {
        func__Charger_ExitManualTestMode();
    }

#if MODULE_FAULT
    /* [EN] Battery-lost mirror (detection and clear timing live in the Fault
       module). While the bit is latched, stop PWM once per channel and hold
       the state at BAT_LOST; when Fault clears the bit, release the channel
       to OFF so the first normal pass soft-restarts BULK at 1% duty. Running
       here BEFORE the FAULT/SAFE gate keeps the mirror working while the bit
       forces app_state FAULT; SafeIdle preserves BAT_LOST states meanwhile.
       [FA] آینهٔ پرچم قطع باتری: وقتی بیت قفل است یک‌بار PWM را قطع و کانال
       را BAT_LOST نگه می‌داریم؛ با پاک‌شدن بیت کانال به OFF آزاد می‌شود تا
       رمپ نرم از ۱٪ شروع شود. این بلوک قبل از گِیت FAULT اجرا می‌شود. */
    {
        bool bool__batLostLatched;

        bool__batLostLatched =
            ((func__Fault_Get() & FAULT_CHARGER_BAT_LOST) != FAULT_NONE);

        for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
        {
            /* [EN] A FINAL_FAULT latch is never overwritten by the mirror
               (full-program audit 2026-09-26): bat-lost arriving AFTER the
               third JIT used to demote the channel to BAT_LOST, wiping the
               final state (only the trip count survived). The fault bit
               still forces app FAULT + SafeIdle, so nothing is lost by
               skipping - the relay/pwm handling is identical.
               [FA] قفل FINAL_FAULT هرگز با آینه بازنویسی نمی‌شود (ممیزی کل
               برنامه): قطع باتریِ بعد از سومین JIT، کانال را به BAT_LOST
               تنزل می‌داد و حالت نهایی را پاک می‌کرد (فقط شمارش می‌ماند).
               بیت خطا همچنان FAULT و SafeIdle را می‌آورد، پس با ردشدن چیزی
               گم نمی‌شود - رفتار رله/PWM یکسان است. */
            if ((bool__batLostLatched == true) &&
                (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state !=
                 CHG_STATE_BAT_LOST) &&
                (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state !=
                 CHG_STATE_FINAL_FAULT))
            {
                func__Charger_StopOneChannel(uint8_t__channelIndex);
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
                    CHG_STATE_BAT_LOST;
            }
            else if ((bool__batLostLatched == false) &&
                     (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state ==
                      CHG_STATE_BAT_LOST))
            {
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
                    CHG_STATE_OFF;
            }
            else
            {
                /* [EN] Already mirrored. [FA] هم‌اکنون هماهنگ است. */
            }
        }
    }
#endif

    /* [EN] Refresh the diag array before any gate so it stays live in every
       path (safe-idle, fault, normal charge). Duty/state are the values the
       previous pass applied; the current/estimate slots are exactly what the
       regulation below will decide on this pass.
       [FA] آرایهٔ دیاگ قبل از هر گِیت به‌روز می‌شود تا در همهٔ مسیرها زنده
       بماند؛ duty/state مقدار اعمال‌شدهٔ پاس قبل است و جریان/تخمین دقیقاً
       همان چیزی است که تنظیم پایین‌تر در همین پاس رویش تصمیم می‌گیرد. */
    func__Charger_CaptureDiag(measurement_snapshot_t__snap);

    if (CHG_MASTER_ENABLE == 0u)
    {
        func__Charger_SafeIdle();
        return;
    }

    if ((measurement_snapshot_t__snap == NULL) ||
        (measurement_snapshot_t__snap->valid == false) ||
        (CHG_INSTALLED_CHANNEL_MASK == 0u) ||
        (app_state_t__state == APP_STATE_FAULT) ||
        (app_state_t__state == APP_STATE_SAFE))
    {
        func__Charger_SafeIdle();
        return;
    }

    bool__controlAllowed = false;
    if (CHG_TRANSFORMER_KNOWN != 0u)
    {
        bool__controlAllowed = true;
    }
    else if (CHG_BRINGUP_TEST_ENABLE != 0u)
    {
        bool__controlAllowed = true;
    }

    if (bool__controlAllowed == false)
    {
        func__Charger_SafeIdle();
        return;
    }

    bool__inputAdcValid = (measurement_snapshot_t__snap->v_in_mv >= CHG_INPUT_VALID_MV);

    /* [EN] Connection-settle bookkeeping for every installed channel
       (user refinement 2026-09-19): the 15-second count must START FROM THE
       MOMENT BATTERY VOLTAGE IS SEEN - the stamp ticks up while the
       snapshot is live and this channel's battery voltage is in range, any
       lapse resets it to 0. The input is deliberately NOT part of the
       predicate (input validity gates the bulk start separately upstream),
       so a battery that sat connected for minutes charges ~instantly when
       the mains comes back.
       [FA] دفترچهٔ ثبات اتصال (اصلاحیهٔ کاربر): شمارش ۱۵ ثانیه از لحظهٔ
       دیده‌شدن ولتاژ باتری شروع می‌شود - فقط snapshot سالم + ولتاژ باتری در
       محدوده؛ ورودی عمداً در این شرط نیست چون گیت جداگانه‌اش بالادست است؛
       پس باتری‌ای که از قبل وصل است با برگشت برق تقریباً بلافاصله شارژ
       می‌شود. */
    {
        for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
        {
            bool bool__readyNow;

            bool__readyNow =
                (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__installed == true) &&
                (func__Charger_BatteryVoltageIsValid(
                    func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap,
                                                   uint8_t__channelIndex)) == true);

            if (bool__readyNow == true)
            {
                if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__stableFromTick == 0u)
                {
                    CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__stableFromTick =
                        uint32_t__nowTick;
                }
            }
            else
            {
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__stableFromTick = 0u;
            }
        }
    }

    bool__anyFinalFault = false;
    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state ==
            CHG_STATE_FINAL_FAULT)
        {
            bool__anyFinalFault = true;
        }
    }

    if (bool__anyFinalFault == true)
    {
        /* [EN] With the input cut, keep the latch but release the coil so it
           cannot drain the battery; FinalDisconnect resumes when input returns.
           [FA] با ورودی قطع، قفل خطا بماند ولی کویل رها شود تا باتری خالی نشود. */
        if (bool__inputAdcValid == true)
        {
            func__Charger_FinalDisconnect();
        }
        else
        {
            func__Charger_FinalDisconnectIdle();
        }
        return;
    }

    if (bool__inputAdcValid == false)
    {
        /* [EN] SafeIdle FIRST, then stamp INPUT_WAIT (full-program audit
           2026-09-26): the old order wrote INPUT_WAIT and immediately
           wiped it back to OFF inside SafeIdle, so the state was never
           observable. BAT_LOST cannot be present here (its bit forces app
           FAULT, caught by the earlier gate), only FINAL is skipped.
           [FA] اول SafeIdle بعد مهر INPUT_WAIT (ممیزی کل برنامه): ترتیب
           قدیم INPUT_WAIT را می‌نوشت و بلافاصله داخل SafeIdle به OFF
           برمی‌گرداند، پس حالت هرگز دیده نمی‌شد. BAT_LOST اینجا ممکن
           نیست (بیتش FAULT می‌آورد و گیت قبلی می‌گیرد)، فقط FINAL رد می‌شود. */
        func__Charger_SafeIdle();
        for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
        {
            if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state !=
                CHG_STATE_FINAL_FAULT)
            {
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
                    CHG_STATE_INPUT_WAIT;
            }
        }
        return;
    }

#if MODULE_JITTER
    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        if ((CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__installed == true) &&
            (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state !=
             CHG_STATE_FINAL_FAULT) &&
            (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state !=
             CHG_STATE_JIT_RETRY_WAIT) &&
            (func__Jitter_ChannelTripped((uint8_t)(uint8_t__channelIndex + 1u)) == true))
        {
            /* [EN] Park EVERY tripped channel at zero duty immediately (two-
               channel audit 2026-09-20: the old "only while no retry is
               running" gate left the second channel pumping for up to one
               full 3 s lockout against an asserted comparator). Waiting in
               JIT_RETRY_WAIT is queued per channel; func__Charger_ServiceRetry
               revives them one at a time, so simultaneous restarts still
               cannot happen.
               [FA] کانال تریپ‌شده بلافاصله با دیوتی صفر پارک می‌شود (ممیزی
               دوکاناله: گیت قدیمی کانال دوم را تا ۳ ثانیه با کمپریتور مسلط
               در حال پمپ نگه می‌داشت)؛ احیای صف تک‌تک با ServiceRetry. */
            func__Charger_HandleJitTrip(uint8_t__channelIndex, uint32_t__nowTick);
        }
    }
#endif

    /* [EN] No automatic JIT revival in manual test mode: a parked channel
       waits for the human to re-send its duty value.
       [FA] در مود تست دستی احیای خودکار JIT نیست: کانال پارک‌شده منتظر
       فرستادن دوبارهٔ duty توسط کاربر می‌ماند. */
    if (BOOL__G__ChargerManualModeActive == false)
    {
        func__Charger_ServiceRetry(uint32_t__nowTick);
    }

    if (BOOL__G__RelayOpen == true)
    {
        if (bool__inputAdcValid == true)
        {
            func__Charger_FinalDisconnect();
        }
        else
        {
            func__Charger_FinalDisconnectIdle();
        }
        return;
    }

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__installed == true)
        {
            if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state ==
                CHG_STATE_INPUT_WAIT)
            {
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
                    CHG_STATE_OFF;
                func__Charger_ClearAbsorbWindow(
                    &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex]);
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint8_t__jitTripCount = 0u;
            }

            if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state ==
                CHG_STATE_JIT_RETRY_WAIT)
            {
                if ((BOOL__G__ChargerManualModeActive != false) &&
                    (BOOL__G__ChargerManualRearmRequest[uint8_t__channelIndex] != false))
                {
                    /* [EN] Manual JIT re-arm (protocol v1.2 section 5.2): a
                       fresh duty write is the re-arm gesture - clear the park
                       and the trip latch, then the manual drive below applies
                       the commanded duty in this same pass. The trip COUNT is
                       untouched, so the 3rd trip still latches FINAL_FAULT.
                       [FA] مسلح‌کردن دوبارهٔ JIT در مود دستی (بخش 5.2 ی
                       v1.2): نوشتن دوبارهٔ duty یعنی re-arm - پارک و لچ
                       تریپ پاک می‌شوند و درایو دستی پایین همان پاس duty
                       فرمان‌شده را اعمال می‌کند. شمارش تریپ دست نمی‌خورد،
                       پس سومین تریپ همچنان FINAL_FAULT را قفل می‌کند. */
                    BOOL__G__ChargerManualRearmRequest[uint8_t__channelIndex] = false;
                    CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__retryDeadlineTick = 0u;
                    if (UINT8_T__G__RetryChannel == uint8_t__channelIndex)
                    {
                        UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
                    }
#if MODULE_JITTER
                    func__Jitter_ClearChannel((uint8_t)(uint8_t__channelIndex + 1u));
#endif
                    CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
                        CHG_STATE_MANUAL;
                }
                else
                {
                    continue;
                }
            }

            if (BOOL__G__ChargerManualModeActive != false)
            {
                /* [EN] Manual test mode owns the channel: commanded duty,
                   battery gates bypassed, hardware floor only.
                   [FA] مود تست دستی مالک کانال است: duty فرمان‌شده، گیت‌های
                   باتری رد شده، فقط کف سخت‌افزاری. */
                func__Charger_ManualDriveChannel(uint8_t__channelIndex,
                                                 measurement_snapshot_t__snap);
            }
            else if ((CHG_TRANSFORMER_KNOWN == 0u) && (CHG_BRINGUP_TEST_ENABLE != 0u))
            {
                func__Charger_BringupRegulateChannel(uint8_t__channelIndex,
                                                     measurement_snapshot_t__snap);
            }
            else
            {
                func__Charger_RegulateChannel(uint8_t__channelIndex,
                                              measurement_snapshot_t__snap,
                                              uint32_t__nowTick);
            }
        }
        else
        {
            CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
            func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
        }
    }
}

/* ==================== Charger_IsAnyChannelActive ==================== */

/**
 * @brief  [EN] Report whether any installed channel is currently PUMPING
 *         charge into a battery (BULK / ABSORB only). OFF, JIT_RETRY_WAIT,
 *         INPUT_WAIT, FINAL_FAULT, BAT_LOST and - since 2026-09-19 - FLOAT
 *         (pump parked at zero duty = charge DONE) do not count. Used by
 *         (a) the UI Charging face (the full face keys on
 *         IsChargeComplete) and (b) the fault pump-window, so a transient
 *         above 14.8 V in the parked/done phase cannot catch the
 *         battery-lost buzzer.
 *         [FA] آیا کانال نصب‌شده‌ای واقعاً پمپ می‌کند؟ فقط BULK/ABSORB؛
 *         FLOAT پارک‌شده یعنی کار تمام است و حساب نمی‌شود (چهرهٔ فول با
 *         IsChargeComplete می‌آید) و آشکارساز قطع باتری هم آنجا مسلح نیست.
 * @return bool [EN] true if any installed channel is pumping / اگر کانالی پمپ کند true
 */
/**
 * @brief  [EN] True while ONE given channel (0 = charger 1 / upper
 *              battery, 1 = charger 2 / lower battery) is actually
 *              pumping: installed and sitting in BULK or ABSORB - the
 *              same predicate as func__Charger_IsAnyChannelActive, split
 *              per channel (v1.21, user order 2026-09-28) so the fault
 *              pump-rule can arm each battery half by ITS OWN charger: a
 *              parked channel has no pump, so its half cannot fly up to
 *              the disconnect threshold.
 *         [FA] کانالِ داده‌شده (۰ = شارژر ۱ / باتری بالا، ۱ = شارژر ۲ /
 *              باتری پایین) واقعاً پمپ می‌کند؟ نصب‌شده و در BULK یا
 *              ABSORB - همان محمولِ IsAnyChannelActive، تجزیه‌شده
 *              به‌ازای کانال (v1.21، دستور کاربر ۲۰۲۶-۰۹-۲۸) تا قانون
 *              پمپِ فالت هر نیم را با شارژر خودش مسلح کند: کانال
 *              پارک‌شده پمپی ندارد پس نیمش نمی‌تواند تا آستانهٔ قطع بالا
 *              پرود.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @return bool [EN] true while that channel pumps / وقتی همان کانال پمپ کند
 */
bool func__Charger_IsChannelActive(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex >= 2u)
    {
        return false;
    }

    return ((CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__installed == true) &&
            ((CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state ==
              CHG_STATE_BULK) ||
             (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state ==
              CHG_STATE_ABSORB)));
}

bool func__Charger_IsAnyChannelActive(void)
{
    uint8_t uint8_t__channelIndex;

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        if (func__Charger_IsChannelActive(uint8_t__channelIndex) == true)
        {
            return true;
        }
    }

    return false;
}

/* ==================== Charger_IsChargeComplete ==================== */

/**
 * @brief  [EN] True when every relevant channel finished its charge: at
 *         least one installed+enabled channel exists and ALL of them sit
 *         in FLOAT (entered from ABSORB done only - soak + taper, or the
 *         1 h safety ceiling). Disabled channels (bench SOLO runs) and
 *         uninstalled channels are excluded, never blockers.
 *         [FA] شارژ همهٔ کانال‌های مربوط (نصب+فعال) کامل شده؟ دست‌کم یکی
 *         هست و همه در FLOATاند. کانال غیرفعال/نصب‌نشده کنار گذاشته
 *         می‌شود و مانع نیست.
 * @return bool [EN] true if the charge is complete on all relevant channels /
 *         اگر شارژ کامل شده true
 */
bool func__Charger_IsChargeComplete(void)
{
    uint8_t uint8_t__channelIndex;
    uint8_t uint8_t__relevantCount = 0u;

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        if ((CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__installed == true) &&
            (func__Charger_GetChannelEspEnable(uint8_t__channelIndex) != false))
        {
            uint8_t__relevantCount = (uint8_t)(uint8_t__relevantCount + 1u);
            if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state !=
                CHG_STATE_FLOAT)
            {
                return false;
            }
        }
    }

    return (uint8_t__relevantCount > 0u);
}

/* ==================== Charger runtime config API (ESP panel) ==================== */

/**
 * @brief  [EN] Set the runtime conversion factor of one channel, clamped
 *              to 0..999 permille (v1.3): 0 = identity bypass (compiled
 *              default), non-zero = live iest = I x Vin x eta / (1000 x
 *              Vbat). Channel 0 = charger 1 (upper battery), 1 = charger 2.
 *              Flash-persisted since v1.14 (NVM ids 9/10); written by the
 *              ESP panel (params 9/10) and by CAL_REFERENCE (user order
 *              2026-09-24).
 *         [FA] ضریب تبدیل زمان اجرای یک کانال، گیرهٔ ۰..۹۹۹ پرمیل (v1.3):
 *              صفر = همانی (پیش‌فرض)، غیرصفر = تبدیل زندهٔ iest = I × Vin ×
 *              η ÷ (۱۰۰۰ × Vbat). کانال ۰ = شارژر ۱ (باتری بالا)، ۱ = شارژر
 *              ۲. روی فلش از v1.14 (شناسه ۹/۱۰)؛ از پنل ESP و CAL_REFERENCE.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  uint32_t__etaPermille [EN] Requested efficiency / بازدهی درخواستی
 * @return uint32_t [EN] Applied efficiency permille / بازدهی اعمال‌شده
 */
uint32_t func__Charger_SetEfficiencyPermille(uint8_t uint8_t__channelIndex,
                                             uint32_t uint32_t__etaPermille)
{
    /* [EN] The lower clamp compiles out while CHG_ETA_MIN_PERMILLE is 0
       (a u32 can never be below 0; keeps -Wtype-limits green). If the floor
       ever rises above zero the clamp reactivates automatically.
       [FA] گیرهٔ پایین تا وقتی کف صفر است کامپایل نمی‌شود (u32 هرگز زیر صفر
       نیست). اگر کف روزی بالای صفر رفت، گیره خودکار برمی‌گردد. */
#if (CHG_ETA_MIN_PERMILLE != 0u)
    if (uint32_t__etaPermille < CHG_ETA_MIN_PERMILLE)
    {
        uint32_t__etaPermille = CHG_ETA_MIN_PERMILLE;
    }
#endif
    if (uint32_t__etaPermille > CHG_ETA_MAX_PERMILLE)
    {
        uint32_t__etaPermille = CHG_ETA_MAX_PERMILLE;
    }
    else
    {
        /* [EN] Value already inside the window. [FA] مقدار داخل پنجره است. */
    }

    if (uint8_t__channelIndex == 0u)
    {
        UINT32_T__G__ChargerEta1Permille = uint32_t__etaPermille;
    }
    else
    {
        UINT32_T__G__ChargerEta2Permille = uint32_t__etaPermille;
    }

    return uint32_t__etaPermille;
}

/**
 * @brief  [EN] Read the live flyback efficiency of one channel.
 *         [FA] بازدهی flyback زندهٔ یک کانال را می‌خواند.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @return uint32_t [EN] Live efficiency permille / بازدهی زندهٔ پرمیل
 */
uint32_t func__Charger_GetEfficiencyPermille(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return UINT32_T__G__ChargerEta1Permille;
    }

    return UINT32_T__G__ChargerEta2Permille;
}

/**
 * @brief  [EN] Set the ESP enable gate of one charger channel. false = cut:
 *              PWM forced off and the state held at OFF (a latched
 *              FINAL_FAULT is never released by this gate); true = reconnect
 *              with the normal soft BULK restart from 1% duty.
 *              Flash-persisted since v1.14 - a reboot keeps the gates
 *              (ESP panel, user order 2026-09-22).
 *         [FA] گیت فعال‌سازی ESP یک کانال شارژر. false = قطع: PWM قطع و
 *              وضعیت روی OFF نگه داشته می‌شود (قفل FINAL_FAULT هرگز با این
 *              گیت آزاد نمی‌شود)؛ true = وصل با ری‌استارت نرم BULK از duty
 *              ۱٪. روی فلش می‌ماند از نسخهٔ ۱.۱۴ - ری‌استارت گیت‌ها را
 *              نگه می‌دارد (پنل ESP، دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  bool__enable [EN] true = channel allowed / کانال آزاد
 */
void func__Charger_SetChannelEspEnable(uint8_t uint8_t__channelIndex, bool bool__enable)
{
    if (uint8_t__channelIndex < 2u)
    {
        BOOL__G__ChargerEspEnableCh[uint8_t__channelIndex] = bool__enable;
    }
}

/**
 * @brief  [EN] Read the ESP enable gate of one charger channel.
 *         [FA] گیت فعال‌سازی ESP یک کانال شارژر را می‌خواند.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @return bool [EN] true = channel allowed / کانال آزاد
 */
bool func__Charger_GetChannelEspEnable(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex < 2u)
    {
        return BOOL__G__ChargerEspEnableCh[uint8_t__channelIndex];
    }

    return true;
}

/* ==================== Manual test mode API / API مود تست دستی ==================== */

/* ==================== Charge Profile (user order 2026-09-25) ==================== */

/* [EN] Interdependency clamps: after ANY profile write the whole set is
 *      re-clamped so it stays physically consistent. The hard safety stack
 *      (the hard-fault trip, CHG_MAX_VALID_BATTERY_MV 15000, the
 *      15.0 V hardware cutoff) can NEVER be raised above the compile maxima
 *      from the panel (v1.15: ids 35..37 lower them only) - ABSORB tops
 *      out at 14.6 V and the current band at 900 mA (limit = band+25).
 * [FA] گیره‌های وابستگی: بعد از هر نوشتن، کل مجموعه دوباره گیره می‌خورد تا
 *      فیزیکیِ سازنده بماند. پشتهٔ ایمنی سخت از پنل هرگز بالای سقف کامپایل
 *      نمی‌رود (v1.15: شناسه‌های ۳۵..۳۷ فقط پایین می‌برند) - ابزورب حداکثر
 *      ۱۴٫۶V و باند جریان حداکثر ۹۰۰mA (سقف = باند + ۲۵). */

/* [EN] v1.15 alarm clamps: hard >= imax+50 (never above 950),
 *      OV >= over+150 (never above 15000), floor in 0..8000.
 * [FA] گیره‌های آلارم v1.15: خطای سخت بالای imax+50 (هرگز بالای ۹۵۰)،
 *      قطع OV بالای over+150 (هرگز بالای ۱۵۰۰۰)، فلور در ۰..۸۰۰۰. */
static void func__Charger_ClampAlarms(void)
{
    uint32_t uint32_t__imax =
        CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa;
    uint32_t uint32_t__over =
        CHARGER_PROFILE_T__G__Profile.uint32_t__absorbOverMv;
    uint32_t uint32_t__floorHard = uint32_t__imax + 50u;
    uint32_t uint32_t__floorOv = uint32_t__over + 150u;

    if (uint32_t__floorOv < 14000u)
    {
        uint32_t__floorOv = 14000u;
    }
    if (UINT32_T__G__ChargerHardFaultMa < uint32_t__floorHard)
    {
        UINT32_T__G__ChargerHardFaultMa = uint32_t__floorHard;
    }
    if (UINT32_T__G__ChargerHardFaultMa > CHG_CURRENT_HARD_FAULT_MAX_MA)
    {
        UINT32_T__G__ChargerHardFaultMa = CHG_CURRENT_HARD_FAULT_MAX_MA;
    }
    if (UINT32_T__G__ChargerOvCutoffMv < uint32_t__floorOv)
    {
        UINT32_T__G__ChargerOvCutoffMv = uint32_t__floorOv;
    }
    if (UINT32_T__G__ChargerOvCutoffMv > CHG_MAX_VALID_BATTERY_MV)
    {
        UINT32_T__G__ChargerOvCutoffMv = CHG_MAX_VALID_BATTERY_MV;
    }
    if (UINT32_T__G__ChargerValidFloorMv > 8000u)
    {
        UINT32_T__G__ChargerValidFloorMv = 8000u;
    }
}

/* [EN] Re-assert every PID window. Called from the tail of
 *      func__Charger_ClampProfile so one NVM restore, one panel write or
 *      one factory reset can never leave a gain or a slew rate outside the
 *      compiled band: enable is 0/1, every gain is 0..CHG_PID_GAIN_MAX and
 *      every slew rate is CHG_PID_RATE_MIN..CHG_PID_RATE_MAX.
 *      Unlike the profile there is no cross-field geometry left to defend
 *      (the old stage-1/2 voltage border is gone - the voltage loop now
 *      switches rows at the live absorb setpoint itself), so this is a
 *      pure per-field clamp.
 * [FA] بازاعمال همهٔ پنجره‌های PID. از انتهای func__Charger_ClampProfile
 *      صدا زده می‌شود تا یک بازیابی NVM، یک نوشتن از پنل یا یک ریست
 *      کارخانه هرگز ضریب یا شیبی را بیرون از باند کامپایل جا نگذارد:
 *      فعال‌ساز ۰/۱، هر ضریب ۰ تا CHG_PID_GAIN_MAX و هر شیب بین
 *      CHG_PID_RATE_MIN و CHG_PID_RATE_MAX. برخلاف پروفایل دیگر هندسهٔ
 *      بین‌فیلدی‌ای برای دفاع نمانده (مرز ولتاژی مرحلهٔ ۱و۲ حذف شد - حلقهٔ
 *      ولتاژ حالا دقیقاً روی ست‌پوینت زندهٔ ابزورب ردیف عوض می‌کند)، پس این
 *      فقط یک گیرهٔ فیلد‌به‌فیلد است. */
static void func__Charger_ClampPid(void)
{
    uint32_t uint32_t__index;
    volatile uint32_t *uint32_t__ptr_words;

    /* [EN] Gains and rates share one indexed sweep: the struct packs the
       three stage rows as (Kp, Ki, Kd, up-rate, down-rate), so index % 5
       >= 3 is a slew rate and everything else is a gain.
       [FA] ضرایب و شیب‌ها با یک پیمایش نمایه‌ای: ساختار دو ردیف حلقه را
       به‌صورت (Kp، Ki، Kd، نرخ صعود، نرخ نزول) می‌چیند، پس باقی‌ماندهٔ
       نمایه بر ۵ اگر ۳ یا بیشتر باشد یعنی شیب و بقیه ضریب‌اند. */
    uint32_t__ptr_words = &CHARGER_PID_T__G__Pid.uint32_t__currentKp;
    for (uint32_t__index = 0u; uint32_t__index < 10u; uint32_t__index++)
    {
        if ((uint32_t__index % 5u) >= 3u)
        {
            if (uint32_t__ptr_words[uint32_t__index] < CHG_PID_RATE_MIN)
            {
                uint32_t__ptr_words[uint32_t__index] = CHG_PID_RATE_MIN;
            }
            if (uint32_t__ptr_words[uint32_t__index] > CHG_PID_RATE_MAX)
            {
                uint32_t__ptr_words[uint32_t__index] = CHG_PID_RATE_MAX;
            }
        }
        else
        {
            if (uint32_t__ptr_words[uint32_t__index] > CHG_PID_GAIN_MAX)
            {
                uint32_t__ptr_words[uint32_t__index] = CHG_PID_GAIN_MAX;
            }
        }
    }
}

static void func__Charger_ClampProfile(void)
{
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv < 11000u)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv = 11000u;
    }
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv > 14600u)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv = 14600u;
    }

    if (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbEnterMv <
        (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv - 500u))
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__absorbEnterMv =
            CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv - 500u;
    }
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbEnterMv >
        (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv - 50u))
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__absorbEnterMv =
            CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv - 50u;
    }

    if (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbOverMv <
        (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv + 100u))
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__absorbOverMv =
            CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv + 100u;
    }
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbOverMv >
        (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv + 400u))
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__absorbOverMv =
            CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv + 400u;
    }
    /* [EN] Keep the coarse-step ceiling 50 mV under the 14.8 V
            battery-disconnect fault (boot default; the threshold itself is
            runtime since v1.15, id 27, and ClampAlarms keeps it >= over+50)
            so regulation always acts before the fault does (absorb <= 14600
            keeps this range non-empty).
       [FA] سقف کاهش سریع را ۵۰mV زیر خطای قطع باتری ۱۴٫۸V (پیش‌فرض بوت؛
            خود آستانه از v1.15 زمان‌اجرا است، شناسهٔ ۲۷، و ClampAlarms آن
            را بالای over+50 نگه می‌دارد) نگه می‌داریم تا تنظیم همیشه قبل
            از خطا عمل کند (ابزورب ≤ ۱۴۶۰۰ این بازه را تهی نمی‌کند). */
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbOverMv > 14750u)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__absorbOverMv = 14750u;
    }

    if (CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv < 9000u)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv = 9000u;
    }
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv >
        (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv - 300u))
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv =
            CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv - 300u;
    }

    if (CHARGER_PROFILE_T__G__Profile.uint32_t__reentryMv < 8000u)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__reentryMv = 8000u;
    }
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__reentryMv >
        (CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv - 300u))
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__reentryMv =
            CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv - 300u;
    }

    if (CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa < 100u)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa = 100u;
    }
    /* [EN] USER-ORDERED LOGIC CHANGE 2026-10-03 ("my batteries may be in
     *      parallel and I may want more current"): the band used to stop at
     *      a frozen 900 mA. It now stops 50 mA below the hard-fault
     *      ceiling, which is where it always logically belonged - the trip
     *      must stay above the setpoint or the charger faults on its own
     *      target (func__Charger_ClampAlarms keeps hard >= imax + 50). One
     *      number moves, the geometry is unchanged.
     * [FA] تغییر منطق به دستور کاربر: باند قبلاً روی ۹۰۰ میلی‌آمپرِ منجمد
     *      می‌ایستاد. حالا ۵۰ میلی‌آمپر زیر سقف خطای سخت می‌ایستد، یعنی
     *      همان‌جایی که منطقاً جایش بود - تریپ باید بالای ست‌پوینت بماند
     *      وگرنه شارژر روی هدف خودش خطا می‌دهد. */
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa >
        (CHG_CURRENT_HARD_FAULT_MAX_MA - 50u))
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa =
            (CHG_CURRENT_HARD_FAULT_MAX_MA - 50u);
    }

    if (CHARGER_PROFILE_T__G__Profile.uint32_t__taperCurrentMa < 10u)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__taperCurrentMa = 10u;
    }
    /* [EN] Taper is "the charge is finished" expressed as a fraction of the
     *      pack - typically C/50..C/100 - so a frozen 300 mA ceiling was
     *      really a frozen assumption about pack size. It now tracks the
     *      band (half of it), and the clamp below still keeps it under the
     *      band itself.
     * [FA] جریان Taper یعنی «شارژ تمام شد» و کسری از ظرفیت پک است، پس سقف
     *      منجمد ۳۰۰ در واقع فرضِ منجمدی دربارهٔ اندازهٔ پک بود. حالا نصف
     *      باند را دنبال می‌کند. */
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__taperCurrentMa >
        ((CHG_CURRENT_HARD_FAULT_MAX_MA - 50u) / 2u))
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__taperCurrentMa =
            ((CHG_CURRENT_HARD_FAULT_MAX_MA - 50u) / 2u);
    }
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__taperCurrentMa >
        CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__taperCurrentMa =
            CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa;
    }

    /* [EN] v1.15: cascade - the charger alarms ride on the profile band
     *      (hard >= imax+50, OV >= over+150) and the fault alarms ride on
     *      the charger alarms (disconnect < OV). Order: profile -> charger
     *      alarms -> fault alarms.
     * [FA] آبشار v1.15: آلارم‌های شارژر سوار باند پروفایل‌اند و آلارم‌های
     *      فالت سوار آلارم‌های شارژر (قطع زیر OV). ترتیب: پروفایل، آلارم
     *      شارژر، آلارم فالت. */
    func__Charger_ClampAlarms();
    /* [EN] v1.22 joins the same cascade: the PID stage border is expressed
       relative to the absorb setpoint, so it is re-clamped here too.
       [FA] نسخهٔ ۱.۲۲ به همین آبشار می‌پیوندد: مرز مرحلهٔ PID نسبت به
       ست‌پوینت ابزورب تعریف شده، پس اینجا هم دوباره گیره می‌خورد. */
    func__Charger_ClampPid();
    func__Fault_OnSupervisionChange();
}

/* [EN] Layout contract for the indexed Set/GetProfileParam below (flash
   diet 2026-09-27): wire ids 20..26 dense, one packed uint32_t per id in
   the same order (host test pins every wire id).
   [FA] قرارداد چیدمان Set/Get نمایه‌ای: شناسه‌های ۲۰..۲۶ پشت‌سرهم، یک
   کلمه به همان ترتیب. */
_Static_assert(CHG_PROFILE_PARAM_ABSORB_MV == 20u,
               "profile id base must be 20");
_Static_assert(CHG_PROFILE_PARAM_TAPER_CURRENT_MA == 26u,
               "profile id top must be 26");
_Static_assert(sizeof(charger_profile_t) == (7u * sizeof(uint32_t)),
               "charger_profile_t must pack exactly 7 words");
_Static_assert(offsetof(charger_profile_t, uint32_t__absorbMv) == 0u,
               "first field must be the id-20 word");
_Static_assert(offsetof(charger_profile_t, uint32_t__taperCurrentMa) ==
                   (6u * sizeof(uint32_t)),
               "last field must be the id-26 word");

bool func__Charger_SetProfileParam(uint8_t uint8_t__paramId,
                                   uint32_t uint32_t__value,
                                   uint32_t *uint32_t__appliedValue)
{
    /* [EN] Writer-side scheduler lock (v1.16 audit C11): the comm task
       (Low1) writes, the control task (Low2) preempts mid-clamp and would
       read a torn set for one pass (fresh absorb vs stale reentry). Store
       + clamp run atomic; pre-kernel (NVM replay) the lock call fails and
       the plain path runs single-threaded (same pattern as measurement.c).
       [FA] قفل زمان‌بند سمت نویسنده: تسک ارتباط می‌نویسد و تسک کنترل وسط
       گیره پیشی می‌گیرد و یک پاس ست پاره می‌خواند؛ ذخیره + گیره اتمیک
       می‌شود. پیش از کرنل مسیر سادهٔ تک‌نخی اجرا می‌شود. */
    int32_t int32_t__savedKernelLock = osKernelLock();

    /* [EN] Indexed store (flash diet 2026-09-27): wire ids 20..26 are
       dense and charger_profile_t packs the same fields in the same order
       (layout asserts above, wire-id density pinned by the host test):
       identical store, identical clamp call, identical lock/unlock paths.
       [FA] ذخیرهٔ نمایه‌ای (رژیم فلش): شناسه‌های ۲۰..۲۶ پشت‌سرهم و فیلدها
       به همان ترتیب‌اند - همان ذخیره، همان گیره، همان قفل. */
    if ((uint8_t__paramId < CHG_PROFILE_PARAM_ABSORB_MV) ||
        (uint8_t__paramId > CHG_PROFILE_PARAM_TAPER_CURRENT_MA))
    {
        if (int32_t__savedKernelLock >= 0)
        {
            (void)osKernelRestoreLock(int32_t__savedKernelLock);
        }
        return false;
    }
    ((volatile uint32_t *)&CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv)
        [uint8_t__paramId - CHG_PROFILE_PARAM_ABSORB_MV] = uint32_t__value;

    func__Charger_ClampProfile();
    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
    return func__Charger_GetProfileParam(uint8_t__paramId, uint32_t__appliedValue);
}

bool func__Charger_GetProfileParam(uint8_t uint8_t__paramId,
                                   uint32_t *uint32_t__value)
{
    /* [EN] Indexed read: same dense-id/struct contract as the setter.
       [FA] خواندن نمایه‌ای: همان قرارداد شناسه/ساختار. */
    if ((uint8_t__paramId < CHG_PROFILE_PARAM_ABSORB_MV) ||
        (uint8_t__paramId > CHG_PROFILE_PARAM_TAPER_CURRENT_MA))
    {
        return false;
    }
    *uint32_t__value =
        ((volatile uint32_t *)&CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv)
        [uint8_t__paramId - CHG_PROFILE_PARAM_ABSORB_MV];
    return true;
}

/* ==================== Charger Alarms (v1.15, wire ids 35..37) ==================== */

bool func__Charger_SetAlarmParam(uint8_t uint8_t__paramId,
                                 uint32_t uint32_t__value,
                                 uint32_t *uint32_t__appliedValue)
{
    /* [EN] Writer-side scheduler lock (v1.16 audit C11): same torn-set
       closure as the profile path; the supervision cascade below is pure
       computation, lock-safe. Pre-kernel the plain path runs (NVM replay).
       [FA] قفل زمان‌بند سمت نویسنده: همان بستن پارگی مسیر پروفایل؛ آبشار
       نظارت محاسبهٔ خالص و امن زیر قفل است. */
    int32_t int32_t__savedKernelLock = osKernelLock();

    switch (uint8_t__paramId)
    {
        case CHG_ALARM_PARAM_HARD_CURRENT_MA:
            UINT32_T__G__ChargerHardFaultMa = uint32_t__value;
            break;
        case CHG_ALARM_PARAM_OV_CUTOFF_MV:
            UINT32_T__G__ChargerOvCutoffMv = uint32_t__value;
            break;
        case CHG_ALARM_PARAM_VALID_FLOOR_MV:
            UINT32_T__G__ChargerValidFloorMv = uint32_t__value;
            break;
        default:
            if (int32_t__savedKernelLock >= 0)
            {
                (void)osKernelRestoreLock(int32_t__savedKernelLock);
            }
            return false;
    }

    /* [EN] Same cascade as the profile path: charger alarms first (a
     *      lowered OV floor re-floats here), then the fault alarms ride
     *      along (disconnect stays < OV).
     * [FA] همان آبشار مسیر پروفایل: اول آلارم‌های شارژر، بعد آلارم‌های
     *      فالت سوار می‌شوند (قطع زیر OV می‌ماند). */
    func__Charger_ClampAlarms();
    func__Fault_OnSupervisionChange();

    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
    return func__Charger_GetAlarmParam(uint8_t__paramId, uint32_t__appliedValue);
}

bool func__Charger_GetAlarmParam(uint8_t uint8_t__paramId,
                                 uint32_t *uint32_t__value)
{
    switch (uint8_t__paramId)
    {
        case CHG_ALARM_PARAM_HARD_CURRENT_MA:
            *uint32_t__value = UINT32_T__G__ChargerHardFaultMa;
            return true;
        case CHG_ALARM_PARAM_OV_CUTOFF_MV:
            *uint32_t__value = UINT32_T__G__ChargerOvCutoffMv;
            return true;
        case CHG_ALARM_PARAM_VALID_FLOOR_MV:
            *uint32_t__value = UINT32_T__G__ChargerValidFloorMv;
            return true;
        default:
            return false;
    }
}

/* ==================== Two-loop CC/CV PID params (v1.22, ids 83..92) ==================== */

/* [EN] Same dense-id/packed-struct contract as the profile: wire ids 83..92
   map 1:1 onto charger_pid_t's 16 words in order, so Set/Get index instead
   of switching (host test pins every wire id).
   [FA] همان قرارداد شناسهٔ پشت‌سرهم و ساختار فشرده: ۸۳..۹۲ یک‌به‌یک روی ۱۰
   کلمهٔ charger_pid_t می‌افتند، پس Set/Get نمایه می‌زنند. */
_Static_assert(CHG_PID_PARAM_CURRENT_KP == 83u, "PID id base must be 83");
_Static_assert(CHG_PID_PARAM_VOLTAGE_DOWN_RATE == 92u, "PID id top must be 92");
_Static_assert((CHG_PID_PARAM_VOLTAGE_DOWN_RATE - CHG_PID_PARAM_CURRENT_KP) == 9u,
               "PID id block must stay dense");
_Static_assert(sizeof(charger_pid_t) == (10u * sizeof(uint32_t)),
               "charger_pid_t must pack exactly 10 words");
_Static_assert(offsetof(charger_pid_t, uint32_t__currentKp) == 0u,
               "current row must start at the id-83 word");
_Static_assert(offsetof(charger_pid_t, uint32_t__voltageKp) ==
                   (5u * sizeof(uint32_t)),
               "voltage row must start at the id-88 word");
_Static_assert(offsetof(charger_pid_t, uint32_t__voltageDownRate) ==
                   (9u * sizeof(uint32_t)),
               "last field must be the id-92 word");

/* [EN] Arithmetic headroom proofs for func__Charger_PidStep - the whole
   loop runs in int32_t, so the worst case a panel user can dial in must
   still fit. Worst P or D term is GAIN_MAX x (2 x ERROR_CLAMP); the demand
   adds the integral, which never exceeds the duty ceiling in
   milli-permille. Worst integral numerator is RATE_MAX x DT_MAX_MS plus a
   carried sub-unit remainder below 1000.
   [FA] اثبات جای محاسباتی برای func__Charger_PidStep - کل حلقه در int32_t
   می‌چرخد، پس بدترین حالتی که کاربر از پنل می‌تواند بگذارد هم باید جا شود.
   بدترین جملهٔ P یا D برابر GAIN_MAX×(۲×ERROR_CLAMP) است و تقاضا انتگرال را
   هم اضافه می‌کند که هرگز از سقف دیوتی بر حسب میلی‌پرمیل بیشتر نمی‌شود.
   بدترین صورت کسر انتگرال RATE_MAX×DT_MAX_MS به‌علاوهٔ باقی‌ماندهٔ زیر ۱۰۰۰. */
_Static_assert((((int64_t)CHG_PID_GAIN_MAX * (int64_t)CHG_PID_ERROR_CLAMP) +
                ((int64_t)CHG_PID_GAIN_MAX * 2LL * (int64_t)CHG_PID_ERROR_CLAMP) +
                ((int64_t)CHG_PID_DUTY_SCALE * (int64_t)CHG_DUTY_MAX_PERMILLE)) <
                   2147483647LL,
               "worst-case PID demand must fit in int32_t");
_Static_assert((((int64_t)CHG_PID_RATE_MAX * (int64_t)CHG_PID_DT_MAX_MS) + 1000LL) <
                   2147483647LL,
               "worst-case PID integral numerator must fit in int32_t");
/* [EN] The output hysteresis deliberately lets the applied duty lag the
   demand. That lag must stay SMALLER than the re-seed tolerance, or the
   PID's own quantisation would look like a foreign writer and the two
   mechanisms would fight (dither back, re-seed, dither back...).
   [FA] هیسترزیس خروجی عمداً می‌گذارد دیوتی اعمالی از تقاضا عقب بماند. این
   عقب‌ماندگی باید کوچک‌تر از تحمل بذرگیری دوباره بماند وگرنه گردکردنِ خودِ
   PID شبیه نویسندهٔ بیرونی دیده می‌شود و این دو سازوکار با هم می‌جنگند. */
/* [EN] Now that the hysteresis is panel-settable the assert has to bind the
   WHOLE settable window, not just the boot default: checking the default
   would have let the user type a value the firmware forbids itself. The
   window ceiling is therefore DERIVED from the tolerance instead of being a
   number someone picked - the first draft of this block used a round 5000,
   which the compiler rejected here, correctly.
   [FA] حالا که هیسترزیس از پنل تنظیم می‌شود، این گزاره باید کل پنجرهٔ
   تنظیم‌پذیر را مقید کند نه فقط پیش‌فرض بوت: سنجیدن پیش‌فرض اجازه می‌داد
   کاربر عددی بنویسد که خود فرم‌ور آن را ممنوع کرده. پس سقف پنجره از همان
   تحمل «مشتق» می‌شود نه عددی که کسی انتخاب کرده - پیش‌نویس اول این بلوک
   عدد رُند ۵۰۰۰ را گذاشته بود و کامپایلر به‌درستی ردش کرد. */
#define CHG_LIMIT_MAX_PID_OUT_HYST_MILLI \
    (((uint32_t)CHG_PID_RESEED_TOLERANCE_PERMILLE * CHG_PID_DUTY_SCALE) - 1u)
_Static_assert(CHG_LIMIT_MAX_PID_OUT_HYST_MILLI <
                   ((uint32_t)CHG_PID_RESEED_TOLERANCE_PERMILLE * CHG_PID_DUTY_SCALE),
               "output hysteresis window must stay inside the bumpless re-seed tolerance");
_Static_assert(CHG_PID_OUTPUT_HYST_MILLI <= CHG_LIMIT_MAX_PID_OUT_HYST_MILLI,
               "the boot default must itself fit the settable window");
/* [EN] The two rows tune loops whose errors are in DIFFERENT units (amps
   versus volts), so their Kp values are not interchangeable and must never
   be silently swapped: keep a cheap sanity floor on the pairing.
   [FA] دو ردیف، حلقه‌هایی را تیون می‌کنند که خطایشان واحد متفاوت دارد
   (آمپر در برابر ولت)، پس Kp آن‌ها قابل جابه‌جایی نیست: یک کف سلامت ارزان
   روی این جفت‌شدن. */
_Static_assert(CHG_PID_CURRENT_KP <= CHG_PID_VOLTAGE_KP,
               "current-loop Kp is per-amp and must stay below the per-volt row");
_Static_assert((CHG_PID_CURRENT_KI <= CHG_PID_GAIN_MAX) &&
                   (CHG_PID_VOLTAGE_KI <= CHG_PID_GAIN_MAX),
               "default Ki rows must fit the panel window");
_Static_assert(CHG_PID_VOLTAGE_UP_RATE < CHG_PID_CURRENT_UP_RATE,
               "the absorb rise must stay slower than bulk (user order 2026-09-28)");

bool func__Charger_SetPidParam(uint8_t uint8_t__paramId,
                               uint32_t uint32_t__value,
                               uint32_t *uint32_t__appliedValue)
{
    /* [EN] Writer-side scheduler lock, same reason as the profile setter:
       the comm task writes while the control task may be mid-PID-update
       and would otherwise read a half-applied gain row.
       [FA] قفل زمان‌بند سمت نویسنده، به همان دلیل ستر پروفایل: تسک ارتباط
       می‌نویسد و تسک کنترل ممکن است وسط به‌روزرسانی PID باشد و ردیف
       نیمه‌اعمال‌شده بخواند. */
    int32_t int32_t__savedKernelLock = osKernelLock();

    if ((uint8_t__paramId < CHG_PID_PARAM_CURRENT_KP) ||
        (uint8_t__paramId > CHG_PID_PARAM_VOLTAGE_DOWN_RATE))
    {
        if (int32_t__savedKernelLock >= 0)
        {
            (void)osKernelRestoreLock(int32_t__savedKernelLock);
        }
        return false;
    }

    ((volatile uint32_t *)&CHARGER_PID_T__G__Pid.uint32_t__currentKp)
        [uint8_t__paramId - CHG_PID_PARAM_CURRENT_KP] = uint32_t__value;

    func__Charger_ClampPid();
    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
    return func__Charger_GetPidParam(uint8_t__paramId, uint32_t__appliedValue);
}

bool func__Charger_GetPidParam(uint8_t uint8_t__paramId,
                               uint32_t *uint32_t__value)
{
    if ((uint8_t__paramId < CHG_PID_PARAM_CURRENT_KP) ||
        (uint8_t__paramId > CHG_PID_PARAM_VOLTAGE_DOWN_RATE))
    {
        return false;
    }
    *uint32_t__value =
        ((volatile uint32_t *)&CHARGER_PID_T__G__Pid.uint32_t__currentKp)
        [uint8_t__paramId - CHG_PID_PARAM_CURRENT_KP];
    return true;
}

/* ========== Charger limits & backstop gains, ids 93..107 (user 2026-10-03) ==========
 * [EN] USER-ORDERED LOGIC CHANGE: the remaining compile-time gains, limits
 *      and stage timers become panel-settable. One const row per limit keeps
 *      the clamp in exactly one place; fifteen hand-written switch cases is
 *      the shape that has gone stale here before, and flash is tight.
 *      ROW ORDER IS THE WIRE-ID ORDER - the _Static_assert below pins the
 *      table length to the id span, so adding an id without a row (or the
 *      reverse) will not compile rather than silently reading past the end.
 * [FA] تغییر منطق به دستور کاربر: گین‌ها، حدها و تایمرهای مرحله‌ای باقی‌مانده
 *      از پنل تنظیم‌شدنی می‌شوند. یک ردیف const برای هر حد، گیره را دقیقاً در
 *      یک جا نگه می‌دارد؛ پانزده case دستی همان شکلی است که قبلاً اینجا کهنه
 *      شده و فلش هم تنگ است. ترتیب ردیف‌ها همان ترتیب شناسه‌های سیمی است -
 *      _Static_assert پایین طول جدول را به بازهٔ شناسه‌ها قفل می‌کند، پس
 *      افزودن شناسه بدون ردیف (یا برعکس) کامپایل نمی‌شود به‌جای اینکه بی‌صدا
 *      از انتهای جدول رد شود. */
typedef struct
{
    uint32_t uint32_t__min;
    uint32_t uint32_t__max;
    uint32_t uint32_t__def;
} charger_limit_def_t;

/* [EN] ONE list, expanded twice. The window table and the live array both
 *      come from these rows, so a default cannot disagree with itself - the
 *      first attempt at this block kept the defaults in two literal lists
 *      and that is precisely the duplication that has gone stale all over
 *      this project. Columns: NAME, min, max, boot default.
 * [FA] یک فهرست، دو بار بسط. هم جدول پنجره و هم آرایهٔ زنده از همین ردیف‌ها
 *      ساخته می‌شوند، پس یک پیش‌فرض نمی‌تواند با خودش اختلاف پیدا کند -
 *      تلاش اول این بلوک پیش‌فرض‌ها را در دو فهرست جدا نگه داشت و همین
 *      تکرار است که در کل این پروژه بارها کهنه شده. ستون‌ها: نام، کمینه،
 *      بیشینه، پیش‌فرض بوت. */
#define CHG_LIMIT_ROWS(X)                                                      \
    X(ABSORB_MAX_MS,       0u, 21600000u, CHG_ABSORB_MAX_MS)                   \
    X(ABSORB_MAX_ARM_MA,  10u, (CHG_CURRENT_HARD_FAULT_MAX_MA/2u), CHG_ABSORB_MAX_ARM_MA) \
    X(ABSORB_HOLD_MS,      0u,  7200000u, CHG_ABSORB_HOLD_MS)                  \
    X(TAPER_SUSTAIN_MS, 1000u,   600000u, CHG_TAPER_SUSTAIN_MS)                \
    X(PID_MAX_STEP_PM,     1u,      100u, CHG_PID_MAX_STEP_PERMILLE)           \
    X(PID_OUT_HYST_MILLI,  0u, CHG_LIMIT_MAX_PID_OUT_HYST_MILLI, CHG_PID_OUTPUT_HYST_MILLI)           \
    X(PID_VOLT_FILTER_N,   1u,       64u, CHG_PID_VOLT_FILTER_N)               \
    X(BACKSTOP_MV,     13000u, CHG_PID_BACKSTOP_MV, CHG_PID_BACKSTOP_MV)       \
    X(BACKSTOP_GAIN_I,     0u,     2000u, CHG_PID_BACKSTOP_GAIN_I)             \
    X(BACKSTOP_GAIN_V,     0u,     2000u, CHG_PID_BACKSTOP_GAIN_V)             \
    X(PID_CUR_MARGIN_MA,   0u,      100u, CHG_PID_CURRENT_MARGIN_MA)           \
    X(CONNECT_SETTLE_MS,   0u,   120000u, CHG_CONNECT_SETTLE_MS)               \
    X(JIT_LOCKOUT_MS,      0u,    60000u, CHG_JIT_LOCKOUT_MS)                  \
    X(MANUAL_WATCHDOG_MS, 500u,   60000u, CHG_MANUAL_WATCHDOG_MS)              \
    X(RAMP_DOWN_INT_MS,   50u,     5000u, CHG_DUTY_RAMP_DOWN_INTERVAL_MS)

#define CHG_LIMIT_ROW_DEF(name, lo, hi, def)  { (lo), (hi), (def) },
#define CHG_LIMIT_ROW_VAL(name, lo, hi, def)  (def),
/* [EN] Row order must stay the wire-id order; this pins it. The window check
       is the one mutation testing asked for: dropping a row's ceiling below
       its own factory default used to compile cleanly and then silently clamp
       the default at boot, so the number printed in the panel as "factory"
       was a number the board would never actually hold.
   [FA] ترتیب ردیف‌ها باید همان ترتیب شناسهٔ روی سیم بماند و این آن را میخ
       می‌کند. چک پنجره همان چیزی است که موتیشن‌تست خواست: پایین‌آوردن سقف یک
       ردیف زیر پیش‌فرض کارخانهٔ خودش قبلاً بی‌صدا کامپایل می‌شد و بعد سر بوت
       پیش‌فرض را گیره می‌زد، یعنی عددی که پنل به‌عنوان «کارخانه» نشان می‌داد
       عددی بود که برد هرگز نگه نمی‌داشت. */
#define CHG_LIMIT_ROW_CHK(name, lo, hi, def)                                   \
    _Static_assert(CHG_LIMIT_PARAM_##name >= CHG_LIMIT_PARAM_FIRST_ID,         \
                   "limit row " #name " is outside the wire-id block");        \
    _Static_assert((lo) <= (def),                                              \
                   "limit row " #name " default is below its own floor");      \
    _Static_assert((def) <= (hi),                                              \
                   "limit row " #name " default is above its own ceiling");

static const charger_limit_def_t CHARGER_LIMIT_DEF_T__A__Defs[CHG_LIMIT_COUNT] =
{
    CHG_LIMIT_ROWS(CHG_LIMIT_ROW_DEF)
};

/* [EN] volatile: written by the EspLink task, read by the control task.
   [FA] بین دو تسک بدون قفل پس volatile. */
static volatile uint32_t UINT32_T__G__ChargerLimit[CHG_LIMIT_COUNT] =
{
    CHG_LIMIT_ROWS(CHG_LIMIT_ROW_VAL)
};

CHG_LIMIT_ROWS(CHG_LIMIT_ROW_CHK)

_Static_assert((sizeof(CHARGER_LIMIT_DEF_T__A__Defs) /
                sizeof(CHARGER_LIMIT_DEF_T__A__Defs[0])) == CHG_LIMIT_COUNT,
               "limit table must have exactly one row per wire id 93..107");
_Static_assert((sizeof(UINT32_T__G__ChargerLimit) /
                sizeof(UINT32_T__G__ChargerLimit[0])) == CHG_LIMIT_COUNT,
               "live limit array must have exactly one slot per wire id");

/**
 * @brief  [EN] Live value of one charger limit, by table index.
 *         [FA] مقدار زندهٔ یک حد شارژر، با نمایهٔ جدول.
 * @param  uint8_t__index [EN] 0..CHG_LIMIT_COUNT-1 / نمایه
 * @return uint32_t [EN] Live value / مقدار زنده
 */
static uint32_t func__Charger_Limit(uint8_t uint8_t__index)
{
    return UINT32_T__G__ChargerLimit[uint8_t__index];
}

bool func__Charger_SetLimitParam(uint8_t uint8_t__paramId,
                                 uint32_t uint32_t__value,
                                 uint32_t *uint32_t__appliedValue)
{
    uint8_t uint8_t__index;
    uint32_t uint32_t__applied;

    if ((uint8_t__paramId < CHG_LIMIT_PARAM_FIRST_ID) ||
        (uint8_t__paramId > CHG_LIMIT_PARAM_LAST_ID))
    {
        return false;
    }

    uint8_t__index = (uint8_t)(uint8_t__paramId - CHG_LIMIT_PARAM_FIRST_ID);
    uint32_t__applied = uint32_t__value;

    if (uint32_t__applied < CHARGER_LIMIT_DEF_T__A__Defs[uint8_t__index].uint32_t__min)
    {
        uint32_t__applied = CHARGER_LIMIT_DEF_T__A__Defs[uint8_t__index].uint32_t__min;
    }
    if (uint32_t__applied > CHARGER_LIMIT_DEF_T__A__Defs[uint8_t__index].uint32_t__max)
    {
        uint32_t__applied = CHARGER_LIMIT_DEF_T__A__Defs[uint8_t__index].uint32_t__max;
    }

    UINT32_T__G__ChargerLimit[uint8_t__index] = uint32_t__applied;

    if (uint32_t__appliedValue != NULL)
    {
        *uint32_t__appliedValue = uint32_t__applied;
    }
    return true;
}

bool func__Charger_GetLimitParam(uint8_t uint8_t__paramId,
                                 uint32_t *uint32_t__value)
{
    if ((uint8_t__paramId < CHG_LIMIT_PARAM_FIRST_ID) ||
        (uint8_t__paramId > CHG_LIMIT_PARAM_LAST_ID))
    {
        return false;
    }
    *uint32_t__value =
        UINT32_T__G__ChargerLimit[uint8_t__paramId - CHG_LIMIT_PARAM_FIRST_ID];
    return true;
}

void func__Charger_SetManualTestMode(bool bool__enable)
{
    BOOL__G__ChargerManualModeRequested = (bool__enable != false);
}

bool func__Charger_GetManualTestMode(void)
{
    return BOOL__G__ChargerManualModeRequested;
}

bool func__Charger_IsManualTestModeActive(void)
{
    return BOOL__G__ChargerManualModeActive;
}

void func__Charger_NotifyEspLinkActivity(void)
{
    UINT32_T__G__ManualLastLinkTick = osKernelGetTickCount();
}

/**
 * @brief  [EN] Set the runtime PWM duty ceiling of one channel, clamped to
 *              0..CHG_DUTY_MAX_PERMILLE. Every applied duty (ramp,
 *              regulation, fixed mode) is clamped to min(compile max, this
 *              ceiling) inside ApplyDuty. Flash-persisted since v1.14 -
 *              a reboot keeps the ceilings (ESP panel, user order 2026-09-22).
 *         [FA] سقف duty ی PWM یک کانال در زمان اجرا، گیرهٔ
 *              ۰..CHG_DUTY_MAX_PERMILLE. هر duty اعمالی (رمپ، تنظیم، مود
 *              فیکس) داخل ApplyDuty به کمینهٔ سقف کامپایل و این سقف گیره
 *              می‌خورد. روی فلش می‌ماند از نسخهٔ ۱.۱۴ - ری‌استارت سقف‌ها
 *              را نگه می‌دارد (پنل ESP، دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  uint32_t__ceilingPermille [EN] Requested ceiling / سقف درخواستی
 * @return uint32_t [EN] Applied ceiling / سقف اعمال‌شده
 */
uint32_t func__Charger_SetDutyCeilingPermille(uint8_t uint8_t__channelIndex,
                                              uint32_t uint32_t__ceilingPermille)
{
    if (uint32_t__ceilingPermille > CHG_DUTY_MAX_PERMILLE)
    {
        uint32_t__ceilingPermille = CHG_DUTY_MAX_PERMILLE;
    }

    if (uint8_t__channelIndex < 2u)
    {
        UINT32_T__G__ChargerDutyCeilingPermille[uint8_t__channelIndex] =
            uint32_t__ceilingPermille;
    }

    return uint32_t__ceilingPermille;
}

/**
 * @brief  [EN] Read the live PWM duty ceiling of one channel.
 *         [FA] سقف زندهٔ duty ی PWM یک کانال.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @return uint32_t [EN] Ceiling permille / سقف پرمیل
 */
uint32_t func__Charger_GetDutyCeilingPermille(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex < 2u)
    {
        return UINT32_T__G__ChargerDutyCeilingPermille[uint8_t__channelIndex];
    }

    return CHG_DUTY_MAX_PERMILLE;
}

/**
 * @brief  [EN] Set the runtime fixed-duty mode of one channel. While
 *              enabled, the channel holds its fixed duty instead of the
 *              regulation loop, with the bench-test safety wrapper
 *              (switching stops above CHG_ABSORB_MV; JIT/input/battery/ESP
 *              cuts stay active). The duty value is set with
 *              SetDutyFixedPermille and clamped on apply. RAM only (user
 *              order 2026-09-22).
 *         [FA] مود duty فیکس یک کانال: تا وقتی فعال است کانال duty فیکس
 *              را به‌جای حلقهٔ تنظیم نگه می‌دارد با همان پوشش امنیتی بنچ
 *              (توقف سوئیچینگ بالای CHG_ABSORB_MV؛ برش‌های JIT/ورودی/
 *              باتری/ESP فعال). عدد duty با SetDutyFixedPermille تنظیم و
 *              موقع اعمال گیره می‌خورد. فقط RAM (دستور ۲۰۲۶-۰۹-۲۲).
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  bool__enable [EN] true = fixed mode on / مود فیکس روشن
 */
void func__Charger_SetDutyFixedEnable(uint8_t uint8_t__channelIndex, bool bool__enable)
{
    if (uint8_t__channelIndex < 2u)
    {
        BOOL__G__ChargerDutyFixedEnable[uint8_t__channelIndex] = bool__enable;
    }
}

/**
 * @brief  [EN] Read the runtime fixed-duty switch of one channel.
 *         [FA] کلید مود duty فیکس یک کانال.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @return bool [EN] true = fixed mode on / مود فیکس روشن
 */
bool func__Charger_GetDutyFixedEnable(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex < 2u)
    {
        return BOOL__G__ChargerDutyFixedEnable[uint8_t__channelIndex];
    }

    return false;
}

/**
 * @brief  [EN] Set the fixed duty value of one channel, clamped to
 *              0..CHG_DUTY_MAX_PERMILLE. Takes effect only while the
 *              channel's fixed mode is enabled; ApplyDuty additionally
 *              respects the runtime ceiling. Flash-persisted since v1.14
 *              (ESP panel, user order 2026-09-22).
 *         [FA] مقدار duty فیکس یک کانال، گیرهٔ ۰..CHG_DUTY_MAX_PERMILLE.
 *              فقط وقتی مود فیکس همان کانال روشن است اثر دارد؛ ApplyDuty
 *              به‌علاوه سقف زمان اجرا را رعایت می‌کند. روی فلش می‌ماند
 *              از نسخهٔ ۱.۱۴ (پنل ESP، دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  uint32_t__dutyPermille [EN] Requested duty / duty درخواستی
 * @return uint32_t [EN] Applied stored value / مقدار ذخیره‌شده
 */
uint32_t func__Charger_SetDutyFixedPermille(uint8_t uint8_t__channelIndex,
                                            uint32_t uint32_t__dutyPermille)
{
    if (uint32_t__dutyPermille > CHG_DUTY_MAX_PERMILLE)
    {
        uint32_t__dutyPermille = CHG_DUTY_MAX_PERMILLE;
    }

    if (uint8_t__channelIndex < 2u)
    {
        UINT32_T__G__ChargerDutyFixedPermille[uint8_t__channelIndex] =
            uint32_t__dutyPermille;
        if (BOOL__G__ChargerManualModeActive != false)
        {
            /* [EN] Manual test mode: a fresh duty write is the re-arm
               gesture for a JIT-parked channel (protocol v1.2, section
               5.2); the charger task consumes the request.
               [FA] مود تست دستی: نوشتن دوبارهٔ duty حرکتِ مسلح‌کردنِ
               کانالِ پارک‌شده در JIT است (پروتکل v1.2 بخش 5.2)؛ تسک
               شارژر درخواست را مصرف می‌کند. */
            BOOL__G__ChargerManualRearmRequest[uint8_t__channelIndex] = true;
        }
    }

    return uint32_t__dutyPermille;
}

/**
 * @brief  [EN] Read the stored fixed duty value of one channel.
 *         [FA] مقدار ذخیره‌شدهٔ duty فیکس یک کانال.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @return uint32_t [EN] Duty permille / duty پرمیل
 */
uint32_t func__Charger_GetDutyFixedPermille(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex < 2u)
    {
        return UINT32_T__G__ChargerDutyFixedPermille[uint8_t__channelIndex];
    }

    return 0u;
}
