/*
 * power.c - shut down the machine.
 *
 * Real hardware needs ACPI (parsing tables to find the PM1a control port).
 * Emulators offer a shortcut: writing a magic value to a fixed I/O port
 * powers them off. If that doesn't work we just halt.
 */
#include "drivers/power.h"
#include "drivers/serial.h"
#include "arch/io.h"

#define QEMU_SHUTDOWN_PORT      0x604
#define BOCHS_SHUTDOWN_PORT     0xB004  /* also older QEMU versions */
#define ACPI_SHUTDOWN_VALUE     0x2000  /* SLP_EN with sleep type 5 (off) */

void power_off(void) {
    serial_printf("power: shutting down\n");
    outportw(QEMU_SHUTDOWN_PORT, ACPI_SHUTDOWN_VALUE);
    outportw(BOCHS_SHUTDOWN_PORT, ACPI_SHUTDOWN_VALUE);
    for (;;) {
        asm volatile("cli; hlt");
    }
}
