/*
 * mouse.c - PS/2 mouse on IRQ12.
 *
 * The mouse hangs off the same 8042 controller as the keyboard, on its
 * second ("auxiliary") port. It is off by default: mouse_init() enables
 * the port and its IRQ in the controller, then tells the mouse itself to
 * start reporting.
 *
 * Every movement or button change sends a 3-byte packet, one byte per
 * IRQ12:
 *
 *   byte 0: bit 0-2 left/right/middle button, bit 3 always 1,
 *           bit 4 X sign, bit 5 Y sign, bit 6/7 X/Y overflow
 *   byte 1: X movement (low 8 bits of a 9-bit two's complement value)
 *   byte 2: Y movement (same; positive means *up*)
 */
#include "drivers/mouse.h"
#include "drivers/serial.h"
#include "arch/idt.h"
#include "gui/event.h"

#define PS2_MOUSE_IRQ           12
#define PS2_DATA_PORT           0x60
#define PS2_STATUS_PORT         0x64    /* read  */
#define PS2_COMMAND_PORT        0x64    /* write */

#define PS2_STATUS_OUTPUT_FULL  0x01    /* a byte is waiting in 0x60       */
#define PS2_STATUS_INPUT_FULL   0x02    /* controller hasn't taken our byte */
#define PS2_TIMEOUT             100000

#define PS2_CMD_ENABLE_AUX      0xA8
#define PS2_CMD_READ_CONFIG     0x20
#define PS2_CMD_WRITE_CONFIG    0x60
#define PS2_CMD_WRITE_AUX       0xD4    /* send the next data byte to the mouse */
#define PS2_CONFIG_AUX_IRQ      0x02    /* raise IRQ12 for mouse data      */
#define PS2_CONFIG_AUX_CLOCK_OFF 0x20   /* set = mouse port disabled       */

#define MOUSE_CMD_SET_DEFAULTS  0xF6
#define MOUSE_CMD_ENABLE_REPORT 0xF4
#define MOUSE_ACK               0xFA

#define PACKET_SIZE             3
#define PACKET_ALWAYS_ONE       0x08
#define PACKET_BUTTONS          0x07
#define PACKET_X_SIGN           0x10
#define PACKET_Y_SIGN           0x20
#define PACKET_OVERFLOW         0xC0
#define SIGN_EXTEND             0x100   /* 9-bit value: subtract if sign set */

static const uint8_t button_bits[] = {
    MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE
};

static int bound_width;
static int bound_height;
static int mouse_x;
static int mouse_y;
static uint8_t buttons_down;
static uint8_t packet[PACKET_SIZE];
static int packet_index;

/* ---- 8042 controller ---------------------------------------------- */

static void ps2_wait_writable(void) {
    for (int i = 0; i < PS2_TIMEOUT && (inportb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL); i++) {
    }
}

static void ps2_wait_readable(void) {
    for (int i = 0; i < PS2_TIMEOUT && !(inportb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL); i++) {
    }
}

static void ps2_command(uint8_t command) {
    ps2_wait_writable();
    outportb(PS2_COMMAND_PORT, command);
}

static uint8_t ps2_read(void) {
    ps2_wait_readable();
    return inportb(PS2_DATA_PORT);
}

static void ps2_write(uint8_t value) {
    ps2_wait_writable();
    outportb(PS2_DATA_PORT, value);
}

/* Send a command byte to the mouse and wait for its acknowledgement. */
static bool mouse_send(uint8_t command) {
    ps2_command(PS2_CMD_WRITE_AUX);
    ps2_write(command);
    return ps2_read() == MOUSE_ACK;
}

static void ps2_enable_aux_port(void) {
    ps2_command(PS2_CMD_ENABLE_AUX);

    ps2_command(PS2_CMD_READ_CONFIG);
    uint8_t config = ps2_read();
    config |= PS2_CONFIG_AUX_IRQ;
    config &= ~PS2_CONFIG_AUX_CLOCK_OFF;
    ps2_command(PS2_CMD_WRITE_CONFIG);
    ps2_write(config);
}

/* ---- packets ------------------------------------------------------ */

static int clamp(int value, int low, int high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static void mouse_push(event_type type, uint8_t button) {
    event_t event = { .type = type, .x = mouse_x, .y = mouse_y, .button = button };
    event_push(&event);
}

static void mouse_handle_movement(uint8_t flags, uint8_t raw_x, uint8_t raw_y) {
    if (flags & PACKET_OVERFLOW) {
        return;     /* the deltas are garbage when they overflowed */
    }
    int dx = raw_x - ((flags & PACKET_X_SIGN) ? SIGN_EXTEND : 0);
    int dy = raw_y - ((flags & PACKET_Y_SIGN) ? SIGN_EXTEND : 0);
    if (dx == 0 && dy == 0) {
        return;
    }

    /* The mouse counts Y upwards, the screen counts it downwards. */
    mouse_x = clamp(mouse_x + dx, 0, bound_width - 1);
    mouse_y = clamp(mouse_y - dy, 0, bound_height - 1);
    mouse_push(EVENT_MOUSE_MOVE, 0);
}

static void mouse_handle_buttons(uint8_t flags) {
    uint8_t now = flags & PACKET_BUTTONS;
    uint8_t changed = now ^ buttons_down;

    for (uint32_t i = 0; i < sizeof(button_bits); i++) {
        uint8_t bit = button_bits[i];
        if (changed & bit) {
            mouse_push((now & bit) ? EVENT_MOUSE_DOWN : EVENT_MOUSE_UP, bit);
        }
    }
    buttons_down = now;
}

static void mouse_handler(registers_t* regs) {
    (void)regs;
    uint8_t byte = inportb(PS2_DATA_PORT);

    /* Bit 3 of the first byte is always set. If it isn't, we are out of
       step with the packet stream: drop bytes until we line up again. */
    if (packet_index == 0 && !(byte & PACKET_ALWAYS_ONE)) {
        return;
    }
    packet[packet_index++] = byte;
    if (packet_index < PACKET_SIZE) {
        return;
    }

    packet_index = 0;
    mouse_handle_movement(packet[0], packet[1], packet[2]);
    mouse_handle_buttons(packet[0]);
}

void mouse_init(int screen_width, int screen_height) {
    bound_width  = screen_width;
    bound_height = screen_height;
    mouse_x = screen_width / 2;
    mouse_y = screen_height / 2;

    /* Talk to the controller with interrupts off, so the IRQ handlers
       can't steal the replies we are waiting for. */
    asm volatile("cli");
    ps2_enable_aux_port();
    bool ok = mouse_send(MOUSE_CMD_SET_DEFAULTS) && mouse_send(MOUSE_CMD_ENABLE_REPORT);
    while (inportb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) {
        inportb(PS2_DATA_PORT);     /* drop any leftover reply bytes */
    }

    /* The replies above already raised IRQ12, which is still pending. Let
       it fire before the handler is installed, or the handler would read a
       stale byte and get out of step with the packets. */
    asm volatile("sti; nop");
    irq_install_handler(PS2_MOUSE_IRQ, mouse_handler);

    serial_printf("mouse: %s\n", ok ? "ready" : "no acknowledgement");
}
