/**
 * @file    bsp_uart.c
 * @brief   [EN] STM32F103C8T6 USART1 port for the ESP-Link connector:
 *              921600 8-N-1, zero-CPU DMA both ways.
 *              - RX: DMA1_Channel5 CIRCULAR into a 256-byte ring - zero
 *                CPU per received byte, no USART interrupt at all; the
 *                consumer derives the write index from the DMA counter.
 *              - TX: DMA1_Channel4 from a 1024-byte software ring; writes
 *                never block, the pump moves one contiguous chunk per DMA
 *                transfer. In this HAL NORMAL-mode DMA completion does NOT
 *                call the UART callback directly: it clears DMAT and arms
 *                the UART transmit-complete interrupt (TCIE), so the next
 *                chunk's callback fires from USART1_IRQHandler after the
 *                last bit leaves the shifter. Net: a couple of lightweight
 *                IRQs per frame (~20-40/s at the 100 ms telemetry rate).
 *          [FA] پورت USART1 برد برای ‎ESP-Link: 921600‎ و ‎8-N-1‎ با DMA
 *              دوطرفه و بدون بار CPU (مقداردهی ۱۱۵۲۰۰ تولیدی Cube در
 *              زمان اجرا به ۹۲۱۶۰۰ تغییر می‌کند).
 *              - RX روی DMA1_Channel5 حلقوی در بافر ۲۵۶ بایتی - صفر CPU به
 *                ازای هر بایت و بدون وقفهٔ USART؛ مصرف‌کننده اندیس نوشتن را
 *                از شمارندهٔ DMA می‌گیرد.
 *              - TX روی DMA1_Channel4 از حلقهٔ نرم‌افزاری ۱۰۲۴ بایتی؛
 *                نوشتن بدون بلوکه است و پمپ برای هر انتقال یک قطعهٔ پیوسته
 *                می‌کشد. در این HAL کامل‌شدن DMA مود NORMAL مستقیم callback
 *                ی UART را صدا نمی‌زند: DMAT پاک و TCIE مسلح می‌شود و
 *                callback قطعهٔ بعد از USART1_IRQHandler می‌آید. خالص: چند
 *                وقفهٔ سبک به‌ازای هر فریم (~۲۰-۴۰ در ثانیه با نرخ ۱۰۰ms).
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
 *      روی ‎APB2 (72MHz/16/921600 = 4.88‎، خطای +۰.۱۶٪) و نرخی که UART هر
 *      دو ماژول ESP8266 و ESP32 به‌طور مطمئن اجرا می‌کنند. */
#define BSP_UART_BAUD_RATE        921600u

/* [EN] Circular RX DMA ring (power-of-two size keeps the index wrap cheap;
 *      not a correctness requirement).
 * [FA] حلقهٔ DMA حلقوی RX (اندازهٔ توان دو جابه‌جایی اندیس را ارزان
 *      می‌کند؛ شرط درستی نیست). */
#define BSP_UART_RX_RING_SIZE     256u

/* [EN] Software TX ring and the max bytes moved per DMA transfer. 1024
 *      since the v1.16 audit: PARAMS_BULK grew to a 392 B frame (77 params)
 *      and the 256 B ring silently refused every bulk reply, so a fresh
 *      panel never learned the live values. Still a power of two.
 *      v1.17: 83 params = 422 B frame - still fits.
 * [FA] حلقهٔ نرم‌افزاری TX و حداکثر بایت هر انتقال DMA. از ممیزی ۱.۱۶ مقدار
 *      ۱۰۲۴ است: فریم بالک ۳۹۲ بایت شد و حلقهٔ ۲۵۶ هر پاسخ بالک را بی‌صدا رد
 *      می‌کرد پس پنل تازه مقادیر زنده را نمی‌گرفت. همچنان توان دو.
 *      نسخه ۱.۱۷: ۸۳ پارامتر = فریم ۴۲۲ بایتی - هنوز جا می‌شود. */
#define BSP_UART_TX_RING_SIZE     1024u
#define BSP_UART_TX_CHUNK_SIZE    128u

/* [EN] Lowest NVIC priority for both interrupt vectors of this port (the
 *      TX-DMA complete and the UART transmit-complete that this HAL arms
 *      afterwards); neither handler calls any RTOS API.
 * [FA] کمترین اولویت NVIC برای هر دو بردار وقفهٔ این پورت (کامل‌شدن DMA ی
 *      TX و ‎transmit-complete‎ ی UART که این HAL بعدش مسلح می‌کند)؛ هیچ‌کدام
 *      از هندلرها API سیستمعاملی صدا نمی‌زنند. */
#define BSP_UART_IRQ_PRIORITY 15u

static UART_HandleTypeDef * volatile UART_HANDLETYPEDEF__G__EspLink = NULL;
static bool BOOL__G__Initialized = false;

/* [EN] Private DMA handles for USART1 (not in the Cube project: the board
 *      port wires them at runtime). TX = DMA1_Channel4, RX = DMA1_Channel5
 *      - fixed F103 mappings, no conflict with the ADC on Channel1.
 * [FA] هندل‌های خصوصی DMA برای USART1 (در پروژهٔ Cube نیستند: پورت برد
 *      آنها را در زمان اجرا سیم‌پیچی می‌کند). ‎TX = DMA1_Channel4‎ و
 *      ‎RX = DMA1_Channel5‎ - نگاشت ثابت F103، بدون تداخل با ADC روی
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
 *              Repeated calls are ignored after the first setup.
 *         [FA] هندل USART1 برد را انتخاب، آن را به BSP_UART_BAUD_RATE
 *              بازتنظیم می‌کند (Cube روی ۱۱۵۲۰۰ می‌ماند؛ کد تولیدشده
 *              عمداً دست نمی‌خورد)، هر دو کانال DMA را سیم‌پیچی، دریافت
 *              DMA حلقوی را شروع و مسیر TX را ریست می‌کند. پس از اولین
 *              راه‌اندازی، فراخوانی‌های تکراری نادیده گرفته می‌شوند.
 */
void func__BspUart_Init(void)
{
    /* [EN] main.c initializes the board backend before the scheduler and
       EspLink_Init may request the same service from TaskComm. Keep the
       public Init API idempotent so that the second call cannot deinitialize
       a live UART/DMA path.
       [FA] main.c بک‌اند برد را پیش از زمان‌بند راه می‌اندازد و
       EspLink_Init ممکن است همان سرویس را از TaskComm بخواهد. API عمومی Init
       همانی است تا فراخوانی دوم مسیر زندهٔ ‎UART/DMA‎ را DeInit نکند. */
    if (BOOL__G__Initialized != false)
    {
        return;
    }

    UART_HANDLETYPEDEF__G__EspLink = &huart1;

    /* [EN] Reconfigure only the line speed; word length, stop bits and
       parity keep the Cube values (8-N-1).
       [FA] فقط سرعت خط بازتنظیم می‌شود؛ طول کلمه، بیت توقف و پاریتی
       مقادیر ‎Cube (8-N-1)‎ را نگه می‌دارند. */
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
       را صدا نمی‌زند: وقفهٔ ‎transmit-complete‎ ی UART (TCIE) را مسلح
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

    /* [EN] Disarm the error interrupt HAL armed for DMA RX: the first
       framing/noise/overrun flag (guaranteed real-world case: an ESP8266
       reboot prints boot garbage at 74880 baud) would run the HAL error
       path, which ABORTS the circular DMA and silently kills reception
       forever. Disarmed, the flags are harmless: DMAR keeps draining DR,
       a corrupted byte still lands in the ring and the parser drops bad
       frames via the XOR checksum.
       [FA] وقفهٔ خطای RX ی DMA غیرمسلح می‌شود: اولین پرچم خطا (مورد واقعی:
       ریبوت ESP8266 که garbage بوت با 74880 baud می‌ریزد) مسیر خطای HAL را
       می‌راند که DMA حلقوی را abort می‌کند و دریافت برای همیشه می‌میرد.
       غیرمسلح، پرچم‌ها بی‌ضررند: DMAR به تخلیهٔ DR ادامه می‌دهد و پارسر
       فریم خراب را با XOR رها می‌کند. */
    __HAL_UART_DISABLE_IT(UART_HANDLETYPEDEF__G__EspLink, UART_IT_ERR);

    BOOL__G__Initialized = true;
}

/* ==================== BspUart TX pump / پمپ TX ==================== */

/**
 * @brief  [EN] Move the next contiguous chunk of the TX ring into the DMA
 *              engine. Safe from task context and from the DMA-complete
 *              callback: the TxDmaActive guard means only one of them can
 *              ever start a transfer; tail+guard are committed BEFORE the
 *              engine starts (rolled back on refusal), so no interrupt can
 *              observe a half-committed dequeue. If HAL refuses, the data
 *              stays in the ring and the next pump retries.
 *         [FA] قطعهٔ پیوستهٔ بعدی حلقهٔ TX را داخل موتور DMA می‌برد؛ از
 *              زمینهٔ تسک و از کال‌بک DMA امن است (گارد TxDmaActive)؛ اگر
 *              HAL رد کند، داده در حلقه می‌ماند و پمپ بعدی دوباره می‌کوشد.
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

    /* [EN] Commit-first ordering makes the dequeue race-free: a
       DMA-complete interrupt for this chunk can only fire after the
       channel is enabled inside the HAL call, i.e. always after both
       stores; the callback never re-reads a stale tail and never re-sends
       the same chunk. On refusal the bytes stay owned by the ring.
       [FA] «اول ثبت» ترتیب را بدون مسابقه می‌کند: وقفهٔ کامل‌شدن DMA فقط
       بعد از فعال‌شدن کانال داخل HAL می‌تواند بزند، یعنی همیشه بعد از هر
       دو ذخیره؛ در صورت رد، بایت‌ها مالک حلقه می‌مانند. */
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
 *              Whole-frame atomic: false means the frame did NOT fit and
 *              nothing of it was queued (never a partial frame on the
 *              wire); the link self-recovers on the next write.
 *         [FA] یک بافر بایت را برای ارسال صف می‌کند (بدون بلوکه‌شدن):
 *              صف‌کردن کل-فریم اتمیک است: false یعنی فریم جا نشد و هیچ
 *              بایتی از آن صف نشد (هرگز فریم ناقص روی سیم نمی‌رود)؛ لینک
 *              با نوشتن بعدی خودش را بازیابی می‌کند.
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

    /* [EN] Whole-frame atomicity: refuse the complete frame up front when
       it does not fit - a byte-by-byte fill could put a HALF frame on the
       wire and corrupt the next frame too. The tail can only move FORWARD
       while we copy (the DMA pump dequeues), which only grows the free
       space, so the check stays valid for the whole copy.
       [FA] اتمی‌بودن کل فریم: اگر جا نشود کاملش رد می‌شود - پرکردن
       بایت‌به‌بایت ممکن «نصف فریم» روی سیم بگذارد که فریم بعدی را هم خراب
       می‌کند. tail فقط جلو می‌رود (پمپ DMA) که فقط فضا را بیشتر می‌کند، پس
       چک برای کل کپی معتبر می‌ماند. */
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
 *              counter - no interrupt and no CPU cost in reception. The
 *              UART line-error flags are deliberately left alone: on F1
 *              every clear macro reads DR, which could steal a byte that
 *              belongs to the DMA engine.
 *         [FA] یک بایت دریافتی را بدون بلوکه‌کردن از حلقهٔ DMA حلقوی
 *              برمی‌دارد؛ اندیس نوشتن زنده از شمارندهٔ DMA ساخته می‌شود.
 *              پرچم‌های خطای خط عمداً دست نمی‌خورند: در F1 ماکروهای
 *              پاک‌کردن DR را می‌خوانند و بایتِ DMA را می‌دزدند.
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
       که ‎head == SIZE‎ می‌دهد و یک بایت کهنه تحویل می‌دهد. برگرداندن به
       بازهٔ ‎0..SIZE-1‎ همان لحظه را «خالی» می‌کند که دقیقاً وضعیت واقعی
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
 *              handler. NORMAL-mode completion does NOT call the UART
 *              callback from here - it clears DMAT and arms the UART
 *              transmit-complete interrupt, so the actual "frame
 *              finished" callback arrives from USART1_IRQHandler. No RTOS
 *              API is used here.
 *         [FA] ورودی وقفهٔ DMA1_Channel4: به هندلر DMA ی HAL فرستاده
 *              می‌شود؛ callback واقعی «فریم تمام شد» از
 *              USART1_IRQHandler می‌آید. هیچ API سیستمعاملی اینجا نیست.
 */
void DMA1_Channel4_IRQHandler(void);
void DMA1_Channel4_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&DMA_HANDLETYPEDEF__G__TxDma);
}

/**
 * @brief  [EN] USART1 interrupt entry: the second and final stage of a
 *              frame transmission. When the last bit has left the shift
 *              register, HAL finalizes the transfer and calls
 *              HAL_UART_TxCpltCallback below, which pumps the next chunk.
 *              With the HAL error interrupt disarmed at init, this vector
 *              only ever sees the transmit-complete event - reception
 *              stays fully silent on DMA.
 *         [FA] ورودی وقفهٔ USART1: مرحلهٔ دوم و نهایی ارسال فریم؛ وقتی
 *              آخرین بیت خارج شد HAL ارسال را نهایی و
 *              HAL_UART_TxCpltCallback را صدا می‌زند که قطعهٔ بعدی را پمپ
 *              می‌کند. با غیرمسلح‌بودن وقفهٔ خطا، این بردار فقط
 *              ‎transmit-complete‎ را می‌بیند و دریافت کاملاً روی DMA می‌ماند.
 */
void USART1_IRQHandler(void);
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
 *              ‎transmit-complete‎ ی UART صدا می‌زند): گارد در-پرواز آزاد و
 *              بلافاصله قطعهٔ بعدی پمپ می‌شود تا فریم‌های پشت‌سرهم
 *              (تله‌متری + پاسخ‌ها) بدون دخالت تسک خالی شوند.
 * @param  UART_HandleTypeDef__huart [EN] Handle of the UART that completed /
 *                                  هندل UART ای که کامل شد
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *UART_HandleTypeDef__huart)
{
    if (UART_HandleTypeDef__huart == UART_HANDLETYPEDEF__G__EspLink)
    {
        BOOL__G__TxDmaActive = false;
        func__BspUart_PumpTx();
    }
}
