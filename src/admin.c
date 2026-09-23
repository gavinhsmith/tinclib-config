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
    return TINC_OK;
}

tinc_err_t admin_list(admin_ssids_t ssids)
{
    tinc_err_t e = tinc_xfer(TINC_T_WIFI_LIST, NULL, 0, 0);
    const uint8_t *p = tinc_g.parser.payload;
    uint16_t len = tinc_g.parser.len, pos = 0;
    uint8_t i, n;

    if (e != TINC_OK)
        return e;
    for (i = 0; i < TINC_WIFI_SLOTS; i++) {
        if (pos >= len || (n = p[pos]) > TINC_SSID_MAX || pos + 1u + n > len) {
            memset(ssids, 0, sizeof(admin_ssids_t));
            return TINC_ERR_BAD_LEN;
        }
        memcpy(ssids[i], p + pos + 1, n);
        ssids[i][n] = '\0';
        pos = (uint16_t)(pos + 1u + n);
    }
    return TINC_OK; /* trailing bytes ignored: payloads are append-only */
}

tinc_err_t admin_set(uint8_t slot, const char *ssid, const char *pass)
{
    static uint8_t buf[3 + TINC_SSID_MAX + TINC_PASS_MAX];
    size_t sl = strlen(ssid), pl = strlen(pass);
    tinc_piece_t piece = { buf, 0 };
    tinc_err_t e;

    if (slot >= TINC_WIFI_SLOTS || sl == 0 || sl > TINC_SSID_MAX || pl > TINC_PASS_MAX)
        return TINC_ERR_BAD_ARG;
    buf[TINC_WSET_SLOT] = slot;
    buf[TINC_WSET_SSID_LEN] = (uint8_t)sl;
    memcpy(buf + TINC_WSET_SSID, ssid, sl);
    buf[TINC_WSET_SSID + sl] = (uint8_t)pl;
    memcpy(buf + TINC_WSET_SSID + sl + 1, pass, pl);
    piece.len = (uint16_t)(TINC_WSET_SSID + sl + 1 + pl);

    e = tinc_xfer(TINC_T_WIFI_SET, &piece, 1, 0);
    memset(buf, 0, sizeof buf); /* don't leave the password in RAM */
    return e;
}

tinc_err_t admin_forget(uint8_t slot)
{
    tinc_piece_t piece = { &slot, 1 };

    if (slot >= TINC_WIFI_SLOTS)
        return TINC_ERR_BAD_ARG;
    return tinc_xfer(TINC_T_WIFI_FORGET, &piece, 1, 0);
}
