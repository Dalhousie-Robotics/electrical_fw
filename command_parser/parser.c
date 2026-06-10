/*
 * parser.c -- minimal line-based command parser.
 *
 * Commands (one per line, terminated by \r or \n):
 *
 *   help
 *   pos     <id> <deg>     position mode  [-36000 .. 36000 deg]
 *   vel     <id> <erpm>    velocity mode  [-100000 .. 100000 ERPM]
 *   duty    <id> <ratio>   duty cycle     [-1.0 .. 1.0]      (servo only)
 *   current <id> <amps>    current loop / torque             [±60 A]
 *   brake   <id> <amps>    current brake                     (servo only)
 *   zero    <id>           set current position as origin (permanent)
 *   stop    <id>           safe stop (sets duty to zero)
 *
 *   <id> is the motor index (1, 2, or 3) -- it selects which UART/bus to
 *   send on, NOT the motor's internal CAN ID.
 *
 * When migrating to MIT mode: rename `duty/brake` to `enter/exit`, add a
 * raw `cmd <id> <p> <v> <kp> <kd> <t>` for impedance control, and update
 * the help text. The dispatch structure stays identical.
 */

#include "parser.h"
#include "motor.h"

#include <string.h>
#include <stdlib.h>

#define LINE_MAX 96

static print_fn s_print = 0;
static char     s_line[LINE_MAX];
static uint16_t s_len   = 0;

/* ---- helpers ----------------------------------------------------------- */

static void print_help(void)
{
    s_print(
        "\r\n"
        "  pos     <id> <deg>     position  [-36000..36000 deg]\r\n"
        "  vel     <id> <erpm>    velocity  [-100000..100000 ERPM]\r\n"
        "  duty    <id> <ratio>   duty      [-1.0..1.0]\r\n"
        "  current <id> <amps>    current loop / torque\r\n"
        "  brake   <id> <amps>    current brake\r\n"
        "  zero    <id>           set current pos as origin (permanent)\r\n"
        "  stop    <id>           safe stop (duty = 0)\r\n"
        "  help                   this message\r\n"
        "\r\n"
        "  <id> = 1, 2, or 3 (motor index)\r\n"
    );
}

static int tokenize(char *line, char *tk[], int max)
{
    int n = 0;
    char *p = line;
    while (*p && n < max) {
        while (*p == ' ' || *p == '\t') *p++ = '\0';
        if (!*p) break;
        tk[n++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
    }
    return n;
}

/* ---- dispatch ---------------------------------------------------------- */

static void dispatch(char *line)
{
    char *tk[8];
    int n = tokenize(line, tk, 8);
    if (n == 0) return;

    if (strcmp(tk[0], "help") == 0) { print_help(); return; }

    if (n < 2)                      { s_print("err: missing motor id\r\n"); return; }
    uint8_t id = (uint8_t)atoi(tk[1]);
    if (id < 1 || id > MOTOR_COUNT) { s_print("err: id must be 1..3\r\n"); return; }

    /* No-argument commands first. */
    if (strcmp(tk[0], "stop") == 0) { motor_stop(id); s_print("ok\r\n"); return; }
    if (strcmp(tk[0], "zero") == 0) { motor_zero(id); s_print("ok\r\n"); return; }

    /* Single-argument commands. */
    if (n < 3) { s_print("err: missing value\r\n"); return; }
    float v = (float)atof(tk[2]);

    if      (strcmp(tk[0], "pos")     == 0) motor_set_position(id, v);
    else if (strcmp(tk[0], "vel")     == 0) motor_set_velocity(id, v);
    else if (strcmp(tk[0], "duty")    == 0) motor_set_duty    (id, v);
    else if (strcmp(tk[0], "current") == 0) motor_set_current (id, v);
    else if (strcmp(tk[0], "brake")   == 0) motor_set_brake   (id, v);
    else { s_print("err: unknown command (try 'help')\r\n"); return; }

    s_print("ok\r\n");
}

/* ---- public API -------------------------------------------------------- */

void parser_init(print_fn print)
{
    s_print = print;
    s_len   = 0;
    s_print("\r\nTOBY motor parser (3x AK40-10, UART servo). Type 'help'.\r\n");
}

void parser_feed(char c)
{
    if (c == '\r' || c == '\n') {
        if (s_len > 0) {
            s_line[s_len] = '\0';
            dispatch(s_line);
            s_len = 0;
        }
    } else if (s_len < LINE_MAX - 1) {
        s_line[s_len++] = c;
    }
    /* Overflow silently truncates -- user re-sends if no 'ok' appears. */
}
