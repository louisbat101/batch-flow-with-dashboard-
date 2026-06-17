/*
 * rs485_setmode.c - Enable RS-485 half-duplex mode on a serial port
 * Usage: rs485_setmode /dev/ttyS7 [0|1]
 *   1 = enable RS-485 half-duplex (default)
 *   0 = disable RS-485 half-duplex
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <asm-generic/ioctls.h>

#ifndef TIOCSRS485
#define TIOCSRS485 0x542F
#endif

struct serial_rs485 {
    uint32_t flags;
    uint32_t delay_rts_before_send;
    uint32_t delay_rts_after_send;
    uint32_t padding[5];
};

#define SER_RS485_ENABLED        (1 << 0)
#define SER_RS485_RTS_ON_SEND    (1 << 1)
#define SER_RS485_RTS_AFTER_SEND (1 << 2)

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port> [enable=1]\n", argv[0]);
        return 1;
    }
    
    const char *port = argv[1];
    int enable = 1;
    if (argc >= 3) enable = atoi(argv[2]);
    
    int fd = open(port, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    
    struct serial_rs485 rs485conf;
    memset(&rs485conf, 0, sizeof(rs485conf));
    
    if (enable) {
        rs485conf.flags = SER_RS485_ENABLED;
        // RTS high during send (typical for RS-485)
        // rs485conf.flags |= SER_RS485_RTS_ON_SEND;
    }
    
    if (ioctl(fd, TIOCSRS485, &rs485conf) < 0) {
        perror("TIOCSRS485 ioctl");
        close(fd);
        return 1;
    }
    
    printf("RS-485 mode %s on %s\n", enable ? "ENABLED" : "DISABLED", port);
    
    // Verify by reading back
    struct serial_rs485 rs485read;
    memset(&rs485read, 0, sizeof(rs485read));
    if (ioctl(fd, TIOCGRS485, &rs485read) == 0) {
        printf("Readback flags: 0x%x\n", rs485read.flags);
    }
    
    close(fd);
    return 0;
}
