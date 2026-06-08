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
![Licence](https://img.shields.io/badge/Licence-MIT-green?style=for-the-badge)

An advanced, feature-rich custom firmware for the **LilyGO T-Watch S3 Ultra** (ESP32-S3, 410×502 AMOLED), rebranded as the **SCR Terminal**. It turns your wearable into a retro-cybernetic terminal device, serving as a tactical security research tool, a virtual pet keeper, and a companion for the **Watch Dogs Go** mesh ecosystem.

---

## 🚀 Key Features in Detail

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
*   **Interactive Controls:** 
    *   `FEED`: Injects code packets to restore energy.
    *   `HEAL`: Calibrates the kernel to restore health.
    *   `CLEAN`: Sweeps up wire poop scraps (`~=`) that spawn when cleanliness drops below 50%.
    *   `STATUS`: Prints real-time system logs to the scrollable green terminal console at the bottom.

### 3. 📡 Tactical RF Jammer & Tesla Port
Leverages the onboard SX1262 transceiver to provide RF security research tools:
*   **Locked Preset Timers:** The RF Jammer cannot be launched without first selecting a duration preset (`10s`, `20s`, `30s`, `1m`, `3m`, or `5m`).
*   **Dynamic Countdown:** When active, the screen locks onto a glowing red warning status displaying a real-time countdown timer.
*   **Tesla Port:** Sends a single-pass RF burst signal mapped to common automotive diagnostic frequencies for authorized testing.

### 4. 🛜 WiFi Recon & BLE AirTag Hunting
*   **WiFi Sniffing & Deauth:** Scans 2.4GHz channels, lists active BSSIDs, displays RSSI, and allows targeting individual APs for authorized deauthentication or broad blackout tests.
*   **BLE Tracker:** Searches for nearby Bluetooth Low Energy devices and actively checks manufacturer data payloads to flag Apple AirTags or other trackers.

### 5. 🌌 LoRa MeshCore Node ("SCRW")
*   Tunes to `869.618 MHz` (SF8 / BW62.5 / CR5) to act as a wearable node.
*   Secures transmissions using per-packet HMAC-SHA256 authentication.
*   Supports Ed25519-signed presence advertisements and logs the last 20 public messages directly to the SD card.

---

## 📊 Feature Matrix

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
| 😈 **Evil Twin AP Creator** | 🟢 Yes | 🟢 Yes | `evil_twin` | Basic AP |
| 🏷️ **NFC Scan (ISO-14443A/NDEF)** | 🟢 Yes | 🟢 Yes | `nfc_scan` | Stable |
| 💾 **NFC Save & Flipper Export** | 🟢 Yes | 🟢 Yes | `nfc_export` | Stable |
| 🌌 **LoRa MeshCore Node ("SCRW")** | 🟢 Yes | 🟢 Yes | `lora_send` | 869.618 MHz |
| 🔐 **LoRa Ed25519 Adverts** | 🟢 Auto | 🔴 No | — | Cryptographic |
| 👾 **SCR-Bit Virtual Pet** | 🟢 Yes | 🟢 Yes | `pet_status` | 0% Battery Idle |
| 📻 **SX1262 Jammer + Countdown** | 🟢 Yes | 🟢 Yes | `rf_status` | Timer-Locked |
| ⚡ **Tesla Port RF Signal** | 🟢 Yes | 🟢 Yes | `rf_tesla_send` | 1-Pass Burst |
| ⌨️ **BadUSB/BadBLE Keyboard** | 🟢 Yes | 🟢 Yes | `hid_status` | SCR-Keyboard |
| 🔒 **Retro Boot Sequence** | 🟢 Yes | 🔴 No | — | Gesture Locked |

---

## ⚙️ Hardware Specifications

*   **SoC:** ESP32-S3 (8MB PSRAM, 16MB Flash)
*   **Display:** 1.96-inch Touch AMOLED (410×502)
*   **NFC:** ST25R3916 HF transceiver (Reader/Writer only)
*   **Sub-GHz RF:** SX1262 LoRa @ 869.618 MHz (Public MeshCore frequency)
*   **Sensors:** BHI260AP Smart IMU + GPS module
*   **Haptics:** LRA motor driven by DRV2605
*   **Power:** Li-Ion Battery & AXP2101 PMU

---

## 🛠️ Build & Flash Instructions

Ensure you have **PlatformIO** installed (VSCode extension or CLI).

```bash
# Clone the repository
git clone https://github.com/LOCOSP/WDGWatch.git
cd WDGWatch

# Setup WiFi Credentials (Optional)
# Copy src/wifi_config.example.h to src/wifi_config.h and fill SSID/Passwords

# Build & upload firmware via USB
pio run --target upload --upload-port /dev/ttyACM0

# Start serial monitor
pio device monitor --baud 115200
```

---

## 🔌 Unified JSON Command API Reference

Both BLE Nordic UART and HTTP `POST /api/cmd` accept the following JSON schema:

### 1. System Commands
*   **Get Status:** `{"cmd":"status"}`
    *   *Response:* `{"type":"status","time":"12:34:56","date":"2026-06-08","bat":92,"bat_v":4.15,"charging":false,"ntp":true,"heap":185,"lora":false,"nfc":false}`
*   **Haptic Test:** `{"cmd":"haptic"}`
*   **Set Brightness:** `{"cmd":"brightness", "params":{"v":150}}` (v: 10 - 255)
*   **Reboot Device:** `{"cmd":"reboot"}`

### 2. Virtual Pet Commands
*   **Get Stats:** `{"cmd":"pet_status"}`
    *   *Response:* `{"type":"pet_status","level":2,"xp":45,"energy":80,"health":95,"cleanliness":70,"poops":1}`
*   **Feed Pet:** `{"cmd":"pet_feed"}`
*   **Heal Pet:** `{"cmd":"pet_heal"}`
*   **Clean Cage:** `{"cmd":"pet_clean"}`

### 3. RF & Jammer Commands
*   **Start Jammer:** `{"cmd":"rf_jammer_start", "params":{"freq":433920000}}` (frequency in Hz)
*   **Stop Jammer:** `{"cmd":"rf_jammer_stop"}`
*   **Send Tesla Burst:** `{"cmd":"rf_tesla_send"}`
*   **Get RF Status:** `{"cmd":"rf_status"}`
    *   *Response:* `{"type":"rf_status","active":false,"freq":433920000,"tesla_sending":false}`

### 4. HID / BadUSB Commands
*   **Start Advertising:** `{"cmd":"hid_start"}`
*   **Stop Advertising:** `{"cmd":"hid_stop"}`
*   **Get HID Status:** `{"cmd":"hid_status"}`
*   **Run Script from SD:** `{"cmd":"hid_run_script", "params":{"path":"/scripts/payload.txt","ble":true}}`

---

<br>

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
![Lisans](https://img.shields.io/badge/Lisans-MIT-green?style=for-the-badge)

**LilyGO T-Watch S3 Ultra** (ESP32-S3, 410×502 AMOLED) için geliştirilmiş, **SCR Terminal** olarak yeniden markalanan gelişmiş ve zengin özellikli özel yazılımdır. Giyilebilir cihazınızı retro-siberbiyotik bir terminal arayüzüne dönüştürerek güvenlik araştırması, sanal pet yetiştiriciliği ve **Watch Dogs Go** mesh ekosistemi için bir companion sunar.

---

## 🚀 Detaylı Önemli Özellikler

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
*   **Etkileşimli Butonlar:**
    *   `FEED`: Kod paketleri enjekte ederek enerjiyi yeniler.
    *   `HEAL`: Kernel kalibrasyonu gerçekleştirerek sağlığı iyileştirir.
    *   `CLEAN`: Temizlik %50'nin altına düştüğünde yerde biriken kablo dışkılarını (`~=`) temizler.
    *   `STATUS`: Alt kısımdaki kaydırılabilir terminal ekranına detaylı sistem durum loglarını yazdırır.

### 3. 📡 Taktiksel RF Jammer & Tesla Sinyali
SX1262 alıcı-vericisini kullanarak güvenlik araştırması araçları sunar:
*   **Süre Sınırı Güvenliği:** Bir çalışma süresi (`10sn`, `20sn`, `30sn`, `1dk`, `3dk`, `5dk`) seçilmeden Jammer başlatılamaz.
*   **Geri Sayım Ekranı:** Jammer aktifleştiğinde ekran kırmızı renkli uyarı moduna geçer ve canlı bir geri sayım sayacı gösterir. Süre bitince otomatik olarak durur.
*   **Tesla Port:** Yetkili araç testleri için yaygın otomotiv teşhis frekanslarında tek geçişli bir RF sinyal patlaması gönderir.

### 4. 🛜 WiFi Recon & BLE AirTag Algılama
*   **WiFi Tarama & Deauth:** 2.4GHz kanallarını tarar, BSSID ve RSSI değerlerini listeler. Yetkili deauth veya genel sinyal karartma (blackout) testleri için hedef seçimini sağlar.
*   **BLE Tracker:** Çevredeki Bluetooth Low Energy cihazlarını tarar ve Apple AirTag gibi takip cihazlarını tespit etmek için üretici veri paketlerini analiz eder.

### 5. 🌌 LoRa MeshCore Düğümü ("SCRW")
*   `869.618 MHz` (SF8 / BW62.5 / CR5) frekansında çalışarak mesh ağına bağlanır.
*   Veri paketlerini HMAC-SHA256 kimlik doğrulamasıyla şifreler.
*   Ed25519 imzalı reklam paketleri gönderir ve gelen son 20 mesajı SD kartta saklar.

---

## 📊 Özellik Tablosu

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
| 😈 **Evil Twin AP Oluşturucu** | 🟢 Evet | 🟢 Evet | `evil_twin` | Temel AP |
| 🏷️ **NFC Tarama (ISO-14443A/NDEF)** | 🟢 Evet | 🟢 Evet | `nfc_scan` | Kararlı |
| 💾 **NFC Kaydetme ve Flipper Dışa Aktarma** | 🟢 Evet | 🟢 Evet | `nfc_export` | Kararlı |
| 🌌 **LoRa MeshCore Düğümü ("SCRW")** | 🟢 Evet | 🟢 Evet | `lora_send` | 869.618 MHz |
| 🔐 **LoRa Ed25519 İmzalı Reklamlar** | 🟢 Oto | 🔴 Hayır | — | Kriptografik |
| 👾 **SCR-Bit Sanal Pet** | 🟢 Evet | 🟢 Evet | `pet_status` | Sıfır Güç Tüketimi |
| 📻 **SX1262 Jammer + Geri Sayım** | 🟢 Evet | 🟢 Evet | `rf_status` | Süre Kilitli |
| ⚡ **Tesla Port RF Sinyali** | 🟢 Evet | 🟢 Evet | `rf_tesla_send` | Tek Geçişli Sinyal |
| ⌨️ **BadUSB/BadBLE Klavye (HID)** | 🟢 Evet | 🟢 Evet | `hid_status` | SCR-Keyboard |
| 🔒 **Retro Boot Ekranı** | 🟢 Evet | 🔴 Hayır | — | El Hareketi Kilitli |

---

## ⚙️ Donanım Özellikleri

*   **İşlemci:** ESP32-S3 (8MB PSRAM, 16MB Flash)
*   **Ekran:** 1.96 inç Dokunmatik AMOLED (410×502)
*   **NFC:** ST25R3916 HF Alıcı-Verici (Yalnızca Okuyucu/Yazıcı)
*   **Sub-GHz RF:** SX1262 LoRa @ 869.618 MHz (Ortak MeshCore Frekansı)
*   **Sensörler:** BHI260AP Akıllı IMU + GPS Modülü
*   **Haptik:** DRV2605 Sürücülü LRA Motoru
*   **Güç:** Lityum İyon Batarya & AXP2101 Güç Yönetim Çipi (PMU)

---

## 🛠️ Derleme ve Yükleme Kılavuzu

Sisteminizde **PlatformIO**'nun (VSCode eklentisi veya CLI) yüklü olduğundan emin olun.

```bash
# Depoyu klonlayın
git clone https://github.com/LOCOSP/WDGWatch.git
cd WDGWatch

# Wi-Fi Ağ Ayarları (İsteğe Bağlı)
# src/wifi_config.example.h dosyasını src/wifi_config.h olarak kopyalayın ve Wi-Fi bilgilerinizi doldurun.

# USB üzerinden firmware derleyin ve yükleyin
pio run --target upload --upload-port /dev/ttyACM0

# Seri haberleşme ekranını başlatın
pio device monitor --baud 115200
```

---

## 🔌 Birleşik JSON Komut API Referansı

Hem BLE Nordic UART hem de HTTP `POST /api/cmd` istekleri aşağıdaki JSON formatını kabul eder:

### 1. Sistem Komutları
*   **Durum Sorgula:** `{"cmd":"status"}`
    *   *Dönen Yanıt:* `{"type":"status","time":"12:34:56","date":"2026-06-08","bat":92,"bat_v":4.15,"charging":false,"ntp":true,"heap":185,"lora":false,"nfc":false}`
*   **Haptik Testi:** `{"cmd":"haptic"}`
*   **Parlaklık Ayarla:** `{"cmd":"brightness", "params":{"v":150}}` (v: 10 - 255)
*   **Cihazı Yeniden Başlat:** `{"cmd":"reboot"}`

### 2. Sanal Pet Komutları
*   **Durum Sorgula:** `{"cmd":"pet_status"}`
    *   *Dönen Yanıt:* `{"type":"pet_status","level":2,"xp":45,"energy":80,"health":95,"cleanliness":70,"poops":1}`
*   **Besle:** `{"cmd":"pet_feed"}`
*   **Kernel Kalibre Et (İyileştir):** `{"cmd":"pet_heal"}`
*   **Temizle:** `{"cmd":"pet_clean"}`

### 3. RF & Jammer Komutları
*   **Jammer Başlat:** `{"cmd":"rf_jammer_start", "params":{"freq":433920000}}` (Hz cinsinden frekans)
*   **Jammer Durdur:** `{"cmd":"rf_jammer_stop"}`
*   **Tesla Sinyali Gönder:** `{"cmd":"rf_tesla_send"}`
*   **RF Durumu Sorgula:** `{"cmd":"rf_status"}`
    *   *Dönen Yanıt:* `{"type":"rf_status","active":false,"freq":433920000,"tesla_sending":false}`

### 4. HID / BadUSB Komutları
*   **Yayın Başlat:** `{"cmd":"hid_start"}`
*   **Yayın Durdur:** `{"cmd":"hid_stop"}`
*   **HID Durumu Sorgula:** `{"cmd":"hid_status"}`
*   **SD Karttan Payload Çalıştır:** `{"cmd":"hid_run_script", "params":{"path":"/scripts/payload.txt","ble":true}}`

---

<br>

> [!WARNING]
> ## ⚠️ EDUCATIONAL PURPOSES ONLY / SADECE EĞİTİM AMAÇLIDIR
> **English:** This firmware and its features (such as RF transmission, Wi-Fi deauthentication, packet sniffing, etc.) are developed strictly for educational, testing, and authorized security research purposes. The developers and contributors take no responsibility for any misuse, damage, or legal consequences resulting from illegal operations of this software. Always comply with local radio communication and cybersecurity laws.
> 
> **Türkçe:** Bu yazılım ve içerdiği özellikler (RF sinyal gönderimi, Wi-Fi deauth, paket koklama vb.) yalnızca eğitim, test ve yetkili güvenlik araştırmaları amacıyla geliştirilmiştir. Geliştiriciler ve katkıda bulunanlar, bu yazılımın yasal olmayan veya zararlı amaçlarla kullanılmasından ötürü hiçbir sorumluluk veya yasal yükümlülük kabul etmez. Her zaman yerel radyo frekansı ve siber güvenlik yasalarına uyunuz.
