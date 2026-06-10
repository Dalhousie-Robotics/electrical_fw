/*
 * parser.h -- line-based command parser.
 *
 * Hardware-agnostic: motor commands go through motor.h, so the parser
 * doesn't know whether the transport is UART or CAN. The only remaining
 * hardware dependency is "where do I print to?" which is supplied by
 * the caller as a function pointer.
 */

#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>

typedef void (*print_fn)(const char *s);

void parser_init(print_fn print);

/* Feed one received character. On '\r' or '\n', the line is dispatched. */
void parser_feed(char c);

#endif /* PARSER_H */
