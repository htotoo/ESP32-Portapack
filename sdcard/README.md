# SD card content for the WiFi apps

Copy the `WIFI/` folder to your PortaPack SD card root. The apps use this
layout:

```
<SD root>/WIFI/EvilPortal/html/*.html   <- custom portal pages (you provide)
<SD root>/WIFI/EvilPortal/clients.txt   <- saved by the app
<SD root>/WIFI/EvilPortal/captured.txt  <- saved by the app
<SD root>/WIFI/ProbeSniffer/probes.csv  <- saved by the app
```

(The WiFi Analyzer app writes nothing to the SD card.)

## EvilPortal custom pages

Any `.html` file in `WIFI/EvilPortal/html/` shows up in the EvilPortal app's
**Template** selector (Utilities → EvilPortal). Selecting one (before or while
the portal runs) uploads it to the ESP32 module over I2C and serves it as the
captive portal page.

### Rules for a portal page (important)

1. **Keep it under ~8 KB.** The ESP holds the page in a fixed RAM buffer. A
   larger file is **truncated**, so the browser gets a half page and renders
   **blank**. A full, real cloned login page (often 100 KB+) will not work —
   and does not need to. The app shows "Too big >8KB - truncated!" when this
   happens.
2. **Make it fully self-contained.** Inline all CSS; no external `<link>`,
   `<script src>`, or images. Captive-portal mini-browsers (Apple/Android CNA)
   block external resources, so a page that depends on them shows blank.
3. **Use these form field names** so credentials are captured:
   - username field: `email`, `user`, `username`, `login`, `identity` or `usr`
   - password field: `password`, `pass`, `pwd`, `passwd` or `pw`
   - post the form to `action="/login"` (any path works; `/login` is clearest).

The example pages here (`google.html`, `microsoft.html`, `wifi.html`) are all
compact, self-contained lookalikes (~1.5 KB each) — copy one and adjust the
branding/text.

For authorized security-awareness / pentest use only.
