/* sim_board.h - برد STM32 شبیه‌سازی‌شده پشت پنل ساده
   فقط فارسی - مثل بقیهٔ این پوشه

   این برد همان قاب‌هایی را می‌سازد که فرم‌ور واقعی می‌سازد:
   - `TLM_LIVE` (0x10): ۱۲۸ بایت = seq u16 + flags u8 + flags2 u8 + ۳۱×u32
     به همان ترتیب و همان واحدهای جدول §۱۵ سند `ESP_AGENT_SPEC.md`.
   - `DIAG_DATA` (0x32): پنج چانک، دقیقاً با همان چیدمانی که
     `func__Simple_HandleFrame` در سمت ESP می‌خواند.
   و قاب‌های ESP -> برد (`TIME_SYNC` 0x30، `DIAG_REQ` 0x31) را با همان
   SOF/CRC می‌خواند. پس مسیر کامل روی خود کد ESP آزمایش می‌شود.

   مقادیر ولتاژ/حالت/خطا دستی از تب «سیمولاتور» می‌آیند؛ جریان، duty و
   شمارش‌های خام مدل کوچکی هستند (پنل ساده نمایششان نمی‌دهد).
*/
#ifndef SIM_BOARD_H
#define SIM_BOARD_H

#include "stub_arduino.h"
#include "stub_web_server.h"

#include <cstdio>
#include <vector>

/* ==================== نقشهٔ فیلدهای قاب، از جدول §۱۵ سند ==================== */
/* ترتیب همین enum است که قاب را می‌سازد؛ اعداد ستون «ایندکس» همان چیزی
   هستند که JS پنل با T_VIN، T_V24، ... می‌خواند و در static_assert پایین
   قفل شده‌اند. */
enum SimTlmField
{
    SIM_TLM_RAW1_COUNTS = 0,
    SIM_TLM_SHUNT1_UV,
    SIM_TLM_MA1_UNFILTERED,
    SIM_TLM_I1_FILTERED_MA,
    SIM_TLM_IEST1_MA,
    SIM_TLM_DUTY1_PERMILLE,
    SIM_TLM_STATE1,               /* پنل: T_STATE1 = 6 */
    SIM_TLM_RAW2_COUNTS,
    SIM_TLM_SHUNT2_UV,
    SIM_TLM_MA2_UNFILTERED,
    SIM_TLM_I2_FILTERED_MA,
    SIM_TLM_IEST2_MA,
    SIM_TLM_DUTY2_PERMILLE,
    SIM_TLM_STATE2,               /* پنل: T_STATE2 = 13 */
    SIM_TLM_V_IN_MV,              /* پنل: T_VIN = 14 */
    SIM_TLM_V_BAT24_MV,           /* پنل: T_V24 = 15 */
    SIM_TLM_V_BAT12_MV,
    SIM_TLM_V_BAT_LOW_MV,         /* پنل: T_VLOW = 17 */
    SIM_TLM_V_BAT_HIGH_MV,        /* پنل: T_VHIGH = 18 */
    SIM_TLM_FAULT_MASK,           /* پنل: T_FAULT = 19 */
    SIM_TLM_VIN_RAW_COUNTS,
    SIM_TLM_V24_RAW_COUNTS,
    SIM_TLM_V12_RAW_COUNTS,
    SIM_TLM_VREFINT_COUNTS,
    SIM_TLM_VDDA_MV,
    SIM_TLM_IMB_MV,               /* پنل: T_IMB = 25 */
    SIM_TLM_IMB_EVENTS,
    SIM_TLM_IMB_LATCHED_CYCLES,
    SIM_TLM_DEAD_MASK,            /* پنل: T_DEAD = 28 */
    SIM_TLM_DEAD_CHARGE1_S,
    SIM_TLM_DEAD_CHARGE2_S,
    SIM_TLM_FIELD_COUNT
};

static_assert(SIM_TLM_FIELD_COUNT == 31, "قاب تله‌متری باید ۳۱ فیلد باشد");
static_assert(SIM_TLM_STATE1 == 6, "T_STATE1 پنل = ۶");
static_assert(SIM_TLM_STATE2 == 13, "T_STATE2 پنل = ۱۳");
static_assert(SIM_TLM_V_IN_MV == 14, "T_VIN پنل = ۱۴");
static_assert(SIM_TLM_V_BAT24_MV == 15, "T_V24 پنل = ۱۵");
static_assert(SIM_TLM_V_BAT_LOW_MV == 17, "T_VLOW پنل = ۱۷");
static_assert(SIM_TLM_V_BAT_HIGH_MV == 18, "T_VHIGH پنل = ۱۸");
static_assert(SIM_TLM_FAULT_MASK == 19, "T_FAULT پنل = ۱۹");
static_assert(SIM_TLM_IMB_MV == 25, "T_IMB پنل = ۲۵");
static_assert(SIM_TLM_DEAD_MASK == 28, "T_DEAD پنل = ۲۸");

/* ==================== ثابت‌های پروتکل: عیناً مثل simple_config.h ==================== */
#define SIM_LINK_SOF_BYTE0          0xAAu
#define SIM_LINK_SOF_BYTE1          0x55u
#define SIM_LINK_VERSION            2u
#define SIM_LINK_CRC16_INIT         0xFFFFu
#define SIM_LINK_CRC16_POLY         0x1021u
#define SIM_MSG_TLM_LIVE            0x10u
#define SIM_MSG_SET_PARAM           0x01u
#define SIM_MSG_GET_PARAMS          0x02u
#define SIM_MSG_PARAM_REPORT        0x11u
#define SIM_MSG_PARAMS_BULK         0x12u
#define SIM_MSG_TIME_SYNC           0x30u
#define SIM_MSG_DIAG_REQ            0x31u
#define SIM_MSG_DIAG_DATA           0x32u
#define SIM_PARAM_COUNT             171u
#define SIM_PARAM_ITEM_SIZE         5u
#define SIM_PARAMS_PER_FRAME        102u  /* سقف ۵۱۲ بایتی سند: (512-1)/5 */
#define SIM_TLM_SIZE                128u
#define SIM_DIAG_MAX_ERRORS         10u
#define SIM_DIAG_MAX_POWER_EVENTS   20u

/* میلی‌ولت بر هر شمارش ADC برای نت‌های ولتاژ؛ `run_sim.sh` این‌ها را از
   `bsp_measurement.c` می‌خواند و با -D می‌دهد تا هر دو سیمولاتور یک عدد
   بدهند. مقدار پیش‌فرض فقط برای کامپایل مستقیم است. */
#ifndef SIM_MV_PER_COUNT_24
#define SIM_MV_PER_COUNT_24 8.864f
#endif
#ifndef SIM_MV_PER_COUNT_12
#define SIM_MV_PER_COUNT_12 4.882f
#endif
#ifndef SIM_MV_PER_COUNT_ADC
#define SIM_MV_PER_COUNT_ADC 0.806f
#endif
#ifndef SIM_VREFINT_COUNTS
#define SIM_VREFINT_COUNTS 1489u
#endif
#ifndef SIM_VDDA_MV
#define SIM_VDDA_MV 3300u
#endif

/* مدل جریان: میلی‌آمپر بر شمارش از زنجیرهٔ §۲ سند و میکروولت بر شمارش از
   نقطهٔ مرجع بنچ همان سند. فقط برای کامل‌بودن قاب است. */
#define SIM_MA_PER_COUNT 0.8776f
#define SIM_UV_PER_COUNT 8.774f

/* ==================== حالت برد ==================== */
struct SimBoardState
{
    bool linkUp = true;          /* قطع‌کردن لینک: پنل باید «قطع» نشان دهد */
    uint32_t vInMv = 24000u;
    uint32_t vPackMv = 25500u;
    uint32_t vLowMv = 12500u;
    uint32_t vHighMv = 13000u;
    uint32_t state1 = 1u;        /* حالت BULK */
    uint32_t state2 = 0u;        /* حالت OFF */
    uint32_t faultMask = 0u;
    uint32_t deadMask = 0u;
    /* اسلات‌های ریست قفل: ۲۰۰ = رویداد، ۲۰۱ = سیکل پس از قفل، ۲۰۲ = پرچم
       قفل عدم‌توازن، ۲۰۳ = ماسک حکم باتری خراب. پنل با SET_PARAM صفرشان
       می‌کند؛ تناظرشان در تله‌متری `fl2` و فیلدهای ۲۶ تا ۲۸ است. */
    uint32_t imbEvents = 2u;
    uint32_t imbLatchedCycles = 0u;
    uint32_t imbEpisode = 0u;
    uint32_t imbLatch = 0u;

    /* شمارنده‌ها */
    uint16_t seq = 0u;
    uint32_t boardTimeSec = 0u;
    uint32_t uptimeSec = 0u;
    uint32_t resetCount = 3u;
    uint32_t totalChargeSec = 13718u;
    uint32_t chargeCount = 42u;
    uint32_t chargeCountCh1 = 21u;
    uint32_t chargeCountCh2 = 21u;
    uint32_t chargeSecCh1 = 7031u;
    uint32_t chargeSecCh2 = 6687u;
    uint32_t lastTlmMs = 0u;
    uint32_t lastCounterMs = 0u;
    uint32_t framesSent = 0u;

    /* پارامترهای خود برد - پنل درصد باتری و زمان‌بندی چشمک را از همین‌ها می‌خواند */
    uint32_t param[SIM_PARAM_COUNT];
};

extern SimBoardState SIM_BOARD_STATE_T__G__Board;

/* ==================== پارسر بایت‌های ESP -> برد ==================== */
enum SimRxState
{
    SIM_RX_WAIT_SOF0 = 0,
    SIM_RX_WAIT_SOF1,
    SIM_RX_WAIT_VERSION,
    SIM_RX_WAIT_TYPE,
    SIM_RX_WAIT_LEN_LO,
    SIM_RX_WAIT_LEN_HI,
    SIM_RX_WAIT_PAYLOAD,
    SIM_RX_WAIT_CRC_LO,
    SIM_RX_WAIT_CRC_HI
};

static SimRxState SIM_RX_STATE_T__G__SimRxState = SIM_RX_WAIT_SOF0;
static uint8_t UINT8_T__G__SimRxType = 0u;
static uint16_t UINT16_T__G__SimRxLength = 0u;
static uint16_t UINT16_T__G__SimRxIndex = 0u;
static uint8_t UINT8_T__G__SimRxPayload[64];
static uint16_t UINT16_T__G__SimRxCrc = 0u;
static uint8_t UINT8_T__G__SimRxCrcLow = 0u;

/* شمارنده‌های زمانی تیک برد */
static uint32_t UINT32_T__G__SimLastUptimeMs = 0u;
static uint32_t UINT32_T__G__SimLastCountMs = 0u;

/* ==================== پارامترهای پیش‌فرض برد ====================
   همان عددهایی که `Firmware/Modules/Ui/Tester/host_test_ui.py` به‌عنوان
   پیش‌فرض خود فرم‌ور نگه می‌دارد؛ بقیهٔ شناسه‌ها صفر می‌مانند چون پنل
   ساده هیچ‌وقت سراغشان نمی‌رود. */
struct SimParamDefault
{
    uint8_t id;
    uint32_t value;
};

static const SimParamDefault SIM_PARAM_DEFAULT_TABLE[] =
{
    { 52u, 20u }, { 53u, 1u },
    { 66u, 1000u }, { 67u, 10u }, { 68u, 1000u }, { 69u, 150u },
    { 70u, 28000u }, { 71u, 1000u },
    { 74u, 21000u }, { 75u, 29000u }, { 76u, 0u },
    { 77u, 100u }, { 78u, 95u }, { 79u, 5u }, { 80u, 2u }, { 81u, 2u }, { 82u, 3u },
    { 119u, 21000u }, { 120u, 29000u },
    { 143u, 1800000u }, { 144u, 60000u }, { 145u, 120000u }, { 146u, 20u },
    { 33u, 21000u }, { 34u, 28000u }, { 169u, 500u }, { 170u, 500u }
};

static bool BOOL__G__SimParamsReady = false;
static bool BOOL__G__SimInputArmed = false;

/* همان مجوز مشترک اشمیت ورودی: بیرون پنجرهٔ خام فوراً می‌افتد و برگشت
   داخل کف+حاشیهٔ پایین تا سقف−حاشیهٔ بالا است. */
inline bool func__Sim_InputPermitted(uint32_t uint32_t__inputMv)
{
    const uint32_t uint32_t__minMv = SIM_BOARD_STATE_T__G__Board.param[33u];
    const uint32_t uint32_t__maxMv = SIM_BOARD_STATE_T__G__Board.param[34u];
    uint32_t uint32_t__capMv = (uint32_t__maxMv - uint32_t__minMv) / 4u;
    if (uint32_t__capMv > 3000u) { uint32_t__capMv = 3000u; }
    uint32_t uint32_t__hl = SIM_BOARD_STATE_T__G__Board.param[169u];
    uint32_t uint32_t__hh = SIM_BOARD_STATE_T__G__Board.param[170u];
    if (uint32_t__hl > uint32_t__capMv) { uint32_t__hl = uint32_t__capMv; }
    if (uint32_t__hh > uint32_t__capMv) { uint32_t__hh = uint32_t__capMv; }
    if ((uint32_t__inputMv < uint32_t__minMv) || (uint32_t__inputMv > uint32_t__maxMv))
    {
        BOOL__G__SimInputArmed = false;
    }
    else if (!BOOL__G__SimInputArmed)
    {
        BOOL__G__SimInputArmed =
            ((uint32_t__inputMv >= (uint32_t__minMv + uint32_t__hl)) &&
             (uint32_t__inputMv <= (uint32_t__maxMv - uint32_t__hh)));
    }
    return BOOL__G__SimInputArmed;
}

inline void func__Sim_ParamsInit(void)
{
    if (BOOL__G__SimParamsReady) { return; }

    for (uint32_t uint32_t__id = 0u; uint32_t__id < SIM_PARAM_COUNT; uint32_t__id++)
    {
        SIM_BOARD_STATE_T__G__Board.param[uint32_t__id] = 0u;
    }
    for (uint32_t uint32_t__i = 0u; uint32_t__i < (sizeof(SIM_PARAM_DEFAULT_TABLE) / sizeof(SIM_PARAM_DEFAULT_TABLE[0])); uint32_t__i++)
    {
        SIM_BOARD_STATE_T__G__Board.param[SIM_PARAM_DEFAULT_TABLE[uint32_t__i].id] = SIM_PARAM_DEFAULT_TABLE[uint32_t__i].value;
    }
    BOOL__G__SimParamsReady = true;
}

/* ==================== CRC و قاب‌بندی ==================== */
inline uint16_t func__Sim_Crc16(uint16_t uint16_t__crc, uint8_t uint8_t__byte)
{
    uint16_t uint16_t__mixed = (uint16_t)(uint16_t__crc ^ ((uint16_t)uint8_t__byte << 8));
    for (uint8_t uint8_t__bit = 0u; uint8_t__bit < 8u; uint8_t__bit++)
    {
        uint16_t__mixed = ((uint16_t__mixed & 0x8000u) != 0u)
                              ? (uint16_t)(((uint16_t)(uint16_t__mixed << 1)) ^
                                           (uint16_t)SIM_LINK_CRC16_POLY)
                              : (uint16_t)(uint16_t__mixed << 1);
    }
    return uint16_t__mixed;
}

/* فریم برد -> ESP: SOF، نسخه، نوع، طول LE، payload، CRC از نسخه به بعد */
inline void func__Sim_SendFrame(uint8_t uint8_t__type,
                                const uint8_t *uint8_t__ptr_payload,
                                uint16_t uint16_t__length)
{
    std::vector<uint8_t> vector_uint8_t__frame;
    vector_uint8_t__frame.push_back(SIM_LINK_SOF_BYTE0);
    vector_uint8_t__frame.push_back(SIM_LINK_SOF_BYTE1);
    vector_uint8_t__frame.push_back(SIM_LINK_VERSION);
    vector_uint8_t__frame.push_back(uint8_t__type);
    vector_uint8_t__frame.push_back((uint8_t)(uint16_t__length & 0xFFu));
    vector_uint8_t__frame.push_back((uint8_t)((uint16_t__length >> 8) & 0xFFu));

    uint16_t uint16_t__crc = func__Sim_Crc16(SIM_LINK_CRC16_INIT, SIM_LINK_VERSION);
    uint16_t__crc = func__Sim_Crc16(uint16_t__crc, uint8_t__type);
    uint16_t__crc = func__Sim_Crc16(uint16_t__crc, (uint8_t)(uint16_t__length & 0xFFu));
    uint16_t__crc = func__Sim_Crc16(uint16_t__crc, (uint8_t)((uint16_t__length >> 8) & 0xFFu));
    for (uint16_t uint16_t__index = 0u; uint16_t__index < uint16_t__length; uint16_t__index++)
    {
        vector_uint8_t__frame.push_back(uint8_t__ptr_payload[uint16_t__index]);
        uint16_t__crc = func__Sim_Crc16(uint16_t__crc, uint8_t__ptr_payload[uint16_t__index]);
    }
    vector_uint8_t__frame.push_back((uint8_t)(uint16_t__crc & 0xFFu));
    vector_uint8_t__frame.push_back((uint8_t)((uint16_t__crc >> 8) & 0xFFu));

    Serial.boardToEsp(vector_uint8_t__frame.data(), vector_uint8_t__frame.size());
}

/* هر فیلد با نوشتن صریح little-endian می‌نشیند؛ نه cast اشاره‌گر
   (که به هم‌ترازی و اندینِ میزبان وابسته می‌شود) */
inline void func__Sim_PutU32(uint8_t *uint8_t__ptr_target, uint32_t uint32_t__value)
{
    uint8_t__ptr_target[0] = (uint8_t)(uint32_t__value & 0xFFu);
    uint8_t__ptr_target[1] = (uint8_t)((uint32_t__value >> 8) & 0xFFu);
    uint8_t__ptr_target[2] = (uint8_t)((uint32_t__value >> 16) & 0xFFu);
    uint8_t__ptr_target[3] = (uint8_t)((uint32_t__value >> 24) & 0xFFu);
}

inline uint32_t func__Sim_CountsFromMv(uint32_t uint32_t__mv, float float__mvPerCount)
{
    if (float__mvPerCount <= 0.0f) { return 0u; }

    float float__counts = (float)uint32_t__mv / float__mvPerCount;
    return (float__counts <= 0.0f) ? 0u : (uint32_t)(float__counts + 0.5f);
}

/* ==================== مدل کوچک: duty و جریان از حالت شارژر ==================== */
inline uint32_t func__Sim_DutyPermille(uint32_t uint32_t__state)
{
    switch (uint32_t__state)
    {
        case 1u: return 450u;   /* حالت BULK */
        case 2u: return 300u;   /* حالت ABSORB */
        case 3u: return 120u;   /* حالت FLOAT */
        default: return 0u;
    }
}

inline uint32_t func__Sim_CurrentMa(uint32_t uint32_t__state)
{
    return (func__Sim_DutyPermille(uint32_t__state) * 16u) / 10u;
}

inline uint32_t func__Sim_SafeBoardTime(void)
{
    return (SIM_BOARD_STATE_T__G__Board.boardTimeSec != 0u)
               ? SIM_BOARD_STATE_T__G__Board.boardTimeSec
               : (100000u + SIM_BOARD_STATE_T__G__Board.uptimeSec);
}

/* ==================== ساخت و ارسال قاب تله‌متری ==================== */
inline void func__Sim_SendTlm(void)
{
    uint8_t uint8_t__payload[SIM_TLM_SIZE];
    memset(uint8_t__payload, 0, sizeof(uint8_t__payload));

    const uint32_t uint32_t__duty1 = func__Sim_DutyPermille(SIM_BOARD_STATE_T__G__Board.state1);
    const uint32_t uint32_t__duty2 = func__Sim_DutyPermille(SIM_BOARD_STATE_T__G__Board.state2);
    const uint32_t uint32_t__ma1 = func__Sim_CurrentMa(SIM_BOARD_STATE_T__G__Board.state1);
    const uint32_t uint32_t__ma2 = func__Sim_CurrentMa(SIM_BOARD_STATE_T__G__Board.state2);
    const uint32_t uint32_t__shunt1Uv =
        (uint32_t)(((float)uint32_t__ma1 * SIM_MA_PER_COUNT * SIM_UV_PER_COUNT) + 0.5f);
    const uint32_t uint32_t__shunt2Uv =
        (uint32_t)(((float)uint32_t__ma2 * SIM_MA_PER_COUNT * SIM_UV_PER_COUNT) + 0.5f);
    const bool bool__inputPresent = func__Sim_InputPermitted(SIM_BOARD_STATE_T__G__Board.vInMv);

    uint8_t__payload[0] = (uint8_t)(SIM_BOARD_STATE_T__G__Board.seq & 0xFFu);
    uint8_t__payload[1] = (uint8_t)((SIM_BOARD_STATE_T__G__Board.seq >> 8) & 0xFFu);
    /* b0 = قاب لحظه‌ای، b2 = اندازه‌گیری معتبر */
    uint8_t__payload[2] = (uint8_t)(0x01u | 0x04u);
    if (bool__inputPresent) { uint8_t__payload[2] |= 0x02u; }                 /* b1 = ورودی وصل */
    if (SIM_BOARD_STATE_T__G__Board.state1 == 9u) { uint8_t__payload[2] |= 0x20u; }  /* b5 = مود دستی */
    /* b0 = اپیزود عدم‌توازن در جریان، b1 = قفل عدم‌توازن */
    uint8_t__payload[3] = (uint8_t)((SIM_BOARD_STATE_T__G__Board.imbEpisode ? 0x01u : 0u) |
                                   (SIM_BOARD_STATE_T__G__Board.imbLatch ? 0x02u : 0u));

#define SIM_TLM_PUT(field, value) \
    func__Sim_PutU32(&uint8_t__payload[4u + (4u * (uint32_t)(field))], (uint32_t)(value))

    SIM_TLM_PUT(SIM_TLM_RAW1_COUNTS, func__Sim_CountsFromMv(uint32_t__shunt1Uv / 1000u, SIM_MV_PER_COUNT_ADC));
    SIM_TLM_PUT(SIM_TLM_SHUNT1_UV, uint32_t__shunt1Uv);
    SIM_TLM_PUT(SIM_TLM_MA1_UNFILTERED, uint32_t__ma1);
    SIM_TLM_PUT(SIM_TLM_I1_FILTERED_MA, uint32_t__ma1);
    SIM_TLM_PUT(SIM_TLM_IEST1_MA, uint32_t__ma1);
    SIM_TLM_PUT(SIM_TLM_DUTY1_PERMILLE, uint32_t__duty1);
    SIM_TLM_PUT(SIM_TLM_STATE1, SIM_BOARD_STATE_T__G__Board.state1);
    SIM_TLM_PUT(SIM_TLM_RAW2_COUNTS, func__Sim_CountsFromMv(uint32_t__shunt2Uv / 1000u, SIM_MV_PER_COUNT_ADC));
    SIM_TLM_PUT(SIM_TLM_SHUNT2_UV, uint32_t__shunt2Uv);
    SIM_TLM_PUT(SIM_TLM_MA2_UNFILTERED, uint32_t__ma2);
    SIM_TLM_PUT(SIM_TLM_I2_FILTERED_MA, uint32_t__ma2);
    SIM_TLM_PUT(SIM_TLM_IEST2_MA, uint32_t__ma2);
    SIM_TLM_PUT(SIM_TLM_DUTY2_PERMILLE, uint32_t__duty2);
    SIM_TLM_PUT(SIM_TLM_STATE2, SIM_BOARD_STATE_T__G__Board.state2);
    SIM_TLM_PUT(SIM_TLM_V_IN_MV, SIM_BOARD_STATE_T__G__Board.vInMv);
    SIM_TLM_PUT(SIM_TLM_V_BAT24_MV, SIM_BOARD_STATE_T__G__Board.vPackMv);
    SIM_TLM_PUT(SIM_TLM_V_BAT12_MV, SIM_BOARD_STATE_T__G__Board.vLowMv);
    SIM_TLM_PUT(SIM_TLM_V_BAT_LOW_MV, SIM_BOARD_STATE_T__G__Board.vLowMv);
    SIM_TLM_PUT(SIM_TLM_V_BAT_HIGH_MV, SIM_BOARD_STATE_T__G__Board.vHighMv);
    SIM_TLM_PUT(SIM_TLM_FAULT_MASK, SIM_BOARD_STATE_T__G__Board.faultMask);
    SIM_TLM_PUT(SIM_TLM_VIN_RAW_COUNTS, func__Sim_CountsFromMv(SIM_BOARD_STATE_T__G__Board.vInMv, SIM_MV_PER_COUNT_24));
    SIM_TLM_PUT(SIM_TLM_V24_RAW_COUNTS, func__Sim_CountsFromMv(SIM_BOARD_STATE_T__G__Board.vPackMv, SIM_MV_PER_COUNT_24));
    SIM_TLM_PUT(SIM_TLM_V12_RAW_COUNTS, func__Sim_CountsFromMv(SIM_BOARD_STATE_T__G__Board.vLowMv, SIM_MV_PER_COUNT_12));
    SIM_TLM_PUT(SIM_TLM_VREFINT_COUNTS, SIM_VREFINT_COUNTS);
    SIM_TLM_PUT(SIM_TLM_VDDA_MV, SIM_VDDA_MV);
    SIM_TLM_PUT(SIM_TLM_IMB_MV, (SIM_BOARD_STATE_T__G__Board.vHighMv > SIM_BOARD_STATE_T__G__Board.vLowMv)
                                     ? (SIM_BOARD_STATE_T__G__Board.vHighMv - SIM_BOARD_STATE_T__G__Board.vLowMv)
                                     : (SIM_BOARD_STATE_T__G__Board.vLowMv - SIM_BOARD_STATE_T__G__Board.vHighMv));
    SIM_TLM_PUT(SIM_TLM_IMB_EVENTS, SIM_BOARD_STATE_T__G__Board.imbEvents);
    SIM_TLM_PUT(SIM_TLM_IMB_LATCHED_CYCLES, SIM_BOARD_STATE_T__G__Board.imbLatchedCycles);
    SIM_TLM_PUT(SIM_TLM_DEAD_MASK, SIM_BOARD_STATE_T__G__Board.deadMask);
    SIM_TLM_PUT(SIM_TLM_DEAD_CHARGE1_S,
                ((SIM_BOARD_STATE_T__G__Board.state1 == 1u) || (SIM_BOARD_STATE_T__G__Board.state1 == 2u)) ? 240u : 0u);
    SIM_TLM_PUT(SIM_TLM_DEAD_CHARGE2_S,
                ((SIM_BOARD_STATE_T__G__Board.state2 == 1u) || (SIM_BOARD_STATE_T__G__Board.state2 == 2u)) ? 190u : 0u);
#undef SIM_TLM_PUT

    func__Sim_SendFrame(SIM_MSG_TLM_LIVE, uint8_t__payload, (uint16_t)SIM_TLM_SIZE);
    SIM_BOARD_STATE_T__G__Board.seq++;
    SIM_BOARD_STATE_T__G__Board.framesSent++;
}

/* ==================== چانک‌های دیاگ ==================== */
inline void func__Sim_SendDiagErrors(uint8_t uint8_t__chunk)
{
    uint8_t uint8_t__payload[8u + (5u * 32u)];
    memset(uint8_t__payload, 0, sizeof(uint8_t__payload));

    uint8_t uint8_t__start = (uint8_t__chunk == 1u) ? 0u : 5u;
    uint8_t uint8_t__count = 5u;

    uint8_t__payload[0] = uint8_t__chunk;
    uint8_t__payload[1] = uint8_t__start;
    uint8_t__payload[2] = uint8_t__count;

    const uint32_t uint32_t__boardTime = func__Sim_SafeBoardTime();
    const uint32_t uint32_t__faults[10] = {128u, 64u, 2u, 8u, 4u, 16u, 32u, 1u, 0u, 0u};
    for (uint8_t uint8_t__index = 0u; uint8_t__index < uint8_t__count; uint8_t__index++)
    {
        uint8_t *uint8_t__ptr_row = &uint8_t__payload[3u + (uint8_t__index * 32u)];
        func__Sim_PutU32(&uint8_t__ptr_row[0], uint32_t__boardTime - (uint32_t)((uint8_t__index + 1u) * 3600u));
        func__Sim_PutU32(&uint8_t__ptr_row[4], uint32_t__faults[uint8_t__start + uint8_t__index]);
        func__Sim_PutU32(&uint8_t__ptr_row[8], ((uint8_t__start + uint8_t__index) % 2u == 0u) ? 0u : 24000u);
        func__Sim_PutU32(&uint8_t__ptr_row[12], 25900u - ((uint32_t)uint8_t__index * 120u));
        func__Sim_PutU32(&uint8_t__ptr_row[16], 12900u);
        func__Sim_PutU32(&uint8_t__ptr_row[20], 13000u);
        func__Sim_PutU32(&uint8_t__ptr_row[24], (uint8_t__start + uint8_t__index) % 3u);
        func__Sim_PutU32(&uint8_t__ptr_row[28], (uint8_t__start + uint8_t__index) % 2u);
    }
    func__Sim_SendFrame(SIM_MSG_DIAG_DATA,
                        uint8_t__payload,
                        (uint16_t)(3u + (uint8_t__count * 32u)));
}

inline void func__Sim_SendDiagPower(uint8_t uint8_t__chunk)
{
    uint8_t uint8_t__payload[3u + (10u * 12u)];
    memset(uint8_t__payload, 0, sizeof(uint8_t__payload));

    uint8_t uint8_t__start = (uint8_t__chunk == 3u) ? 0u : 10u;
    uint8_t uint8_t__count = 10u;

    uint8_t__payload[0] = uint8_t__chunk;
    uint8_t__payload[1] = uint8_t__start;
    uint8_t__payload[2] = uint8_t__count;

    const uint32_t uint32_t__boardTime = func__Sim_SafeBoardTime();
    for (uint8_t uint8_t__index = 0u; uint8_t__index < uint8_t__count; uint8_t__index++)
    {
        uint8_t *uint8_t__ptr_row = &uint8_t__payload[3u + (uint8_t__index * 12u)];
        uint32_t uint32_t__offTime = uint32_t__boardTime - (uint32_t)((uint8_t__index + 1u) * 5400u);
        func__Sim_PutU32(&uint8_t__ptr_row[0], uint32_t__offTime);
        func__Sim_PutU32(&uint8_t__ptr_row[4], uint32_t__offTime + 600u + ((uint32_t)uint8_t__index * 60u));
        func__Sim_PutU32(&uint8_t__ptr_row[8], 600u + ((uint32_t)uint8_t__index * 60u));
    }
    func__Sim_SendFrame(SIM_MSG_DIAG_DATA,
                        uint8_t__payload,
                        (uint16_t)(3u + (uint8_t__count * 12u)));
}

inline void func__Sim_SendDiagSet(void)
{
    uint8_t uint8_t__payload[39];
    memset(uint8_t__payload, 0, sizeof(uint8_t__payload));

    uint8_t__payload[0] = 0u;                            /* چانک صفر */
    func__Sim_PutU32(&uint8_t__payload[1], SIM_BOARD_STATE_T__G__Board.totalChargeSec);
    func__Sim_PutU32(&uint8_t__payload[5], SIM_BOARD_STATE_T__G__Board.chargeCount);
    func__Sim_PutU32(&uint8_t__payload[9], SIM_BOARD_STATE_T__G__Board.uptimeSec);
    func__Sim_PutU32(&uint8_t__payload[13], SIM_BOARD_STATE_T__G__Board.resetCount);
    func__Sim_PutU32(&uint8_t__payload[17], func__Sim_SafeBoardTime());
    uint8_t__payload[21] = (uint8_t)SIM_DIAG_MAX_ERRORS;
    uint8_t__payload[22] = (uint8_t)SIM_DIAG_MAX_POWER_EVENTS;

    /* بخش توسعه‌یافته (طول ۳۹) که ESP تعداد شارژ هر کانال را از آن می‌خواند */
    func__Sim_PutU32(&uint8_t__payload[23], SIM_BOARD_STATE_T__G__Board.chargeCountCh1);
    func__Sim_PutU32(&uint8_t__payload[27], SIM_BOARD_STATE_T__G__Board.chargeCountCh2);
    func__Sim_PutU32(&uint8_t__payload[31], SIM_BOARD_STATE_T__G__Board.chargeSecCh1);
    func__Sim_PutU32(&uint8_t__payload[35], SIM_BOARD_STATE_T__G__Board.chargeSecCh2);
    func__Sim_SendFrame(SIM_MSG_DIAG_DATA, uint8_t__payload, sizeof(uint8_t__payload));

    func__Sim_SendDiagErrors(1u);
    func__Sim_SendDiagErrors(2u);
    func__Sim_SendDiagPower(3u);
    func__Sim_SendDiagPower(4u);
}

/* ==================== فرستادن همهٔ پارامترها (جواب GET_PARAMS) ====================
   مثل خود برد: همهٔ ۱۴۳ پارامتر در فریم‌های حداکثر ۱۰۲ آیتمی (۱ + ۱۰۲×۵ = ۵۱۱
   بایت، زیر سقف ۵۱۲ سند). ESP با کلید شناسه ذخیره می‌کند، پس چانک‌بندی مهم نیست. */
inline void func__Sim_SendParamReport(uint8_t uint8_t__id)
{
    uint8_t uint8_t__payload[SIM_PARAM_ITEM_SIZE];
    uint32_t uint32_t__value = SIM_BOARD_STATE_T__G__Board.param[uint8_t__id];

    uint8_t__payload[0] = uint8_t__id;
    uint8_t__payload[1] = (uint8_t)(uint32_t__value & 0xFFu);
    uint8_t__payload[2] = (uint8_t)((uint32_t__value >> 8) & 0xFFu);
    uint8_t__payload[3] = (uint8_t)((uint32_t__value >> 16) & 0xFFu);
    uint8_t__payload[4] = (uint8_t)((uint32_t__value >> 24) & 0xFFu);
    func__Sim_SendFrame(SIM_MSG_PARAM_REPORT, uint8_t__payload, (uint16_t)SIM_PARAM_ITEM_SIZE);
}

/* پژواک یک اسلات ریست: پارامترهای ۲۰۰..۲۰۳ در آرایهٔ پارامترها جا ندارند،
   پس مقدار اعمال‌شده مستقیم در قاب REPORT نوشته می‌شود. */
inline void func__Sim_SendParamEcho(uint8_t uint8_t__id, uint32_t uint32_t__value)
{
    uint8_t uint8_t__payload[SIM_PARAM_ITEM_SIZE];

    uint8_t__payload[0] = uint8_t__id;
    uint8_t__payload[1] = (uint8_t)(uint32_t__value & 0xFFu);
    uint8_t__payload[2] = (uint8_t)((uint32_t__value >> 8) & 0xFFu);
    uint8_t__payload[3] = (uint8_t)((uint32_t__value >> 16) & 0xFFu);
    uint8_t__payload[4] = (uint8_t)((uint32_t__value >> 24) & 0xFFu);
    func__Sim_SendFrame(SIM_MSG_PARAM_REPORT, uint8_t__payload, (uint16_t)SIM_PARAM_ITEM_SIZE);
}

inline void func__Sim_SendParamsBulk(void)
{
    uint8_t uint8_t__payload[1u + (SIM_PARAMS_PER_FRAME * SIM_PARAM_ITEM_SIZE)];
    uint32_t uint32_t__start = 0u;
    uint32_t uint32_t__frames = 0u;

    while (uint32_t__start < SIM_PARAM_COUNT)
    {
        uint32_t uint32_t__count = SIM_PARAM_COUNT - uint32_t__start;
        uint32_t uint32_t__item;

        if (uint32_t__count > SIM_PARAMS_PER_FRAME) { uint32_t__count = SIM_PARAMS_PER_FRAME; }
        uint8_t__payload[0] = (uint8_t)uint32_t__count;

        for (uint32_t__item = 0u; uint32_t__item < uint32_t__count; uint32_t__item++)
        {
            uint8_t uint8_t__id = (uint8_t)(uint32_t__start + uint32_t__item);
            uint16_t uint16_t__offset = (uint16_t)(1u + (uint32_t__item * SIM_PARAM_ITEM_SIZE));
            uint32_t uint32_t__value = SIM_BOARD_STATE_T__G__Board.param[uint8_t__id];

            uint8_t__payload[uint16_t__offset] = uint8_t__id;
            uint8_t__payload[uint16_t__offset + 1u] = (uint8_t)(uint32_t__value & 0xFFu);
            uint8_t__payload[uint16_t__offset + 2u] = (uint8_t)((uint32_t__value >> 8) & 0xFFu);
            uint8_t__payload[uint16_t__offset + 3u] = (uint8_t)((uint32_t__value >> 16) & 0xFFu);
            uint8_t__payload[uint16_t__offset + 4u] = (uint8_t)((uint32_t__value >> 24) & 0xFFu);
        }

        func__Sim_SendFrame(SIM_MSG_PARAMS_BULK, uint8_t__payload,
                            (uint16_t)(1u + (uint32_t__count * SIM_PARAM_ITEM_SIZE)));
        uint32_t__start += uint32_t__count;
        uint32_t__frames++;
    }

    printf("[sim] درخواست پارامترها رسید - %lu فریم PARAMS_BULK فرستاده شد\n",
           (unsigned long)uint32_t__frames);
}

/* ==================== خواندن قاب‌های ESP -> برد ==================== */
inline void func__Sim_HandleEspFrame(const uint8_t *uint8_t__ptr_payload,
                                     uint16_t uint16_t__length,
                                     uint8_t uint8_t__type)
{
    if ((uint8_t__type == SIM_MSG_DIAG_REQ) && (uint16_t__length == 0u))
    {
        func__Sim_SendDiagSet();
        printf("[sim] درخواست دیاگ رسید - پنج چانک فرستاده شد\n");
        return;
    }
    if ((uint8_t__type == SIM_MSG_GET_PARAMS) && (uint16_t__length == 0u))
    {
        func__Sim_ParamsInit();
        func__Sim_SendParamsBulk();
        return;
    }
    if ((uint8_t__type == SIM_MSG_TIME_SYNC) && (uint16_t__length >= 4u))
    {
        uint32_t uint32_t__seconds = (uint32_t)uint8_t__ptr_payload[0] |
                                     ((uint32_t)uint8_t__ptr_payload[1] << 8) |
                                     ((uint32_t)uint8_t__ptr_payload[2] << 16) |
                                     ((uint32_t)uint8_t__ptr_payload[3] << 24);
        SIM_BOARD_STATE_T__G__Board.boardTimeSec = uint32_t__seconds;
        printf("[sim] ساعت برد از گوشی تنظیم شد: %lu\n", (unsigned long)uint32_t__seconds);
        return;
    }
    if ((uint8_t__type == SIM_MSG_SET_PARAM) && (uint16_t__length >= 5u))
    {
        /* نوشتن پارامتر مثل برد واقعی: [شناسه:u8][مقدار:u32 LE]. اسلات‌های
           ریست قفل با مقدار صفر پاک می‌شوند و مثل برد واقعی PARAM_REPORT
           پژواک داده می‌شود تا پنل بتواند تأیید بگیرد. */
        const uint8_t uint8_t__id = uint8_t__ptr_payload[0];
        uint32_t uint32_t__value = (uint32_t)uint8_t__ptr_payload[1] |
                                   ((uint32_t)uint8_t__ptr_payload[2] << 8) |
                                   ((uint32_t)uint8_t__ptr_payload[3] << 16) |
                                   ((uint32_t)uint8_t__ptr_payload[4] << 24);

        if (uint8_t__id == 202u) { SIM_BOARD_STATE_T__G__Board.imbLatch = (uint32_t__value != 0u) ? 1u : 0u; }
        else if (uint8_t__id == 201u) { SIM_BOARD_STATE_T__G__Board.imbLatchedCycles = uint32_t__value; }
        else if (uint8_t__id == 200u) { SIM_BOARD_STATE_T__G__Board.imbEvents = uint32_t__value; }
        else if (uint8_t__id == 203u) { SIM_BOARD_STATE_T__G__Board.deadMask = uint32_t__value & 0x03u; }
        else if (uint8_t__id < SIM_PARAM_COUNT) { SIM_BOARD_STATE_T__G__Board.param[uint8_t__id] = uint32_t__value; }
        else { return; }   /* شناسهٔ ناشناخته: بی‌صدا رد می‌شود */

        printf("[sim] SET_PARAM %u = %lu اعمال شد\n",
               (unsigned)uint8_t__id, (unsigned long)uint32_t__value);
        func__Sim_SendParamEcho(uint8_t__id, uint32_t__value);
    }
}

inline void func__Sim_PollEspBytes(void)
{
    uint8_t uint8_t__byte = 0u;
    while (Serial.takeEspByte(&uint8_t__byte))
    {
        switch (SIM_RX_STATE_T__G__SimRxState)
        {
            case SIM_RX_WAIT_SOF0:
                if (uint8_t__byte == SIM_LINK_SOF_BYTE0) { SIM_RX_STATE_T__G__SimRxState = SIM_RX_WAIT_SOF1; }
                break;

            case SIM_RX_WAIT_SOF1:
                if (uint8_t__byte == SIM_LINK_SOF_BYTE1)
                {
                    SIM_RX_STATE_T__G__SimRxState = SIM_RX_WAIT_VERSION;
                }
                else if (uint8_t__byte != SIM_LINK_SOF_BYTE0)
                {
                    SIM_RX_STATE_T__G__SimRxState = SIM_RX_WAIT_SOF0;
                }
                break;

            case SIM_RX_WAIT_VERSION:
                if (uint8_t__byte != SIM_LINK_VERSION)
                {
                    SIM_RX_STATE_T__G__SimRxState = SIM_RX_WAIT_SOF0;
                }
                else
                {
                    UINT16_T__G__SimRxCrc = func__Sim_Crc16(SIM_LINK_CRC16_INIT, uint8_t__byte);
                    SIM_RX_STATE_T__G__SimRxState = SIM_RX_WAIT_TYPE;
                }
                break;

            case SIM_RX_WAIT_TYPE:
                UINT8_T__G__SimRxType = uint8_t__byte;
                UINT16_T__G__SimRxCrc = func__Sim_Crc16(UINT16_T__G__SimRxCrc, uint8_t__byte);
                SIM_RX_STATE_T__G__SimRxState = SIM_RX_WAIT_LEN_LO;
                break;

            case SIM_RX_WAIT_LEN_LO:
                UINT16_T__G__SimRxLength = uint8_t__byte;
                UINT16_T__G__SimRxCrc = func__Sim_Crc16(UINT16_T__G__SimRxCrc, uint8_t__byte);
                SIM_RX_STATE_T__G__SimRxState = SIM_RX_WAIT_LEN_HI;
                break;

            case SIM_RX_WAIT_LEN_HI:
                UINT16_T__G__SimRxLength = (uint16_t)(UINT16_T__G__SimRxLength | ((uint16_t)uint8_t__byte << 8));
                UINT16_T__G__SimRxCrc = func__Sim_Crc16(UINT16_T__G__SimRxCrc, uint8_t__byte);
                if (UINT16_T__G__SimRxLength > sizeof(UINT8_T__G__SimRxPayload))
                {
                    SIM_RX_STATE_T__G__SimRxState = SIM_RX_WAIT_SOF0;
                }
                else
                {
                    UINT16_T__G__SimRxIndex = 0u;
                    SIM_RX_STATE_T__G__SimRxState =
                        (UINT16_T__G__SimRxLength == 0u) ? SIM_RX_WAIT_CRC_LO : SIM_RX_WAIT_PAYLOAD;
                }
                break;

            case SIM_RX_WAIT_PAYLOAD:
                UINT8_T__G__SimRxPayload[UINT16_T__G__SimRxIndex] = uint8_t__byte;
                UINT16_T__G__SimRxIndex++;
                UINT16_T__G__SimRxCrc = func__Sim_Crc16(UINT16_T__G__SimRxCrc, uint8_t__byte);
                if (UINT16_T__G__SimRxIndex >= UINT16_T__G__SimRxLength)
                {
                    SIM_RX_STATE_T__G__SimRxState = SIM_RX_WAIT_CRC_LO;
                }
                break;

            case SIM_RX_WAIT_CRC_LO:
                UINT8_T__G__SimRxCrcLow = uint8_t__byte;
                SIM_RX_STATE_T__G__SimRxState = SIM_RX_WAIT_CRC_HI;
                break;

            case SIM_RX_WAIT_CRC_HI:
            default:
            {
                uint16_t uint16_t__received =
                    (uint16_t)(((uint16_t)uint8_t__byte << 8) | (uint16_t)UINT8_T__G__SimRxCrcLow);
                if (uint16_t__received == UINT16_T__G__SimRxCrc)
                {
                    func__Sim_HandleEspFrame(UINT8_T__G__SimRxPayload,
                                             UINT16_T__G__SimRxLength,
                                             UINT8_T__G__SimRxType);
                }
                else
                {
                    printf("[sim] قاب ESP با CRC غلط رسید\n");
                }
                SIM_RX_STATE_T__G__SimRxState =
                    (uint8_t__byte == SIM_LINK_SOF_BYTE0) ? SIM_RX_WAIT_SOF1 : SIM_RX_WAIT_SOF0;
                break;
            }
        }
    }
}

/* ==================== تیک برد ==================== */
inline void func__Sim_Tick(uint32_t uint32_t__nowMs)
{
    if ((uint32_t__nowMs - UINT32_T__G__SimLastUptimeMs) >= 1000u)
    {
        UINT32_T__G__SimLastUptimeMs = uint32_t__nowMs;
        SIM_BOARD_STATE_T__G__Board.uptimeSec++;

        /* همان کاری که RTC واقعی برد می‌کند: بعد از گرفتن ساعت از پنل، خودش
           ثانیه‌شماری را ادامه می‌دهد تا مقایسهٔ اختلاف دو ساعت معنا داشته باشد */
        if (SIM_BOARD_STATE_T__G__Board.boardTimeSec >= 100000u)
        {
            SIM_BOARD_STATE_T__G__Board.boardTimeSec++;
        }

        if (func__Sim_InputPermitted(SIM_BOARD_STATE_T__G__Board.vInMv) &&
            ((SIM_BOARD_STATE_T__G__Board.state1 == 1u) || (SIM_BOARD_STATE_T__G__Board.state2 == 1u)))
        {
            SIM_BOARD_STATE_T__G__Board.totalChargeSec++;
            SIM_BOARD_STATE_T__G__Board.chargeSecCh1++;
        }
    }

    /* شمارندهٔ شارژ کامل هر ۶۰ ثانیه یکی بالا می‌رود تا پنل ردیف «تاریخچه
       شارژ» را واقعاً از خودش بسازد (همان مسیری که روی برد اجرا می‌شود) */
    if ((uint32_t__nowMs - UINT32_T__G__SimLastCountMs) >= 60000u)
    {
        UINT32_T__G__SimLastCountMs = uint32_t__nowMs;
        SIM_BOARD_STATE_T__G__Board.chargeCountCh1++;
        SIM_BOARD_STATE_T__G__Board.chargeCount++;
        SIM_BOARD_STATE_T__G__Board.chargeSecCh1 += 120u;
    }

    func__Sim_PollEspBytes();

    if (!SIM_BOARD_STATE_T__G__Board.linkUp) { return; }

    if ((uint32_t__nowMs - SIM_BOARD_STATE_T__G__Board.lastTlmMs) >= 100u)   /* ۱۰ هرتز، مثل برد */
    {
        SIM_BOARD_STATE_T__G__Board.lastTlmMs = uint32_t__nowMs;
        func__Sim_SendTlm();
    }
}

/* ==================== اعمال درخواست‌های /sim ==================== */
inline bool func__Sim_ArgU32(const SimWebServerT &web_server__server,
                            const char *char__ptr_name,
                            uint32_t *uint32_t__ptr_out)
{
    if (!web_server__server.hasArg(char__ptr_name)) { return false; }

    String string__value = web_server__server.arg(char__ptr_name);
    if (string__value.length() == 0u) { return false; }

    long long__parsed = string__value.toInt();
    if (long__parsed < 0) { long__parsed = 0; }

    *uint32_t__ptr_out = (uint32_t)long__parsed;
    return true;
}

inline void func__Sim_ApplyRequest(const SimWebServerT &web_server__server)
{
    uint32_t uint32_t__value = 0u;

    if (func__Sim_ArgU32(web_server__server, "vin", &uint32_t__value))
    {
        SIM_BOARD_STATE_T__G__Board.vInMv = uint32_t__value;
    }
    if (func__Sim_ArgU32(web_server__server, "vpack", &uint32_t__value))
    {
        SIM_BOARD_STATE_T__G__Board.vPackMv = uint32_t__value;
    }
    if (func__Sim_ArgU32(web_server__server, "vlow", &uint32_t__value))
    {
        SIM_BOARD_STATE_T__G__Board.vLowMv = uint32_t__value;
    }
    if (func__Sim_ArgU32(web_server__server, "vhigh", &uint32_t__value))
    {
        SIM_BOARD_STATE_T__G__Board.vHighMv = uint32_t__value;
    }
    if (func__Sim_ArgU32(web_server__server, "s1", &uint32_t__value))
    {
        SIM_BOARD_STATE_T__G__Board.state1 = (uint32_t__value > 10u) ? 10u : uint32_t__value;
    }
    if (func__Sim_ArgU32(web_server__server, "s2", &uint32_t__value))
    {
        SIM_BOARD_STATE_T__G__Board.state2 = (uint32_t__value > 10u) ? 10u : uint32_t__value;
    }
    if (func__Sim_ArgU32(web_server__server, "fault", &uint32_t__value))
    {
        SIM_BOARD_STATE_T__G__Board.faultMask = uint32_t__value & 0xFFu;
    }
    if (func__Sim_ArgU32(web_server__server, "dead", &uint32_t__value))
    {
        SIM_BOARD_STATE_T__G__Board.deadMask = uint32_t__value & 0x03u;
    }
    /* قفل/اپیزود عدم‌توازن برای آزمون دکمهٔ ریست: ‎imb=1‎ قفل، ‎imb=2‎ فقط
       اپیزود، ‎imb=0‎ پاک. مقدار ۲۰۰ (شمارندهٔ رویداد) هم با ریست صفر می‌شود. */
    if (func__Sim_ArgU32(web_server__server, "imb", &uint32_t__value))
    {
        SIM_BOARD_STATE_T__G__Board.imbLatch = (uint32_t__value == 1u) ? 1u : 0u;
        SIM_BOARD_STATE_T__G__Board.imbEpisode = (uint32_t__value == 2u) ? 1u : 0u;
        if (uint32_t__value != 0u)
        {
            SIM_BOARD_STATE_T__G__Board.imbEvents = 10u;
            SIM_BOARD_STATE_T__G__Board.imbLatchedCycles = 3u;
        }
    }
    if (func__Sim_ArgU32(web_server__server, "link", &uint32_t__value))
    {
        SIM_BOARD_STATE_T__G__Board.linkUp = (uint32_t__value != 0u);
    }
    /* ساعت RTC خود برد: برای آزمون حالتی که برد ساعت دارد و پنل ندارد/عقب است */
    if (func__Sim_ArgU32(web_server__server, "rtc", &uint32_t__value))
    {
        SIM_BOARD_STATE_T__G__Board.boardTimeSec = uint32_t__value;
    }

    /* پارامترهای خود برد: با تغییرشان، پنل باید همان لحظه با عددهای تازه
       حساب کند (مثل SET_PARAM روی برد واقعی، جواب PARAM_REPORT می‌رود). */
    func__Sim_ParamsInit();
    {
        static const struct { const char *name; uint8_t id; } SIM_PARAM_ARG_TABLE[] =
        {
            { "p66", 66u }, { "p67", 67u }, { "p74", 74u }, { "p75", 75u }
        };
        for (uint32_t uint32_t__i = 0u; uint32_t__i < (sizeof(SIM_PARAM_ARG_TABLE) / sizeof(SIM_PARAM_ARG_TABLE[0])); uint32_t__i++)
        {
            if (func__Sim_ArgU32(web_server__server, SIM_PARAM_ARG_TABLE[uint32_t__i].name, &uint32_t__value))
            {
                SIM_BOARD_STATE_T__G__Board.param[SIM_PARAM_ARG_TABLE[uint32_t__i].id] = uint32_t__value;
                func__Sim_SendParamReport(SIM_PARAM_ARG_TABLE[uint32_t__i].id);
                printf("[sim] پارامتر %u = %lu شد\n",
                       (unsigned)SIM_PARAM_ARG_TABLE[uint32_t__i].id, (unsigned long)uint32_t__value);
            }
        }
    }
}

#endif /* SIM_BOARD_H */
