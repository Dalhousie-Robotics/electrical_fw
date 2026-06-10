# TOBY motor command parser — UART servo mode (3× AK40-10)

Plug Nucleo into laptop over USB, open any serial terminal at 115200 baud, type commands, watch motors move.

## Files

```
motor.h          abstract command API (parser depends on this)
motor_uart.c     UART + servo mode implementation        (CURRENT)
crc.h / crc.c    CRC-16 CCITT for servo frames
parser.h         parser interface
parser.c         tokenize + dispatch
README.md        this file
```

Total: ~250 lines of code. If your team's version is much larger, look at where the lines went.

## Wiring

```
PC ── USB ── ST-Link ── USART2 (PA2/PA3) ── STM32 ──┬── USART1 ── Motor 1
                                                    ├── USART3 ── Motor 2
                                                    └── USART6 ── Motor 3
```

USART2 is the user terminal (auto-routed to the ST-Link virtual COM). The three motor UARTs run at 921 600 baud (CubeMars default). Each motor has TX/RX/GND on its 3-pin serial connector.

Pin-mapping table (also at the top of `motor_uart.c` for editing):

| Motor | UART   | TX pin | RX pin | Header           |
|-------|--------|--------|--------|------------------|
| 1     | USART1 | PA9    | PA10   | Arduino D8 / D2  |
| 2     | USART3 | PB10   | PC11   | D6 / Morpho CN7-2 |
| 3     | USART6 | PC6    | PC7    | Morpho CN10      |

## Pre-flight: put each motor in servo mode

The AK40-10 ships in MIT mode by default. UART works only when the motor is in servo mode (manual §5.2.2). For each motor:

1. Plug the motor into your R-link.
2. Open the CubeMars upper computer software.
3. Connect, then go to **Mode Switch → Servo mode**.
4. **Save Parameters** (the icon for this is on the Home menu — important; servo mode reverts on power-cycle without it).
5. While you're there, give each motor a unique CAN ID (1, 2, 3) — useful later for MIT mode and harmless now.

Skipping this step → the motor ignores everything you send. No error, just silence.

## CubeMX setup

1. Pinout & Configuration:
   - **USART1** Asynchronous, 921600 baud — assign PA9/PA10
   - **USART2** Asynchronous, 115200 baud — already auto-assigned to PA2/PA3 for ST-Link
   - **USART3** Asynchronous, 921600 baud — assign PB10/PC11
   - **USART6** Asynchronous, 921600 baud — assign PC6/PC7
2. NVIC: enable **USART2 global interrupt** (only USART2 — motor UARTs are TX-only blocking transmits, no interrupts needed).
3. Generate code, drop the source files into `Core/Src` and `Core/Inc`.

## main.c integration

```c
/* USER CODE BEGIN Includes */
#include <string.h>
#include "parser.h"
#include "motor.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
static uint8_t uart2_rx_byte;
/* USER CODE END PV */

/* USER CODE BEGIN PFP */
static void uart2_print(const char *s)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 100);
}
/* USER CODE END PFP */
```

In `main()`, after all `MX_USARTx_UART_Init()` calls:

```c
/* USER CODE BEGIN 2 */
motor_init();
parser_init(uart2_print);
HAL_UART_Receive_IT(&huart2, &uart2_rx_byte, 1);
/* USER CODE END 2 */
```

Receive callback (paste under `USER CODE BEGIN 4`):

```c
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        parser_feed((char)uart2_rx_byte);
        HAL_UART_Receive_IT(&huart2, &uart2_rx_byte, 1);   /* re-arm */
    }
}
```

That's it for integration.

## Example bring-up session

Open a terminal at 115200 baud on the ST-Link COM port. Press Enter to see the banner, then:

```
help
duty 1 0.1            ← motor 1 should spin slowly
stop 1
duty 2 0.1            ← motor 2 spins
stop 2
duty 3 0.1            ← motor 3 spins
stop 3
pos 1 90              ← motor 1 rotates to 90°
pos 1 -90             ← back the other way
pos 1 0
zero 2                ← redefine motor 2's current pose as origin
vel 3 5000            ← motor 3 spins at ~5000 ERPM
stop 3
```

If a motor doesn't move: confirm it's in servo mode (R-link), confirm 24–48 V is applied, confirm TX/RX aren't swapped, confirm GND is shared between the STM32 and motor.

## Migration to CAN + MIT mode

When your transceivers arrive:

1. Add `motor_can.c` implementing the same `motor.h` API. Position/velocity become radians and rad/s respectively. `motor_init` sends the MIT enter-mode frame. `motor_stop` sends exit-mode.
2. Delete `motor_uart.c`, `crc.c`, `crc.h`. Remove the three motor UARTs from CubeMX, add CAN1.
3. The parser, the help text wording aside, doesn't change.
4. Update the help text to reflect units (radians, rad/s) and add an MIT-only `cmd <id> <p> <v> <kp> <kd> <t>` for raw impedance control.

## Rubric — what to check in your team's version

1. Is there a single point of edit for the motor-to-UART mapping? (Here: `MOTOR_UART_TABLE` at the top of `motor_uart.c`.)
2. Can you swap the transport without touching the parser?
3. Are the magic numbers from the manual (cmd bytes 5/6/7/8/9/95, scale factors 1e6/1e5/1e3) named and commented with their source?
4. Is the CRC verifiable? Does it match the manual's worked example (`CRC([0x04]) == 0x4084`)?
5. Are servo-mode-only commands flagged as such in comments, so the MIT-mode migration is mechanical?
