/*
 * motor_uart.c -- AK40-10 servo mode over UART.
 *
 * Protocol: CubeMars AK Series Actuator Manual section 5.2.2
 * Frame:    [0x02][len][cmd][payload..][crc_hi][crc_lo][0x03]
 *           where len = 1 + sizeof(payload), and crc16 is over (cmd + payload)
 *
 * Each motor gets its own UART peripheral (point-to-point). The mapping
 * lives in MOTOR_UART_TABLE below -- that is the only place to edit when
 * rewiring.
 */

#include "motor.h"
#include "crc.h"
#include "stm32f4xx_hal.h"

/* ============================================================================
 *  WIRING -- EDIT HERE TO REMAP MOTORS TO UARTS
 * ============================================================================
 *
 *   PC ── USB ── ST-Link ── USART2 (PA2/PA3) ── STM32  [user terminal]
 *                                                │
 *                                                ├── USART1 ──> Motor 1
 *                                                ├── USART3 ──> Motor 2
 *                                                └── USART6 ──> Motor 3
 *
 *   STM32F446RE Nucleo pin assignments:
 *     Motor 1   USART1   TX = PA9   (Arduino D8)
 *                        RX = PA10  (Arduino D2)
 *     Motor 2   USART3   TX = PB10  (Arduino D6)
 *                        RX = PC11  (Morpho CN7-2)
 *     Motor 3   USART6   TX = PC6   (Morpho CN10-4)
 *                        RX = PC7   (Morpho CN10-19)
 *
 *   All motor UARTs run at 921600 baud, 8-N-1 (CubeMars default).
 *   Verify pin assignments against the F446RE Nucleo board pinout in CubeMX.
 * ============================================================================ */

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart6;

static UART_HandleTypeDef * const MOTOR_UART_TABLE[MOTOR_COUNT] = {
    &huart1,   /* Motor 1 */
    &huart3,   /* Motor 2 */
    &huart6,   /* Motor 3 */
};

/* ============================================================================
 *  Servo-mode command bytes (AK manual table 4.2 / section 5.2.2)
 * ============================================================================ */

#define COMM_SET_DUTY            5
#define COMM_SET_CURRENT         6
#define COMM_SET_CURRENT_BRAKE   7
#define COMM_SET_RPM             8
#define COMM_SET_POS             9
#define COMM_SET_POS_ORIGIN     95

/* ============================================================================
 *  Frame builder
 * ============================================================================ */

static void send_frame(uint8_t id, uint8_t cmd,
                       const uint8_t *payload, uint8_t payload_len)
{
    if (id < 1 || id > MOTOR_COUNT) return;

    uint8_t buf[16];      /* worst-case 14 bytes; round up */
    uint8_t i = 0;
    buf[i++] = 0x02;                          /* frame header              */
    buf[i++] = (uint8_t)(1 + payload_len);    /* length = cmd + payload    */
    buf[i++] = cmd;
    for (uint8_t k = 0; k < payload_len; k++) buf[i++] = payload[k];

    /* CRC is computed over (cmd + payload), not the framing bytes. */
    uint16_t c = crc16(&buf[2], (uint16_t)(1 + payload_len));
    buf[i++] = (uint8_t)(c >> 8);
    buf[i++] = (uint8_t)(c & 0xFF);
    buf[i++] = 0x03;                          /* frame end                 */

    HAL_UART_Transmit(MOTOR_UART_TABLE[id - 1], buf, i, 100);
}

static void put_int32_be(uint8_t *buf, int32_t v)
{
    buf[0] = (uint8_t)(v >> 24);
    buf[1] = (uint8_t)(v >> 16);
    buf[2] = (uint8_t)(v >>  8);
    buf[3] = (uint8_t)(v);
}

/* ============================================================================
 *  motor.h API
 * ============================================================================ */

void motor_init(void)
{
    /* Servo mode is "always on" once the motor is configured for it via
     * R-link. No enter-mode handshake needed. When we move to MIT mode,
     * this is where we'll send the FF...FC enter-motor frames. */
}

void motor_set_position(uint8_t id, float deg)
{
    /* Manual: int32, scale 1e6, range ±36 000° */
    uint8_t p[4];
    put_int32_be(p, (int32_t)(deg * 1000000.0f));
    send_frame(id, COMM_SET_POS, p, 4);
}

void motor_set_velocity(uint8_t id, float erpm)
{
    /* Manual: int32, raw ERPM, range ±100 000 */
    uint8_t p[4];
    put_int32_be(p, (int32_t)erpm);
    send_frame(id, COMM_SET_RPM, p, 4);
}

void motor_set_duty(uint8_t id, float duty)
{
    /* Manual: int32, scale 1e5, default range ±0.95 */
    uint8_t p[4];
    put_int32_be(p, (int32_t)(duty * 100000.0f));
    send_frame(id, COMM_SET_DUTY, p, 4);
}

void motor_set_current(uint8_t id, float amps)
{
    /* Manual: int32, scale 1e3, range ±60 A */
    uint8_t p[4];
    put_int32_be(p, (int32_t)(amps * 1000.0f));
    send_frame(id, COMM_SET_CURRENT, p, 4);
}

void motor_set_brake(uint8_t id, float amps)
{
    uint8_t p[4];
    put_int32_be(p, (int32_t)(amps * 1000.0f));
    send_frame(id, COMM_SET_CURRENT_BRAKE, p, 4);
}

void motor_zero(uint8_t id)
{
    /* Manual section 5.1.6: 1 = permanent zero (auto-saved to flash). */
    uint8_t p[1] = { 1 };
    send_frame(id, COMM_SET_POS_ORIGIN, p, 1);
}

void motor_stop(uint8_t id)
{
    motor_set_duty(id, 0.0f);
}
