/*
 * TINCHND appvar: app <-> TINCLIBC handoff. Plain C99, no CE headers, so the
 * host tests build it too.
 *
 * This layout is defined here first; tinclib's tinc_openConfig() must write
 * exactly this. A mismatch fails silently, so change both sides together.
 *
 *   magic u8 'T', version u8, nonce u32,
 *   return_to[9] (NUL-terminated), action u8, requirements u8,
 *   hint_len u8, hint[hint_len],
 *   ---- filled in by TINCLIBC ----
 *   result u8, detail_len u8, detail[detail_len]
 *
 * The app writes result = HND_PENDING and detail_len = 0. No secrets ever go
 * in here: there is deliberately no field that could hold a password.
 */
#ifndef HANDOFF_H
#define HANDOFF_H

#include <stdint.h>

#define HND_MAGIC       0x54u /* 'T' */
#define HND_VERSION     1u

#define HND_OFF_MAGIC   0
#define HND_OFF_VERSION 1
#define HND_OFF_NONCE   2
#define HND_OFF_RETURN  6
#define HND_OFF_ACTION  15
#define HND_OFF_REQS    16
#define HND_OFF_HINTLEN 17
#define HND_OFF_HINT    18
/* then result at HND_OFF_HINT + hint_len, detail_len right after it */

#define HND_RETURN_LEN  9u  /* 8 chars + NUL */
#define HND_HINT_MAX    64u
#define HND_DETAIL_MAX  32u
#define HND_MIN_LEN     (HND_OFF_HINT + 2u)
#define HND_MAX_LEN     (HND_MIN_LEN + HND_HINT_MAX + HND_DETAIL_MAX)

enum { HND_SETUP_WIFI = 1, HND_TEST_CONN = 2 };
enum { HND_NEEDS_WIFI = 0x01, HND_NEEDS_TIME = 0x02 };
enum { HND_PENDING = 0, HND_OK = 1, HND_CANCELLED = 2, HND_FAILED = 3 };

typedef struct {
    uint32_t nonce;
    char return_to[HND_RETURN_LEN];
    uint8_t action;
    uint8_t requirements;
    char hint[HND_HINT_MAX + 1];
} hnd_request_t;

/* 1 if buf holds a fresh (result still PENDING) well-formed request. A
 * filled-in result is a leftover from an earlier request, not a new one. */
int hnd_parse(const uint8_t *buf, uint16_t len, hnd_request_t *req);

/* Writes result + detail into buf (capacity cap), only if buf still holds a
 * request with this nonce. Returns the new length, 0 on failure. */
uint16_t hnd_write_result(uint8_t *buf, uint16_t len, uint16_t cap,
                          uint32_t nonce, uint8_t result,
                          const uint8_t *detail, uint8_t detail_len);

/* App side (reference for tinclib): the result for *this* nonce, or -1 if
 * there is none yet or it belongs to another request. */
int hnd_result_for(const uint8_t *buf, uint16_t len, uint32_t nonce);

#endif /* HANDOFF_H */
