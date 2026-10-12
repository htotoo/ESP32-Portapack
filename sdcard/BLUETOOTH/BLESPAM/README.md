# BLE Spam custom sets

Copy the `BLUETOOTH/` folder to your PortaPack SD card so the path becomes:

```
<SD root>/BLUETOOTH/BLESPAM/*.txt
```

In the **BLE Spam** app (Utilities), **[ Load from SD ]** lists these files.
Pick one and the app uploads its advertisements to the ESP and switches to
**SD custom** mode; **Start spam** then cycles through them with a random
address each time.

## File format

- One **advertisement per line**, written as **hex** (spaces/colons ignored).
- Max **31 bytes** (62 hex chars) per line — longer lines are truncated.
- Lines starting with `#` or `;` are comments.
- Each line is the raw BLE advertising data (a sequence of `len, type, data…`
  AD structures), exactly what goes on air.
- Up to 24 lines per file.

Build your own sets by adding files here. Examples included:

- `apple.txt` — iOS proximity-pairing popups (device model in bytes 8-9)
- `windows.txt` — Windows "Swift Pair" popups (device name)

Authorized security-awareness / demo use only. See the repo DISCLAIMER.
