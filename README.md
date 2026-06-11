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


### 4. 🛜 WiFi Recon, BLE Hunting & Timezone Management
*   **WiFi Sniffing & Deauth:** Scans 2.4GHz channels, lists active BSSIDs, displays RSSI, and allows targeting individual APs for authorized deauthentication or broad blackout tests.
*   **BLE Tracker:** Searches for nearby Bluetooth Low Energy devices and actively checks manufacturer data payloads to flag Apple AirTags or other trackers.
*   **Dynamic Timezone Configuration:** Redesigned the WiFi menu into a split-layout to accommodate a new **CLOCK** button. When connected to a WiFi network, users can open a touch-navigable, scrollable list of 38 global cities to configure the system timezone. This timezone is persisted in NVS (`timesync` namespace) and automatically triggers an NTP re-sync to apply local time offset and DST rules.

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
| 🕰️ **Dynamic Timezone & NTP Sync** | 🟢 Yes | 🔴 No | — | Stable |

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



### 4. 🛜 WiFi Keşif, BLE Algılama ve Zaman Dilimi Yönetimi
*   **WiFi Tarama & Deauth:** 2.4GHz kanallarını tarar, BSSID ve RSSI değerlerini listeler. Yetkili deauth veya genel sinyal karartma (blackout) testleri için hedef seçimini sağlar.
*   **BLE Tracker:** Çevredeki Bluetooth Low Energy cihazlarını tarar ve Apple AirTag gibi takip cihazlarını tespit etmek için üretici veri paketlerini analiz eder.
*   **Dinamik Zaman Dilimi Ayarı (Saat Dilimi):** WiFi menüsünde yapılan arayüz güncellemesiyle **CLOCK** butonu eklenmiştir. Cihaz WiFi ağına bağlandığında aktifleşen bu buton, 38 farklı dünya şehrini içeren dokunmatik ve kaydırılabilir bir liste sunar. Seçilen zaman dilimi kalıcı olarak NVS hafızasına (`timesync` isim uzayı altında) kaydedilir ve anında yerel saat dilimi ile yaz/kış saati (DST) kurallarına göre NTP senkronizasyonunu tetikler.

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
| 🕰️ **Dinamik Saat Dilimi ve NTP** | 🟢 Evet | 🔴 Hayır | — | Kararlı |

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
git clone https://github.com/sacriphanius/WDGWatch.git
cd WDGWatch

# Wi-Fi Ağ Ayarları (İsteğe Bağlı)
# src/wifi_config.example.h dosyasını src/wifi_config.h olarak kopyalayın ve Wi-Fi bilgilerinizi doldurun.

# USB üzerinden firmware derleyin ve yükleyin
pio run --target upload --upload-port /dev/ttyACM0

# Seri haberleşme ekranını başlatın
pio device monitor --baud 115200
```


## 🔌 Unified JSON Command API Reference / Birleşik JSON Komut Kılavuzu

Both **BLE Nordic UART** and HTTP `POST /api/cmd` accept single-line, `\n` (newline / LF) terminated JSON objects. Below is the complete catalog of all **34 commands** supported by the SCR Terminal firmware.

Hem **BLE Nordic UART** hem de HTTP `POST /api/cmd` istekleri tek satırlık, `\n` (satır sonu / LF) ile sonlandırılmış JSON objelerini kabul eder. Aşağıda SCR Terminal yazılımı tarafından desteklenen **34 komutun tamamı** listelenmiştir.

---

### 1. ⚙️ System Commands / Sistem Komutları

*   **System Status / Sistem Durumu (`status`):**
    *   *Usage / Kullanım:* `{"cmd":"status"}`
    *   *Description / Açıklama:* Returns detailed hardware telemetry (battery %, battery voltage, charging status, NTP status, free RAM heap, active services). / Detaylı donanım telemetrisini (batarya yüzdesi, voltajı, şarj durumu, NTP durumu, boş RAM miktarı ve aktif servisleri) döner.
*   **Version Info / Versiyon Bilgisi (`version`):**
    *   *Usage / Kullanım:* `{"cmd":"version"}`
    *   *Description / Açıklama:* Returns firmware build version, codename, board hardware type, and enabled compile-time features. / Yazılım derleme sürümünü, kod adını, donanım tipini ve aktif özellikleri döner.
*   **Haptic Test / Titreşim Testi (`haptic`):**
    *   *Usage / Kullanım:* `{"cmd":"haptic"}`
    *   *Description / Açıklama:* Triggers a short haptic feedback vibration using the DRV2605 driver. / DRV2605 sürücüsü aracılığıyla saatte kısa bir titreşim geri bildirimi tetikler.
*   **Set Brightness / Parlaklık Ayarı (`brightness`):**
    *   *Usage / Kullanım:* `{"cmd":"brightness", "params":{"v":150}}`
    *   *Params / Parametre:* `v` (Integer: `10` to `255`) — Target brightness value. / `10` ile `255` arası parlaklık tamsayı değeri.
*   **Cycle Watchface / Kadran Değiştirme (`watchface`):**
    *   *Usage / Kullanım:* `{"cmd":"watchface", "params":{"style":"next"}}`
    *   *Params / Parametre:* `style` (String: `"next"` or `"prev"`)
*   **Reboot Device / Yeniden Başlatma (`reboot`):**
    *   *Usage / Kullanım:* `{"cmd":"reboot"}`
    *   *Description / Açıklama:* Performs a clean software reboot of the ESP32-S3. / ESP32-S3 çipini yazılımsal olarak temiz bir şekilde yeniden başlatır.
*   **Compass Data / Pusula Bilgisi (`compass`):**
    *   *Usage / Kullanım:* `{"cmd":"compass"}`
    *   *Description / Açıklama:* Returns current vector headings (heading, roll, pitch) from the BHI260AP IMU. / BHI260AP IMU sensöründen anlık yönelim vektörlerini (sapma, yuvarlanma, yunuslama) döner.
*   **Sensor Telemetry / Sensör Telemetrisi (`sensor_data`):**
    *   *Usage / Kullanım:* `{"cmd":"sensor_data"}`
    *   *Description / Açıklama:* Returns live battery data, heap storage, system uptime in seconds, and GPS coordinates if a valid fix exists. / Canlı batarya verilerini, bellek durumunu, saniye cinsinden çalışma süresini ve eğer GPS bağlantısı varsa konum koordinatlarını döner.

---

### 2. 👾 SCR-Bit Pet Commands / Sanal Pet Komutları

*   **Get Pet Stats / Pet İstatistikleri (`pet_status`):**
    *   *Usage / Kullanım:* `{"cmd":"pet_status"}`
    *   *Description / Açıklama:* Returns the virtual pet's stats (Level, XP progress %, Energy, Health, Cleanliness, and wire poop count on the ground). / Sanal petin seviyesini, XP ilerlemesini, Enerji, Sağlık ve Temizlik barlarını ve yerdeki kablo pisliklerinin sayısını döner.
*   **Feed Pet / Besleme (`pet_feed`):**
    *   *Usage / Kullanım:* `{"cmd":"pet_feed"}`
    *   *Description / Açıklama:* Feeds the pet code packets, restoring `Energy` by 20 units and granting XP. / Robota kod paketleri yükleyerek `Enerji` değerini 20 birim artırır ve XP kazandırır.
*   **Heal Pet / İyileştirme (`pet_heal`):**
    *   *Usage / Kullanım:* `{"cmd":"pet_heal"}`
    *   *Description / Açıklama:* Re-calibrates the core system to restore `Health` by 30 units. / Sistem çekirdeğini kalibre ederek petin `Sağlık` durumunu 30 birim iyileştirir.
*   **Clean Pet / Temizleme (`pet_clean`):**
    *   *Usage / Kullanım:* `{"cmd":"pet_clean"}`
    *   *Description / Açıklama:* Sweeps all wire scrap poops from the display and restores `Cleanliness` to 100%. / Ekrandaki tüm kablo atıklarını temizler ve petin `Temizlik` barını 100%'e çıkarır.

---

### 3. 📻 Sub-GHz RF Commands / RF & Jammer Komutları

*   **Start Jammer / Jammer Başlat (`rf_jammer_start`):**
    *   *Usage / Kullanım:* `{"cmd":"rf_jammer_start", "params":{"freq":433920000}}`
    *   *Params / Parametre:* `freq` (Integer: Frequency in Hz. Default: `433920000` / 433.92 MHz). / Hz cinsinden hedef kesici frekansı.
*   **Stop Jammer / Jammer Durdur (`rf_jammer_stop`):**
    *   *Usage / Kullanım:* `{"cmd":"rf_jammer_stop"}`
    *   *Description / Açıklama:* Disables the jammer transmission engine. / Jammer verici sinyalini tamamen kapatır.
*   **Tesla Signal / Tesla Sinyali (`rf_tesla_send`):**
    *   *Usage / Kullanım:* `{"cmd":"rf_tesla_send"}`
    *   *Description / Açıklama:* Transmits a single-pass burst simulation pattern at common automotive charger frequencies. / Yaygın şarj portu frekanslarında tek geçişli bir RF Tesla simülasyon sinyali gönderir.
*   **RF Status / RF Durumu (`rf_status`):**
    *   *Usage / Kullanım:* `{"cmd":"rf_status"}`
    *   *Description / Açıklama:* Queries whether the jammer is active, target frequency, and Tesla burst status. / Jammer'ın aktiflik durumunu, frekansını ve Tesla gönderim durumunu sorgular.

---

### 4. ⌨️ BLE & USB HID Keyboard Commands / Klavye Komutları

*   **Start HID / Klavyeyi Aktifleştir (`hid_start`):**
    *   *Usage / Kullanım:* `{"cmd":"hid_start"}`
    *   *Description / Açıklama:* Initiates Bluetooth HID service, advertising the watch as `SCR-Keyboard`. / Bluetooth HID klavye servisini `SCR-Keyboard` ismiyle yayına açar.
*   **Stop HID / Klavyeyi Kapat (`hid_stop`):**
    *   *Usage / Kullanım:* `{"cmd":"hid_stop"}`
    *   *Description / Açıklama:* Stops BLE advertising and disconnects the keyboard link. / BLE yayınını durdurur ve mevcut HID bağlantılarını sonlandırır.
*   **Run DuckyScript / Script Çalıştır (`hid_run_script`):**
    *   *Usage / Kullanım:* `{"cmd":"hid_run_script", "params":{"path":"pay.txt","ble":true,"layout":"US"}}`
    *   *Params / Parametre:* `path` (String: Absolute or relative SD card file path. If relative, prefix `/badusb/` is applied), `ble` (Boolean: `true` for BLE, `false` for USB), `layout` (Optional String: keyboard layout, e.g. `"US"`, `"TR"`). / SD karttaki script dosyasının tam veya göreceli konumu (göreceli ise `/badusb/` öneki uygulanır), BLE/USB seçimi ve isteğe bağlı klavye düzeni (`layout`).
*   **Run Instant DuckyScript / Anlık Script Çalıştır (`hid_run_instant`):**
    *   *Usage / Kullanım:* `{"cmd":"hid_run_instant", "params":{"script":"DELAY 500\nSTRING Hello","ble":true,"layout":"TR"}}`
    *   *Params / Parametre:* `script` (String: raw Ducky Script content), `ble` (Boolean: `true` for BLE, `false` for USB), `layout` (Optional String: keyboard layout, e.g. `"US"`, `"TR"`). / Doğrudan çalıştırılacak ham Ducky Script kodu, BLE/USB seçimi ve isteğe bağlı klavye düzeni (`layout`).
*   **Set Keyboard Layout / Klavye Düzenini Ayarla (`hid_set_layout`):**
    *   *Usage / Kullanım:* `{"cmd":"hid_set_layout", "params":{"layout":"TR"}}`
    *   *Params / Parametre:* `layout` (String: Target layout code, supported: `US`, `DK`, `UK`, `FR`, `DE`, `HU`, `IT`, `BR`, `PT`, `SI`, `ES`, `SV`, `TR`). / Hedef klavye dil düzeni kodu.
*   **List Script Files / Script Dosyalarını Listele (`hid_list_scripts`):**
    *   *Usage / Kullanım:* `{"cmd":"hid_list_scripts"}`
    *   *Description / Açıklama:* Returns a list of all `.txt`, `.duck`, and `.ducky` files stored in `/badusb` directory of the SD card. / SD karttaki `/badusb` klasöründe bulunan tüm `.txt`, `.duck` ve `.ducky` uzantılı script dosyalarının listesini döner.
*   **Abort Script / Script İptali (`hid_abort_script`):**
    *   *Usage / Kullanım:* `{"cmd":"hid_abort_script"}`
    *   *Description / Açıklama:* Immediately stops any running DuckyScript execution. / Çalışmakta olan DuckyScript (BadUSB/BadBLE) payload'ını anında durdurur.
*   **Get HID Status / HID Durumu (`hid_status`):**
    *   *Usage / Kullanım:* `{"cmd":"hid_status"}`
    *   *Description / Açıklama:* Returns active state, BLE link connection status, USB link connection status, name, layout, and if a payload is currently executing. / HID modülünün aktifliğini, BLE ve USB bağlantı durumlarını, yayın adını, klavye dil düzenini ve çalışan script durumunu döner.

---

### 5. 🏷️ HF NFC Transceiver Commands / NFC Komutları

*   **Start NFC Scan / Kart Okuma Başlat (`nfc_scan`):**
    *   *Usage / Kullanım:* `{"cmd":"nfc_scan"}`
    *   *Description / Açıklama:* Powers up ST25R3916 RF field to listen for ISO-14443A HF tags or NDEF payloads. / NFC modülünü aktif hale getirerek yakındaki kart veya etiketleri taramaya başlar.
*   **Stop NFC Scan / Kart Okumayı Durdur (`nfc_stop`):**
    *   *Usage / Kullanım:* `{"cmd":"nfc_stop"}`
    *   *Description / Açıklama:* Turns off HF reader field to save power. / NFC okuyucu alanını kapatarak güç tasarrufu moduna geçer.
*   **Save Tag / Kartı Kaydet (`nfc_save`):**
    *   *Usage / Kullanım:* `{"cmd":"nfc_save"}`
    *   *Description / Açıklama:* Saves the last scanned tag metadata to the internal SD card storage. / En son okunan NFC kart bilgilerini dahili SD kart belleğine kaydeder.
*   **List Tags / Kartları Listele (`nfc_list`):**
    *   *Usage / Kullanım:* `{"cmd":"nfc_list"}`
    *   *Description / Açıklama:* Returns a list of all saved card profiles stored on the SD card. / SD karta kaydedilmiş tüm NFC kart profillerini listeler.
*   **Delete Tag / Kart Sil (`nfc_delete`):**
    *   *Usage / Kullanım:* `{"cmd":"nfc_delete", "params":{"idx":0}}`
    *   *Params / Parametre:* `idx` (Integer: Index of target tag in list). / Silinecek kartın listedeki sıra indeksi.
*   **Download Tag / Kart İndir (`nfc_download`):**
    *   *Usage / Kullanım:* `{"cmd":"nfc_download", "params":{"idx":0}}`
    *   *Description / Açıklama:* Returns base64-encoded file contents of the selected NFC tag in Flipper NFC format. / Seçilen NFC kartının içeriğini Flipper NFC formatında Base64 şifrelemeyle bilgisayar/telefona indirir.
*   **Export Tags / Flipper Dışa Aktarma (`nfc_export`):**
    *   *Usage / Kullanım:* `{"cmd":"nfc_export"}`
    *   *Description / Açıklama:* Processes saved tag files and exports them directly into the `/nfc/` directory structure. / Kaydedilmiş NFC dosyalarını Flipper Zero cihazlarının okuyabileceği formatta dışa aktarır.

---

### 6. 🌌 LoRa MeshCore Commands / LoRa Komutları

*   **Start LoRa / LoRa Başlat (`lora_start`):**
    *   *Usage / Kullanım:* `{"cmd":"lora_start"}`
    *   *Description / Açıklama:* Turns on the SX1262 LoRa module on frequency `869.618 MHz`. / SX1262 LoRa modülünü `869.618 MHz` frekansında dinleme moduna alır.
*   **Stop LoRa / LoRa Durdur (`lora_stop`):**
    *   *Usage / Kullanım:* `{"cmd":"lora_stop"}`
    *   *Description / Açıklama:* Puts the SX1262 radio into sleep mode to conserve power. / LoRa modülünü güç tasarrufu için uyku moduna alır.
*   **Send Message / Mesaj Gönder (`lora_send`):**
    *   *Usage / Kullanım:* `{"cmd":"lora_send", "params":{"text":"hello"}}`
    *   *Params / Parametre:* `text` (String: Broadcast chat message text). / LoRa mesh kanalı üzerinden gönderilecek genel sohbet mesajı.
*   **Send Advertisement / Reklam Paketi Gönder (`lora_advert`):**
    *   *Usage / Kullanım:* `{"cmd":"lora_advert"}`
    *   *Description / Açıklama:* Dispatches an Ed25519-signed node presence packet detailing coordinates, public key, and name. / Konum, kullanıcı adı ve Ed25519 genel anahtarı içeren imzalı bir varlık bildirimi paketi yayınlar.
*   **Get History / Sohbet Geçmişi (`lora_history`):**
    *   *Usage / Kullanım:* `{"cmd":"lora_history"}`
    *   *Description / Açıklama:* Retrieves the last 20 public messages parsed from the SD card storage file. / SD kartta kayıtlı olan son 20 LoRa mesaj geçmişini listeler.

---

### 7. 🛜 WiFi & BLE Recon Commands / Keşif & Ağ Komutları

*   **WiFi Scan / WiFi Tarama (`recon_wifi`):**
    *   *Usage / Kullanım:* `{"cmd":"recon_wifi"}`
    *   *Description / Açıklama:* Scans 2.4GHz WiFi networks, gathering SSID, RSSI, BSSID, channel, and authentication type. / Çevredeki 2.4GHz WiFi ağlarını taramaya başlar.
*   **BLE Scan / BLE Tarama (`recon_ble`):**
    *   *Usage / Kullanım:* `{"cmd":"recon_ble", "params":{"duration":15}}`
    *   *Params / Parametre:* `duration` (Integer: Scan window duration in seconds. Default: `10`). / Saniye cinsinden BLE tarama penceresi süresi.
*   **Get Results / Tarama Sonuçları (`recon_results`):**
    *   *Usage / Kullanım:* `{"cmd":"recon_results"}`
    *   *Description / Açıklama:* Returns gathered WiFi networks and BLE devices from the last scanning cycle. / Son taramadan elde edilen WiFi ve BLE cihaz listesini döner.
*   **Stop Scan / Taramayı Durdur (`recon_stop`):**
    *   *Usage / Kullanım:* `{"cmd":"recon_stop"}`
    *   *Description / Açıklama:* Cancels any active WiFi scan, BLE scan, deauth attack, or sniffing activity. / Devam eden tarama, deauth veya koklama faaliyetlerini durdurur.
*   **Targeted Deauth / Hedefli Deauth (`recon_deauth`):**
    *   *Usage / Kullanım:* `{"cmd":"recon_deauth", "params":{"bssid":"AA:BB:CC:DD:EE:FF","ch":6}}`
    *   *Params / Parametre:* `bssid` (String: Target network BSSID), `ch` (Integer: Channel). / Hedef ağın BSSID adresi ve yayın kanalı.
*   **Blackout / Genel Deauth (`deauth_all`):**
    *   *Usage / Kullanım:* `{"cmd":"deauth_all"}`
    *   *Description / Açıklama:* Iterates through all channels, broadcasting deauth frames to audit nearby Wi-Fi endpoints. / Tüm kanallarda deauth paketleri yayınlayarak genel bir sinyal karartma testi başlatır.
*   **Packet Sniffer / Paket Koklama (`sniffer_start`):**
    *   *Usage / Kullanım:* `{"cmd":"sniffer_start", "params":{"ch":11}}`
    *   *Params / Parametre:* `ch` (Integer: Wi-Fi channel to monitor. `0` for hopping). / İzlenecek WiFi kanalı (atlama modu için `0`).
*   **Stop Sniffer / Koklamayı Durdur (`sniffer_stop`):**
    *   *Usage / Kullanım:* `{"cmd":"sniffer_stop"}`
    *   *Description / Açıklama:* Stops the passive frame monitoring engine. / Pasif paket koklama motorunu kapatır.
*   **Deauth Detect / Deauth Algılayıcı (`deauth_detect`):**
    *   *Usage / Kullanım:* `{"cmd":"deauth_detect"}`
    *   *Description / Açıklama:* Puts the radio into passive monitoring, triggering event warnings if deauth bursts are detected. / Çevre kaynaklı deauth saldırılarını pasif olarak tarar ve algıladığında uyarır.
*   **Evil Twin AP / Sahte Erişim Noktası (`evil_twin`):**
    *   *Usage / Kullanım:* `{"cmd":"evil_twin", "params":{"ssid":"MyFreeWiFi","ch":1}}`
    *   *Params / Parametre:* `ssid` (String: AP Name), `ch` (Integer: Wi-Fi Channel). / Sahte ağ adı ve yayın yapacağı WiFi kanalı.
*   **Stop Evil Twin / Sahte Erişimi Kapat (`evil_twin_stop`):**
    *   *Usage / Kullanım:* `{"cmd":"evil_twin_stop"}`
    *   *Description / Açıklama:* Turns off the rogue access point. / Sahte Wi-Fi erişim noktasını kapatır.

---

## 🔵 Bluetooth Terminal Connection Guide / Bağlantı Kılavuzu

### English
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

### Türkçe
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

<br>

> [!WARNING]
> ## ⚠️ EDUCATIONAL PURPOSES ONLY / SADECE EĞİTİM AMAÇLIDIR
> **English:** This firmware and its features (such as RF transmission, Wi-Fi deauthentication, packet sniffing, etc.) are developed strictly for educational, testing, and authorized security research purposes. The developers and contributors take no responsibility for any misuse, damage, or legal consequences resulting from illegal operations of this software. Always comply with local radio communication and cybersecurity laws.
> 
> **Türkçe:** Bu yazılım ve içerdiği özellikler (RF sinyal gönderimi, Wi-Fi deauth, paket koklama vb.) yalnızca eğitim, test ve yetkili güvenlik araştırmaları amacıyla geliştirilmiştir. Geliştiriciler ve katkıda bulunanlar, bu yazılımın yasal olmayan veya zararlı amaçlarla kullanılmasından ötürü hiçbir sorumluluk veya yasal yükümlülük kabul etmez. Her zaman yerel radyo frekansı ve siber güvenlik yasalarına uyunuz.
