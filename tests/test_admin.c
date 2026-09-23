/* Host tests for src/admin.c, with tinc_xfer() stubbed: it records the frame
 * sent and hands back a canned reply. make -C tests */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "admin.h"
#include "tinc_internal.h"

tinc_state_store_t tinc_g;

static uint8_t sent_type, sent[256];
static uint16_t sent_len;
static const uint8_t *reply;
static uint16_t reply_len;
static tinc_err_t reply_err;

tinc_err_t tinc_xfer(uint8_t type, const tinc_piece_t *pieces, uint8_t n,
                     uint16_t wait_ms)
{
    uint8_t i;

    (void)wait_ms;
    sent_type = type;
    sent_len = 0;
    for (i = 0; i < n; i++) {
        memcpy(sent + sent_len, pieces[i].p, pieces[i].len);
        sent_len = (uint16_t)(sent_len + pieces[i].len);
    }
    tinc_g.parser.payload = reply;
    tinc_g.parser.len = reply_len;
    return reply_err;
}

static void set_reply(const void *p, uint16_t len, tinc_err_t err)
{
    reply = p;
    reply_len = len;
    reply_err = err;
}

int main(void)
{
    admin_status_t st;
    admin_ssids_t ss;
    char longpass[TINC_PASS_MAX + 2];

    /* STATUS */
    {
        static const uint8_t r[] = { TINC_WIFI_CONNECTED, 1, (uint8_t)-61,
                                     192, 168, 1, 7, 0, 0, 0, 0, 0, 0xEE };
        set_reply(r, sizeof r, TINC_OK);
        assert(admin_status(&st) == TINC_OK && sent_type == TINC_T_STATUS);
        assert(st.wifi_state == TINC_WIFI_CONNECTED && st.slot == 1 && st.rssi == -61);
        assert(st.ip[0] == 192 && st.ip[3] == 7);
        set_reply(r, TINC_STATUS_RESP_LEN - 1, TINC_OK);
        assert(admin_status(&st) == TINC_ERR_BAD_LEN);
        set_reply(NULL, 0, TINC_ERR_NO_REPLY);
        assert(admin_status(&st) == TINC_ERR_NO_REPLY);
    }

    /* WIFI_LIST */
    {
        static const uint8_t r[] = { 4, 'h', 'o', 'm', 'e', 0, 2, 'o', 'k', 0x99 };
        static const uint8_t bad[] = { 4, 'h', 'o', 'm', 'e', 0, 9, 'x' };
        static const uint8_t big[] = { TINC_SSID_MAX + 1 };
        set_reply(r, sizeof r, TINC_OK);
        assert(admin_list(ss) == TINC_OK && sent_type == TINC_T_WIFI_LIST);
        assert(!strcmp(ss[0], "home") && !strcmp(ss[1], "") && !strcmp(ss[2], "ok"));
        set_reply(bad, sizeof bad, TINC_OK);
        assert(admin_list(ss) == TINC_ERR_BAD_LEN && !ss[0][0]);
        set_reply(big, sizeof big, TINC_OK);
        assert(admin_list(ss) == TINC_ERR_BAD_LEN);
        set_reply(r, 3, TINC_OK);
        assert(admin_list(ss) == TINC_ERR_BAD_LEN);
    }

    /* WIFI_SET: exact payload */
    {
        static const uint8_t want[] = { 2, 3, 'n', 'e', 't', 8,
                                        'p', 'a', 's', 's', 'w', 'o', 'r', 'd' };
        set_reply(NULL, 0, TINC_OK);
        assert(admin_set(2, "net", "password") == TINC_OK);
        assert(sent_type == TINC_T_WIFI_SET && sent_len == sizeof want);
        assert(!memcmp(sent, want, sizeof want));
        assert(admin_set(0, "open", "") == TINC_OK && sent_len == 7 && sent[6] == 0);

        memset(longpass, 'p', sizeof longpass - 1);
        longpass[sizeof longpass - 1] = '\0'; /* TINC_PASS_MAX + 1 chars */
        assert(admin_set(0, "n", longpass) == TINC_ERR_BAD_ARG);
        longpass[TINC_PASS_MAX] = '\0';
        assert(admin_set(0, "n", longpass) == TINC_OK);
        assert(admin_set(3, "n", "") == TINC_ERR_BAD_ARG);
        assert(admin_set(0, "", "") == TINC_ERR_BAD_ARG);
        assert(admin_set(0, "012345678901234567890123456789012", "") == TINC_ERR_BAD_ARG);
        set_reply(NULL, 0, TINC_ERR_BAD_ARG);
        assert(admin_set(0, "n", "x") == TINC_ERR_BAD_ARG); /* board's error passes through */
    }

    /* WIFI_FORGET */
    set_reply(NULL, 0, TINC_OK);
    assert(admin_forget(1) == TINC_OK && sent_type == TINC_T_WIFI_FORGET);
    assert(sent_len == 1 && sent[0] == 1);
    assert(admin_forget(TINC_WIFI_SLOTS) == TINC_ERR_BAD_ARG);

    puts("test_admin: all passed");
    return 0;
}
