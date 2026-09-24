/* TINCLIBC: board configuration (Wi-Fi slots, connection test) and the
 * landing point for app handoffs via the TINCHND appvar. See AGENTS.md. */

#include <stdio.h>
#include <string.h>
#include <fileioc.h>
#include "titrm.h"
#include "tinclib.h"
#include "admin.h"
#include "handoff.h"

#define TINCLIBC_VERSION "0.4.2" /* bump with each v* tag */
#define HND_NAME "TINCHND"
/* hello.txt in this repo: a known body to print back */
#define TEST_URL "https://raw.githubusercontent.com/gavinhsmith/tinclib-config/refs/heads/main/hello.txt"
#define COLOR_GREY 0xB5 /* graphx default palette */
/* Test result marks: code page 437 has no check mark, so its square root. */
#define S_OK "\xFB"
#define S_FAIL "x"

enum { M_STATUS, M_TEST, M_TLS, M_ABOUT };
static const char *const menu_items[] = {
    "Status", "Test connection", "Insecure TLS", "About",
};
static const char *const slot_actions[] = { "Set network", "Forget", "Back" };

static term_ctx_t *ctx;
static term_panel_t *menu, *slots, *detail, *credit, *status;
static term_panel_t *dlg, *dlg_list, *f_ssid_p, *f_pass_p, *f_hint, *chk_hidden,
                    *save_btn, *ok_btn;

static tinc_err_t link_err = TINC_ERR_NOT_INIT;
static admin_status_t st;
/* The board says how many slots it has (up to 254); the list shows
 * SLOTS_VISIBLE at a time and scrolls (titrmlib adds a scrollbar). */
#define SLOTS_VISIBLE 5
static uint8_t n_slots;
static char slot_rows[TINC_WIFI_SLOTS_MAX][TINC_SSID_MAX + 24];
static const char *slot_items[TINC_WIFI_SLOTS_MAX];
static char slots_title[40];
static uint8_t cur_slot;
static bool started, testing, test_ok;
static char msg[64];
static char test_body[64];
static uint8_t test_len;

/* ---- Text fields --------------------------------------------------------
 * titrmlib's input only types upper case and holds 48 chars; SSIDs and
 * passwords are case-sensitive and up to 32/64. [mode] toggles lower case. */

typedef struct {
    char buf[TINC_PASS_MAX + 1];
    uint8_t len, max;
    bool mask;
} field_t;

static field_t f_ssid = { "", 0, TINC_SSID_MAX, false };
static field_t f_pass = { "", 0, TINC_PASS_MAX, true };
static bool lower = true;

static void field_draw(term_panel_t *p, const field_t *f)
{
    term_panel_clear(p);
    if (f->mask)
        term_panel_repeat(p, '*', f->len);
    else
        term_panel_print(p, f->buf);
    if (term_focused(ctx) == p)
        term_panel_putc(p, '_');
}

static void form_draw(void)
{
    field_draw(f_ssid_p, &f_ssid);
    field_draw(f_pass_p, &f_pass);
    term_panel_clear(f_hint);
    term_panel_printf(f_hint, "[mode] %s  [enter] next", lower ? "abc" : "ABC");
}

static bool field_keys(term_panel_t *p, const term_event_t *ev, void *state)
{
    field_t *f = state;

    if (ev->key == TERM_KEY_CHAR) {
        char c = ev->ch;
        if (lower && c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        if (f->len < f->max) {
            f->buf[f->len++] = c;
            f->buf[f->len] = '\0';
        }
    } else if (ev->key == TERM_KEY_DEL) {
        if (f->len)
            f->buf[--f->len] = '\0';
    } else if (ev->key == TERM_KEY_MODE) {
        lower = !lower;
    } else {
        return false;
    }
    (void)p;
    form_draw();
    return true;
}

static void wipe_fields(void)
{
    memset(&f_pass.buf, 0, sizeof f_pass.buf);
    f_pass.len = 0;
    memset(&f_ssid.buf, 0, sizeof f_ssid.buf);
    f_ssid.len = 0;
}

/* ---- Board state ------------------------------------------------------- */

static const char *bars(int8_t rssi)
{
    return rssi >= -60 ? TERM_S_SIG3 : rssi >= -70 ? TERM_S_SIG2
         : rssi >= -80 ? TERM_S_SIG1 : TERM_S_SIG0;
}

static void set_status(const char *text)
{
    term_panel_clear(status);
    term_panel_print(status, text);
}

static void show_detail(int item);

static void refresh(void)
{
    char ssid[TINC_SSID_MAX + 1];
    uint8_t i, wflags;

    if (link_err != TINC_OK)
        link_err = tinc_init(&(tinc_config_t){ "TINCLIBC", 0, 0 });
    if (link_err == TINC_OK)
        link_err = admin_status(&st);
    if (link_err == TINC_OK)
        link_err = admin_slot_count(&n_slots);

    for (i = 0; link_err == TINC_OK && i < n_slots; i++) {
        bool on = st.wifi_state == TINC_WIFI_CONNECTED && st.slot == i;
        const char *hid;

        if ((link_err = admin_get(i, ssid, &wflags)) != TINC_OK)
            break;
        hid = wflags & TINC_WF_HIDDEN ? " (hidden)" : "";
        if (!ssid[0])
            snprintf(slot_rows[i], sizeof slot_rows[i], "%u (empty)", i + 1);
        else if (on)
            snprintf(slot_rows[i], sizeof slot_rows[i], "%u %s%s %s%d", i + 1,
                     ssid, hid, bars(st.rssi), st.rssi);
        else
            snprintf(slot_rows[i], sizeof slot_rows[i], "%u %s%s", i + 1, ssid, hid);
        slot_items[i] = slot_rows[i];
    }
    if (link_err != TINC_OK) {
        n_slots = 0;
        st.wifi_locked = false;
    }

    /* Locked on the board: the list stays readable but greyed out. */
    term_panel_set_colors(slots, st.wifi_locked ? COLOR_GREY : TERM_COLOR_WHITE,
                          TERM_COLOR_BLACK);
    snprintf(slots_title, sizeof slots_title, "Saved networks (%u)%s", n_slots,
             st.wifi_locked ? " locked" : "");
    term_panel_set_title(slots, slots_title);
    term_list_set_items(slots, slot_items, n_slots);
    show_detail(term_list_selected(menu));
}

static const char *wifi_name(uint8_t s)
{
    switch (s) {
    case TINC_WIFI_NO_CREDS:   return "no saved networks";
    case TINC_WIFI_CONNECTING: return "connecting";
    case TINC_WIFI_CONNECTED:  return "connected";
    default:                   return "can't connect";
    }
}

static void show_detail(int item)
{
    term_text_clear(detail);
    term_panel_show(credit, item == M_ABOUT);
    switch (item) {
    case M_STATUS:
        if (link_err == TINC_ERR_VERSION) {
            term_text_appendf(detail, "The board's firmware speaks a different "
                              "protocol. Update it to protocol %u.%u, then "
                              "[enter] to retry.", TINC_PROTO_MAJOR, TINC_PROTO_MINOR);
            break;
        }
        if (link_err != TINC_OK) {
            term_text_appendf(detail, "No board: %s.\n\nPlug in the ESP board, "
                              "then [enter] to retry.", tinc_errString(link_err));
            break;
        }
        term_text_appendf(detail, "Wi-Fi: %s\n", wifi_name(st.wifi_state));
        term_text_appendf(detail, "Clock: %s\n", st.time_valid ? "set" : "not set");
        if (st.wifi_locked)
            term_text_append(detail, "Settings: locked by the board\n");
        if (st.wifi_state == TINC_WIFI_CONNECTED)
            term_text_appendf(detail, "Slot %u  %s %d dBm\nIP %u.%u.%u.%u\n",
                              st.slot + 1, bars(st.rssi), st.rssi,
                              st.ip[0], st.ip[1], st.ip[2], st.ip[3]);
        term_text_append(detail, "\nThe board joins the saved network with "
                         "the strongest signal, not the lowest slot.\n\n"
                         "[enter] refresh");
        break;
    case M_TEST:
        term_text_append(detail, "Checks Wi-Fi, the board's clock and one "
                         "real HTTPS request.\n\n"
                         "[enter] run");
        break;
    case M_TLS:
        term_text_appendf(detail, "GLOBAL SECURITY SETTING\n\nTurns off "
                          "certificate checks for every app on this board, not "
                          "just one connection.\n\nNot in protocol %u.%u yet.",
                          TINC_PROTO_MAJOR, TINC_PROTO_MINOR);
        break;
    default:
        /* TODO: firmware version and board name once the protocol reports them */
        term_text_appendf(detail, "tinclib config v" TINCLIBC_VERSION "\n\n"
                          "Protocol v%u.%u\ntinclib v%s\n"
                          "Firmware v0.0.0 (dummy board)\ntitrmlib v%s",
                          TINC_PROTO_MAJOR, TINC_PROTO_MINOR,
                          TINC_VERSION, TITRM_VERSION);
        break;
    }
}

/* ---- Connection test (driven by ticks) --------------------------------- */

static const char *tls_reason(uint8_t r)
{
    switch (r) {
    case TINC_TLSR_VERSION:       return "no common TLS version";
    case TINC_TLSR_CIPHER:        return "no common cipher";
    case TINC_TLSR_ALERT:         return "server refused";
    case TINC_TLSR_PROTO:         return "bad handshake";
    case TINC_TLSR_EXPIRED:       return "certificate expired";
    case TINC_TLSR_NOT_YET_VALID: return "certificate not valid yet";
    case TINC_TLSR_HOSTNAME:      return "certificate for another host";
    case TINC_TLSR_UNTRUSTED:     return "no trusted root on the board";
    case TINC_TLSR_BAD_CHAIN:     return "bad certificate chain";
    default:                      return "unknown";
    }
}

static void test_start(void)
{
    static const tinc_request_t req = { TINC_GET, TEST_URL, NULL, NULL, 0 };
    tinc_err_t e;

    term_text_clear(detail);
    test_ok = false;
    test_len = 0;
    if (link_err != TINC_OK) {
        term_text_append(detail, "No board.");
        return;
    }
    if (!tinc_isActive(TINC_WIFI)) {
        term_text_append(detail, "Wi-Fi: " S_FAIL " not joined");
        return;
    }
    admin_status(&st);
    term_text_appendf(detail, "Wi-Fi: " S_OK " joined, %s %d dBm\n",
                      bars(st.rssi), st.rssi);
    /* Not fatal: the board waits for its clock in the TLS phase (ERR_TIME). */
    term_text_append(detail, st.time_valid ? "Clock: " S_OK " set\n"
                                           : "Clock: not set yet\n");
    if ((e = tinc_request(&req)) != TINC_OK) {
        term_text_appendf(detail, "HTTPS: " S_FAIL " %s", tinc_errString(e));
        return;
    }
    term_text_append(detail, "GET " TEST_URL "...\n");
    testing = true;
    term_set_tick(ctx, 100);
}

static void test_tick(void)
{
    static uint8_t sink[64];
    tinc_state_t s = tinc_poll();
    int16_t n;

    /* Keep the start of the body to print; drop the rest. */
    while ((n = tinc_read(sink, sizeof sink)) > 0) {
        if (n > (int16_t)(sizeof test_body - 1 - test_len))
            n = sizeof test_body - 1 - test_len;
        memcpy(test_body + test_len, sink, n);
        test_len += n;
    }
    if (s != TINC_DONE && s != TINC_ERROR)
        return;
    testing = false;
    term_set_tick(ctx, 0);
    if (s == TINC_DONE) {
        test_ok = true;
        term_text_appendf(detail, "HTTPS: " S_OK " %u\n", tinc_httpStatus());
        if (tinc_httpStatus() == 200) {
            test_body[test_len] = '\0';
            term_text_appendf(detail, "\n%s", test_body);
        }
    } else {
        tinc_err_t e = tinc_error();

        term_text_appendf(detail, "HTTPS: " S_FAIL " %s\n", tinc_errString(e));
        if (e == TINC_ERR_TLS || e == TINC_ERR_CERT)
            term_text_appendf(detail, "Reason: %s\n", tls_reason(tinc_errDetail()));
    }
}

/* ---- Slot dialogs ------------------------------------------------------ */

static void close_dlg(void)
{
    if (dlg) {
        term_overlay_close(dlg);
        dlg = NULL;
        dlg_list = f_ssid_p = f_pass_p = chk_hidden = save_btn = ok_btn = NULL;
        wipe_fields();
    }
    term_focus(ctx, slots);
}

/* The board has Wi-Fi locked (firmware build flag or switch, e.g. the PC
 * build, which only uses the PC's connection); nothing here can unlock it. */
static void open_locked_notice(void)
{
    term_panel_t *text;

    dlg = term_overlay_open_centered(ctx, 36, 9);
    term_panel_set_border(dlg, true);
    term_panel_set_title(dlg, "Wi-Fi locked");
    text = term_split(dlg, TERM_VERTICAL, TERM_FILL);
    term_make_text(text, "This board doesn't allow changing its Wi-Fi "
                   "networks. It's set on the board itself (firmware or a "
                   "switch), not here.");
    ok_btn = term_split(dlg, TERM_VERTICAL, TERM_FIXED(1));
    term_make_button(ok_btn, "OK");
    term_focus(ctx, ok_btn);
}

static void open_actions(uint8_t slot)
{
    cur_slot = slot;
    dlg = term_overlay_open_centered(ctx, 22, 5);
    term_panel_set_border(dlg, true);
    term_panel_set_title(dlg, slot_rows[slot]);
    dlg_list = term_split(dlg, TERM_VERTICAL, TERM_FILL);
    term_make_list(dlg_list, slot_actions, 3);
    term_focus(ctx, dlg_list);
}

static term_panel_t *label(term_panel_t *parent, const char *text)
{
    term_panel_t *p = term_split(parent, TERM_VERTICAL, TERM_FIXED(1));
    term_make_text(p, text);
    return p;
}

static term_panel_t *field(term_panel_t *parent, field_t *f)
{
    term_panel_t *p = term_split(parent, TERM_VERTICAL, TERM_FIXED(1));
    term_panel_set_focusable(p, true);
    term_panel_set_submit(p, true);
    term_panel_set_keys(p, field_keys, f);
    return p;
}

static void open_form(void)
{
    char title[16];

    term_overlay_close(dlg);
    wipe_fields();
    dlg = term_overlay_open_centered(ctx, 40, 10);
    term_panel_set_border(dlg, true);
    snprintf(title, sizeof title, "Slot %u", cur_slot + 1);
    term_panel_set_title(dlg, title);
    label(dlg, "Network name (SSID):");
    f_ssid_p = field(dlg, &f_ssid);
    label(dlg, "Password (empty = open):");
    f_pass_p = field(dlg, &f_pass);
    chk_hidden = term_split(dlg, TERM_VERTICAL, TERM_FIXED(1));
    term_make_checkbox(chk_hidden, "Hidden network (not broadcast)", false);
    f_hint = label(dlg, "");
    save_btn = term_split(dlg, TERM_VERTICAL, TERM_FIXED(1));
    term_make_button(save_btn, "Save");
    term_focus(ctx, f_ssid_p);
    form_draw();
}

static void form_move(int dir)
{
    term_panel_t *order[] = { f_ssid_p, f_pass_p, chk_hidden, save_btn };
    term_panel_t *cur = term_focused(ctx);
    int i;

    for (i = 0; i < 4 && order[i] != cur; i++)
        ;
    i = (i + dir + 4) % 4;
    term_focus(ctx, order[i]);
    form_draw();
}

static void form_save(void)
{
    tinc_err_t e;

    if (!f_ssid.len) {
        set_status("Enter a network name first");
        return;
    }
    /* Keypad -> admin command -> board. Never stored, never shown again. */
    e = admin_set(cur_slot, f_ssid.buf, f_pass.buf,
                  term_checkbox_checked(chk_hidden) ? TINC_WF_HIDDEN : 0);
    close_dlg();
    snprintf(msg, sizeof msg, e == TINC_OK ? "Saved slot %u; joining..."
             : "Slot %u not saved: %s", cur_slot + 1, tinc_errString(e));
    set_status(msg);
    refresh();
}

/* ---- Events ------------------------------------------------------------ */

static bool on_event(term_ctx_t *c, const term_event_t *ev, void *state)
{
    term_panel_t *src = ev->panel;
    (void)state;

    switch (ev->type) {
    case TERM_EV_TICK:
        if (!started) {
            started = true;
            term_set_tick(c, 0);
            refresh();
        } else if (testing) {
            test_tick();
        }
        return true;
    case TERM_EV_CHANGE:
        if (src == menu && !testing)
            show_detail(ev->value);
        return true;
    case TERM_EV_SUBMIT:
        if (testing)
            return true;
        if (src == menu && ev->value == M_STATUS) {
            refresh();
        } else if (src == menu && ev->value == M_TEST) {
            test_start();
        } else if (src == slots) {
            if (link_err == TINC_OK && st.wifi_locked)
                open_locked_notice();
            else if (link_err == TINC_OK)
                open_actions((uint8_t)ev->value);
        } else if (src == ok_btn) {
            close_dlg();
        } else if (src == dlg_list) {
            if (ev->value == 0) {
                open_form();
            } else if (ev->value == 1) {
                tinc_err_t e = admin_forget(cur_slot);
                close_dlg();
                if (e != TINC_OK)
                    set_status(tinc_errString(e));
                refresh();
            } else {
                close_dlg();
            }
        } else if (src == f_ssid_p || src == f_pass_p) {
            form_move(1);
        } else if (src == save_btn) {
            form_save();
        }
        return true;
    case TERM_EV_KEY:
        if (dlg && ev->key == TERM_KEY_CLEAR) {
            close_dlg();
        } else if (dlg && f_ssid_p && (ev->key == TERM_KEY_UP || ev->key == TERM_KEY_DOWN)) {
            form_move(ev->key == TERM_KEY_UP ? -1 : 1);
        } else if (dlg) {
            return false;
        } else if (ev->key == TERM_KEY_CLEAR) {
            if (testing)
                tinc_abort();
            term_quit(c, 0);
        } else if (ev->key == TERM_KEY_RIGHT && term_focused(c) == menu) {
            term_focus(c, slots);
        } else if (ev->key == TERM_KEY_LEFT && term_focused(c) == slots) {
            term_focus(c, menu);
        } else {
            return false;
        }
        return true;
    default:
        return false;
    }
}

/* ---- Handoff and main -------------------------------------------------- */

static uint16_t read_handoff(uint8_t *buf)
{
    uint8_t h = ti_Open(HND_NAME, "r");
    uint16_t len = 0;

    if (h) {
        len = (uint16_t)ti_Read(buf, 1, HND_MAX_LEN, h);
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

/* What the app hears back: did it get what it came for? */
static uint8_t handoff_result(const hnd_request_t *req)
{
    if (link_err != TINC_OK)
        return HND_FAILED;
    if (req->action == HND_TEST_CONN)
        return test_ok ? HND_OK : HND_CANCELLED;
    return admin_status(&st) == TINC_OK && st.wifi_state == TINC_WIFI_CONNECTED
           ? HND_OK : HND_CANCELLED;
}

int main(void)
{
    static uint8_t buf[HND_MAX_LEN];
    static hnd_request_t req;
    uint16_t len = read_handoff(buf);
    bool handoff = hnd_parse(buf, len, &req);
    term_panel_t *body, *right, *box, *row, *heart;

    ctx = term_init();
    body = term_split(term_root(ctx), TERM_VERTICAL, TERM_FILL);
    status = term_split(term_root(ctx), TERM_VERTICAL, TERM_FIXED(1));

    menu = term_split(body, TERM_HORIZONTAL, TERM_FIXED(19));
    term_panel_set_border(menu, true);
    term_panel_set_title(menu, "TINCLIBC");
    term_make_list(menu, menu_items, sizeof menu_items / sizeof *menu_items);

    right = term_split(body, TERM_HORIZONTAL, TERM_FILL);
    slots = term_split(right, TERM_VERTICAL, TERM_FIXED(SLOTS_VISIBLE + 2));
    term_panel_set_border(slots, true);
    term_panel_set_title(slots, "Saved networks");
    term_make_list(slots, slot_items, 0);
    box = term_split(right, TERM_VERTICAL, TERM_FILL);
    term_panel_set_border(box, true);
    detail = term_split(box, TERM_VERTICAL, TERM_FILL);
    term_make_text(detail, "Connecting to the board...");
    /* About's credit: its own panels, since a text widget has one color and
     * the heart (code page 437 0x03) is red. Shown by show_detail(). */
    credit = term_split(box, TERM_VERTICAL, TERM_FIXED(2));
    row = term_split(credit, TERM_VERTICAL, TERM_FIXED(1));
    term_make_text(term_split(row, TERM_HORIZONTAL, TERM_FIXED(8)), "Made w/ ");
    heart = term_split(row, TERM_HORIZONTAL, TERM_FIXED(1));
    term_panel_set_colors(heart, TERM_COLOR_RED, TERM_COLOR_BLACK);
    term_make_text(heart, "\x03");
    term_make_text(term_split(row, TERM_HORIZONTAL, TERM_FILL), " by");
    term_make_text(term_split(credit, TERM_VERTICAL, TERM_FIXED(1)),
                   "github.com/gavinhsmith");
    term_panel_show(credit, false);

    term_panel_set_attr(status, TERM_ATTR_REVERSE);
    term_make_text(status, handoff && req.hint[0] ? req.hint
                   : "[clear] quit  [<] [>] switch pane");
    if (handoff && req.action == HND_TEST_CONN)
        term_list_select(menu, M_TEST);

    term_focus(ctx, menu);
    term_set_tick(ctx, 50); /* first tick connects, after the first frame */
    term_run(ctx, on_event, NULL);
    term_shutdown(ctx);

    if (handoff) {
        /* Result only: no secrets are ever written here. The app comes back
         * through its own os_RunPrgm callback once we exit. */
        len = hnd_write_result(buf, len, sizeof buf, req.nonce,
                               handoff_result(&req),
                               link_err != TINC_OK ? &link_err : NULL,
                               link_err != TINC_OK ? 1 : 0);
        if (len)
            write_handoff(buf, len);
    }
    tinc_shutdown();
    return 0;
}
