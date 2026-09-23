#include <string.h>
#include "handoff.h"
#include "protocol.h" /* tinc_get_u32 / tinc_put_u32 */

/* Offset of the tail (result byte), or 0 if the header is malformed. */
static uint16_t tail_off(const uint8_t *buf, uint16_t len)
{
    uint16_t off;

    if (len < HND_MIN_LEN || buf[HND_OFF_MAGIC] != HND_MAGIC ||
        buf[HND_OFF_VERSION] != HND_VERSION ||
        buf[HND_OFF_HINTLEN] > HND_HINT_MAX)
        return 0;
    off = HND_OFF_HINT + buf[HND_OFF_HINTLEN];
    if (off + HND_T_DETAIL > len || buf[off + HND_T_DETLEN] > HND_DETAIL_MAX ||
        off + HND_T_DETAIL + buf[off + HND_T_DETLEN] > len)
        return 0;
    return off;
}

int hnd_parse(const uint8_t *buf, uint16_t len, hnd_request_t *req)
{
    uint16_t off = tail_off(buf, len);
    uint8_t action, hint_len;

    if (!off || buf[off + HND_T_RESULT] != HND_NONE)
        return 0;
    if (!memchr(buf + HND_OFF_RETURN, '\0', HND_RETURN_LEN) ||
        buf[HND_OFF_RETURN] == '\0')
        return 0;
    action = buf[HND_OFF_ACTION];
    if (action != HND_SETUP_WIFI && action != HND_TEST_CONN)
        return 0;

    hint_len = buf[HND_OFF_HINTLEN];
    req->nonce = tinc_get_u32(buf + HND_OFF_NONCE);
    memcpy(req->return_to, buf + HND_OFF_RETURN, HND_RETURN_LEN);
    req->action = action;
    req->requirements = buf[HND_OFF_REQS];
    memcpy(req->hint, buf + HND_OFF_HINT, hint_len);
    req->hint[hint_len] = '\0';
    return 1;
}

uint16_t hnd_write_result(uint8_t *buf, uint16_t len, uint16_t cap,
                          uint32_t nonce, uint8_t result,
                          const uint8_t *detail, uint8_t detail_len)
{
    uint16_t off = tail_off(buf, len);

    if (!off || tinc_get_u32(buf + HND_OFF_NONCE) != nonce ||
        detail_len > HND_DETAIL_MAX || off + HND_T_DETAIL + detail_len > cap)
        return 0;
    buf[off + HND_T_RESULT] = result;
    tinc_put_u32(buf + off + HND_T_ECHO, nonce);
    buf[off + HND_T_DETLEN] = detail_len;
    if (detail_len)
        memcpy(buf + off + HND_T_DETAIL, detail, detail_len);
    return (uint16_t)(off + HND_T_DETAIL + detail_len);
}

int hnd_result_for(const uint8_t *buf, uint16_t len, uint32_t nonce)
{
    uint16_t off = tail_off(buf, len);

    if (!off || buf[off + HND_T_RESULT] == HND_NONE ||
        tinc_get_u32(buf + off + HND_T_ECHO) != nonce)
        return -1;
    return buf[off + HND_T_RESULT];
}
