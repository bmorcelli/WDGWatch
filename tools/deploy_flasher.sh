#!/usr/bin/env bash

# Renkli terminal çıktıları
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0;m' # No Color

echo -e "${BLUE}=== WDGWatch Web Flasher Deployer ===${NC}"

# 1. Kullanıcıdan yeni sürüm numarasını al
current_version=$(git describe --tags --abbrev=0 2>/dev/null || echo "v2.4.1")
echo -e "${YELLOW}En son git etiketi: ${current_version}${NC}"
read -p "Yayınlamak istediğiniz sürüm numarası (örn: v2.4.2): " new_version

if [ -z "$new_version" ]; then
    echo -e "${RED}Hata: Sürüm numarası boş bırakılamaz!${NC}"
    exit 1
fi

# 2. Firmware derlemesi yap
echo -e "${BLUE}[1/4] Firmware derleniyor...${NC}"
~/.platformio/penv/bin/pio run -e twatch-ultra
if [ $? -ne 0 ]; then
    echo -e "${RED}Hata: PlatformIO derleme işlemi başarısız oldu!${NC}"
    exit 1
fi
echo -e "${GREEN}✔ Derleme başarılı!${NC}"

# 3. Binary dosyalarını taşı
echo -e "${BLUE}[2/4] Dosyalar kopyalanıyor...${NC}"
mkdir -p web-flasher/firmware
cp .pio/build/twatch-ultra/bootloader.bin web-flasher/firmware/
cp .pio/build/twatch-ultra/partitions.bin web-flasher/firmware/
cp .pio/build/twatch-ultra/firmware.bin web-flasher/firmware/

# boot_app0 dosyasını bul ve kopyala
boot_app0_path="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
if [ -f "$boot_app0_path" ]; then
    cp "$boot_app0_path" web-flasher/firmware/
else
    echo -e "${YELLOW}Uyarı: boot_app0.bin dosyası platformio paketlerinde bulunamadı. Kopyalanamadı.${NC}"
fi

# 4. manifest.json dosyasını güncelle
echo -e "${BLUE}[3/4] manifest.json güncelleniyor...${NC}"
cat <<EOT > web-flasher/manifest.json
{
  "name": "SCR Terminal (WDGWatch)",
  "version": "$new_version",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "firmware/bootloader.bin", "offset": 0 },
        { "path": "firmware/partitions.bin", "offset": 32768 },
        { "path": "firmware/boot_app0.bin", "offset": 57344 },
        { "path": "firmware/firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
EOT

# 5. Git commit ve push işlemleri
echo -e "${BLUE}[4/4] Dosyalar GitHub Pages dalına gönderiliyor...${NC}"
git add -f web-flasher/firmware
git add web-flasher/manifest.json
git commit -m "feat: deploy web flasher version $new_version"

# GitHub gh-pages dalına subtree push yap
git subtree push --prefix web-flasher origin gh-pages
if [ $? -ne 0 ]; then
    echo -e "${RED}Hata: GitHub Pages dalına push yapılamadı!${NC}"
    exit 1
fi

echo -e "${GREEN}✔ Başarıyla tamamlandı! Sayfa birkaç saniye içinde güncellenecektir.${NC}"
echo -e "${BLUE}Adres: https://sacriphanius.github.io/WDGWatch/${NC}"
