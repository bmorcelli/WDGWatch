# 📟 SCR Terminal (formerly WDGWatch) — LilyGO T-Watch Ultra Custom Firmware

```text
===================================================
||   ___   ___  ___    _____  ___  ___  __  __   ||
||  / __| / __|| _ \  |_   _|| __|| _ \|  \/  |  ||
||  \__ \| (__ |   /    | |  | _| |   /| |\/| |  ||
||  |___/ \___||_|_\    |_|  |___||_|\\|_|  |_|  ||
===================================================
```

![Theme](https://img.shields.io/badge/Theme-Retro%20Cyan-00E5FF?style=for-the-badge&logoColor=black)
![Device](https://img.shields.io/badge/Hardware-T--Watch%20Ultra-blue?style=for-the-badge)
![Purpose](https://img.shields.io/badge/Purpose-Security%20%26%20Research-red?style=for-the-badge)
![Licence](https://img.shields.io/badge/Licence-GPL--3.0-green?style=for-the-badge)

<div align="center">

| 📟 DEDSEC // WEB FLASHER |
| :---: |
| **Flash or update WDGWatch firmware directly from your browser** |
| [![CONNECT & FLASH](https://img.shields.io/badge/CONNECT%20%26%20FLASH%20NOW-005f6b?style=for-the-badge&logo=esphome&logoColor=00e5ff&labelColor=00151c)](https://sacriphanius.github.io/WDGWatch/) |

</div>

An advanced, feature-rich custom firmware for the **LilyGO T-Watch S3 Ultra** (ESP32-S3, 410×502 AMOLED), rebranded as the **SCR Terminal**. It turns your wearable into a retro-cybernetic terminal device, serving as a tactical security research tool, a virtual pet keeper, and a companion for the **Watch Dogs Go** mesh ecosystem.

---

## 🚀 Key Features in Detail (English)

### 1. 🔒 Retro Boot Sequence & Gesture Lock
When the device boots, it initiates a high-fidelity **Retro Terminal Sequence** simulating:
*   RAM and ROM integrity tests.
*   Hardware peripheral discovery (NFC transceiver, LoRa transceiver, BHI260AP IMU, GPS).
*   SD card mount checks.
*   **Safety Lock:** Touchscreen swipe gestures are completely disabled during the boot animation, preventing menu access until all systems are checked and initialized.

### 2. 👾 SCR-Bit Virtual Pet (Terminal Droid)
A Tamagotchi-style virtual pet living directly on your wrist:
*   **Evolutionary Path:** Starts as a basic **Microchip** (Level 1) with gold contact pins. Upon reaching 100% XP, it evolves into an animated **Droid** (Level 2) featuring tread tracks, a flashing LED antenna, and dynamic arms.
*   **0% Background Battery Usage:** Rather than using blocking loops or CPU tasks that drain the watch battery, SCR-Bit uses **RTC Time-Delta Calculations**. When the application is closed or the watch is sleeping, parameters (Health, Energy, Cleanliness) decay mathematically based on elapsed time saved in NVS preferences.
*   **Interactive Controls / Web UI Interaction (Web UI):** 
    *   `FEED`: Injects code packets to restore energy. *(Available via Web UI)*
    *   `HEAL`: Calibrates the kernel to restore health. *(Available via Web UI)*
    *   `CLEAN`: Sweeps up wire poop scraps (`~=`) that spawn when cleanliness drops below 50%. *(Available via Web UI)*
    *   `STATUS`: Prints real-time system logs to the scrollable green terminal console at the bottom. *(Status monitored via Web UI)*

### 3. 📡 Tactical RF Jammer & Tesla Port
Leverages the onboard SX1262 transceiver to provide RF security research tools:
*   **Locked Preset Timers:** The RF Jammer cannot be launched without first selecting a duration preset (`10s`, `20s`, `30s`, `1m`, `3m`, or `5m`) **(Web UI)**.
*   **Dynamic Countdown:** When active, the screen locks onto a glowing red warning status displaying a real-time countdown timer.
*   **Tesla Port:** Sends a single-pass RF burst signal mapped to common automotive diagnostic frequencies for authorized testing **(Web UI)**.

### 4. 🛜 WiFi Recon, BLE Hunting & Timezone Management
*   **WiFi Sniffing & Deauth:** Scans 2.4GHz channels, lists active BSSIDs, displays RSSI, and allows targeting individual APs for authorized deauthentication or broad blackout tests **(Web UI)**.
*   **BLE Tracker:** Searches for nearby Bluetooth Low Energy devices and actively checks manufacturer data payloads to flag Apple AirTags or other trackers **(Web UI)**.
*   **Dynamic Timezone Configuration:** WiFi menu includes a **CLOCK** button. When connected to WiFi, users can select from 38 global cities to configure the system timezone. Timezone is persisted in NVS and automatically triggers an NTP re-sync to apply local time offset and DST rules.

### 5. 🌌 Multi-Protocol LoRa & FSK Radio Integration
The device utilizes the onboard SX1262 transceiver to interface with multiple decentralized radio networks and paging protocols:
*   **MeshCore Protocol**  
    ![MeshCore](https://img.shields.io/badge/MeshCore-00E5FF?style=for-the-badge&logo=hive&logoColor=black)  
    Operates on `869.618 MHz` (SF8 / BW62.5 / CR5) for the Watch Dogs Go mesh network. Features secure packet authentication via HMAC-SHA256 and Ed25519 signature verification. Saves session message logs directly to the SD card. Allows custom node name configuration (`lora setname <name>`) directly via the Web UI terminal.
*   **Meshtastic Protocol**  
    ![Meshtastic](https://img.shields.io/badge/Meshtastic-005f6b?style=for-the-badge&logo=esphome&logoColor=00e5ff)  
    Operates on the Standard `869.525 MHz` LongFast channel (SF11 / BW250 / CR5) for decentralized open-mesh text messaging and automatic NodeInfo network announcements. Supports custom chat alias name configurations (`lora setname <name>`).
*   **POCSAG Pager (FSK)**  
    ![POCSAG Pager](https://img.shields.io/badge/POCSAG_Pager-00E5FF?style=for-the-badge&logo=pagerduty&logoColor=black)  
    Configurable RIC pager transmission (default `439.9875 MHz` FSK) with ASCII encoding. Tap the RIC code box in the "Other Devices" menu to dynamically input target RIC pager addresses.
*   **Bruce Firmware Chat (FSK)**  
    ![Bruce Firmware](https://img.shields.io/badge/Bruce_Firmware-005f6b?style=for-the-badge&logo=gitbook&logoColor=00e5ff)  
    FSK compatibility with Bruce firmware LORA chat devices, running on selected presets (`433.920 MHz`, `868.000 MHz`, `915.000 MHz`). Supports setting custom screen chat alias names (`lora setname <name>`).
*   **LoRa Web & Watch Terminal UI:** Includes a dynamic, 4-direction scrollable console terminal logging incoming messages across all networks in real-time, with automated session-specific logging to the SD card (`/lora/`).

### 6. 🏷️ NFC Scan, Save & Emulation
*   **NFC Scan:** Reads ISO-14443A HF tags and NDEF payloads via the ST25R3916 chip **(Web UI)**.
*   **NFC Save & Export:** Saves scanned tag metadata to SD card, exports tags to Flipper Zero compatible `.nfc` format, and allows downloading saved tag files as Base64 **(Web UI)**.
*   **NFC Emulation:** Emulates saved card profiles through the ST25R3916 transceiver.

### 7. ⌨️ HID Controller (BadUSB, BadBLE, Air Mouse)
*   **BadUSB & BadBLE:** Emulates a keyboard/mouse over wired USB or wireless BLE HID.
*   **Web UI DuckyScript Editor & Exfiltration Tool (Web UI):**
    *   List and load `.txt`/`.duck` scripts from the SD card `/badusb` directory **(Web UI)**.
    *   Edit scripts directly inside the web interface and save them back to SD **(Web UI)**.
    *   Send and execute instant DuckyScript payloads over USB or BLE **(Web UI)**.
    *   **Bidirectional File exfiltration:** Includes endpoints for fetching files from target host systems to save on the watch's microSD card `/exfil` directory (`/api/sd/upload`), and sending tools or payloads from the watch's microSD directly to target hosts (`/api/sd/download`). Features a 4-second auto-dimming UI overlay popup displaying transfer progress ("UPLOAD: emre.txt", "GET: emre.txt").
*   **Bilingual Keyboard Layouts (Web UI):** Support for 13 keyboard layouts (US, TR, DE, FR, etc.) dynamically selectable via the layout modal **(Web UI)**.
*   **Air Mouse:** Controls mouse cursor using built-in IMU sensor movements. Watch screen provides Left Click, Right Click, and touch-sensitive Scroll wheel.
*   **Smart USB/HID Lifecycle Management:** Automatically disconnects/restores USB CDC serial port (`/dev/ttyACM0`) when toggling HID services to prevent connection hangs.

### 8. 🎤 Audio Recorder (Mic to SD)
*   **Voice Sniffing & Audio Capturing:** Integrated directly into the **Recon** suite, allowing users to record ambient environment audio directly via the watch's built-in PDM microphone.
*   **Structured SD Card Storage:** Automatically creates a `/rec` directory on the SD card if it does not exist, naming recorded files sequentially as `recrd_1.wav`, `recrd_2.wav`, `recrd_3.wav`, etc.
*   **Hardware Integrity Protection:** Uses clean I2S lifecycle hooks (`i2s_driver_uninstall`) before and after recording to eliminate pin-sharing and hardware bus lockups with other audio features.

### 9. ✈️ ADS-B Flight Radar (Live Aircraft Tracking)
A real-time aircraft surveillance panel powered by the public [adsb.fi](https://api.adsb.fi) API:
*   **Live Flight List:** When connected to WiFi, the Recon menu's **ADSB** button fetches all aircraft currently visible within a configurable radius (default: 150 km) of the device's GPS coordinates.
*   **Rich Telemetry:** Displays ICAO hex code, callsign, altitude (ft), ground speed (kt), heading (°), vertical rate (fpm), and squawk code for each aircraft.
*   **Route Lookup:** Tap any aircraft to perform an asynchronous route lookup, displaying the origin and destination airport IATA codes directly on the watch screen.
*   **Auto-Refresh:** The panel automatically refreshes every 30 seconds while open.
*   **Offline Graceful Degradation:** If GPS has no fix or WiFi is unavailable, the panel displays a clear error status instead of crashing.

### 10. 🦋 Bit-Gotchi Recon Agent (Passive Handshake Hunter)
The SCR-Bit virtual pet evolves into an autonomous cyber-recon agent via the **Politician** passive capture engine:
*   **Passive EAPOL/PMKID Capture:** Activating **BIT-GOTCHI** mode enables autonomous channel hopping and WPA handshake capture without sending any deauth frames. All captured handshakes are saved as `.pcap` files inside a `/bit` directory on the SD card.
*   **SD Log Loot Modal:** A dedicated **SD LOG LOOT** button (below the console) opens an on-device file browser listing all captured `.pcap` files with their sizes.
*   **Butterfly Catch Animation:** When a new handshake is captured, the pet displays a butterfly net animation (`Ƹ̵̡Ӝ̵̨̄Ʒ 🕸️`) for 3 seconds, gains +15 Energy and +25 XP, triggers haptic feedback, and auto-saves state to NVS.
*   **Detective Hat Overlay:** The pet wears a `🎩` detective hat while BIT-GOTCHI mode is active — it disappears when the mode is turned off.
*   **Achievement Badge System (persistent via NVS):**
    *   `🐾 First Step` — Unlocked on the first captured handshake.
    *   `🕵️ Silent Detective` — Unlocked after 5 total captures.
    *   `🏹 PMKID Hunter` — Unlocked when a PMKID packet is captured.
*   **Badge Display:** Pressing the **STATUS** button prints the pet's level, XP, health, and all unlocked badges to the terminal console.

### 11. 📟 Responsive Web UI & CLI Terminal
<table>
  <tr>
    <td width="40%"><img src="cli.png" alt="SCR Terminal Web UI" width="100%"></td>
    <td width="60%" valign="top">
      The watch features an integrated Web Server allowing you to control and interact with the SCR Terminal remotely from any browser.
      <br/><br/>
      <b>How to Connect:</b>
      <ol>
        <li>Open the <b>WiFi / WEB</b> app on the watch screen.</li>
        <li>Tap the <b>WEB SERVER ON / OFF</b> button to turn the web server ON.</li>
        <li>Connect your computer or smartphone to the WiFi Access Point:
          <ul>
            <li><b>SSID:</b> <code>SCR Terminal</code></li>
            <li><b>Password:</b> <code>pip12345</code></li>
          </ul>
        </li>
        <li>Open your browser and navigate to: <b><code>http://192.168.4.1</code></b></li>
      </ol>
      <b>Web UI Enhancements:</b>
      <ul>
        <li><b>Full-Viewport Responsive Layout:</b> Re-engineered CSS using flexbox, allowing the Terminal, SD File Browser, and Nano Editor to occupy 100% height of your browser window.</li>
        <li><b>Retro CLI Solid Blinking Cursor:</b> Custom simulated blinking solid box cursor following retro green terminal UX.</li>
        <li><b>Automated Scan Output:</b> WiFi/BLE scans automatically dump formatted results directly to your terminal session when completed.</li>
        <li><b>Flipper Zero Identification:</b> BLE scan automatically tags active Flipper Zero devices with a <code>[FLIPPER]</code> label.</li>
      </ul>
    </td>
  </tr>
</table>

#### 💻 Web UI Terminal Commands:
*   **Global Utilities:** `help`, `clear`, `exit`, `sysinfo`, `status`, `brightness <10-255>`, `haptic`, `reboot`, `watchface`
*   **Shortcut Aliases:** 
    *   `wifi scan` or `scan wifi` or `wifi` -> Trigger background WiFi AP scanning.
    *   `ble scan [sec]` or `scan ble` or `ble` -> Trigger BLE device scanning (default 10s).
    *   `results` or `scan results` or `wifi results` -> List last discovered APs and BLE devices.
*   **Recon Sub-commands:**
    *   `recon wifi` / `recon ble [seconds]` / `recon stop` / `recon results`
    *   `deauth <bssid> <ch>` -> Targeted deauthentication attack.
    *   `blackout` -> Deauth attack all discovered APs.
    *   `sniffer <ch>` / `sniffer stop` -> Capture raw frames on channel.
    *   `deauth detect` -> Monitor for incoming deauth management frame attacks.
    *   `eviltwin <ssid> [ch]` / `eviltwin stop` -> Launch Evil Twin captive portal.
    *   `arp scan` / `arp results` -> Map active hosts in local subnet.
    *   `ipsniff <ip>` / `ipsniff results` -> Perform network traffic analysis.
    *   `beaconspam <ssids>` / `beaconspam stop` -> Flood area with fake SSIDs.
    *   `adsb start <lat> <lon>` / `adsb status` -> Track local aircraft.
*   **LoRa & Radio:**
    *   `lora start [0:Mesh, 1:Meshtastic, 2:POCSAG, 3:Bruce]` / `lora stop` / `lora send <msg>`
    *   `lora history` -> Show last 20 received mesh/chat messages.
    *   `lora setname <name>` / `lora setfreq <mhz>` / `lora setric <ric>`
*   **NFC Transceiver:**
    *   `nfc scan` / `nfc stop` / `nfc save` / `nfc list` / `nfc emulate` / `nfc delete <idx>`
*   **RF Sub-GHz:**
    *   `rf jam <freq_hz> [seconds]` / `rf stop` / `rf status` / `tesla`
*   **SD File System:**
    *   `ls` / `cd <dir>` / `mkdir <name>` / `rm <path>` / `mv <src> <dst>` / `cp <src> <dst>`
    *   `cat <file>` / `write <file> <text>` / `nano <file>` / `upload` / `download`

---

## 📊 Feature Matrix (English)

| Feature | Watch UI | Web UI | BLE API | Status |
| :--- | :---: | :---: | :---: | :---: |
| 🕰️ **Watchface + System Info** | 🟢 Yes | 🟢 Yes | `status` | Stable |
| 📳 **Haptic Feedback (DRV2605)** | 🟢 Yes | 🟢 Yes | `haptic` | Stable |
| ☀️ **AMOLED Brightness Slider** | 🟢 Yes | 🟢 Yes | `brightness` | Stable |
| 🧭 **Compass (BHI260AP Vector)** | 🟢 Yes | 🔴 No | `compass` | Calibratable |
| 📍 **GPS Location & Sats** | 🟢 Yes | 🟢 Yes | `gps_on/off` | Stable |
| 📶 **WiFi Recon (2.4GHz Scan)** | 🟢 Yes | 🟢 Yes | `recon_wifi` | Stable |
| 🛜 **BLE Recon + AirTag Hunt** | 🟢 Yes | 🟢 Yes | `recon_ble` | Stable |
| ⚡ **WiFi Deauth Targeted/Blackout** | 🟢 Yes | 🟢 Yes | `recon_deauth` | Working |
| 🕵️ **Packet Sniffer & Detect** | 🟢 Yes | 🟢 Yes | `sniffer_start` | Passive |
| 🛜 **SSID Beacon Spoofer** | 🟢 Yes | 🔴 No | — | Up to 15 SSIDs |
| 😈 **Evil Twin AP Creator** | 🟢 Yes | 🟢 Yes | `evil_twin` | Basic AP |
| 🏷️ **NFC Scan (ISO-14443A/NDEF)** | 🟢 Yes | 🟢 Yes | `nfc_scan` | Stable |
| 🌌 **LoRa MeshCore Node ("SCRW")** | 🟢 Yes | 🟢 Yes | `lora_send` | 869.618 MHz |
| 🛜 **Meshtastic Chat (LongFast)** | 🟢 Yes | 🟢 Yes | — | 869.525 MHz |
| 📟 **POCSAG Pager (FSK)** | 🟢 Yes | 🟢 Yes | — | RIC Code Input |
| 📻 **Bruce Chat (FSK)** | 🟢 Yes | 🟢 Yes | — | 433/868/915 MHz |
| 🔐 **LoRa Ed25519 Adverts** | 🟢 Auto | 🔴 No | — | Cryptographic |
| 👾 **SCR-Bit Virtual Pet** | 🟢 Yes | 🟢 Yes | `pet_status` | 0% Battery Idle |
| 📻 **SX1262 Jammer + Countdown** | 🟢 Yes | 🟢 Yes | `rf_status` | Timer-Locked |
| ⚡ **Tesla Port RF Signal** | 🟢 Yes | 🟢 Yes | `rf_tesla_send` | 1-Pass Burst |
| ⌨️ **BadUSB/BadBLE Keyboard** | 🟢 Yes | 🟢 Yes | `hid_status` | SCR-Keyboard |
| 🔒 **Retro Boot Sequence** | 🟢 Yes | 🔴 No | — | Gesture Locked |
| 🕰️ **Dynamic Timezone & NTP Sync** | 🟢 Yes | 🔴 No | — | Stable |
| 🎤 **Audio Recorder (Mic to SD)** | 🟢 Yes | 🔴 No | — | 16kHz 16-bit Mono |
| ✈️ **ADS-B Flight Radar (Live)** | 🟢 Yes | 🔴 No | — | adsb.fi API / GPS |
| 🦋 **Bit-Gotchi Recon Agent** | 🟢 Yes | 🔴 No | — | PCAP → `/bit` on SD |

---

## ⚙️ Hardware Specifications (English)

*   **SoC:** ESP32-S3 (8MB PSRAM, 16MB Flash)
*   **Display:** 1.96-inch Touch AMOLED (410×502)
*   **NFC:** ST25R3916 HF transceiver (Reader/Writer only)
*   **Sub-GHz RF:** SX1262 LoRa @ 869.618 MHz (Public MeshCore frequency)
*   **Sensors:** BHI260AP Smart IMU + GPS module
*   **Haptics:** LRA motor driven by DRV2605
*   **Power:** Li-Ion Battery & AXP2101 PMU

---

## 🛠️ Build & Flash Instructions (English)

Ensure you have **PlatformIO** installed (VSCode extension or CLI).

```bash
# Clone the repository
git clone https://github.com/sacriphanius/WDGWatch.git
cd WDGWatch

# Setup WiFi Credentials (Optional)
# Copy src/wifi_config.example.h to src/wifi_config.h and fill SSID/Passwords

# Build & upload firmware via USB
pio run --target upload --upload-port /dev/ttyACM0

# Start serial monitor
pio device monitor --baud 115200
```

---

## 🔌 Unified JSON Command API Reference (English)

Both BLE Nordic UART and HTTP `POST /api/cmd` accept single-line, `\n` (newline / LF) terminated JSON objects. Below is the complete catalog of all commands supported by the SCR Terminal firmware.

### 1. ⚙️ System Commands
*   **System Status (`status`):** `{"cmd":"status"}`
    *   *Response:* `{"type":"status","time":"12:34:56","date":"2026-06-08","bat":92,"bat_v":4.15,"charging":false,"ntp":true,"heap":185,"lora":false,"nfc":false}`
*   **Version Info (`version`):** `{"cmd":"version"}`
*   **Haptic Test (`haptic`):** `{"cmd":"haptic"}`
*   **Set Brightness (`brightness`):** `{"cmd":"brightness", "params":{"v":150}}` (v: 10 - 255)
*   **Cycle Watchface (`watchface`):** `{"cmd":"watchface", "params":{"style":"next"}}` (style: `"next"` or `"prev"`)
*   **Reboot Device (`reboot`):** `{"cmd":"reboot"}`
*   **Compass Data (`compass`):** `{"cmd":"compass"}`
*   **Enable GPS (`gps_on`):** `{"cmd":"gps_on"}`
*   **Disable GPS (`gps_off`):** `{"cmd":"gps_off"}`
*   **Sensor Telemetry (`sensor_data`):** `{"cmd":"sensor_data"}`

### 2. 👾 SCR-Bit Pet Commands
*   **Get Pet Stats (`pet_status`):** `{"cmd":"pet_status"}`
    *   *Response:* `{"type":"pet_status","level":2,"xp":45,"energy":80,"health":95,"cleanliness":70,"poops":1}`
*   **Feed Pet (`pet_feed`):** `{"cmd":"pet_feed"}`
*   **Heal Pet (`pet_heal`):** `{"cmd":"pet_heal"}`
*   **Clean Cage (`pet_clean`):** `{"cmd":"pet_clean"}`

### 3. 📻 Sub-GHz RF Commands
*   **Start Jammer (`rf_jammer_start`):** `{"cmd":"rf_jammer_start", "params":{"freq":433920000}}` (freq in Hz)
*   **Stop Jammer (`rf_jammer_stop`):** `{"cmd":"rf_jammer_stop"}`
*   **Tesla Signal (`rf_tesla_send`):** `{"cmd":"rf_tesla_send"}`
*   **RF Status (`rf_status`):** `{"cmd":"rf_status"}`
    *   *Response:* `{"type":"rf_status","active":false,"freq":433920000,"tesla_sending":false}`

### 4. ⌨️ BLE & USB HID Keyboard Commands
*   **Start HID (`hid_start`):** `{"cmd":"hid_start"}`
*   **Stop HID (`hid_stop`):** `{"cmd":"hid_stop"}`
*   **Run DuckyScript (`hid_run_script`):** `{"cmd":"hid_run_script", "params":{"path":"pay.txt","ble":true,"layout":"US"}}`
*   **Run Instant DuckyScript (`hid_run_instant`):** `{"cmd":"hid_run_instant", "params":{"script":"GUI r\nDELAY 500\nSTRING notepad.exe\nENTER","ble":true,"layout":"TR"}}`
*   **Set Keyboard Layout (`hid_set_layout`):** `{"cmd":"hid_set_layout", "params":{"layout":"TR"}}`
*   **List Script Files (`hid_list_scripts`):** `{"cmd":"hid_list_scripts"}`
*   **Abort Script (`hid_abort_script`):** `{"cmd":"hid_abort_script"}`
*   **Save Script (`hid_save_script`):** `{"cmd":"hid_save_script", "params":{"path":"pay.txt","content":"STRING hello"}}`
*   **Read Script (`hid_read_script`):** `{"cmd":"hid_read_script", "params":{"path":"pay.txt"}}`
*   **Get HID Status (`hid_status`):** `{"cmd":"hid_status"}`

### 5. 🏷️ HF NFC Transceiver Commands
*   **Start NFC Scan (`nfc_scan`):** `{"cmd":"nfc_scan"}`
*   **Stop NFC Scan (`nfc_stop`):** `{"cmd":"nfc_stop"}`
*   **Save Tag (`nfc_save`):** `{"cmd":"nfc_save"}`
*   **List Tags (`nfc_list`):** `{"cmd":"nfc_list"}`
*   **Delete Tag (`nfc_delete`):** `{"cmd":"nfc_delete", "params":{"idx":0}}`
*   **Download Tag (`nfc_download`):** `{"cmd":"nfc_download", "params":{"idx":0}}`
*   **Export Tags (`nfc_export`):** `{"cmd":"nfc_export"}`

### 6. 🌌 LoRa MeshCore Commands
*   **Start LoRa (`lora_start`):** `{"cmd":"lora_start"}`
*   **Stop LoRa (`lora_stop`):** `{"cmd":"lora_stop"}`
*   **Send Message (`lora_send`):** `{"cmd":"lora_send", "params":{"text":"hello"}}`
*   **Send Advertisement (`lora_advert`):** `{"cmd":"lora_advert"}`
*   **Get History (`lora_history`):** `{"cmd":"lora_history"}`

### 7. 🛜 WiFi & BLE Recon Commands
*   **WiFi Scan (`recon_wifi`):** `{"cmd":"recon_wifi"}`
*   **BLE Scan (`recon_ble`):** `{"cmd":"recon_ble", "params":{"duration":15}}`
*   **Get Results (`recon_results`):** `{"cmd":"recon_results"}`
*   **Stop Scan (`recon_stop`):** `{"cmd":"recon_stop"}`
*   **Targeted Deauth (`recon_deauth`):** `{"cmd":"recon_deauth", "params":{"bssid":"AA:BB:CC:DD:EE:FF","ch":6}}`
*   **Blackout / General Deauth (`deauth_all`):** `{"cmd":"deauth_all"}`
*   **Packet Sniffer (`sniffer_start`):** `{"cmd":"sniffer_start", "params":{"ch":11}}`
*   **Stop Sniffer (`sniffer_stop`):** `{"cmd":"sniffer_stop"}`
*   **Deauth Detect (`deauth_detect`):** `{"cmd":"deauth_detect"}`
*   **Evil Twin AP (`evil_twin`):** `{"cmd":"evil_twin", "params":{"ssid":"MyFreeWiFi","ch":1}}`
*   **Stop Evil Twin (`evil_twin_stop`):** `{"cmd":"evil_twin_stop"}`

---

## 🔵 Bluetooth Terminal Connection Guide (English)

The **SCR Terminal** exposes a Bluetooth Low Energy (BLE) Serial Interface using the **Nordic UART Service (NUS)** protocol. You can use any BLE Terminal App (e.g., *Serial Bluetooth Terminal* on Android, *Bluefy* on iOS, or `bluetoothctl` on Linux) to send JSON commands and receive asynchronous system events.

#### How to Connect:
1. Open the **WiFi** application on the watch.
2. Tap the **WATCH DOGS CONNECT** button to enable BLE.
3. The watch status bar will display `BLE ON` and print a unique 6-digit **BLE PIN** on the serial console or a fullscreen pairing overlay.
4. Open your BLE Terminal app on your phone/PC and scan for devices.
5. Connect to the device named **`PipBoy-xxxxx`** (where `xxxxx` is a unique suffix from the MAC address).
6. Enter the 6-digit PIN displayed on the watch screen when prompted by your system.
7. Once paired, the watch displays the **WATCH_DOGS** skull screen and dims brightness.
8. Set your BLE terminal app to use **`\n` (newline / LF)** as the end-of-line character (all JSON commands must end with `\n`).
9. Send any command from the API list above (e.g., `{"cmd":"status"}\n`).

---

# 📟 SCR Terminal (eski adıyla WDGWatch) — LilyGO T-Watch Ultra Özel Yazılımı

```text
===================================================
||   ___   ___  ___    _____  ___  ___  __  __   ||
||  / __| / __|| _ \  |_   _|| __|| _ \|  \/  |  ||
||  \__ \| (__ |   /    | |  | _| |   /| |\/| |  ||
||  |___/ \___||_|_\    |_|  |___||_|\\|_|  |_|  ||
===================================================
```

![Tema](https://img.shields.io/badge/Tema-Retro%20Camg%C3%B6be%C4%9Fi-00E5FF?style=for-the-badge&logoColor=black)
![Cihaz](https://img.shields.io/badge/Donan%C4%B1m-T--Watch%20Ultra-blue?style=for-the-badge)
![Amaç](https://img.shields.io/badge/Ama%C3%A7-G%C3%BCvenlik%20ve%20Ara%C5%9Ft%C4%B1rma-red?style=for-the-badge)
![Lisans](https://img.shields.io/badge/Lisans-GPL--3.0-green?style=for-the-badge)

<div align="center">

| 📟 DEDSEC // WEB FLASHER |
| :---: |
| **WDGWatch firmware'ini doğrudan tarayıcınızdan yükleyin veya güncelleyin** |
| [![CONNECT & FLASH](https://img.shields.io/badge/BA%C4%9ELAN%20%26%20Y%C3%9CKLE-005f6b?style=for-the-badge&logo=esphome&logoColor=00e5ff&labelColor=00151c)](https://sacriphanius.github.io/WDGWatch/) |

</div>

**LilyGO T-Watch S3 Ultra** (ESP32-S3, 410×502 AMOLED) için geliştirilmiş, **SCR Terminal** olarak yeniden markalanan gelişmiş ve zengin özellikli özel yazılımdır. Giyilebilir cihazınızı retro-siberbiyotik bir terminal arayüzüne dönüştürerek güvenlik araştırması, sanal pet yetiştiriciliği ve **Watch Dogs Go** mesh ekosistemi için bir companion sunar.

---

## 🚀 Detaylı Önemli Özellikler (Türkçe)

### 1. 🔒 Retro Boot Ekranı & Hareket Kilidi
Cihaz başlatıldığında aşağıdaki adımları simüle eden bir **Retro Terminal Açılış Ekranı** devreye girer:
*   RAM ve ROM bütünlük testleri.
*   Donanım bileşenleri taraması (NFC, LoRa, IMU, GPS).
*   SD kart bağlantı kontrolü.
*   **Güvenlik Kilidi:** Animasyon oynatılırken ekran kaydırma (gesture) hareketleri tamamen kilitlenir; böylece sistem güvenli şekilde yüklenene kadar menülere geçilmesi engellenir.

### 2. 👾 SCR-Bit Sanal Pet (Terminal Droid)
Bileğinizde yaşayan retro piksel tarza sahip bir sanal robot:
*   **Evrim Sistemi:** Hayata Level 1 (altın pinli yeşil bir **Microchip**) olarak başlar. 100% XP'ye ulaştığında Level 2 (paletli, yanıp sönen anten LED'li ve hareketli kolları olan bir **Droid**) ünitesine evrimleşir.
*   **%0 Arka Plan Pil Tüketimi:** Saat pilini hızlıca tüketecek arka plan döngüleri (loop/task) yerine **RTC Zaman Farkı Hesaplaması** kullanılır. Uygulama kapalıyken geçen zaman NVS hafızası üzerinden hesaplanır; böylece açlık, sağlık ve temizlik durumları arka planda sıfır güç tüketimi ile güncellenir.
*   **Etkileşimli Butonlar ve Web Arayüzü (Web Arayüzü):**
    *   `FEED`: Kod paketleri enjekte ederek enerjiyi yeniler. *(Web arayüzünden kontrol edilebilir)*
    *   `HEAL`: Kernel kalibrasyonu gerçekleştirerek sağlığı iyileştirir. *(Web arayüzünden kontrol edilebilir)*
    *   `CLEAN`: Temizlik %50'nin altına düştüğünde yerde biriken kablo dışkılarını (`~=`) temizler. *(Web arayüzünden kontrol edilebilir)*
    *   `STATUS`: Alt kısımdaki kaydırılabilir terminal ekranına detaylı sistem durum loglarını yazdırır. *(Web arayüzünden anlık izlenebilir)*

### 3. 📡 Taktiksel RF Jammer & Tesla Sinyali
SX1262 alıcı-vericisini kullanarak güvenlik araştırması araçları sunar:
*   **Süre Sınırı Güvenliği:** Bir çalışma süresi (`10sn`, `20sn`, `30sn`, `1dk`, `3dk`, `5dk`) seçilmeden Jammer başlatılamaz **(Web Arayüzü)**.
*   **Geri Sayım Ekranı:** Jammer aktifleştiğinde ekran kırmızı renkli uyarı moduna geçer ve canlı bir geri sayım sayacı gösterir. Süre bitince otomatik olarak durur.
*   **Tesla Port:** Yetkili araç testleri için yaygın otomotiv teşhis frekanslarında tek geçişli bir RF Tesla sinyal patlaması gönderir **(Web Arayüzü)**.

### 4. 🛜 WiFi Keşif, BLE Algılama ve Zaman Dilimi Yönetimi
*   **WiFi Tarama & Deauth:** 2.4GHz kanallarını tarar, BSSID ve RSSI değerlerini listeler. Yetkili deauth veya genel sinyal karartma (blackout) testleri için hedef seçimini sağlar **(Web Arayüzü)**.
*   **BLE Tracker:** Çevredeki Bluetooth Low Energy cihazlarını tarar ve Apple AirTag gibi takip cihazlarını tespit etmek için üretici veri paketlerini analiz eder **(Web Arayüzü)**.
*   **Dinamik Zaman Dilimi Ayarı (Saat Dilimi):** WiFi menüsünde **CLOCK** butonu eklenmiştir. Cihaz WiFi ağına bağlandığında aktifleşen bu buton, 38 farklı dünya şehrini içeren dokunmatik ve kaydırılabilir bir liste sunar. NVS'ye kaydedilir ve otomatik NTP senkronizasyonunu tetikler.

### 5. 🌌 Çoklu Protokol LoRa & FSK Radyo Entegrasyonu
Cihaz, dahili SX1262 alıcı-vericisini kullanarak çeşitli merkeziyetsiz telsiz ağları ve çağrı cihazı (pager) protokolleri ile haberleşebilir:
*   **MeshCore Protokolü**  
    ![MeshCore](https://img.shields.io/badge/MeshCore-00E5FF?style=for-the-badge&logo=hive&logoColor=black)  
    Watch Dogs Go mesh ağı için `869.618 MHz` (SF8 / BW62.5 / CR5) frekansında çalışır. HMAC-SHA256 paket doğrulama ve Ed25519 imzalı reklam paketleri desteği ile güvenli iletişim sunar.
*   **Meshtastic Protokolü**  
    ![Meshtastic](https://img.shields.io/badge/Meshtastic-005f6b?style=for-the-badge&logo=esphome&logoColor=00e5ff)  
    Merkeziyetsiz açık mesh mesajlaşması için standart `869.525 MHz` LongFast kanalı (SF11 / BW250 / CR5) üzerinde çalışır ve otonom NodeInfo duyuruları yayınlar.
*   **POCSAG Çağrı Cihazı (FSK Pager)**  
    ![POCSAG Pager](https://img.shields.io/badge/POCSAG_Pager-00E5FF?style=for-the-badge&logo=pagerduty&logoColor=black)  
    Alfanümerik çağrı cihazlarına ASCII formatında mesaj göndermek için ayarlanabilir RIC kodlu FSK modülasyonu (varsayılan `439.9875 MHz`). "Other Devices" ayar ekranından hedef çağrı kodunu (RIC) dinamik olarak değiştirebilirsiniz.
*   **Bruce Firmware Chat (FSK)**  
    ![Bruce Firmware](https://img.shields.io/badge/Bruce_Firmware-005f6b?style=for-the-badge&logo=gitbook&logoColor=00e5ff)  
    Bruce yazılımı kullanan cihazlarla FSK tabanlı LORA mesajlaşma uyumluluğu. En çok kullanılan frekanslar (`433.920 MHz`, `868.000 MHz`, `915.000 MHz`) arasında dinamik geçiş imkanı sağlar.
*   **LoRa Web ve Saat Terminal Arayüzü:** Gelen mesajları anlık olarak gösteren, 4 yöne kaydırılabilir retro konsol ekranı ve her protokol oturumu için SD kartta `/lora/` dizininde otomatik dosya tabanlı kayıt (log) tutma sistemi.

### 6. 🏷️ NFC Tarama ve Emülasyon
*   **NFC Tarama:** ST25R3916 entegresini kullanarak 13.56 MHz NFC etiketlerini ve NDEF mesaj içeriklerini okur **(Web Arayüzü)**.
*   **NFC Kaydetme & Flipper Dışa Aktarma:** Okunan etiket verilerini SD karta kaydeder, Flipper Zero formatıyla uyumlu `.nfc` dosyası olarak dışa aktarır ve API ile Base64 formatında bilgisayara indirilmesini sağlar **(Web Arayüzü)**.
*   **NFC Emülasyon:** Kayıtlı etiket verilerini ST25R3916 üzerinden taklit eder (emüle eder).

### 7. ⌨️ HID Denetleyici (BadUSB, BadBLE, Air Mouse)
*   **BadUSB ve BadBLE:** Kablolu USB veya kablosuz BLE HID üzerinden klavye/fare emülasyonu.
*   **Web Tabanlı DuckyScript Editörü ve SD Yönetimi (Web Arayüzü):**
    *   microSD karttaki `/badusb` klasöründeki dosyaları listeleme ve yükleme **(Web Arayüzü)**.
    *   Web editöründe hazırlanan betikleri doğrudan microSD karta kaydetme **(Web Arayüzü)**.
    *   Editördeki betiği anında USB veya BLE HID üzerinden tetikleme **(Web Arayüzü)**.
*   **Çoklu Klavye Düzenleri (Web Arayüzü):** TR (Türkçe) dahil 13 klavye dil düzenini destekler. Web modal arayüzünden dinamik seçilebilir **(Web Arayüzü)**.
*   **Air Mouse:** Dahili IMU hareketleriyle fare imleci kontrolü. Ekran üzeri Sol/Sağ tık ve Scroll bar desteği.
*   **USB/HID Yaşam Döngüsü Yönetimi:** HID aktifken seri port (`/dev/ttyACM0`) çakışmalarını önlemek için USB PHY modüllerini otomatik yönetir.

### 8. 🎤 Ses Kaydedici (SD Karta Kayıt)
*   **Ortam Sesi Yakalama:** **Recon** menüsüne entegre edilen bu özellik sayesinde, saatin dahili PDM mikrofonu kullanılarak ortamdaki sesler doğrudan yakalanır.
*   **Düzenli SD Klasör Yapısı:** SD kart üzerinde `/rec` klasörü yoksa otomatik olarak oluşturulur ve kaydedilen dosyalar ardışık olarak `recrd_1.wav`, `recrd_2.wav` vb. şeklinde isimlendirilir.
*   **I2S Yaşam Döngüsü Koruması:** I2S donanım çakışmalarını ve kilitlenmelerini önlemek adına, ses kaydı başlangıcında ve bitişinde sürücüler (`i2s_driver_uninstall`) temiz bir şekilde yönetilir.

### 9. ✈️ ADS-B Uçuş Radarı (Canlı Uçak Takibi)
[adsb.fi](https://api.adsb.fi) genel API'si ile beslenen gerçek zamanlı uçak gözetleme paneli:
*   **Canlı Uçuş Listesi:** WiFi bağlantısı varken Recon menüsündeki **ADSB** butonu, cihazın GPS koordinatlarından itibaren yapılandırılabilir bir yarıçap (varsayılan: 150 km) içindeki tüm aktif uçakları listeler.
*   **Detaylı Telemetri:** Her uçak için ICAO hex kodu, çağrı işareti (callsign), irtifa (ft), yer hızı (kt), yön (°), dikey hız (fpm) ve squawk kodu görüntülenir.
*   **Rota Sorgulama:** Listeden bir uçağa dokunulduğunda asenkron olarak rota sorgulanır ve kalkış/varış havalimanı IATA kodları saat ekranında gösterilir.
*   **Otomatik Yenileme:** Panel açıkken her 30 saniyede bir veri otomatik yenilenir.
*   **Çevrimdışı Hata Yönetimi:** GPS bağlantısı yoksa veya WiFi bağlı değilse panel çökmek yerine açık bir hata durumu gösterir.

### 10. 🦋 Bit-Gotchi Recon Ajanı (Pasif El Sıkışma Avcısı)
SCR-Bit sanal peti, **Politician** pasif yakalama motoru sayesinde otonom bir siber-keşif ajanına dönüşür:
*   **Pasif EAPOL/PMKID Yakalama:** **BIT-GOTCHI** modu aktifleştirildiğinde, herhangi bir deauth çerçevesi göndermeden otonom kanal atlama (channel hopping) ve WPA el sıkışması yakalama devreye girer. Yakalanan tüm el sıkışmaları SD karttaki `/bit` klasörüne `.pcap` dosyası olarak kaydedilir.
*   **SD Log Loot Modalı:** Terminelin altındaki **SD LOG LOOT** butonu, yakalanan tüm `.pcap` dosyalarını boyutlarıyla birlikte listeleyen bir dosya tarayıcı modalı açar.
*   **Kelebek Yakalama Animasyonu:** Yeni bir el sıkışma yakalandığında, pet 3 saniye boyunca kelebek ağı animasyonu (`Ƹ̵̡Ӝ̵̨̄Ʒ 🕸️`) gösterir, +15 Enerji ve +25 XP kazanır, haptik titreşim tetiklenir ve durum NVS'e otomatik kaydedilir.
*   **Dedektif Şapkası Görsel Katmanı:** BIT-GOTCHI modu aktifken pet, `🎩` dedektif şapkası takar; mod kapatıldığında şapka kaybolur.
*   **Başarım Rozeti Sistemi (NVS ile kalıcı):**
    *   `🐾 First Step` — İlk el sıkışma yakalandığında açılır.
    *   `🕵️ Silent Detective` — Toplam 5 yakalama sonrasında açılır.
    *   `🏹 PMKID Hunter` — Bir PMKID paketi yakalandığında açılır.
*   **Rozet Görüntüleme:** **STATUS** butonuna basıldığında pet'in seviyesi, XP'si, canı ve kazanılan tüm rozetler terminal konsoluna yazdırılır.

### 11. 📟 Duyarlı Web Arayüzü & CLI Terminali
<table>
  <tr>
    <td width="40%"><img src="cli.png" alt="SCR Terminal Web UI" width="100%"></td>
    <td width="60%" valign="top">
      Saat, herhangi bir tarayıcıdan SCR Terminali ile uzaktan etkileşim kurmanıza ve onu kontrol etmenize olanak tanıyan entegre bir Web Sunucusuna sahiptir.
      <br/><br/>
      <b>Nasıl Bağlanılır:</b>
      <ol>
        <li>Saat ekranındaki <b>WiFi / WEB</b> uygulamasını açın.</li>
        <li><b>WEB SERVER ON / OFF</b> butonuna basarak web sunucusunu açın.</li>
        <li>Bilgisayarınızı veya akıllı telefonunuzu şu WiFi Erişim Noktasına bağlayın:
          <ul>
            <li><b>SSID:</b> <code>SCR Terminal</code></li>
            <li><b>Şifre:</b> <code>pip12345</code></li>
          </ul>
        </li>
        <li>Tarayıcınızı açın ve şu adrese gidin: <b><code>http://192.168.4.1</code></b></li>
      </ol>
      <b>Web Arayüzü Geliştirmeleri:</b>
      <ul>
        <li><b>Tam Ekran Esnek Arayüz:</b> CSS Flexbox kullanılarak yenilenen düzen sayesinde Terminal, SD Dosya Tarayıcısı ve Nano Editör tarayıcı pencerenizin %100 yüksekliğini doldurur.</li>
        <li><b>Retro CLI Kutu İmleç:</b> Premium retro yeşil terminal havası veren, içi dolu yanıp sönen kutu şeklinde tasarlanmış özel imleç.</li>
        <li><b>Otomatik Tarama Çıktısı:</b> WiFi/BLE taramaları tamamlandığında, sonuçları doğrudan terminal ekranına otomatik ve biçimlendirilmiş olarak döker.</li>
        <li><b>Flipper Zero Tespiti:</b> BLE taramalarında çevredeki Flipper Zero cihazlarını otomatik olarak <code>[FLIPPER]</code> etiketiyle işaretler.</li>
      </ul>
    </td>
  </tr>
</table>

#### 💻 Web Arayüzü Terminal Komutları:
*   **Genel Araçlar:** `help`, `clear`, `exit`, `sysinfo`, `status`, `brightness <10-255>`, `haptic`, `reboot`, `watchface`
*   **Kısayollar (Alias'lar):** 
    *   `wifi scan` veya `scan wifi` veya `wifi` -> Arka planda WiFi taraması başlatır.
    *   `ble scan [süre]` veya `scan ble` veya `ble` -> BLE taraması başlatır (varsayılan 10 saniye).
    *   `results` veya `scan results` veya `wifi results` -> En son taranan ağları ve cihazları listeler.
*   **Recon Alt Komutları:**
    *   `recon wifi` / `recon ble [süre]` / `recon stop` / `recon results`
    *   `deauth <bssid> <ch>` -> Hedef BSSID ve kanala yönelik deauth saldırısı başlatır.
    *   `blackout` -> Tespit edilen tüm AP'lere deauth saldırısı düzenler.
    *   `sniffer <ch>` / `sniffer stop` -> Belirtilen kanalda ham paket yakalar.
    *   `deauth detect` -> Gelen yönetim çerçevesi saldırılarını izler.
    *   `eviltwin <ssid> [ch]` / `eviltwin stop` -> Evil Twin sahte portalı başlatır.
    *   `arp scan` / `arp results` -> Yerel alt ağdaki aktif cihazları listeler.
    *   `ipsniff <ip>` / `ipsniff results` -> Ağ trafiğini analiz eder.
    *   `beaconspam <ssids>` / `beaconspam stop` -> Sahte SSID yayını başlatır.
    *   `adsb start <lat> <lon>` / `adsb status` -> Bölgedeki uçakları canlı takip eder.
*   **LoRa ve Telsiz:**
    *   `lora start [0:Mesh, 1:Meshtastic, 2:POCSAG, 3:Bruce]` / `lora stop` / `lora send <mesaj>`
    *   `lora history` -> Son alınan 20 telsiz/sohbet mesajını gösterir.
    *   `lora setname <isim>` / `lora setfreq <mhz>` / `lora setric <ric>`
*   **NFC İşlemleri:**
    *   `nfc scan` / `nfc stop` / `nfc save` / `nfc list` / `nfc emulate` / `nfc delete <idx>`
*   **RF Sub-GHz:**
    *   `rf jam <frekans_hz> [süre]` / `rf stop` / `rf status` / `tesla`
*   **SD Dosya Sistemi:**
    *   `ls` / `cd <klasör>` / `mkdir <ad>` / `rm <yol>` / `mv <kaynak> <hedef>` / `cp <kaynak> <hedef>`
    *   `cat <dosya>` / `write <dosya> <metin>` / `nano <dosya>` / `upload` / `download`

---

## 📊 Özellik Tablosu (Türkçe)

| Özellik | Saat Arayüzü | Web Arayüzü | BLE API | Durum |
| :--- | :---: | :---: | :---: | :---: |
| 🕰️ **Saat Arayüzü & Sistem Bilgisi** | 🟢 Evet | 🟢 Evet | `status` | Kararlı |
| 📳 **Haptik Titreşim Geri Bildirimi** | 🟢 Evet | 🟢 Evet | `haptic` | Kararlı |
| ☀️ **AMOLED Parlaklık Ayarı** | 🟢 Evet | 🟢 Evet | `brightness` | Kararlı |
| 🧭 **Pusula (BHI260AP Vektör)** | 🟢 Evet | 🔴 Hayır | `compass` | Kalibre Edilebilir |
| 📍 **GPS Konum ve Uydu Bilgisi** | 🟢 Evet | 🟢 Evet | `gps_on/off` | Kararlı |
| 📶 **WiFi Recon (2.4GHz Tarama)** | 🟢 Evet | 🟢 Evet | `recon_wifi` | Kararlı |
| 🛜 **BLE Tarama + AirTag Bulucu** | 🟢 Evet | 🟢 Evet | `recon_ble` | Kararlı |
| ⚡ **Hedefli WiFi Deauth / Blackout** | 🟢 Evet | 🟢 Evet | `recon_deauth` | Çalışıyor |
| 🕵️ **Paket Koklayıcı & Deauth Algılayıcı** | 🟢 Evet | 🟢 Evet | `sniffer_start` | Pasif |
| 🛜 **SSID Beacon Spoofer** | 🟢 Evet | 🔴 Hayır | — | 15 SSID'ye Kadar |
| 😈 **Evil Twin AP Oluşturucu** | 🟢 Evet | 🟢 Evet | `evil_twin` | Temel AP |
| 🏷️ **NFC Tarama (ISO-14443A/NDEF)** | 🟢 Evet | 🟢 Evet | `nfc_scan` | Kararlı |
| 🌌 **LoRa MeshCore Düğümü ("SCRW")** | 🟢 Evet | 🟢 Evet | `lora_send` | 869.618 MHz |
| 🛜 **Meshtastic Sohbet (LongFast)** | 🟢 Evet | 🟢 Evet | — | 869.525 MHz |
| 📟 **POCSAG Pager (FSK)** | 🟢 Evet | 🟢 Evet | — | RIC Kod Girişi |
| 📻 **Bruce Sohbet (FSK)** | 🟢 Evet | 🟢 Evet | — | 433/868/915 MHz |
| 🔐 **LoRa Ed25519 İmzalı Reklamlar** | 🟢 Oto | 🔴 Hayır | — | Kriptografik |
| 👾 **SCR-Bit Sanal Pet** | 🟢 Evet | 🟢 Evet | `pet_status` | Sıfır Güç Tüketimi |
| 📻 **SX1262 Jammer + Geri Sayım** | 🟢 Evet | 🟢 Evet | `rf_status` | Süre Kilitli |
| ⚡ **Tesla Port RF Sinyali** | 🟢 Evet | 🟢 Evet | `rf_tesla_send` | Tek Geçişli Sinyal |
| ⌨️ **BadUSB/BadBLE Klavye (HID)** | 🟢 Evet | 🟢 Evet | `hid_status` | SCR-Keyboard |
| 🔒 **Retro Boot Ekranı** | 🟢 Evet | 🔴 Hayır | — | El Hareketi Kilitli |
| 🕰️ **Dinamik Saat Dilimi ve NTP** | 🟢 Evet | 🔴 Hayır | — | Kararlı |
| 🎤 **Ses Kaydedici (SD Karta Kayıt)** | 🟢 Evet | 🔴 Hayır | — | 16kHz 16-bit Mono |
| ✈️ **ADS-B Uçuş Radarı (Canlı)** | 🟢 Evet | 🔴 Hayır | — | adsb.fi API / GPS |
| 🦋 **Bit-Gotchi Recon Ajanı** | 🟢 Evet | 🔴 Hayır | — | PCAP → SD `/bit` |

---

## ⚙️ Donanım Özellikleri (Türkçe)

*   **İşlemci:** ESP32-S3 (8MB PSRAM, 16MB Flash)
*   **Ekran:** 1.96 inç Dokunmatik AMOLED (410×502)
*   **NFC:** ST25R3916 HF Alıcı-Verici (Yalnızca Okuyucu/Yazıcı)
*   **Sub-GHz RF:** SX1262 LoRa @ 869.618 MHz (Ortak MeshCore Frekansı)
*   **Sensörler:** BHI260AP Akıllı IMU + GPS Modülü
*   **Haptik:** DRV2605 Sürücülü LRA Motoru
*   **Güç:** Lityum İyon Batarya & AXP2101 Güç Yönetim Çipi (PMU)

---

## 🛠️ Derleme ve Yükleme Kılavuzu (Türkçe)

Sisteminizde **PlatformIO**'nun (VSCode eklentisi veya CLI) yüklü olduğundan emin olun.

```bash
# Depoyu klonlayın
git clone https://github.com/sacriphanius/WDGWatch.git
cd WDGWatch

# Wi-Fi Ağ Ayarları (İsteğe Bağlı)
# src/wifi_config.example.h dosyasını src/wifi_config.h olarak kopyalayın ve Wi-Fi bilgilerinizi doldurun.

# USB üzerinden firmware derleyin ve yükleyin
pio run --target upload --upload-port /dev/ttyACM0

# Seri haberleşme ekranını başlatın
pio device monitor --baud 115200
```

---

## 🔌 Birleşik JSON Komut Kılavuzu (Türkçe)

Hem BLE Nordic UART hem de HTTP `POST /api/cmd` istekleri tek satırlık, `\n` (satır sonu / LF) ile sonlandırılmış JSON objelerini kabul eder. Aşağıda yazılım tarafından desteklenen komutların tamamı listelenmiştir.

### 1. ⚙️ Sistem Komutları
*   **Sistem Durumu (`status`):** `{"cmd":"status"}`
    *   *Yanıt:* `{"type":"status","time":"12:34:56","date":"2026-06-08","bat":92,"bat_v":4.15,"charging":false,"ntp":true,"heap":185,"lora":false,"nfc":false}`
*   **Versiyon Bilgisi (`version`):** `{"cmd":"version"}`
*   **Titreşim Testi (`haptic`):** `{"cmd":"haptic"}`
*   **Parlaklık Ayarı (`brightness`):** `{"cmd":"brightness", "params":{"v":150}}` (v: 10 ile 255 arası)
*   **Kadran Değiştirme (`watchface`):** `{"cmd":"watchface", "params":{"style":"next"}}` (style: `"next"` veya `"prev"`)
*   **Yeniden Başlatma (`reboot`):** `{"cmd":"reboot"}`
*   **Pusula Bilgisi (`compass`):** `{"cmd":"compass"}`
*   **GPS Aç (`gps_on`):** `{"cmd":"gps_on"}`
*   **GPS Kapat (`gps_off`):** `{"cmd":"gps_off"}`
*   **Sensör Telemetrisi (`sensor_data`):** `{"cmd":"sensor_data"}`

### 2. 👾 Sanal Pet Komutları
*   **Pet Durumu (`pet_status`):** `{"cmd":"pet_status"}`
    *   *Yanıt:* `{"type":"pet_status","level":2,"xp":45,"energy":80,"health":95,"cleanliness":70,"poops":1}`
*   **Besleme (`pet_feed`):** `{"cmd":"pet_feed"}`
*   **İyileştirme (`pet_heal`):** `{"cmd":"pet_heal"}`
*   **Temizleme (`pet_clean`):** `{"cmd":"pet_clean"}`

### 3. 📻 Sub-GHz RF Komutları
*   **Jammer Başlat (`rf_jammer_start`):** `{"cmd":"rf_jammer_start", "params":{"freq":433920000}}` (Hz cinsinden frekans)
*   **Jammer Durdur (`rf_jammer_stop`):** `{"cmd":"rf_jammer_stop"}`
*   **Tesla Sinyali (`rf_tesla_send`):** `{"cmd":"rf_tesla_send"}`
*   **RF Durumu (`rf_status`):** `{"cmd":"rf_status"}`
    *   *Yanıt:* `{"type":"rf_status","active":false,"freq":433920000,"tesla_sending":false}`

### 4. ⌨️ BLE & USB HID Klavye Komutları
*   **Klavyeyi Aktifleştir (`hid_start`):** `{"cmd":"hid_start"}`
*   **Klavyeyi Kapat (`hid_stop`):** `{"cmd":"hid_stop"}`
*   **Script Çalıştır (`hid_run_script`):** `{"cmd":"hid_run_script", "params":{"path":"pay.txt","ble":true,"layout":"US"}}`
*   **Anlık Script Çalıştır (`hid_run_instant`):** `{"cmd":"hid_run_instant", "params":{"script":"GUI r\nDELAY 500\nSTRING notepad.exe\nENTER","ble":true,"layout":"TR"}}`
*   **Klavye Düzenini Ayarla (`hid_set_layout`):** `{"cmd":"hid_set_layout", "params":{"layout":"TR"}}`
*   **Script Dosyalarını Listele (`hid_list_scripts`):** `{"cmd":"hid_list_scripts"}`
*   **Script İptali (`hid_abort_script`):** `{"cmd":"hid_abort_script"}`
*   **Script Kaydet (`hid_save_script`):** `{"cmd":"hid_save_script", "params":{"path":"pay.txt","content":"STRING hello"}}`
*   **Script Oku (`hid_read_script`):** `{"cmd":"hid_read_script", "params":{"path":"pay.txt"}}`
*   **HID Durumu (`hid_status`):** `{"cmd":"hid_status"}`

### 5. 🏷️ NFC Komutları
*   **Kart Okuma Başlat (`nfc_scan`):** `{"cmd":"nfc_scan"}`
*   **Kart Okumayı Durdur (`nfc_stop`):** `{"cmd":"nfc_stop"}`
*   **Kartı Kaydet (`nfc_save`):** `{"cmd":"nfc_save"}`
*   **Kartları Listele (`nfc_list`):** `{"cmd":"nfc_list"}`
*   **Kart Sil (`nfc_delete`):** `{"cmd":"nfc_delete", "params":{"idx":0}}`
*   **Kart İndir (`nfc_download`):** `{"cmd":"nfc_download", "params":{"idx":0}}`
*   **Flipper Dışa Aktarma (`nfc_export`):** `{"cmd":"nfc_export"}`

### 6. 🌌 LoRa Komutları
*   **LoRa Başlat (`lora_start`):** `{"cmd":"lora_start"}`
*   **LoRa Durdur (`lora_stop`):** `{"cmd":"lora_stop"}`
*   **Mesaj Gönder (`lora_send`):** `{"cmd":"lora_send", "params":{"text":"hello"}}`
*   **Reklam Paketi Gönder (`lora_advert`):** `{"cmd":"lora_advert"}`
*   **Sohbet Geçmişi (`lora_history`):** `{"cmd":"lora_history"}`

### 7. 🛜 WiFi & BLE Keşif Komutları
*   **WiFi Tarama (`recon_wifi`):** `{"cmd":"recon_wifi"}`
*   **BLE Tarama (`recon_ble`):** `{"cmd":"recon_ble", "params":{"duration":15}}`
*   **Tarama Sonuçları (`recon_results`):** `{"cmd":"recon_results"}`
*   **Taramayı Durdur (`recon_stop`):** `{"cmd":"recon_stop"}`
*   **Hedefli Deauth (`recon_deauth`):** `{"cmd":"recon_deauth", "params":{"bssid":"AA:BB:CC:DD:EE:FF","ch":6}}`
*   **Genel Deauth (`deauth_all`):** `{"cmd":"deauth_all"}`
*   **Paket Koklama (`sniffer_start`):** `{"cmd":"sniffer_start", "params":{"ch":11}}`
*   **Koklamayı Durdur (`sniffer_stop`):** `{"cmd":"sniffer_stop"}`
*   **Deauth Algılayıcı (`deauth_detect`):** `{"cmd":"deauth_detect"}`
*   **Sahte Erişim Noktası (`evil_twin`):** `{"cmd":"evil_twin", "params":{"ssid":"MyFreeWiFi","ch":1}}`
*   **Sahte Erişimi Kapat (`evil_twin_stop`):** `{"cmd":"evil_twin_stop"}`

---

## 🔵 Bluetooth Terminal Bağlantı Kılavuzu (Türkçe)

**SCR Terminal**, **Nordic UART Service (NUS)** protokolünü kullanarak Bluetooth Low Energy (BLE) üzerinden çalışan bir kablosuz seri terminal sunar. Herhangi bir BLE Terminal uygulaması (örneğin Android'de *Serial Bluetooth Terminal*, iOS'ta *Bluefy* veya Linux'ta `bluetoothctl`) kullanarak cihaza JSON komutları gönderebilir ve sistem durum loglarını canlı olarak alabilirsiniz.

#### Nasıl Bağlanılır:
1. Saatteki **WiFi** uygulamasını açın.
2. BLE modülünü aktif hale getirmek için **WATCH DOGS CONNECT** butonuna dokunun.
3. Saat ekranında `BLE ON` yazısı görünecektir. Cihaz ilk kez eşleşiyorsa ekranda 6 haneli **BLE PIN** kodu belirecektir.
4. Telefon veya bilgisayarınızdaki BLE Terminal uygulamasını açarak tarama yapın.
5. **`PipBoy-xxxxx`** (buradaki `xxxxx` MAC adresine özel benzersiz koddur) isimli cihaza bağlanın.
6. Bağlantı esnasında telefon/PC ekranında şifre istendiğinde, saat ekranındaki 6 haneli PIN kodunu girin.
7. Eşleşme tamamlandığında saat ekranı **WATCH_DOGS** kuru kafa görseline geçecek ve parlaklığı kısacaktır.
8. Terminal uygulamanızın satır sonu karakterini **`\n` (LF / satır sonu)** olarak ayarlayın (JSON komutları `\n` ile sonlanmak zorundadır).
9. Yukarıdaki API listesinde yer alan herhangi bir komutu gönderin (örneğin: `{"cmd":"status"}\n`).

---

> [!WARNING]
> ## ⚠️ EDUCATIONAL PURPOSES ONLY / SADECE EĞİTİM AMAÇLIDIR
> **English:** This firmware and its features (such as RF transmission, Wi-Fi deauthentication, packet sniffing, etc.) are developed strictly for educational, testing, and authorized security research purposes. The developers and contributors take no responsibility for any misuse, damage, or legal consequences resulting from illegal operations of this software. Always comply with local radio communication and cybersecurity laws.
> 
> **Türkçe:** Bu yazılım ve içerdiği özellikler (RF sinyal gönderimi, Wi-Fi deauth, paket koklama vb.) yalnızca eğitim, test ve yetkili güvenlik araştırmaları amacıyla geliştirilmiştir. Geliştiriciler ve katkıda bulunanlar, bu yazılımın yasal olmayan veya zararlı amaçlarla kullanılmasından ötürü hiçbir sorumluluk veya yasal yükümlülük kabul etmez. Her zaman yerel radyo frekansı ve siber güvenlik yasalarına uyunuz.

## 🤝 Credits & Special Thanks / Katkıda Bulunanlar & Teşekkürler

### English
* Special thanks to **[@bmorcelli](https://github.com/bmorcelli)** for the excellent Wi-Fi Deauthentication improvements and other invaluable code assistance that significantly enhanced the stability and performance of the Recon service.

### Türkçe
* Recon servisinin kararlılığını ve performansını önemli ölçüde artıran mükemmel Wi-Fi Deauthentication geliştirmeleri ve diğer değerli kod yardımlarından ötürü **[@bmorcelli](https://github.com/bmorcelli)**'ye teşekkür ederiz.

---

## 📄 License & Copyright / Lisans ve Telif Hakkı

### English
**Copyright (C) 2026 sacriphanius. All rights reserved.**

This program is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License v3 (GPLv3)** as published by the Free Software Foundation.

Under this license:
*   **Ownership Protection:** The original copyright remains with the author (`sacriphanius`). No one may claim sole ownership of this project or its derivatives.
*   **Copyleft Condition:** Any modified versions or derivative works of this software **must** also be open-source and licensed under the same GPLv3 license.
*   **Liability:** This software is provided "as is", without warranty of any kind.

For the full license terms, see the [LICENSE](file:///home/sacriphanius/Masaüstü/xiaozhieyes/WDGWatch/LICENSE) file.

---

### Türkçe
**Telif Hakkı (C) 2026 sacriphanius. Tüm Hakları Saklıdır.**

Bu program özgür bir yazılımdır: Free Software Foundation tarafından yayınlanan **GNU Genel Kamu Lisansı v3 (GPLv3)** koşulları altında yeniden dağıtabilir ve/veya değiştirebilirsiniz.

Bu lisans kapsamında:
*   **Mülkiyet Koruması:** Orijinal telif hakları yazara (`sacriphanius`) aittir. Hiç kimse bu projenin veya türevlerinin tek başına hak sahibi olduğunu iddia edemez.
*   **Paylaşım Koşulu (Copyleft):** Bu yazılımın değiştirilmiş veya türetilmiş herhangi bir sürümü de **açık kaynak kodlu** olmak ve aynı GPLv3 lisansı altında dağıtılmak zorundadır.
*   **Sorumluluk:** Bu yazılım, herhangi bir garanti verilmeksizin "olduğu gibi" sunulmaktadır.

Tüm lisans koşulları için [LICENSE](file:///home/sacriphanius/Masaüstü/xiaozhieyes/WDGWatch/LICENSE) dosyasına bakabilirsiniz.
