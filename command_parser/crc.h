/*
 * crc.h -- CRC-16 CCITT (XMODEM variant) for CubeMars servo-mode UART frames.
 * Polynomial 0x1021, initial value 0x0000. Per AK manual section 5.2.2.
 */

#ifndef CRC_H
#define CRC_H

#include <stdint.h>

uint16_t crc16(const uint8_t *buf, uint16_t len);

#endif /* CRC_H */
