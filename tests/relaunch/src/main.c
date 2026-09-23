/* TSTRET: relaunch target for the os_RunPrgm spike. TINCLIBC chains here
 * after a handoff; the autotest hashes this screen, then checks the result
 * TINCLIBC left in TINCHND for the fixture's nonce. */
#include <fileioc.h>
#include <ti/getcsc.h>
#include <ti/screen.h>
#include "handoff.h"

int main(void)
{
    static uint8_t buf[HND_MAX_LEN];
    uint8_t h = ti_Open("TINCHND", "r");
    uint16_t len = 0;

    if (h) {
        len = ti_Read(buf, 1, sizeof buf, h);
        ti_Close(h);
    }
    os_ClrHome();
    os_PutStrFull(hnd_result_for(buf, len, 0x1234ABCD) == HND_CANCELLED
                  ? "RETURNED CANCELLED" : "RETURNED BAD RESULT");
    while (!os_GetCSC());
    return 0;
}
