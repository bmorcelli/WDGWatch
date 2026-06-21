# 📟 SCR Terminal v2.5 — The Ultimate Cyber-Research Update! 🚀

We are thrilled to present **v2.5** of the **SCR Terminal** (WDGWatch) firmware! This release elevates the remote Web UI and CLI environment to a professional, premium standard, delivering an incredibly responsive layout, intuitive aliases, and retro-themed visual excellence.

---

## 🌟 Key Highlights in v2.5

### ⚡ Full-Viewport Responsive Layout
The entire Web UI has been re-architected from the ground up! By employing modern CSS Flexbox containers, the green terminal, SD file browser, and built-in Nano editor now dynamically scale to fill **100% of your viewport**. No more scrollable container artifacts—enjoy a true full-screen console experience on both desktop and mobile devices.

### 🟩 Retro Solid Blinking Box Cursor
Typing in the terminal now feels incredibly authentic. We implemented a custom, high-precision blinking solid box cursor that matches the character width of your text, bringing the classic terminal experience to life.

### 🛰️ Automated Recon & Push Notifications
Forget manual polling! The Web UI now listens for real-time WebSocket events. The moment a WiFi or BLE scan finishes, the firmware automatically pushes and prints the formatted results directly onto your console screen.

### 🐬 Flipper Zero Detection
Identify security research devices in your vicinity. The passive BLE scanner now parses advertiser packets to automatically identify active Flipper Zero units, tagging them with a prominent `[FLIPPER]` label in your results.

### ⌨️ Natural CLI Aliases & Shortcuts
Command entry is now faster and more intuitive. We mapped common command shortcuts:
* `wifi` / `wifi scan` -> Triggers AP scan.
* `ble` / `ble scan [seconds]` -> Triggers BLE scan.
* `results` -> Prints latest WiFi and BLE scan details.

---

## 💻 Web UI CLI Terminal Reference Guide

| Command | Category | Description | Example / Usage |
| :--- | :--- | :--- | :--- |
| **`help`** | Global | Display the interactive command helper | `help` |
| **`clear`** | Global | Clear the green terminal screen | `clear` |
| **`status`** | Global | View real-time system and battery logs | `status` |
| **`reboot`** | Global | Hard reboot the watch | `reboot` |
| **`wifi scan`** | Recon | Scan for 2.4GHz WiFi Access Points | `wifi scan` |
| **`ble scan [sec]`**| Recon | Scan for nearby BLE devices (default 10s) | `ble scan 15` |
| **`results`** | Recon | Print results of the latest WiFi/BLE scan | `results` |
| **`deauth <mac> <ch>`**| Recon| Target an AP with deauthentication frames | `deauth AA:BB:CC:11:22:33 6` |
| **`blackout`** | Recon | Deauthenticate all discovered access points | `blackout` |
| **`sniffer <ch>`** | Recon | Sniff raw WiFi frames on a specific channel | `sniffer 1` |
| **`deauth detect`** | Recon | Monitor for incoming deauth frame attacks | `deauth detect` |
| **`eviltwin <ssid> [ch]`**| Recon| Start a rogue captive portal access point | `eviltwin FreeWiFi 11` |
| **`rf jam <hz> [sec]`**| RF | Jam sub-GHz frequencies with SX1262 | `rf jam 433920000 30` |
| **`tesla`** | RF | Send a single automotive RF diagnostic burst | `tesla` |
| **`lora start <mode>`**| LoRa | Start MeshCore(0), Meshtastic(1), POCSAG(2), Bruce(3)| `lora start 1` |
| **`nfc scan`** | NFC | Scan for HF RFID cards (ISO-14443A) | `nfc scan` |
| **`nfc emulate`** | NFC | Emulate the last scanned/selected tag | `nfc emulate` |
| **`ls`** | SD | List files and directories on the SD card | `ls /badusb` |
| **`nano <file>`** | SD | Edit any file on the SD card dynamically | `nano /badusb/payload.txt` |

---

## 📦 Binary Verification
* **Firmware Image:** `WDGWatch_SCR_Edition_V_2_5.bin`
* **Release Tag:** `v2.5`
