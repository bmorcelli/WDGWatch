# WDGWatch (aka PipBoy-3000) — T-Watch Ultra Firmware

Custom firmware for **LilyGO T-Watch Ultra** (ESP32-S3, 410×502 AMOLED) turning
it into a Pip-Boy-styled multi-role device and wearable companion for the
**Watch Dogs Go** uConsole game.

- Standalone smartwatch (time, battery, GPS, compass, sensors)
- Security research tool (WiFi recon, BLE scan with AirTag detection, NFC reader, LoRa MeshCore)
- Wearable companion for the **Watch Dogs Go** uConsole game (BLE NUS link)
- Remote-controllable from phone via built-in SoftAP + web UI on `192.168.4.1`

Codename / SoftAP name stays **`PipBoy-3000`** (password `pip12345`). Project
name **WDGWatch** — Watch Dogs Go Watch.

Theme colour: `#00E5FF` cyan (matches [WatchDogsGo portal](https://locosp.github.io/WatchDogsGo/)).

---

## Feature Status

### Working

| Feature | Watch UI | Web UI | BLE API |
|---|---|---|---|
| Watchface + time/date/NTP | yes | n/a | `status` |
| Haptic feedback | yes | yes | `haptic` |
| Brightness control | — | yes (slider) | `brightness` |
| Battery / charging | yes | yes | included in `status` |
| GPS on/off + fix | yes | yes | `gps_on/off`, `status` |
| Compass (BHI260 + manual calibration) | yes | — | `compass` |
| WiFi 2.4G scan | — | yes (WiFi tab + RECON tab) | `recon_wifi` |
| BLE scan (+ AirTag detect) | yes (RECON app) | yes (RECON tab) | `recon_ble` |
| Deauth targeted / blackout | yes | yes | `recon_deauth`, `deauth_all` |
| Packet sniffer + deauth detector | yes | yes | `sniffer_start`, `deauth_detect` |
| Evil twin AP | yes | yes | `evil_twin` |
| NFC read (ISO-14443A, NDEF) | yes | yes | event push |
| NFC save to SD + Flipper `.nfc` export | yes | yes | `nfc_save`, `nfc_export` |
| LoRa MeshCore RX/TX on public channel | yes | yes | `lora_send`, `lora_advert` |
| MeshCore advert signing (Orlp Ed25519) | auto | — | — |
| Message history on SD (20 msgs) | yes | yes (HTTP `/api/lora/history`) | — |
| WatchDogs Connect (skull overlay, dimmed) | yes | — | on connect |
| Unified JSON command API | — | `/api/cmd` | Nordic UART |

### Unstable / known limitations

- **NFC card emulation** — ST25R3916 chip supports it but LilyGO antenna design is
  reader-only; tag emulation code is present but does not transmit. Do not use.
- **Evil Twin captive portal** — AP starts but there is no captive portal HTML
  yet. Credentials not captured.
- **LoRa range at SF8 BW62.5k** — tuned for MeshCore compatibility, not long-range.
  Typical urban range ~200 m LOS.
- **Compass drift** — uses `GAME_ROTATION_VECTOR` (no magnetometer fusion); manual
  one-time north calibration required, saved in NVS.
- **BLE scan + WiFi AP concurrency** — stable in short bursts; don't run BLE scan
  while heavy Web UI traffic is going.
- **PIN pairing on MacOS** — macOS auto-prompts, but on some versions the prompt
  appears on Bluetooth preference pane, not inline.

---

## Hardware

- LilyGO T-Watch Ultra (ESP32-S3, 8 MB PSRAM, 16 MB flash)
- Touch AMOLED 410×502
- ST25R3916 NFC
- SX1262 LoRa @ 869.618 MHz
- BHI260AP IMU (+ compass)
- GPS module
- DRV2605 haptic driver
- Li-ion battery + charger via AXP2101 PMU

---

## Build & flash

```bash
git clone https://github.com/LOCOSP/WDGWatch.git
cd WDGWatch
pio run --target upload          # builds + flashes via USB
pio device monitor --baud 115200
```

Default serial device on macOS: `/dev/cu.usbmodem101`. On Linux typically
`/dev/ttyACM0`.

If the watch boots into ROM download mode (rst reason `0x15` + `boot:0x23`),
press the side button (RST) or re-run `pio run --target upload`.

---

## Flow: enable Web Interface from phone

1. On the watch, navigate to the app menu (swipe from watchface).
2. Open **WiFi** app.
3. Tap **WEB SERVER** button → watch starts SoftAP `PipBoy-3000` with WPA2
   password `pip12345`.
4. Status bar shows `WEB SERVER ON` + AP IP (always `192.168.4.1`).
5. On phone, connect to WiFi `PipBoy-3000` (ignore "no internet" warning).
6. Open browser → `http://192.168.4.1`.
7. Tabs: **DASH / NFC / LORA / WiFi / RECON / SET**.
8. To turn off: return to WiFi app on watch, tap button again.

The web server survives backgrounding; NTP, GPS, NFC and LoRa keep working
while web UI is up.

---

## Flow: calibrate the compass

The T-Watch Ultra IMU (BHI260AP) does **not** include a magnetometer fusion
mode that survives the watch sleeping/rebooting. The compass uses
`GAME_ROTATION_VECTOR` (gyro + accel only), so on every cold boot the heading
is **relative to whatever orientation the watch happened to wake up in** — it
will drift until you tell it where north is.

Calibration is a one-shot offset stored in NVS (namespace `compass`, key
`offset`). It survives reboots until you do `pio run -t erase` or re-calibrate.
Drift over a few hours of normal use is small but noticeable; recalibrate when
the heading no longer matches reality.

**How to calibrate:**

1. Open the **Sensor** app on the watch (menu → Sensor).
2. Get a reference compass:
   - iPhone built-in **Compass** app (works great), or
   - Any Android compass app, or
   - A real magnetic compass.
3. Lay both devices flat next to each other on a non-metal surface, screens
   up, **pointing the same direction**. Avoid laptops, speakers, anything with
   a magnet or thick steel underneath.
4. Slowly rotate both together until the **reference** reads **0° / N**
   (north). Hold still for a second to let the gyro settle.
5. On the watch, tap the **"POINT N + CALIBRATE"** button at the bottom of
   the Sensor screen.
6. The button hides itself — calibration saved. The watch dial now reads ~0°
   when pointed north.
7. Rotate the watch and verify the heading tracks (E should read ~90°, S
   ~180°, W ~270°).

**To recalibrate later:** the button is hidden after first calibration. Reset
NVS to bring it back: hold the side button to reboot, or rebuild + flash with
`pio run -t erase -t upload`. Easier alternative: add a "force recalibrate"
gesture later (TODO).

**Tip:** calibrate near a window or outdoors — phone compasses (the reference)
are themselves easily fooled by buildings' steel framing. If iPhone keeps
asking you to "wave it in a figure-8", do that first, then calibrate the
watch against the freshly-calibrated phone.

---

## Flow: pair with Watch Dogs Go game (uConsole, BLE)

**First connection (PIN required):**

1. On the watch, open **WiFi** app → tap **WATCH DOGS CONNECT** (BLE toggle).
2. Watch advertises as `PipBoy-xxxxx` (unique 5-char suffix from MAC), prints
   `[BLE] Advertising` and `[BLE] PIN: 123456` on serial.
3. In the game on uConsole, start pairing / scan.
4. When the game attempts to subscribe or write to the NUS service, it gets
   **Insufficient Authentication (0x05)**. BlueZ then requests a passkey.
5. Watch shows a full-screen **BLE PAIRING** overlay with huge 48px PIN digits
   (6 digits, max brightness).
6. User reads PIN, enters it on the uConsole.
7. Bond is stored on both sides. Watch serial: `[BLE] Auth OK - encrypted`.
8. Watch switches to **WATCH_DOGS** overlay (skull + "L I N K E D" + clock,
   brightness dimmed to 20).

**Subsequent connections:**

Silent — bond is reused. Skull overlay appears immediately after the game
connects. Disconnect (range, crash, deliberate) hides the overlay and restores
default brightness.

**Bond reset:** `pio run -t erase` on the watch, or `bluetoothctl remove <MAC>`
on uConsole — either side triggers fresh pairing on next connection.

Detailed integration recipe for the game developer: see
[`BLE_PAIRING_CHANGE.md`](BLE_PAIRING_CHANGE.md) (BlueZ Agent example,
bluetoothctl pre-pair workflow, IO capability requirements).

---

## Flow: send LoRa MeshCore message

1. On watch LoRa app or web UI LORA tab: tap **START RX**.
2. Radio powers up, tunes to 869.618 MHz / SF8 / BW62.5 / CR5 / sync 0x1424
   (MeshCore public channel).
3. Type message → **SEND** (watch) or textarea + SEND button (web).
4. TX packet: group text with per-message HMAC-SHA256 auth (32-byte PSK
   padded to 32). Unix epoch timestamp. TX complete in 200–400 ms.
5. Other MeshCore nodes on the same PSK decode and display.
6. Received messages buzz the haptic, append to on-watch list, and push to web
   chat via WebSocket.
7. Last 20 messages persisted to `/meshcore_log.txt` on SD, reloaded on boot.

**Advertise node presence:** tap **ADVERTISE** — sends signed advert with node
name, optional GPS, Ed25519 pubkey. Signed with Orlp ed25519 (same stack as
stock MeshCore firmware).

---

## Web UI tabs — brief

- **DASH** — time, date, battery, GPS fix, NTP status, free heap, uptime,
  HAPTIC TEST + MAX BRIGHT shortcuts.
- **NFC** — SCAN TAG (enables reader), shows last UID / NDEF text, SAVE TAG
  (persists to SD + auto-export Flipper Zero v4 `.nfc`), list of saved tags
  with DELETE, EXPORT ALL button.
- **LORA** — MeshCore START/STOP, public channel chat (type + SEND),
  ADVERTISE button, message list loaded from SD history.
- **WiFi** — basic 2.4 GHz network list (SSID / RSSI / channel). Force NTP
  sync button.
- **RECON** — SCAN WiFi (with auth type, BSSID), SCAN BLE (with AirTag
  flag), BLACKOUT (deauth all channels), SNIFFER, DEAUTH DETECT (passive),
  EVIL TWIN (custom SSID AP), targeted DEAUTH (click network to autofill
  BSSID/channel).
- **SET** — brightness slider, GPS/NFC/Haptic toggles, watchface next/prev,
  REBOOT WATCH.

---

## Unified JSON API

Same command vocabulary over **BLE UART** (Nordic UART Service,
`6E400001-B5A3-F393-E0A9-E50E24DCCA9E`) and **HTTP** (`POST /api/cmd` with
`cmd=<JSON string>` form-encoded body).

```json
{"cmd":"status"}                          → {"type":"status","time":...,"bat":87,...}
{"cmd":"version"}                         → {"version":"PipBoy-3000 v0.3",...}
{"cmd":"haptic"}                          → {"ok":true}
{"cmd":"brightness","params":{"v":200}}   → {"ok":true}
{"cmd":"recon_wifi"}                      → {"ok":true,"msg":"wifi scanning"}
{"cmd":"recon_ble","params":{"duration":10}} → {"ok":true,"msg":"ble scanning"}
{"cmd":"recon_results"}                   → {"wifi":[...],"ble":[...]}
{"cmd":"nfc_scan"} / nfc_stop / nfc_save  → {"ok":true}
{"cmd":"lora_send","params":{"text":"hi"}} → {"ok":true}
{"cmd":"compass"}                         → {"heading":123.4,"calibrated":true}
{"cmd":"sensor_data"}                     → {"accel":[...],"gyro":[...],...}
```

Events auto-pushed from watch to both BLE and Web clients:

```json
{"event":"scan_done","wifi":26,"ble":24}
{"event":"lora_msg","channel":"public","text":"...","hops":1,"rssi":-87,"ts":...}
{"event":"nfc_tag","uid":"04:AB:...","ndef":"..."}
{"event":"deauth_detected","count":3}
```

Full list: [`BLE_API_GUIDE.md`](BLE_API_GUIDE.md).

---

## Key files

```
src/main.cpp              - init + main loop (LVGL, services, power)
src/app_manager.cpp       - app routing (watchface, menu, per-app)
src/hal/nfc_service.cpp   - ST25R3916 reader, flag-based commands
src/hal/lora_service.cpp  - SX1262 MeshCore protocol impl
src/hal/recon_service.cpp - WiFi scan / deauth / sniffer / evil twin
src/hal/ble_uart_service.cpp - NimBLE NUS with MITM PIN pairing
src/hal/power_hal.cpp     - cached PMU I2C reads, screen sleep
src/web/web_server.cpp    - ESPAsyncWebServer + WebSocket on 80/ws
src/web/web_ui.h          - PROGMEM HTML/CSS/JS for phone UI
src/api/command_handler.cpp - unified JSON API (BLE + HTTP)
src/apps/*.cpp            - per-feature LVGL screens
src/ui/action_overlay.cpp - IN ACTION / WATCH_DOGS / PAIRING overlays
```

---

## License

MIT. Check individual third-party libraries (LilyGoLib, NimBLE-Arduino,
RadioLib, ArduinoJson, Orlp ed25519) for their licenses.
