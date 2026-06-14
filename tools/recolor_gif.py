#!/usr/bin/env python3
"""
WDGWatch Theme GIF Recolorer
Masaüstündeki dedsec.gif dosyasını WDGWatch SCR Terminal tema renklerine boyar.

Tema Paleti:
  BG    : #000000  (siyah arka plan)
  DARK  : #003840  (koyu cyan)
  DIM   : #007280  (orta cyan)
  MAIN  : #00E5FF  (ana cyan / electric glow)
"""

from PIL import Image
import os
import sys

INPUT  = os.path.expanduser("~/Masaüstü/dedsec.gif")
OUTPUT = os.path.expanduser("~/Masaüstü/dedsec_themed.gif")

# WDGWatch renk paleti (luminance eşikleri → RGB hedefi)
PALETTE = [
    (0,   40,  (0,   0,   0  )),   # 0-40   → Siyah BG
    (40,  90,  (0,   56,  64 )),   # 40-90  → #003840 Dark
    (90,  160, (0,   114, 128)),   # 90-160 → #007280 Dim
    (160, 210, (0,   180, 210)),   # 160-210→ Ara ton
    (210, 256, (0,   229, 255)),   # 210-255→ #00E5FF Ana Cyan Glow
]

def map_luminance(lum):
    """Luminance değerini tema rengine çevirir."""
    for lo, hi, color in PALETTE:
        if lo <= lum < hi:
            return color
    return (0, 229, 255)

def recolor_frame(frame):
    """Tek bir GIF frame'ini tema renklerine dönüştürür."""
    # RGBA'ya çevir (şeffaflık korunur)
    rgba = frame.convert("RGBA")
    pixels = rgba.load()
    w, h = rgba.size

    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            if a < 30:
                # Şeffaf pikseller siyah BG olsun
                pixels[x, y] = (0, 0, 0, 255)
                continue
            # Luminance hesapla (perceptual)
            lum = int(0.299 * r + 0.587 * g + 0.114 * b)
            nr, ng, nb = map_luminance(lum)
            pixels[x, y] = (nr, ng, nb, 255)

    return rgba

def main():
    if not os.path.exists(INPUT):
        print(f"[HATA] Dosya bulunamadı: {INPUT}")
        sys.exit(1)

    src = Image.open(INPUT)
    print(f"[OK] Kaynak GIF: {INPUT}")
    print(f"     Boyut: {src.size[0]}x{src.size[1]}px")
    print(f"     Format: {src.format}")

    # Frame sayısını bul
    try:
        n_frames = src.n_frames
    except AttributeError:
        n_frames = 1
    print(f"     Frame sayısı: {n_frames}")

    frames = []
    durations = []

    for i in range(n_frames):
        src.seek(i)
        try:
            dur = src.info.get("duration", 100)
        except Exception:
            dur = 100
        durations.append(dur)

        colored = recolor_frame(src.copy())
        # PIL GIF için P moduna çevir (256 renk paleti)
        frames.append(colored.convert("P", palette=Image.ADAPTIVE, colors=64))

        pct = int((i + 1) / n_frames * 100)
        bar = "#" * (pct // 5) + "-" * (20 - pct // 5)
        print(f"\r  [{bar}] {pct}% (frame {i+1}/{n_frames})", end="", flush=True)

    print()

    # GIF olarak kaydet
    frames[0].save(
        OUTPUT,
        save_all=True,
        append_images=frames[1:],
        loop=0,
        duration=durations,
        optimize=False
    )

    size_kb = os.path.getsize(OUTPUT) / 1024
    print(f"[OK] Kaydedildi: {OUTPUT}")
    print(f"     Boyut: {size_kb:.1f} KB")
    print(f"[DONE] Masaüstünde 'dedsec_themed.gif' olarak hazır!")

if __name__ == "__main__":
    main()
