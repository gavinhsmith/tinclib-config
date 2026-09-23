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

#include <stdint.h>
#include "tinclib.h"

typedef struct {
    uint8_t wifi_state;  /* TINC_WIFI_* */
    uint8_t slot;        /* connected slot, TINC_SLOT_NONE if none */
    int8_t rssi;         /* dBm, while connected */
    uint8_t ip[4];
} admin_status_t;

typedef char admin_ssids_t[TINC_WIFI_SLOTS][TINC_SSID_MAX + 1]; /* "" = empty slot */

tinc_err_t admin_status(admin_status_t *st);
tinc_err_t admin_list(admin_ssids_t ssids);
/* ssid 1..TINC_SSID_MAX chars, pass 0..TINC_PASS_MAX (0 = open network). */
tinc_err_t admin_set(uint8_t slot, const char *ssid, const char *pass);
tinc_err_t admin_forget(uint8_t slot);

#endif /* ADMIN_H */
