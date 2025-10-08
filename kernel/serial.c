/* serial.c - 串行端口驱动 */

#include "serial.h"
#include "io.h"

void serial_init(uint16_t port)
{
    outb(port + SERIAL_IER, 0x00);
    outb(port + SERIAL_LINE_CTRL, SERIAL_LCR_DLAB);
    outb(port + SERIAL_DIV_LOW,  SERIAL_BAUD_115200 & 0xFF);
    outb(port + SERIAL_DIV_HIGH, (SERIAL_BAUD_115200 >> 8) & 0xFF);
    outb(port + SERIAL_LINE_CTRL, SERIAL_LCR_8BITS | SERIAL_LCR_NO_PARITY | SERIAL_LCR_1STOP);
    outb(port + SERIAL_FIFO_CTRL, 0xC7);
    outb(port + SERIAL_MODEM_CTRL, 0x0B);
    outb(port + SERIAL_MODEM_CTRL, 0x1E);
    outb(port + SERIAL_DATA, 0xAE);
    if (inb(port + SERIAL_DATA) != 0xAE) {
        // Serial port faulty
    }
    outb(port + SERIAL_MODEM_CTRL, 0x0F);
}

int serial_transmit_empty(uint16_t port)
{
    return inb(port + SERIAL_LINE_STATUS) & SERIAL_LSR_THR_EMPTY;
}

void serial_putc(uint16_t port, char c)
{
    while (!serial_transmit_empty(port))
        ;
    outb(port + SERIAL_DATA, c);
}

void serial_puts(uint16_t port, const char *str)
{
    while (*str) {
        serial_putc(port, *str++);
    }
}

int serial_received(uint16_t port)
{
    return inb(port + SERIAL_LINE_STATUS) & SERIAL_LSR_DATA_READY;
}

char serial_getc(uint16_t port)
{
    while (!serial_received(port))
        ;
    return inb(port + SERIAL_DATA);
}
