/* Handoff interop: tinclib's real tinc_openConfig()/tinc_takeSetupResult()
 * (lib/tinclib/src/tinc_config.c) against this repo's src/handoff.c, on
 * tinclib's host stubs (in-memory appvar, fake os_RunPrgm). A layout
 * mismatch between the two repos fails here instead of silently on-calc. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "fake_esp.h"
#include "admin.h"
#include "handoff.h"
#include "tinc_internal.h"

static jmp_buf back;
static char launched[16];

static void run_prgm(const char *name)
{
    strncpy(launched, name, sizeof launched - 1);
    longjmp(back, 1); /* os_RunPrgm doesn't return */
}

/* The app side: hand off, as MYAPP. */
static void open_config(const char *hint)
{
    launched[0] = '\0';
    if (!setjmp(back))
        tinc_openConfig(hint);
    assert(!strcmp(launched, "TINCLIBC"));
}

int main(void)
{
    hnd_request_t r;
    uint16_t n;

    fake_reset();
    fake_run_prgm = run_prgm;
    tinc_g.cfg.appName = "MYAPP";

    /* tinclib's request parses here */
    open_config("Need Wi-Fi");
    assert(hnd_parse(fake_appvar, fake_appvar_len, &r));
    assert(!strcmp(r.return_to, "MYAPP"));
    assert(!strcmp(r.hint, "Need Wi-Fi"));
    assert(r.action == HND_SETUP_WIFI && (r.requirements & HND_NEEDS_WIFI));

    /* our result is what tinclib reports, exactly once */
    n = hnd_write_result(fake_appvar, fake_appvar_len, sizeof fake_appvar,
                         r.nonce, HND_CANCELLED, NULL, 0);
    assert(n);
    fake_appvar_len = n;
    assert(tinc_takeSetupResult() == TINC_ERR_SETUP_CANCELLED);
    assert(tinc_takeSetupResult() == TINC_OK);

    open_config("");
    assert(hnd_parse(fake_appvar, fake_appvar_len, &r));
    fake_appvar_len = hnd_write_result(fake_appvar, fake_appvar_len,
                                       sizeof fake_appvar, r.nonce, HND_FAILED,
                                       (const uint8_t *)"\x80", 1);
    assert(tinc_takeSetupResult() == TINC_ERR_SETUP_FAILED);

    open_config("x");
    assert(hnd_parse(fake_appvar, fake_appvar_len, &r));
    fake_appvar_len = hnd_write_result(fake_appvar, fake_appvar_len,
                                       sizeof fake_appvar, r.nonce, HND_OK, NULL, 0);
    assert(tinc_takeSetupResult() == TINC_OK);

    /* abandoned (TINCLIBC never answered): a stale request, dropped */
    open_config("stale");
    assert(tinc_takeSetupResult() == TINC_OK);
    assert(!fake_appvar_exists);

    /* the maximum hint tinclib writes still parses */
    open_config("0123456789012345678901234567890123456789012345678901234567890123456789");
    assert(hnd_parse(fake_appvar, fake_appvar_len, &r));
    assert(strlen(r.hint) == HND_HINT_MAX);

    /* admin.c's own HELLO works over tinclib's real link: the fake board
     * reports 3 slots, and the link stays usable afterwards */
    {
        admin_status_t st;
        uint8_t n = 0;

        fake_reset();
        tinc_g.cfg.appName = NULL;
        assert(tinc_init(NULL) == TINC_OK);
        assert(admin_slot_count(&n) == TINC_OK && n == 3);
        assert(admin_status(&st) == TINC_OK && st.wifi_state == TINC_WIFI_CONNECTED);
        assert(!st.wifi_locked);
        fake.wifi_locked = true;
        assert(admin_status(&st) == TINC_OK && st.wifi_locked);
        tinc_shutdown();
    }

    puts("test_interop: all passed");
    return 0;
}
