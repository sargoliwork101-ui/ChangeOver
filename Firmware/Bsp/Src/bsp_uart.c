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
 * @note    [EN] Reception is interrupt driven (user order 2026-09-22: the
 *              ESP command protocol cannot run on 100 ms polling - a byte
 *              lasts 87 us at 115200). One-byte HAL reception is chained in
 *              the RX-complete callback into a 128-byte single-producer /
 *              single-consumer ring; the consumer side is the EspLink task.
 *              The Cube project does not enable the USART1 interrupt, so
 *              this file owns USART1_IRQHandler and enables the NVIC line
 *              itself at init.
 *         [FA] دریافت با وقفه است (دستور کاربر ۲۰۲۶-۰۹-۲۲: پروتکل فرمان
 *              ESP با poll هر ۱۰۰ms کار نمی‌کند - هر بایت در 115200 فقط
 *              ۸۷ میکروثانیه است). دریافت تک‌بایتی HAL در کال‌بک کامل‌شدن
 *              دوباره زده می‌شود و بایت‌ها در حلقهٔ ۱۲۸ بایتی تک-تولیدکننده/
 *              تک-مصرف‌کننده می‌نشینند؛ مصرف‌کننده تسک EspLink است. پروژهٔ
 *              Cube وقفهٔ USART1 را فعال نکرده، پس این فایل مالک
 *              USART1_IRQHandler است و خط NVIC را خودش در init فعال می‌کند.
 */

#include "bsp_uart.h"
#include "main.h"

#include <stddef.h>

#define BSP_UART_TX_TIMEOUT_MS 100u

/* [EN] RX ring geometry and interrupt priority (lowest, no RTOS calls in
   the ISR). / [FA] هندسهٔ حلقهٔ RX و اولویت وقفه (کمترین؛ ISR هیچ فراخوانی
   RTOS ندارد). */
#define BSP_UART_RX_RING_SIZE    128u
#define BSP_UART_RX_IRQ_PRIORITY 15u

static UART_HandleTypeDef * volatile UART_HANDLETYPEDEF__G__EspLink = NULL;
static bool BOOL__G__Initialized = false;

/* [EN] RX ring: head is written only by the ISR, tail only by the reader
   task - lock-free single-producer/single-consumer on Cortex-M3.
   [FA] حلقهٔ RX: head فقط از ISR و tail فقط از تسک خوانده‌شده/نوشته
   می‌شود - بدون قفل، تک-تولیدکننده/تک-مصرف‌کننده روی Cortex-M3. */
static volatile uint8_t UINT8_T__G__RxRing[BSP_UART_RX_RING_SIZE];
static volatile uint16_t UINT16_T__G__RxRingHead;
static volatile uint16_t UINT16_T__G__RxRingTail;

/* [EN] Single-byte HAL reception target. / [FA] مقصد تک‌بایتی دریافت HAL. */
static uint8_t UINT8_T__G__RxSlot;

/* ==================== BspUart_Init ==================== */
/**
 * @brief  [EN] Select the initialized USART1 board handle, clear the RX
 *              ring, enable the USART1 interrupt and arm the first
 *              one-byte reception.
 *         [FA] هندل USART1 برد را انتخاب، حلقهٔ RX را پاک، وقفهٔ USART1 را
 *              فعال و اولین دریافت تک‌بایتی را مسلح می‌کند.
 */
void func__BspUart_Init(void)
{
    UART_HANDLETYPEDEF__G__EspLink = &huart1;
    BOOL__G__Initialized = true;
    UINT16_T__G__RxRingHead = 0u;
    UINT16_T__G__RxRingTail = 0u;
    UINT8_T__G__RxSlot = 0u;

    HAL_NVIC_SetPriority(USART1_IRQn, BSP_UART_RX_IRQ_PRIORITY, 0u);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    (void)HAL_UART_Receive_IT(UART_HANDLETYPEDEF__G__EspLink, &UINT8_T__G__RxSlot, 1u);
}

/* ==================== BspUart_Write ==================== */
/**
 * @brief  [EN] Transmit a complete byte buffer through USART1.
 *         [FA] یک بافر کامل بایت را از USART1 ارسال می‌کند.
 * @param  uint8_t__data [EN] Data buffer / بافر داده
 * @param  uint16_t__length [EN] Number of bytes / تعداد بایت‌ها
 * @return bool [EN] true when transmission succeeds / اگر ارسال موفق باشد true
 */
bool func__BspUart_Write(const uint8_t *uint8_t__data, uint16_t uint16_t__length)
{
    if ((BOOL__G__Initialized == false) ||
        (UART_HANDLETYPEDEF__G__EspLink == NULL) ||
        ((uint8_t__data == NULL) && (uint16_t__length != 0u)))
    {
        return false;
    }

    if (uint16_t__length == 0u)
    {
        return true;
    }

    return (HAL_UART_Transmit(UART_HANDLETYPEDEF__G__EspLink,
                              (uint8_t *)uint8_t__data,
                              uint16_t__length,
                              BSP_UART_TX_TIMEOUT_MS) == HAL_OK);
}

/* ==================== BspUart_ReadByte ==================== */
/**
 * @brief  [EN] Pop one received byte from the RX ring without blocking.
 *              When the ring is empty and the HAL reception chain has come
 *              to rest (for example after an error), arm it again so the
 *              link self-heals.
 *         [FA] یک بایت دریافتی را بدون بلوکه‌کردن از حلقهٔ RX برمی‌دارد؛
 *              اگر حلقه خالی و زنجیرهٔ دریافت HAL متوقف مانده باشد (مثلاً
 *              پس از خطا) دوباره مسلحش می‌کند تا لینک خودش ترمیم شود.
 * @param  uint8_t__byte [EN] Output byte pointer / اشاره‌گر بایت خروجی
 * @return bool [EN] true if one byte was read / اگر بایت خوانده شد true
 */
bool func__BspUart_ReadByte(uint8_t *uint8_t__byte)
{
    uint16_t uint16_t__tail;
    uint16_t uint16_t__next;

    if ((BOOL__G__Initialized == false) ||
        (UART_HANDLETYPEDEF__G__EspLink == NULL) ||
        (uint8_t__byte == NULL))
    {
        return false;
    }

    uint16_t__tail = UINT16_T__G__RxRingTail;

    if (uint16_t__tail == UINT16_T__G__RxRingHead)
    {
        /* [EN] Empty ring: recover a stalled reception chain once.
           [FA] حلقهٔ خالی: زنجیرهٔ دریافت متوقف‌مانده را یک‌بار ترمیم کن. */
        if (UART_HANDLETYPEDEF__G__EspLink->RxState == HAL_UART_STATE_READY)
        {
            (void)HAL_UART_Receive_IT(UART_HANDLETYPEDEF__G__EspLink,
                                      &UINT8_T__G__RxSlot, 1u);
        }
        return false;
    }

    *uint8_t__byte = UINT8_T__G__RxRing[uint16_t__tail];

    uint16_t__next = uint16_t__tail + 1u;
    if (uint16_t__next >= BSP_UART_RX_RING_SIZE)
    {
        uint16_t__next = 0u;
    }
    UINT16_T__G__RxRingTail = uint16_t__next;

    return true;
}

/* ==================== USART1 IRQ / کال‌بک‌های RX ==================== */

/**
 * @brief  [EN] USART1 interrupt entry: forward to the HAL UART handler,
 *              which completes the one-byte reception and calls the
 *              callbacks below. No RTOS API is used here.
 *         [FA] ورودی وقفهٔ USART1: به هندلر HAL UART فرستاده می‌شود که
 *              دریافت تک‌بایتی را کامل و کال‌بک‌های زیر را صدا می‌زند.
 *              هیچ API سیستمعاملی اینجا استفاده نمی‌شود.
 */
void USART1_IRQHandler(void)
{
    if (UART_HANDLETYPEDEF__G__EspLink != NULL)
    {
        HAL_UART_IRQHandler(UART_HANDLETYPEDEF__G__EspLink);
    }
}

/**
 * @brief  [EN] One byte received: push it into the ring (drop it silently
 *              when the ring is full) and immediately arm the next
 *              one-byte reception.
 *         [FA] یک بایت دریافت شد: در حلقه نشانده می‌شود (در پربودن حلقه
 *              بی‌صدا رها می‌شود) و دریافت تک‌بایتی بعدی بلافاصله مسلح
 *              می‌شود.
 * @param  uart_handle_t__huart [EN] Handle of the UART that completed /
 *                                  هندل UART ای که کامل شد
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart_handle_t__huart)
{
    uint16_t uint16_t__head;
    uint16_t uint16_t__next;

    if (uart_handle_t__huart != UART_HANDLETYPEDEF__G__EspLink)
    {
        return;
    }

    uint16_t__head = UINT16_T__G__RxRingHead;
    uint16_t__next = uint16_t__head + 1u;
    if (uint16_t__next >= BSP_UART_RX_RING_SIZE)
    {
        uint16_t__next = 0u;
    }

    if (uint16_t__next != UINT16_T__G__RxRingTail)
    {
        UINT8_T__G__RxRing[uint16_t__head] = UINT8_T__G__RxSlot;
        UINT16_T__G__RxRingHead = uint16_t__next;
    }

    (void)HAL_UART_Receive_IT(uart_handle_t__huart, &UINT8_T__G__RxSlot, 1u);
}

/**
 * @brief  [EN] UART error (overrun, framing, noise): abort the stuck
 *              reception and re-arm the chain so the link keeps running.
 *         [FA] خطای UART (سرریز، فریم، نویز): دریافت گیرکرده ساقط و
 *              زنجیره دوباره مسلح می‌شود تا لینک کار خود را ادامه دهد.
 * @param  uart_handle_t__huart [EN] Handle of the UART in error /
 *                                  هندل UART خطادار
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart_handle_t__huart)
{
    if (uart_handle_t__huart == UART_HANDLETYPEDEF__G__EspLink)
    {
        (void)HAL_UART_AbortReceive(uart_handle_t__huart);
        (void)HAL_UART_Receive_IT(uart_handle_t__huart, &UINT8_T__G__RxSlot, 1u);
    }
}
