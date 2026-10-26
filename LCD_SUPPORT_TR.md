# Freslest: kurulumda LCD seçimi

Kurulum akışı: Başlangıç → **1. Ekranlar** → **2. COM portu** → **3. Firmware yükle**.

| Seçim | Fiziksel ekran | OS görüntüsü | Doom görüntüsü |
|---|---|---|---|
| ILI9488 (varsayılan) | 480×320 | Mevcut boyut | Mevcut ölçekli / ortalanmış mod |
| ILI9341 | 320×240 | 480×320 mantıksal arayüzün tamamı 320×240'a ölçeklenir | 320×240 doğrudan RGB565 |

Ekran seçimi geri gidildiğinde ve dil değiştirildiğinde korunur. Yükleme onayında seçilen model gösterilir. Kurucu, seçilen modelin **hem OS hem Doom** dosyasını ve yedi flash bölümünün boyut/hash değerlerini kontrol eder. Derlenmiş OS/Doom içindeki model etiketi uyuşmazsa veya seçilen profil eksikse flash yazılmaz.

## Bağlantılar

İki ekran aynı SPI pinlerini kullanır: MOSI/SDI=11, SCK=12, CS=10, DC=9, RST=8. MISO kullanılmaz. Mevcut MOSFET üzerinden arka ışık kontrolü GPIO41'dir. Ekran modülünün güç bağlantısını kendi modülünün özelliklerine göre yap.

## Hazır EXE hakkında

Bu güncelleme **kaynak kodu güncellemesidir**. Eklenen ILI9341 kodu mevcut `Freslest Installer(2).exe` içine işlenmiş değildir. Arşivdeki eski firmware payload'ı ILI9488 sürümüdür; dosyasını yeniden adlandırmak ILI9341 desteği sağlamaz. İki profili kaynak kodundan derleyip yeni Windows EXE oluşturmak gerekir.

## İki ekranı içeren kurucuyu derleme

ESP-IDF **v5.4.4**, ESP32-S3 araç zinciri ve Python bulunan ESP-IDF komut isteminde, proje kökünde:

```powershell
python tools/build_lcd_profiles.py
```

Bu komut her ekran için OS ve Doom'u ayrı `build-ili9488` / `build-ili9341` dizinlerine derler. WAD/LittleFS verilerini orijinal payload'dan alır; profil dosyalarını `firmware/ili9488` ve `firmware/ili9341` altında hazırlar. İşlem, derleme başarısızsa veya binary bölüm sınırını aşıyorsa hata verir.

Ardından Windows'ta 64-bit Python **3.11** ile:

```powershell
py -3.11 -m pip install -r requirements-build.txt
py -3.11 build_windows.py
```

Yeni kurucu `dist/Freslest Installer.exe` olur. `build_windows.py`, iki profil de hazır değilse EXE paketlemesini durdurur.

GitHub Actions dosyası da aynı sırayla önce iki ekranın firmware'ini, ardından Windows EXE'lerini oluşturacak şekilde güncellenmiştir. Bu çalışma sırasında workflow çalıştırılmamıştır.

## Yalnız firmware derlemek

```powershell
idf.py -C OS -B build-ili9341 -DFRESLEST_LCD_MODEL=9341 build
idf.py -C Doom -B build-ili9341 -DFRESLEST_LCD_MODEL=9341 build
```

ILI9488 için hem `build-ili9341` hem `9341` değerlerini sırasıyla `build-ili9488` ve `9488` yap. OS ve Doom aynı model için derlenmelidir. Derleme modeli cihazdan otomatik tespit edilmez; kurulumda kullanıcı seçer.

Arayüz, USB Viewer ve fare koordinatları mantıksal 480×320 alanında kalır. ILI9341 üzerinde yazılar ve şekiller tam ekrana ölçeklenir; uygulamalar ayrı bir 320×240 yerleşimiyle yeniden yazılmamıştır. Doom'un oyun kodu değiştirilmemiştir; yalnız LCD sürücüsü uyarlanmıştır. Ekran menüsü derlenen LCD modelini gösterir.

ILI9341 firmware'i, ILI9488 hedefli USB güncelleme paketini farklı kart modeli olarak reddeder. Bu paket oluşturucusu iki profilli **ilk kurulum** içindir; ILI9341'e özel USB güncelleme ZIP'i üretmez.

## Doğrulama

Yeni ekran testleri gerçek OS `gfxPresent` gövdesini ve Doom SPI LCD sürücüsünü host ortamında derleyip çalıştırır. Piksel sınırları, renk baytları, tüm ekranın ölçeklenmesi, değişen satırlar, fare hareketi/geri yükleme ve mevcut AV yoluna yönlendirme kontrol edilir. Kurucu testleri model seçimi, eksik profil, yanlış model ve flash komutundaki dosya yollarını kontrol eder.

Bu ortamda ESP-IDF araç zinciri, Windows ve fiziksel LCD bulunmadığından firmware/EXE derlemesi ve cihaz üzerinde test yapılmamıştır. Ayrıntılar `validation/lcd-support-checks.txt` içindedir.
