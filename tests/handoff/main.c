/* THANDOFF: a tinclib app that hands off to the real TINCLIBC.
 *
 *   1st run: tinc_openConfig() starts TINCLIBC and doesn't return.
 *   2nd run: main() again via tinclib's os_RunPrgm callback, after TINCLIBC
 *            exited; tinc_init() reports TINCLIBC's answer. In CEmu there's
 *            no board, so TINCLIBC answers FAILED.
 *
 * The autotest hashes the "BACK: ..." screen. */
#include <ti/getcsc.h>
#include <ti/screen.h>
#include "tinclib.h"

int main(void)
{
    static const tinc_config_t cfg = { "THANDOFF", 0, 0 };
    tinc_err_t err = tinc_init(&cfg);
    const char *text = "NO CONFIG APP";

    if (err == TINC_ERR_SETUP_FAILED)
        text = "BACK: FAILED";
    else if (err == TINC_ERR_SETUP_CANCELLED)
        text = "BACK: CANCELLED";
    else if (err == TINC_OK)
        text = "BACK: OK";
    else
        tinc_openConfig("Handoff test");  /* returns only on failure */
    tinc_shutdown();

    os_ClrHome();
    os_PutStrFull(text);
    while (!os_GetCSC());
    return 0;
}
