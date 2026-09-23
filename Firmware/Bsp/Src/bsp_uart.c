/**
 * @file    bsp_uart.c
 * @brief   [EN] STM32F103C8T6 USART1 port for the ESP-Link connector.
 *          [FA] پورت USART1 روی STM32F103C8T6 برای کانکتور ESP-Link.
 *
 * @note    [EN] CubeMX initializes USART1 at 115200 8-N-1 on PA9/PA10.
 *              Product modules use only the logical byte-stream API.
 *          [FA] CubeMX، USART1 را با 115200 و قالب 8-N-1 روی PA9/PA10
 *              مقداردهی می‌کند و ماژول‌ها فقط API منطقی بایت را می‌بینند.
 *
 * @note    [EN] High-speed / zero-CPU transport (user order 2026-09-23:
 *              raise the link speed as far as possible without loading
 *              the CPU):
 *              - The baud rate is reconfigured at runtime to 921600 (the
 *                Cube-generated 115200 init stays untouched; PA9/PA10 and
 *                8-N-1 are unchanged).
 *              - RX runs on DMA1_Channel5 in CIRCULAR mode into a 256-byte
 *                buffer - zero CPU per received byte, no USART interrupt
 *                at all. The consumer derives the write index from the
 *                DMA counter (classic STM32 circular-DMA ring).
 *              - TX runs on DMA1_Channel4 from a 256-byte software ring:
 *                writes are non-blocking, the pump copies one contiguous
 *                chunk into a linear buffer per DMA transfer. In this HAL
 *                a NORMAL-mode DMA completion does NOT call the UART
 *                callback directly: it clears DMAT and arms the UART
 *                transmit-complete interrupt (TCIE), and the callback that
 *                dequeues the next chunk fires from USART1_IRQHandler when
 *                the last bit has left the shifter. Net cost: a couple of
 *                lightweight interrupts per frame (~20-40 IRQs/s at the
 *                100 ms telemetry cadence), still ~0 CPU per byte. One
 *                96-byte frame takes ~1 ms of wire time at 921600.
 *          [FA] انتقال پرسرعت بدون بار CPU (دستور کاربر ۲۰۲۶-۰۹-۲۳:
 *              سرعت لینک تا جای ممکن بالا، بدون درگیر شدن CPU):
 *              - baud در زمان اجرا به ۹۲۱۶۰۰ تغییر می‌کند (مقداردهی
 *                ۱۱۵۲۰۰ تولیدشدهٔ Cube دست‌نخورده می‌ماند؛ PA9/PA10 و
 *                قالب 8-N-1 بدون تغییر).
 *              - RX روی DMA1_Channel5 به‌صورت CIRCULAR داخل بافر ۲۵۶
 *                بایتی - صفر CPU به‌ازای هر بایت دریافتی و اصلاً بدون
 *                وقفهٔ USART. مصرف‌کننده اندیس نوشتن را از شمارندهٔ DMA
 *                می‌گیرد (الگوی معروف حلقهٔ DMA حلقوی STM32).
 *              - TX روی DMA1_Channel4 از یک حلقهٔ نرم‌افزاری ۲۵۶ بایتی:
 *                نوشتن بدون بلوکه شدن است و پمپ برای هر انتقال DMA یک
 *                قطعهٔ پیوسته را داخل بافر خطی کپی می‌کند. در این HAL،
 *                کامل‌شدن DMA ی مود NORMAL مستقیم callback ی UART را صدا
 *                نمی‌زند: DMAT را پاک و وقفهٔ transmit-complete ی UART
 *                (TCIE) را مسلح می‌کند و callback ی قطعهٔ بعدی از
 *                USART1_IRQHandler می‌آید وقتی آخرین بیت از شیفت‌رجیستر
 *                خارج شده است. هزینهٔ خالص: چند وقفهٔ سبک به‌ازای هر فریم
 *                (حدود ۲۰ تا ۴۰ وقفه در ثانیه با cadence ی ۱۰۰ms)، باز هم
 *                تقریباً صفر CPU به‌ازای هر بایت. یک فریم ۹۶ بایتی روی
 *                ۹۲۱۶۰۰ حدود ۱ms زمانِ سیم می‌خورد.
 */

#include "bsp_uart.h"
#include "main.h"

#include <stddef.h>

/* ==================== Link constants / ثابت‌های لینک ==================== */

/* [EN] Wire speed after the runtime reconfiguration (user order
 *      2026-09-23). 921600 is 8x the Cube default, well inside the
 *      USART1 limit on APB2 (72 MHz / 16 / 921600 = 4.88, +0.16% error)
 *      and a rate both ESP8266 and ESP32 UARTs run reliably.
 * [FA] سرعت سیم بعد از پیکربندی مجدد زمان اجرا (دستور کاربر
 *      ۲۰۲۶-۰۹-۲۳). ۹۲۱۶۰۰ هشت برابر پیش‌فرض Cube، داخل محدودهٔ USART1
 *      روی APB2 (72MHz/16/921600 = 4.88، خطای +۰.۱۶٪) و نرخی که UART هر
 *      دو ماژول ESP8266 و ESP32 به‌طور مطمئن اجرا می‌کنند. */
#define BSP_UART_BAUD_RATE        921600u

/* [EN] Circular RX DMA ring (power-of-two size keeps the index wrap cheap;
 *      not a correctness requirement).
 * [FA] حلقهٔ DMA حلقوی RX (اندازهٔ توان دو جابه‌جایی اندیس را ارزان
 *      می‌کند؛ شرط درستی نیست). */
#define BSP_UART_RX_RING_SIZE     256u

/* [EN] Software TX ring and the max bytes moved per DMA transfer.
 * [FA] حلقهٔ نرم‌افزاری TX و حداکثر بایت منتقل‌شده در هر انتقال DMA. */
#define BSP_UART_TX_RING_SIZE     256u
#define BSP_UART_TX_CHUNK_SIZE    128u

/* [EN] Lowest NVIC priority for both interrupt vectors of this port (the
 *      TX-DMA complete and the UART transmit-complete that this HAL arms
 *      afterwards); neither handler calls any RTOS API.
 * [FA] کمترین اولویت NVIC برای هر دو بردار وقفهٔ این پورت (کامل‌شدن DMA ی
 *      TX و transmit-complete ی UART که این HAL بعدش مسلح می‌کند)؛ هیچ‌کدام
 *      از هندلرها API سیستمعاملی صدا نمی‌زنند. */
#define BSP_UART_IRQ_PRIORITY 15u

static UART_HandleTypeDef * volatile UART_HANDLETYPEDEF__G__EspLink = NULL;
static bool BOOL__G__Initialized = false;

/* [EN] Private DMA handles for USART1 (not in the Cube project: the board
 *      port wires them at runtime). TX = DMA1_Channel4, RX = DMA1_Channel5
 *      - fixed F103 mappings, no conflict with the ADC on Channel1.
 * [FA] هندل‌های خصوصی DMA برای USART1 (در پروژهٔ Cube نیستند: پورت برد
 *      آنها را در زمان اجرا سیم‌پیچی می‌کند). TX = DMA1_Channel4 و
 *      RX = DMA1_Channel5 - نگاشت ثابت F103، بدون تداخل با ADC روی
 *      Channel1. */
static DMA_HandleTypeDef DMA_HANDLETYPEDEF__G__TxDma;
static DMA_HandleTypeDef DMA_HANDLETYPEDEF__G__RxDma;

/* [EN] RX ring: the DMA engine writes this buffer circularly; the consumer
 *      owns only the tail index, the head comes from the DMA counter.
 * [FA] حلقهٔ RX: موتور DMA این بافر را حلقوی می‌نویسد؛ مصرف‌کننده فقط
 *      مالک اندیس tail است و head از شمارندهٔ DMA می‌آید. */
static uint8_t UINT8_T__G__RxDmaBuffer[BSP_UART_RX_RING_SIZE];
static volatile uint16_t UINT16_T__G__RxTailIndex;

/* [EN] TX software ring: the EspLink task produces (Write), the DMA pump
 *      consumes. TxDmaActive is the single transfer-in-flight guard, so
 *      the task pump and the DMA-complete callback can never race: the
 *      callback only fires while the flag is true, the task pump only
 *      starts while it is false.
 * [FA] حلقهٔ نرم‌افزاری TX: تسک EspLink تولید می‌کند (Write) و پمپ DMA
 *      مصرف می‌کند. TxDmaActive تنها گاردِ یک-انتقال-در-پرواز است، پس
 *      پمپ تسک و کال‌بک کامل‌شدن DMA هرگز با هم مسابقه نمی‌کنند: کال‌بک
 *      فقط وقتی پرچم true است شلیک می‌شود و پمپ تسک فقط وقتی false
 *      است شروع می‌کند. */
static uint8_t UINT8_T__G__TxRing[BSP_UART_TX_RING_SIZE];
static volatile uint16_t UINT16_T__G__TxHeadIndex;
static volatile uint16_t UINT16_T__G__TxTailIndex;
static uint8_t UINT8_T__G__TxDmaChunk[BSP_UART_TX_CHUNK_SIZE];
static volatile bool BOOL__G__TxDmaActive;

/* ==================== BspUart_Init ==================== */
/**
 * @brief  [EN] Select the USART1 board handle, retune it to
 *              BSP_UART_BAUD_RATE (Cube stays at 115200; the generated
 *              code is deliberately untouched), wire both DMA channels,
 *              start the circular DMA reception and reset the TX path.
 *         [FA] هندل USART1 برد را انتخاب، آن را به BSP_UART_BAUD_RATE
 *              بازتنظیم می‌کند (Cube روی ۱۱۵۲۰۰ می‌ماند؛ کد تولیدشده
 *              عمداً دست نمی‌خورد)، هر دو کانال DMA را سیم‌پیچی، دریافت
 *              DMA حلقوی را شروع و مسیر TX را ریست می‌کند.
 */
void func__BspUart_Init(void)
{
    UART_HANDLETYPEDEF__G__EspLink = &huart1;

    /* [EN] Reconfigure only the line speed; word length, stop bits and
       parity keep the Cube values (8-N-1).
       [FA] فقط سرعت خط بازتنظیم می‌شود؛ طول کلمه، بیت توقف و پاریتی
       مقادیر Cube (8-N-1) را نگه می‌دارند. */
    (void)HAL_UART_DeInit(UART_HANDLETYPEDEF__G__EspLink);
    UART_HANDLETYPEDEF__G__EspLink->Init.BaudRate = BSP_UART_BAUD_RATE;
    (void)HAL_UART_Init(UART_HANDLETYPEDEF__G__EspLink);

    __HAL_RCC_DMA1_CLK_ENABLE();

    /* [EN] TX DMA: memory-to-peripheral, increment memory only, normal
       mode - one frame chunk per transfer.
       [FA] DMA ی TX: حافظه-به-پریفرال، فقط حافظه افزایشی، مود نرمال -
       یک قطعهٔ فریم در هر انتقال. */
    DMA_HANDLETYPEDEF__G__TxDma.Instance = DMA1_Channel4;
    DMA_HANDLETYPEDEF__G__TxDma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    DMA_HANDLETYPEDEF__G__TxDma.Init.PeriphInc = DMA_PINC_DISABLE;
    DMA_HANDLETYPEDEF__G__TxDma.Init.MemInc = DMA_MINC_ENABLE;
    DMA_HANDLETYPEDEF__G__TxDma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    DMA_HANDLETYPEDEF__G__TxDma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    DMA_HANDLETYPEDEF__G__TxDma.Init.Mode = DMA_NORMAL;
    DMA_HANDLETYPEDEF__G__TxDma.Init.Priority = DMA_PRIORITY_LOW;
    (void)HAL_DMA_Init(&DMA_HANDLETYPEDEF__G__TxDma);

    /* [EN] RX DMA: peripheral-to-memory, increment memory only, circular
       mode - the engine refills the ring forever without any CPU or IRQ.
       [FA] DMA ی RX: پریفرال-به-حافظه، فقط حافظه افزایشی، مود حلقوی -
       موتور برای همیشه حلقه را پر می‌کند بدون هیچ CPU یا وقفه‌ای. */
    DMA_HANDLETYPEDEF__G__RxDma.Instance = DMA1_Channel5;
    DMA_HANDLETYPEDEF__G__RxDma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    DMA_HANDLETYPEDEF__G__RxDma.Init.PeriphInc = DMA_PINC_DISABLE;
    DMA_HANDLETYPEDEF__G__RxDma.Init.MemInc = DMA_MINC_ENABLE;
    DMA_HANDLETYPEDEF__G__RxDma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    DMA_HANDLETYPEDEF__G__RxDma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    DMA_HANDLETYPEDEF__G__RxDma.Init.Mode = DMA_CIRCULAR;
    DMA_HANDLETYPEDEF__G__RxDma.Init.Priority = DMA_PRIORITY_HIGH;
    (void)HAL_DMA_Init(&DMA_HANDLETYPEDEF__G__RxDma);

    __HAL_LINKDMA(UART_HANDLETYPEDEF__G__EspLink, hdmatx,
                  DMA_HANDLETYPEDEF__G__TxDma);
    __HAL_LINKDMA(UART_HANDLETYPEDEF__G__EspLink, hdmarx,
                  DMA_HANDLETYPEDEF__G__RxDma);

    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, BSP_UART_IRQ_PRIORITY, 0u);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);

    /* [EN] In this HAL the NORMAL-mode DMA-complete does not call the UART
       callback directly: it arms the UART transmit-complete interrupt
       (TCIE) and the callback fires from USART1_IRQHandler. Without this
       NVIC enable the TX would stall forever after the very first frame
       (the in-flight guard would never be released).
       [FA] در این HAL، کامل‌شدن DMA ی مود NORMAL مستقیم callback ی UART
       را صدا نمی‌زند: وقفهٔ transmit-complete ی UART (TCIE) را مسلح
       می‌کند و callback از USART1_IRQHandler می‌آید. بدون فعال‌کردن این
       NVIC، TX بعد از همان اولین فریم برای همیشه می‌ایستاد (گارد
       در-پرواز هرگز آزاد نمی‌شد). */
    HAL_NVIC_SetPriority(USART1_IRQn, BSP_UART_IRQ_PRIORITY, 0u);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    UINT16_T__G__RxTailIndex = 0u;
    UINT16_T__G__TxHeadIndex = 0u;
    UINT16_T__G__TxTailIndex = 0u;
    BOOL__G__TxDmaActive = false;

    (void)HAL_UART_Receive_DMA(UART_HANDLETYPEDEF__G__EspLink,
                               UINT8_T__G__RxDmaBuffer,
                               BSP_UART_RX_RING_SIZE);

    /* [EN] Disarm the error interrupt that HAL armed for DMA RX. With it
       enabled, the very first framing/noise/overrun flag on the line
       (guaranteed real-world case: an ESP8266 reboot prints boot garbage
       at 74880 baud onto this wire) would make USART1_IRQHandler run the
       HAL error path, which ABORTS the circular DMA and silently kills
       reception forever. With the error interrupt disarmed the flags are
       harmless: DMAR keeps draining DR, a corrupted byte still lands in
       the ring, and the frame parser drops bad frames via the XOR
       checksum - the RX ring becomes immortal.
       [FA] وقفهٔ خطایی را که HAL برای DMA ی RX مسلح کرده غیرمسلح می‌کنیم.
       با فعال‌بودنش، اولین پرچم framing/noise/overrun روی خط (مورد صددرصد
       واقعی: ریبوت ESP8266 که garbage بوت را با 74880 baud روی همین سیم
       می‌ریزد) باعث می‌شود USART1_IRQHandler مسیر خطای HAL را برود که DMA
       حلقوی را abort می‌کند و دریافت برای همیشه بی‌صدا می‌میرد. با غیرمسلح
       شدنش پرچم‌ها بی‌ضررند: DMAR به تخلیهٔ DR ادامه می‌دهد، بایت خراب
       باز هم داخل حلقه می‌نشیند و پارسر فریم خراب را با XOR رها می‌کند -
       حلقهٔ RX جاودانه می‌شود. */
    __HAL_UART_DISABLE_IT(UART_HANDLETYPEDEF__G__EspLink, UART_IT_ERR);

    BOOL__G__Initialized = true;
}

/* ==================== BspUart TX pump / پمپ TX ==================== */

/**
 * @brief  [EN] Move the next contiguous chunk of the TX ring into the DMA
 *              engine. Safe from task context and from the DMA-complete
 *              callback: the TxDmaActive guard means only one of them can
 *              ever start a transfer, and the tail+guard are committed
 *              BEFORE the engine starts (rolled back on refusal), so no
 *              interrupt can ever observe a half-committed dequeue. If
 *              the HAL refuses the transfer, the data stays in the ring
 *              and the next pump retries.
 *         [FA] قطعهٔ پیوستهٔ بعدی حلقهٔ TX را داخل موتور DMA می‌برد. از
 *              زمینهٔ تسک و از کال‌بک کامل‌شدن DMA امن است: گارد
 *              TxDmaActive تضمین می‌کند همیشه فقط یکی از آنها داخل
 *              باشد. اگر HAL انتقال را رد کند، داده در حلقه می‌ماند و
 *              پمپ بعدی دوباره می‌کوشد.
 */
static void func__BspUart_PumpTx(void)
{
    uint16_t uint16_t__head;
    uint16_t uint16_t__tail;
    uint16_t uint16_t__chunkLength;
    uint16_t uint16_t__contiguous;
    uint16_t uint16_t__nextTail;

    if ((BOOL__G__TxDmaActive != false) ||
        (UART_HANDLETYPEDEF__G__EspLink == NULL) ||
        (BOOL__G__Initialized == false))
    {
        return;
    }

    uint16_t__head = UINT16_T__G__TxHeadIndex;
    uint16_t__tail = UINT16_T__G__TxTailIndex;

    if (uint16_t__tail == uint16_t__head)
    {
        return;
    }

    /* [EN] One contiguous run from the tail: up to the head or the ring
       end, whichever comes first, capped by the chunk buffer.
       [FA] یک اجرای پیوسته از tail: تا head یا انتهای حلقه، هرکدام
       زودتر برسد، با سقف بافر قطعه. */
    uint16_t__contiguous = (uint16_t)(BSP_UART_TX_RING_SIZE - uint16_t__tail);
    if (uint16_t__contiguous >
        (uint16_t)((uint16_t__head + BSP_UART_TX_RING_SIZE - uint16_t__tail) %
                   BSP_UART_TX_RING_SIZE))
    {
        uint16_t__contiguous =
            (uint16_t)((uint16_t__head + BSP_UART_TX_RING_SIZE - uint16_t__tail) %
                       BSP_UART_TX_RING_SIZE);
    }
    if (uint16_t__contiguous > BSP_UART_TX_CHUNK_SIZE)
    {
        uint16_t__contiguous = BSP_UART_TX_CHUNK_SIZE;
    }

    uint16_t__chunkLength = uint16_t__contiguous;

    for (uint16_t uint16_t__i = 0u;
         uint16_t__i < uint16_t__chunkLength;
         uint16_t__i++)
    {
        UINT8_T__G__TxDmaChunk[uint16_t__i] =
            UINT8_T__G__TxRing[(uint16_t)((uint16_t__tail + uint16_t__i) %
                                          BSP_UART_TX_RING_SIZE)];
    }

    /* [EN] Commit the dequeue and the in-flight guard BEFORE the engine is
       started, and roll both back if HAL refuses. Commit-first makes the
       ordering race-free: a DMA-complete interrupt for this chunk can only
       fire after the channel is enabled inside the HAL call, i.e. always
       after both stores, so the callback never re-reads a stale tail and
       can never re-send the same chunk. On refusal the bytes stay owned by
       the ring (tail rolls back) and the next pump retries them.
       [FA] خارج‌کردن از صف و گاردِ در-پرواز قبل از روشن‌کردن موتور ثبت
       می‌شوند و اگر HAL رد کند هر دو برمی‌گردند. «اول ثبت» ترتیب را
       بدون مسابقه می‌کند: وقفهٔ کامل‌شدن DMA برای همین قطعه فقط بعد از
       فعال‌شدن کانال داخل فراخوانی HAL می‌تواند شلیک شود، یعنی همیشه
       بعد از هر دو ذخیره؛ در نتیجه کال‌بک هرگز tail کهنه نمی‌خواند و
       همان قطعه را دوباره نمی‌فرستد. در صورت رد، بایت‌ها مالک حلقه
       می‌مانند (tail برمی‌گردد) و پمپ بعدی دوباره می‌کوشد. */
    uint16_t__nextTail = (uint16_t)((uint16_t__tail + uint16_t__chunkLength) %
                                    BSP_UART_TX_RING_SIZE);
    UINT16_T__G__TxTailIndex = uint16_t__nextTail;
    BOOL__G__TxDmaActive = true;

    if (HAL_UART_Transmit_DMA(UART_HANDLETYPEDEF__G__EspLink,
                              UINT8_T__G__TxDmaChunk,
                              uint16_t__chunkLength) != HAL_OK)
    {
        UINT16_T__G__TxTailIndex = uint16_t__tail;
        BOOL__G__TxDmaActive = false;
    }
}

/* ==================== BspUart_Write ==================== */
/**
 * @brief  [EN] Queue a byte buffer for transmission (non-blocking): the
 *              bytes land in the software ring and the DMA pump drains it.
 *              The enqueue is whole-frame atomic: false means the frame
 *              did NOT fit and nothing of it was queued (never a partial
 *              frame on the wire); the link self-recovers on the next
 *              write.
 *         [FA] یک بافر بایت را برای ارسال صف می‌کند (بدون بلوکه‌شدن):
 *              بایت‌ها در حلقهٔ نرم‌افزاری می‌نشینند و پمپ DMA خالی‌شان
 *              می‌کند. صف‌کردن به‌صورت کل-فریم اتمیک است: false یعنی
 *              فریم جا نشد و هیچ بایتی از آن صف نشد (هرگز فریم ناقص
 *              روی سیم نمی‌رود)؛ لینک با نوشتن بعدی خودش را بازیابی
 *              می‌کند.
 * @param  uint8_t__data [EN] Data buffer / بافر داده
 * @param  uint16_t__length [EN] Number of bytes / تعداد بایت‌ها
 * @return bool [EN] true when the bytes were queued / بایت‌ها صف شدند
 */
bool func__BspUart_Write(const uint8_t *uint8_t__data, uint16_t uint16_t__length)
{
    uint16_t uint16_t__freeSlots;

    if ((BOOL__G__Initialized == false) ||
        (UART_HANDLETYPEDEF__G__EspLink == NULL) ||
        ((uint8_t__data == NULL) && (uint16_t__length != 0u)) ||
        (uint16_t__length >= BSP_UART_TX_RING_SIZE))
    {
        return false;
    }

    /* [EN] Whole-frame atomicity: check the free space up front and refuse
       the complete frame when it does not fit. A byte-by-byte fill could
       hit a full ring in the middle and put a HALF frame on the wire,
       which corrupts the next frame too before the parser resyncs. With
       the up-front check the ring only ever holds whole frames, so a
       refused frame is cleanly dropped. The tail can only move FORWARD
       while we copy (the DMA pump dequeues), which only grows the free
       space, so the check stays valid for the whole copy.
       [FA] اتمی‌بودن کل فریم: فضای خالی از اول چک می‌شود و اگر فریم
       جا نشود کاملش رد می‌شود. پرکردن بایت‌به‌بایت ممکن وسط کار به
       حلقهٔ پر می‌خورد و «نصف فریم» روی سیم می‌گذارد که فریم بعدی را
       هم تا resync پارسر خراب می‌کند. با این چک، حلقه فقط فریم کامل
       نگه می‌دارد و فریم ردشده تمیز رها می‌شود. tail فقط می‌تواند
       جلو برود (پمپ DMA از صف درمی‌آورد) که فقط فضای خالی را بیشتر
       می‌کند، پس چک برای کل کپی معتبر می‌ماند. */
    uint16_t__freeSlots =
        (uint16_t)((UINT16_T__G__TxTailIndex + BSP_UART_TX_RING_SIZE -
                    UINT16_T__G__TxHeadIndex - 1u) % BSP_UART_TX_RING_SIZE);

    if (uint16_t__freeSlots < uint16_t__length)
    {
        return false;
    }

    for (uint16_t uint16_t__i = 0u; uint16_t__i < uint16_t__length; uint16_t__i++)
    {
        UINT8_T__G__TxRing[UINT16_T__G__TxHeadIndex] = uint8_t__data[uint16_t__i];
        UINT16_T__G__TxHeadIndex =
            (uint16_t)((UINT16_T__G__TxHeadIndex + 1u) % BSP_UART_TX_RING_SIZE);
    }

    func__BspUart_PumpTx();
    return true;
}

/* ==================== BspUart_ReadByte ==================== */
/**
 * @brief  [EN] Pop one received byte from the circular DMA ring without
 *              blocking. The write index is derived live from the DMA
 *              counter, so no interrupt and no CPU cost is involved in
 *              reception. The UART line-error flags are deliberately left
 *              alone: on F1 every clear macro reads DR, which could steal
 *              a byte that belongs to the DMA engine.
 *         [FA] یک بایت دریافتی را بدون بلوکه‌کردن از حلقهٔ DMA حلقوی
 *              برمی‌دارد. اندیس نوشتن به‌صورت زنده از شمارندهٔ DMA ساخته
 *              می‌شود، پس هیچ وقفه و هزینهٔ CPU ای در دریافت نیست.
 *              پرچم‌های خطای خط UART عمداً دست نمی‌خورند: در F1 همهٔ
 *              ماکروهای پاک‌کردن، DR را می‌خوانند و می‌توانند بایتی را
 *              که متعلق به موتور DMA است بدزدند.
 * @param  uint8_t__byte [EN] Output byte pointer / اشاره‌گر بایت خروجی
 * @return bool [EN] true if one byte was read / اگر بایت خوانده شد true
 */
bool func__BspUart_ReadByte(uint8_t *uint8_t__byte)
{
    uint16_t uint16_t__head;
    uint16_t uint16_t__tail;

    if ((BOOL__G__Initialized == false) ||
        (UART_HANDLETYPEDEF__G__EspLink == NULL) ||
        (uint8_t__byte == NULL))
    {
        return false;
    }

    /* [EN] Head = how far the DMA engine has filled the ring (counter
       counts DOWN from the buffer size).
       [FA] head = موتور DMA حلقه را تا کجا پر کرده (شمارنده از اندازهٔ
       بافر به پایین می‌شمارد). */
    /* [EN] The modulo defends a rare F1 circular-mode artifact: around the
       automatic reload the counter can transiently read 0, which would
       yield head == SIZE and deliver one stale byte. Wrapping into
       0..SIZE-1 maps that instant to "empty", which is exactly the true
       state at the wrap point.
       [FA] باقیماندهٔ تقسیم یک آرتیفکت نادر مود حلقوی F1 را خنثی می‌کند:
       حول بارگذاری مجدد خودکار، شمارنده ممکن است لحظه‌ای 0 خوانده شود
       که head == SIZE می‌دهد و یک بایت کهنه تحویل می‌دهد. برگرداندن به
       بازهٔ 0..SIZE-1 همان لحظه را «خالی» می‌کند که دقیقاً وضعیت واقعی
       در نقطهٔ wrap است. */
    uint16_t__head =
        (uint16_t)((BSP_UART_RX_RING_SIZE -
                    __HAL_DMA_GET_COUNTER(&DMA_HANDLETYPEDEF__G__RxDma)) %
                   BSP_UART_RX_RING_SIZE);

    uint16_t__tail = UINT16_T__G__RxTailIndex;

    if (uint16_t__tail == uint16_t__head)
    {
        /* [EN] Nothing new. Line-error flags are deliberately NOT cleared:
           on F1 the clear macro reads DR, which could steal a byte from
           the DMA engine; with DMA reception the flags are harmless.
           [FA] چیز جدیدی نیست. پرچم‌های خطای خط عمداً پاک نمی‌شوند:
           ماکروی پاک‌کردن در F1 رجیستر DR را می‌خواند و می‌تواند یک
           بایت از موتور DMA بدزدد؛ با دریافت DMA این پرچم‌ها بی‌ضررند. */
        return false;
    }

    *uint8_t__byte = UINT8_T__G__RxDmaBuffer[uint16_t__tail];

    uint16_t__tail = (uint16_t)((uint16_t__tail + 1u) % BSP_UART_RX_RING_SIZE);
    UINT16_T__G__RxTailIndex = uint16_t__tail;

    return true;
}

/* ==================== DMA TX complete / کامل‌شدن TX ==================== */

/**
 * @brief  [EN] DMA1_Channel4 interrupt entry: forward to the HAL DMA
 *              handler. In this HAL the NORMAL-mode completion does NOT
 *              call the UART callback from here - it clears DMAT and arms
 *              the UART transmit-complete interrupt, so the actual
 *              "frame finished" callback arrives from USART1_IRQHandler
 *              below. No RTOS API is used here.
 *         [FA] ورودی وقفهٔ DMA1_Channel4: به هندلر DMA ی HAL فرستاده
 *              می‌شود. در این HAL، کامل‌شدن مود NORMAL callback ی UART را
 *              از همین‌جا صدا نمی‌زند - DMAT را پاک و وقفهٔ
 *              transmit-complete ی UART را مسلح می‌کند، پس callback واقعی
 *              «فریم تمام شد» از USART1_IRQHandler پایین می‌آید. هیچ API
 *              سیستمعاملی اینجا استفاده نمی‌شود.
 */
void DMA1_Channel4_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&DMA_HANDLETYPEDEF__G__TxDma);
}

/**
 * @brief  [EN] USART1 interrupt entry: the second and final stage of a
 *              frame transmission. When the last bit has left the shift
 *              register, HAL's UART_IRQHandler finalizes the transfer and
 *              calls HAL_UART_TxCpltCallback below, which pumps the next
 *              chunk. With the HAL error interrupt disarmed at init, this
 *              vector only ever sees the transmit-complete event - the
 *              reception itself stays fully silent on DMA.
 *         [FA] ورودی وقفهٔ USART1: مرحلهٔ دوم و نهاییِ ارسال یک فریم.
 *              وقتی آخرین بیت از شیفت‌رجیستر خارج شد، UART_IRQHandler ی
 *              HAL ارسال را نهایی می‌کند و HAL_UART_TxCpltCallback زیر را
 *              صدا می‌زند که قطعهٔ بعدی را پمپ می‌کند. با غیرمسلح‌بودن
 *              وقفهٔ خطای HAL در init، این بردار فقط transmit-complete
 *              را می‌بیند - خود دریافت کاملاً بی‌صدا روی DMA می‌ماند.
 */
void USART1_IRQHandler(void)
{
    if (UART_HANDLETYPEDEF__G__EspLink != NULL)
    {
        HAL_UART_IRQHandler(UART_HANDLETYPEDEF__G__EspLink);
    }
}

/**
 * @brief  [EN] One frame chunk fully shifted out (called by HAL from the
 *              UART transmit-complete interrupt): release the in-flight
 *              guard and immediately pump the next chunk, so back-to-back
 *              frames (telemetry + replies) drain without any task
 *              involvement.
 *         [FA] یک قطعهٔ فریم کاملاً روی سیم رفت (HAL از وقفهٔ
 *              transmit-complete ی UART صدا می‌زند): گارد در-پرواز آزاد و
 *              بلافاصله قطعهٔ بعدی پمپ می‌شود تا فریم‌های پشت‌سرهم
 *              (تله‌متری + پاسخ‌ها) بدون دخالت تسک خالی شوند.
 * @param  uart_handle_t__huart [EN] Handle of the UART that completed /
 *                                  هندل UART ای که کامل شد
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart_handle_t__huart)
{
    if (uart_handle_t__huart == UART_HANDLETYPEDEF__G__EspLink)
    {
        BOOL__G__TxDmaActive = false;
        func__BspUart_PumpTx();
    }
}
