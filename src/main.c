/* TINCLIBC: board configuration (Wi-Fi slots, insecure-TLS toggle) and the
 * landing point for app handoffs via the TINCHND appvar. See AGENTS.md. */

#include <stdio.h>
#include <string.h>
#include <fileioc.h>
#include <ti/vars.h>
#include "titrm.h"
#include "protocol.h"
#include "handoff.h"

#define HND_NAME "TINCHND"

static const char *const menu_items[] = {
    "Saved networks",
    "Test connection",
    "Insecure TLS",
    "About",
};

static term_panel_t *detail;

/* ponytail: every screen is static text until tinclib provides the link. */
static void show_detail(int item)
{
    unsigned i;

    term_text_clear(detail);
    switch (item) {
    case 0:
        term_text_append(detail, "Ranked by signal strength at connect "
                                 "time, not by slot order.\n\n");
        for (i = 0; i < TINC_WIFI_SLOTS; i++)
            term_text_appendf(detail, "Slot %u: no link\n", i + 1);
        term_text_append(detail, "\n(needs tinclib)");
        break;
    case 1:
        term_text_append(detail, "Checks Wi-Fi, time sync and one real "
                                 "HTTPS request.\n\nNot available yet: "
                                 "needs tinclib.");
        break;
    case 2:
        term_text_appendf(detail, "GLOBAL SECURITY SETTING\n\nTurns off "
                                  "certificate checks for every app on this "
                                  "board, not just one connection.\n\n"
                                  "Not in protocol %u.%u yet.",
                          TINC_PROTO_MAJOR, TINC_PROTO_MINOR);
        break;
    default:
        term_text_appendf(detail, "TINCLIBC\n\nProtocol %u.%u\ntitrmlib %s",
                          TINC_PROTO_MAJOR, TINC_PROTO_MINOR, TITRM_VERSION);
        break;
    }
}

static bool on_event(term_ctx_t *ctx, const term_event_t *ev, void *state)
{
    (void)state;
    if (ev->type == TERM_EV_CHANGE) {
        show_detail(ev->value);
        return true;
    }
    if (ev->type == TERM_EV_KEY && ev->key == TERM_KEY_CLEAR) {
        term_quit(ctx, HND_CANCELLED);
        return true;
    }
    return false;
}

static uint16_t read_handoff(uint8_t *buf)
{
    uint8_t h = ti_Open(HND_NAME, "r");
    uint16_t len = 0;

    if (h) {
        len = ti_Read(buf, 1, HND_MAX_LEN, h);
        ti_Close(h);
    }
    return len;
}

static void write_handoff(const uint8_t *buf, uint16_t len)
{
    uint8_t h = ti_Open(HND_NAME, "w");

    if (h) {
        ti_Write(buf, 1, len, h);
        ti_Close(h);
    }
}

int main(void)
{
    static uint8_t buf[HND_MAX_LEN];
    static hnd_request_t req;
    uint16_t len = read_handoff(buf);
    bool handoff = hnd_parse(buf, len, &req);
    term_ctx_t *ctx = term_init();
    term_panel_t *body, *menu, *status;
    int result;

    body = term_split(term_root(ctx), TERM_VERTICAL, TERM_FILL);
    status = term_split(term_root(ctx), TERM_VERTICAL, TERM_FIXED(1));

    menu = term_split(body, TERM_HORIZONTAL, TERM_FIXED(18));
    term_panel_set_border(menu, true);
    term_panel_set_title(menu, "TINCLIBC");
    term_make_list(menu, menu_items, sizeof menu_items / sizeof *menu_items);

    detail = term_split(body, TERM_HORIZONTAL, TERM_FILL);
    term_panel_set_border(detail, true);
    term_make_text(detail, "");
    show_detail(0);

    term_panel_set_attr(status, TERM_ATTR_REVERSE);
    term_make_text(status, handoff && req.hint[0] ? req.hint
                                                  : "[clear] quit");

    term_focus(ctx, menu);
    result = term_run(ctx, on_event, NULL);
    term_shutdown(ctx);

    if (handoff) {
        /* No secrets are ever written here: result and detail only. */
        len = hnd_write_result(buf, len, sizeof buf, req.nonce,
                               (uint8_t)result, NULL, 0);
        if (len) {
            write_handoff(buf, len);
            /* Does not return on success. */
            os_RunPrgm(req.return_to, NULL, 0, NULL);
        }
    }
    return 0;
}
