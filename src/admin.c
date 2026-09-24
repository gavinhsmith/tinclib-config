#include <string.h>
#include "admin.h"
#include "tinc_internal.h" /* tinc_xfer, tinc_g: the link, not in tinclib.h */

tinc_err_t admin_status(admin_status_t *st)
{
    tinc_err_t e = tinc_xfer(TINC_T_STATUS, NULL, 0, 0);
    const uint8_t *p = tinc_g.parser.payload;

    if (e != TINC_OK)
        return e;
    if (tinc_g.parser.len < TINC_STATUS_RESP_LEN)
        return TINC_ERR_BAD_LEN;
    st->wifi_state = p[TINC_STATUS_WIFI_STATE];
    st->slot = p[TINC_STATUS_SLOT];
    st->rssi = (int8_t)p[TINC_STATUS_RSSI];
    memcpy(st->ip, p + TINC_STATUS_IP, 4);
    st->wifi_locked = (p[TINC_STATUS_FLAGS] & TINC_STATUSF_WIFI_LOCKED) != 0;
    st->time_valid = (p[TINC_STATUS_FLAGS] & TINC_STATUSF_TIME_VALID) != 0;
    return TINC_OK;
}

tinc_err_t admin_slot_count(uint8_t *count)
{
    uint8_t req[TINC_HELLO_REQ_LEN];
    tinc_piece_t piece = { req, sizeof req };
    tinc_err_t e;
    uint8_t n;

    /* Same HELLO tinclib sends, so the board's view of us doesn't change. */
    req[TINC_HELLO_MAJOR] = TINC_PROTO_MAJOR;
    req[TINC_HELLO_MINOR] = TINC_PROTO_MINOR;
    tinc_put_u16(req + TINC_HELLO_CAPS, 0);
    tinc_put_u16(req + TINC_HELLO_MAX_PAYLOAD, TINC_RX_BUF_SIZE);
    if ((e = tinc_xfer(TINC_T_HELLO, &piece, 1, 0)) != TINC_OK)
        return e;
    if (tinc_g.parser.len < TINC_HELLO_RESP_LEN)
        return TINC_ERR_BAD_LEN;
    n = tinc_g.parser.payload[TINC_HELLO_WIFI_SLOTS];
    if (n == 0 || n > TINC_WIFI_SLOTS_MAX)
        return TINC_ERR_BAD_LEN;
    *count = n;
    return TINC_OK;
}

tinc_err_t admin_get(uint8_t slot, char *ssid, uint8_t *wflags)
{
    tinc_piece_t piece = { &slot, 1 };
    tinc_err_t e = tinc_xfer(TINC_T_WIFI_GET, &piece, 1, 0);
    const uint8_t *p = tinc_g.parser.payload;
    uint16_t len = tinc_g.parser.len;
    uint8_t n;

    ssid[0] = '\0';
    *wflags = 0;
    if (e != TINC_OK)
        return e;
    /* ssid_len, ssid, wflags; trailing bytes ignored (append-only) */
    if (len < 1 || (n = p[TINC_WGET_SSID_LEN]) > TINC_SSID_MAX ||
        TINC_WGET_SSID + n + 1u > len)
        return TINC_ERR_BAD_LEN;
    memcpy(ssid, p + TINC_WGET_SSID, n);
    ssid[n] = '\0';
    *wflags = p[TINC_WGET_SSID + n];
    return TINC_OK;
}

tinc_err_t admin_set(uint8_t slot, const char *ssid, const char *pass, uint8_t wflags)
{
    static uint8_t buf[4 + TINC_SSID_MAX + TINC_PASS_MAX];
    size_t sl = strlen(ssid), pl = strlen(pass);
    tinc_piece_t piece = { buf, 0 };
    tinc_err_t e;

    if (slot >= TINC_WIFI_SLOTS_MAX || sl == 0 || sl > TINC_SSID_MAX || pl > TINC_PASS_MAX)
        return TINC_ERR_BAD_ARG;
    buf[TINC_WSET_SLOT] = slot;
    buf[TINC_WSET_SSID_LEN] = (uint8_t)sl;
    memcpy(buf + TINC_WSET_SSID, ssid, sl);
    buf[TINC_WSET_SSID + sl] = (uint8_t)pl;
    memcpy(buf + TINC_WSET_SSID + sl + 1, pass, pl);
    buf[TINC_WSET_SSID + sl + 1 + pl] = wflags;
    piece.len = (uint16_t)(TINC_WSET_SSID + sl + 2 + pl);

    e = tinc_xfer(TINC_T_WIFI_SET, &piece, 1, 0);
    memset(buf, 0, sizeof buf); /* don't leave the password in RAM */
    return e;
}

tinc_err_t admin_forget(uint8_t slot)
{
    tinc_piece_t piece = { &slot, 1 };

    if (slot >= TINC_WIFI_SLOTS_MAX)
        return TINC_ERR_BAD_ARG;
    return tinc_xfer(TINC_T_WIFI_FORGET, &piece, 1, 0);
}
