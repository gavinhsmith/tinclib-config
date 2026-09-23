/* Host tests for src/handoff.c: make -C tests */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "handoff.h"
#include "protocol.h"

/* Builds the request an app would write. Returns its length. */
static uint16_t make_req(uint8_t *b, uint32_t nonce, const char *ret,
                         uint8_t action, const char *hint)
{
    uint8_t hl = (uint8_t)strlen(hint);

    memset(b, 0, HND_MAX_LEN);
    b[HND_OFF_MAGIC] = HND_MAGIC;
    b[HND_OFF_VERSION] = HND_VERSION;
    tinc_put_u32(b + HND_OFF_NONCE, nonce);
    memcpy(b + HND_OFF_RETURN, ret, strlen(ret)); /* <= 8 chars, NUL from memset */
    b[HND_OFF_ACTION] = action;
    b[HND_OFF_REQS] = HND_NEEDS_WIFI;
    b[HND_OFF_HINTLEN] = hl;
    memcpy(b + HND_OFF_HINT, hint, hl);
    /* result = PENDING, detail_len = 0 (already zeroed) */
    return (uint16_t)(HND_MIN_LEN + hl);
}

int main(void)
{
    uint8_t b[HND_MAX_LEN];
    hnd_request_t r;
    uint16_t n, n2;

    /* valid request parses */
    n = make_req(b, 0xDEADBEEF, "MINIBRWS", HND_SETUP_WIFI, "Need Wi-Fi");
    assert(hnd_parse(b, n, &r));
    assert(r.nonce == 0xDEADBEEF);
    assert(!strcmp(r.return_to, "MINIBRWS"));
    assert(r.action == HND_SETUP_WIFI && r.requirements == HND_NEEDS_WIFI);
    assert(!strcmp(r.hint, "Need Wi-Fi"));
    assert(hnd_result_for(b, n, 0xDEADBEEF) == -1); /* nothing yet */

    /* rejections */
    b[HND_OFF_MAGIC] ^= 1;   assert(!hnd_parse(b, n, &r)); b[HND_OFF_MAGIC] ^= 1;
    b[HND_OFF_VERSION] = 9;  assert(!hnd_parse(b, n, &r)); b[HND_OFF_VERSION] = HND_VERSION;
    b[HND_OFF_ACTION] = 7;   assert(!hnd_parse(b, n, &r)); b[HND_OFF_ACTION] = HND_SETUP_WIFI;
    assert(!hnd_parse(b, n - 1, &r));                  /* truncated */
    assert(!hnd_parse(b, 3, &r));
    b[HND_OFF_HINTLEN] = 200; assert(!hnd_parse(b, n, &r)); b[HND_OFF_HINTLEN] = 10;
    b[n - 1] = 5;             assert(!hnd_parse(b, n, &r)); b[n - 1] = 0; /* detail past end */
    memset(b + HND_OFF_RETURN, 'X', HND_RETURN_LEN);
    assert(!hnd_parse(b, n, &r));                      /* return_to not terminated */
    b[HND_OFF_RETURN] = '\0';
    assert(!hnd_parse(b, n, &r));                      /* empty return_to */
    strcpy((char *)b + HND_OFF_RETURN, "MINIBRWS");
    assert(hnd_parse(b, n, &r));

    /* write result, round-trip, nonce echoed */
    n2 = hnd_write_result(b, n, sizeof b, 0xDEADBEEF, HND_CANCELLED,
                          (const uint8_t *)"\x20", 1);
    assert(n2 == n + 1);
    assert(hnd_result_for(b, n2, 0xDEADBEEF) == HND_CANCELLED);
    assert(b[n2 - 1] == 0x20);

    /* stale: a filled result is not a fresh request... */
    assert(!hnd_parse(b, n2, &r));
    /* ...and an app with a different nonce must not take it as its own */
    assert(hnd_result_for(b, n2, 0x12345678) == -1);

    /* result only written for the nonce that was read */
    n = make_req(b, 1, "APP", HND_TEST_CONN, "");
    assert(!hnd_write_result(b, n, sizeof b, 2, HND_OK, NULL, 0));
    assert(!hnd_write_result(b, n, n + 1, 1, HND_OK, (const uint8_t *)"ab", 2)); /* no room */
    assert(hnd_write_result(b, n, sizeof b, 1, HND_OK, NULL, 0) == n);
    assert(hnd_result_for(b, n, 1) == HND_OK);

    puts("test_handoff: all passed");
    return 0;
}
