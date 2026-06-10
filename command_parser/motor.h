/*
 * motor.h -- abstract motor command interface.
 *
 * This header is the migration boundary. The parser depends only on this
 * header. The transport (UART servo mode now, CAN MIT mode later) lives in
 * a separate .c file that implements these functions:
 *
 *   motor_uart.c -- AK40-10 servo mode over UART  [CURRENT]
 *   motor_can.c  -- AK40-10 MIT mode over CAN     [FUTURE, when transceivers arrive]
 *
 * To migrate: delete motor_uart.c, add motor_can.c, recompile. The parser
 * is untouched.
 *
 * Units depend on the underlying protocol:
 *   servo mode: position in degrees, velocity in ERPM
 *   MIT mode:   position in radians, velocity in rad/s
 * The parser passes the user value through verbatim. You'll relabel the
 * help text when you swap implementations.
 */

#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

#define MOTOR_COUNT 3

void motor_init         (void);
void motor_set_position (uint8_t id, float pos);
void motor_set_velocity (uint8_t id, float vel);
void motor_set_duty     (uint8_t id, float duty);    /* servo only */
void motor_set_current  (uint8_t id, float amps);
void motor_set_brake    (uint8_t id, float amps);    /* servo only */
void motor_zero         (uint8_t id);
void motor_stop         (uint8_t id);

#endif /* MOTOR_H */
