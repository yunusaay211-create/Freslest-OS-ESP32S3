# Freslest LCD seçimi ve VGA

Kurulum: Başlangıç → **1. Ekranlar** → **2. COM portu** → **3. Firmware yükle**.

| Profil | LCD | OS tuvali | Doom |
|---|---|---|---|
| ILI9488 (varsayılan) | 480×320 | 480×320 | Mevcut ölçekli/ortalanmış mod |
| ILI9341 | 320×240 | Menü/ayarlar 320×240; diğer uygulamalar 480×320 uyumluluk tuvali | 320×240 RGB565; yön kaydı OS ile eşleşen 0xB8 |

Ekran seçimi geri gidildiğinde ve dil değiştirildiğinde korunur. Kurucu seçilen modelin **hem OS hem Doom** dosyasını ve yedi flash bölümünün boyut/hash değerlerini doğrular. Model etiketi uyuşmazsa veya profil eksikse yazma başlatılmaz.

## LCD bağlantısı

MOSI/SDI=11, SCK=12, CS=10, DC=9, RST=8, arka ışık MOSFET PWM=41. MISO kullanılmaz. Modülün güç bağlantısını kendi modülünün özelliklerine göre yap.

## Görüntü

ILI9341 ana menü, uygulama/oyun/ayar listeleri, görev değiştirici ve ayar sayfaları doğrudan 320×240 pikselde çizilir; fontlar küçültülmez. Başlık, alt bilgi ve fare hedefleri aynı yerleşime uyar. USB Viewer etkin tuvalin gerçek boyutunu alır. Diğer uygulamalar 480×320 uyumluluk tuvalinde kalır.

Kompozit AV kaldırıldı. VGA ayrı arayüz çizmez: LCD için çizilen **aynı RGB565 tuvali RGB332’ye çevirip 640×480’e ölçekler**. Doom indeksli karesi aynı paletle RGB332’ye çevrilir. Fiziksel çıkış RGB332/256 renktir. Settings → Display değişimi yeniden başlatma gerektirir; BACK+RESET LCD’ye döndürür. Bağlantılar ve deneysel VGA sınırları `VGA_DISPLAY_TR.md` dosyasındadır.

## Yeniden üretme

ESP-IDF v5.4.4 / ESP32-S3 araç zinciri ortamında:

```powershell
python tools/build_lcd_profiles.py
```

Her modelin OS ve Doom’u yeniden derlenir. Ortak SDK derlemesi tekrar kullanılabilir; ilk profil model değiştirilmeden ayrı klasöre kopyalanır. WAD/LittleFS veri dosyaları orijinal payload’dan alınır. `firmware/ili9488` ve `firmware/ili9341` hazırlanır; bölüm taşması veya yanlış hash derlemeyi durdurur.

Windows x64 Python 3.11:

```powershell
py -3.11 -m pip install -r requirements-build.txt
py -3.11 build_windows.py
```

Çıktı: `dist/Freslest Installer.exe`. İki profil olmadan paketleme durur. Bu kurucu ilk kurulum içindir; modele özel USB güncelleme ZIP’i üretmez.

## Doğrulama

`tests/test_lcd_profiles.py` gerçek OS ve Doom çıkış kodunu host ortamında çalıştırır; piksel sınırları, endian, fare geri yükleme, doğal menü geometrisi ve VGA’ya aynı tuval aktarımını denetler. `tests/test_vga_transition.py` gerçek VGA backend/renk dönüşümünü, pin/zamanlama konfigürasyonunu, NVS geçişini, BACK kurtarmasını, hata temizliğini ve sınırlı görev yığını kullanımını test eder. Masaüstü kurucu/Viewer testleri ve EXE açılış kontrolleri Windows CI’da çalışır.

Son derleme sonuçları `validation/lcd-support-checks.txt` içindedir. Fiziksel ESP32-S3/LCD/VGA testi yapılmadı.
