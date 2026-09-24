/*
 * Admin commands (protocol 0x40+) and board status, over tinclib's link.
 * Kept out of tinclib.h on purpose: only TINCLIBC links this. Call after a
 * successful tinc_init().
 *
 * Passwords are write-only: admin_set() sends one and wipes its copy; no
 * command ever reads one back.
 */
#ifndef ADMIN_H
#define ADMIN_H

#include <stdbool.h>
#include <stdint.h>
#include "tinclib.h"

typedef struct {
    uint8_t wifi_state;  /* TINC_WIFI_* */
    uint8_t slot;        /* connected slot, TINC_SLOT_NONE if none */
    int8_t rssi;         /* dBm, while connected */
    uint8_t ip[4];
    bool wifi_locked;    /* the board refuses WIFI_SET/FORGET (ERR_LOCKED) */
    bool time_valid;     /* the board's clock is set, so https can verify certs */
} admin_status_t;

tinc_err_t admin_status(admin_status_t *st);

/* How many Wi-Fi slots the board has (1..TINC_WIFI_SLOTS_MAX), from its
 * HELLO reply. tinclib doesn't keep that reply, so this sends HELLO again;
 * HELLO is idempotent and always executed. */
tinc_err_t admin_slot_count(uint8_t *count);

/* INFO: firmware version and board name, display only (e.g. "1.2.0",
 * "Wemos D1 mini"). Each buffer needs TINC_INFO_STR_MAX + 1 bytes; bytes
 * outside printable ASCII come back as '?'. */
tinc_err_t admin_info(char *fw, char *board);

/* One slot: ssid ("" = empty, needs TINC_SSID_MAX + 1 bytes) and wflags. */
tinc_err_t admin_get(uint8_t slot, char *ssid, uint8_t *wflags);

/* ssid 1..TINC_SSID_MAX chars, pass 0..TINC_PASS_MAX (0 = open network),
 * wflags TINC_WF_*. TINC_ERR_LOCKED while the board has Wi-Fi locked;
 * TINC_ERR_BAD_ARG from the board for a slot it doesn't have. */
tinc_err_t admin_set(uint8_t slot, const char *ssid, const char *pass, uint8_t wflags);
tinc_err_t admin_forget(uint8_t slot);

#endif /* ADMIN_H */
