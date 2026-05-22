/*
 *  RS-485 Protocol – shared between Master (WROOM) and Slave (C3)
 *
 *  Frame format:
 *    REQUEST  (Master → Slave):  [SYNC] [ADDR] [CMD] [D0] [D1] [D2] [D3] [CHK]
 *    RESPONSE (Slave → Master):  [SYNC] [ADDR] [CMD] [D0] [D1] [D2] [D3] [CHK]
 *
 *  SYNC  = 0xAA
 *  ADDR  = slave address (1, 2, …)
 *  CMD   = command byte
 *  D0‥D3 = 4 data bytes (interpretation depends on CMD)
 *  CHK   = XOR of bytes 0..6
 *
 *  Total frame size = 8 bytes
 */
#pragma once
#include <stdint.h>

// ── Frame constants ──────────────────────────────────
#define PROTO_SYNC          0xAA
#define PROTO_FRAME_SIZE    8
#define PROTO_DATA_BYTES    4

// ── Commands ─────────────────────────────────────────
#define CMD_PING            0x01   // master→slave: are you there?
#define CMD_PONG            0x81   // slave→master: yes I am (data = firmware ver)

#define CMD_VALVE_OPEN      0x10   // master→slave: open valve
#define CMD_VALVE_CLOSE     0x11   // master→slave: close valve
#define CMD_VALVE_ACK       0x90   // slave→master: valve cmd acknowledged

#define CMD_FLOW_RESET      0x20   // master→slave: reset pulse counter A
#define CMD_FLOW_POLL       0x21   // master→slave: send me pulse count A
#define CMD_FLOW_DATA       0xA1   // slave→master: pulses A (uint32 in D0‥D3)

#define CMD_FLOW_RESET_B    0x22   // master→slave: reset pulse counter B
#define CMD_FLOW_POLL_B     0x23   // master→slave: send me pulse count B
#define CMD_FLOW_DATA_B     0xA2   // slave→master: pulses B (uint32 in D0‥D3)

#define CMD_ERROR           0xFF   // slave→master: error (D0 = error code)

// ── Error codes ──────────────────────────────────────
#define ERR_UNKNOWN_CMD     0x01
#define ERR_BAD_CHECKSUM    0x02

// ── Frame structure ──────────────────────────────────
struct ProtoFrame {
    uint8_t sync;
    uint8_t addr;
    uint8_t cmd;
    uint8_t data[PROTO_DATA_BYTES];
    uint8_t chk;
};

// ── Helper: compute checksum (XOR of first 7 bytes) ──
inline uint8_t protoChecksum(const uint8_t *buf) {
    uint8_t c = 0;
    for (int i = 0; i < PROTO_FRAME_SIZE - 1; i++) c ^= buf[i];
    return c;
}

// ── Helper: build a frame ────────────────────────────
inline void protoBuildFrame(uint8_t *buf, uint8_t addr, uint8_t cmd,
                            uint8_t d0 = 0, uint8_t d1 = 0,
                            uint8_t d2 = 0, uint8_t d3 = 0)
{
    buf[0] = PROTO_SYNC;
    buf[1] = addr;
    buf[2] = cmd;
    buf[3] = d0;
    buf[4] = d1;
    buf[5] = d2;
    buf[6] = d3;
    buf[7] = protoChecksum(buf);
}

// ── Helper: validate a received frame ────────────────
inline bool protoValidate(const uint8_t *buf) {
    return (buf[0] == PROTO_SYNC) && (buf[7] == protoChecksum(buf));
}

// ── Helper: pack uint32 into 4 data bytes (big-endian) ──
inline void protoPackU32(uint8_t *d, uint32_t val) {
    d[0] = (val >> 24) & 0xFF;
    d[1] = (val >> 16) & 0xFF;
    d[2] = (val >>  8) & 0xFF;
    d[3] =  val        & 0xFF;
}

// ── Helper: unpack uint32 from 4 data bytes (big-endian) ──
inline uint32_t protoUnpackU32(const uint8_t *d) {
    return ((uint32_t)d[0] << 24) |
           ((uint32_t)d[1] << 16) |
           ((uint32_t)d[2] <<  8) |
            (uint32_t)d[3];
}
