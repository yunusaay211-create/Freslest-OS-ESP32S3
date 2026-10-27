# Freslest OS 1.0 — LCD/VGA güncellemesi

ESP32-S3 N16R8 için C/C++ tabanlı OS ve Doom kaynakları bu pakettedir. Windows kurucusu ve USB Viewer ayrı PC uygulamalarıdır.

- **ILI9488 480×320** ve **ILI9341 320×240** kurulum profilleri.
- ILI9341’de gerçek 320×240 menüler, ayar sayfaları ve eşleşen fare seçim alanları; diğer uygulamalarda 480×320 uyumluluk tuvali.
- Doom’un ILI9341 yön ayarı OS ile eşleşir.
- Kompozit AV kaldırıldı. LCD’nin aynı RGB565 karesi, fiziksel **RGB332/256 renk VGA** çıkışına dönüştürülür. Ayrı VGA arayüzü yoktur.
- VGA hedefi 640×480 / yaklaşık 60 Hz. Settings → Display üzerinden LCD/VGA seçilir; onaydan sonra yeniden başlar. BACK+RESET LCD’ye döndürür.
- USB PC Viewer, Türkçe/İngilizce kurucu, uygulamalar, NES ve Doom kaynakları korunur.

## Dosyalar

- [Release notu](RELEASE_LCD_VGA.md)
- [VGA pin/direnç tablosu ve sınırlar](VGA_DISPLAY_TR.md)
- [LCD seçimi, derleme ve doğrulama](LCD_SUPPORT_TR.md)
- OS kaynakları: `OS/freslest`; Doom kaynakları: `Doom`; ortak sürücüler: `shared_components`.

## Derleme

ESP-IDF v5.4.4 ortamında `python tools/build_lcd_profiles.py` iki ekranın OS/Doom firmware’lerini derler ve tüm flash bölümlerinin boyut/hash değerlerini doğrular. Windows x64 Python 3.11 ortamında `build_windows.bat` kurucu ve Viewer EXE’lerini üretir. CI `.github/workflows/windows-exes.yml` dosyasındadır.

Kaynak kodu bu güncellemede yeniden derlenir; eski `firmware-payload` yalnız veri/baseline kaynağıdır. Yeni EXE güncel ekran profillerini paketler. OS sürüm alanı `OS/CMakeLists.txt` içindeki 1.0 değerini kullanır.

VGA deneyseldir: fiziksel sinyal kesilmelerinin kesin giderildiği iddia edilmez. GPIO8/41 LCD ve GPIO13 SD ile ortaktır; PCB/mod anahtarlaması bu hatları ayırmalıdır. Kurulumdan önce not, dosya ve ROM’ları yedekleyin. Fiziksel ESP32-S3/LCD/VGA testi bu ortamda yapılamaz.
