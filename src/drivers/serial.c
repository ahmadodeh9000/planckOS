/*
 * serial.c - COM1 (16550 UART) output for debug logging.
 *
 * Only transmitting is supported, and it polls instead of using interrupts.
 * That makes it safe to call from anywhere, including exception handlers
 * and IRQ handlers.
 */
#include "drivers/serial.h"
#include "arch/io.h"
#include <stdarg.h>

#define COM1_PORT           0x3F8

/* UART registers, as offsets from the base port */
#define UART_DATA           0   /* THR when writing, RBR when reading      */
#define UART_INT_ENABLE     1   /* IER; holds the divisor high byte if DLAB */
#define UART_DIVISOR_LOW    0   /* only while DLAB is set                   */
#define UART_DIVISOR_HIGH   1   /* only while DLAB is set                   */
#define UART_FIFO_CTRL      2
#define UART_LINE_CTRL      3
#define UART_MODEM_CTRL     4
#define UART_LINE_STATUS    5

#define UART_LCR_DLAB       0x80    /* next 2 regs are the baud divisor */
#define UART_LCR_8N1        0x03    /* 8 data bits, no parity, 1 stop   */
#define UART_FIFO_ENABLE    0xC7    /* enable + clear FIFOs, 14-byte threshold */
#define UART_MCR_DTR_RTS    0x03
#define UART_LSR_THR_EMPTY  0x20    /* transmit holding register empty  */

#define UART_BASE_BAUD      115200
#define SERIAL_BAUD         38400

void serial_init(void) {
    uint16_t divisor = UART_BASE_BAUD / SERIAL_BAUD;

    outportb(COM1_PORT + UART_INT_ENABLE, 0x00);    /* we poll, no IRQs */
    outportb(COM1_PORT + UART_LINE_CTRL, UART_LCR_DLAB);
    outportb(COM1_PORT + UART_DIVISOR_LOW, divisor & 0xFF);
    outportb(COM1_PORT + UART_DIVISOR_HIGH, (divisor >> 8) & 0xFF);
    outportb(COM1_PORT + UART_LINE_CTRL, UART_LCR_8N1);
    outportb(COM1_PORT + UART_FIFO_CTRL, UART_FIFO_ENABLE);
    outportb(COM1_PORT + UART_MODEM_CTRL, UART_MCR_DTR_RTS);
}

void serial_putc(char c) {
    /* Terminals expect CR LF; the kernel only uses LF. */
    if (c == '\n') {
        serial_putc('\r');
    }
    while (!(inportb(COM1_PORT + UART_LINE_STATUS) & UART_LSR_THR_EMPTY)) {
    }
    outportb(COM1_PORT + UART_DATA, c);
}

static void serial_print(const char* str) {
    while (*str) {
        serial_putc(*str++);
    }
}

static void serial_print_unsigned(uint32_t value, uint32_t base) {
    const char* digits = "0123456789ABCDEF";
    char buf[32];
    int len = 0;

    do {
        buf[len++] = digits[value % base];
        value /= base;
    } while (value);

    while (len--) {
        serial_putc(buf[len]);
    }
}

static void serial_print_signed(int32_t value) {
    if (value < 0) {
        serial_putc('-');
        serial_print_unsigned((uint32_t)-value, 10);
    } else {
        serial_print_unsigned((uint32_t)value, 10);
    }
}

/* Supports %s %c %d %u %x and %%. */
void serial_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            serial_putc(*fmt);
            continue;
        }
        switch (*++fmt) {
            case 's': serial_print(va_arg(args, const char*)); break;
            case 'c': serial_putc((char)va_arg(args, int)); break;
            case 'd': serial_print_signed(va_arg(args, int32_t)); break;
            case 'u': serial_print_unsigned(va_arg(args, uint32_t), 10); break;
            case 'x':
                serial_print("0x");
                serial_print_unsigned(va_arg(args, uint32_t), 16);
                break;
            case '%': serial_putc('%'); break;
            case '\0': fmt--; break;    /* lone '%' at end of string */
            default: serial_putc('%'); serial_putc(*fmt); break;
        }
    }

    va_end(args);
}
