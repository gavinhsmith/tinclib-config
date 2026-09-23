/*
 * TINCHND appvar: app <-> TINCLIBC handoff. Plain C99, no CE headers, so the
 * host tests build it too.
 *
 * The layout is owned by tinclib (lib/tinclib/src/tinc_config.c, the side
 * that writes the request); this file must match it byte for byte. A
 * mismatch fails silently. Multi-byte fields little-endian.
 *
 *   0   magic        u8   'H'
 *   1   version      u8   1
 *   2   nonce        u32
 *   6   return_to    9B   the app's name, NUL-padded (display only)
 *   15  action       u8
 *   16  requirements u8
 *   17  hint_len     u8   <= HND_HINT_MAX
 *   18  hint         hint_len bytes, no NUL
 *   --- written by TINCLIBC, right after the hint ---
 *   +0  result       u8   HND_NONE as the app writes it
 *   +1  nonce echo   u32
 *   +5  detail_len   u8
 *   +6  detail
 *
 * TINCLIBC writes the result and exits; it never relaunches the app (the
 * app returns through its os_RunPrgm callback; see tinclib's AGENTS.md).
 * No secrets ever go in here: there is no field that could hold a password.
 */
#ifndef HANDOFF_H
#define HANDOFF_H

#include <stdint.h>

#define HND_MAGIC       'H'
#define HND_VERSION     1u

#define HND_OFF_MAGIC   0
#define HND_OFF_VERSION 1
#define HND_OFF_NONCE   2
#define HND_OFF_RETURN  6
#define HND_OFF_ACTION  15
#define HND_OFF_REQS    16
#define HND_OFF_HINTLEN 17
#define HND_OFF_HINT    18
/* tail, relative to HND_OFF_HINT + hint_len */
#define HND_T_RESULT    0
#define HND_T_ECHO      1
#define HND_T_DETLEN    5
#define HND_T_DETAIL    6

#define HND_RETURN_LEN  9u  /* 8 chars + NUL */
#define HND_HINT_MAX    63u
#define HND_DETAIL_MAX  32u
#define HND_MIN_LEN     (HND_OFF_HINT + HND_T_DETAIL)
#define HND_MAX_LEN     (HND_MIN_LEN + HND_HINT_MAX + HND_DETAIL_MAX)

enum { HND_SETUP_WIFI = 1, HND_TEST_CONN = 2 };
enum { HND_NEEDS_WIFI = 0x01, HND_NEEDS_TIME = 0x02 };
enum { HND_NONE = 0, HND_OK = 1, HND_CANCELLED = 2, HND_FAILED = 3 };

typedef struct {
    uint32_t nonce;
    char return_to[HND_RETURN_LEN];
    uint8_t action;
    uint8_t requirements;
    char hint[HND_HINT_MAX + 1];
} hnd_request_t;

/* 1 if buf holds a fresh (result still NONE) well-formed request. A
 * filled-in result is a leftover from an earlier request, not a new one. */
int hnd_parse(const uint8_t *buf, uint16_t len, hnd_request_t *req);

/* Writes result, nonce echo and detail into buf (capacity cap), only if buf
 * still holds a request with this nonce. Returns the new length, 0 on
 * failure. */
uint16_t hnd_write_result(uint8_t *buf, uint16_t len, uint16_t cap,
                          uint32_t nonce, uint8_t result,
                          const uint8_t *detail, uint8_t detail_len);

/* App side (mirrors tinclib's check): the result for *this* nonce, or -1 if
 * there is none yet or it belongs to another request. */
int hnd_result_for(const uint8_t *buf, uint16_t len, uint32_t nonce);

#endif /* HANDOFF_H */
