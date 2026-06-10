/*
 * crc.c -- bit-by-bit CRC-16 CCITT.
 *
 * The CubeMars manual prints a 256-entry lookup table for speed. We don't
 * need that here -- motor commands run at parser rates (tens of Hz at most),
 * and the bit-by-bit version is short, obviously correct, and matches the
 * manual's worked example (CRC of [0x04] == 0x4084).
 */

#include "crc.h"

uint16_t crc16(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021)
                                 : (uint16_t)(crc << 1);
        }
    }
    return crc;
}
