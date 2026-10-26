# Freslest: kurulumda LCD seçimi

## ILI9341 netlik/yön düzeltmesi — kaynak hazır

Menüler, görev değiştirici ve ayarlar gerçek 320×240 tuvale taşındı; yazı pikselleri ölçeklenmiyor. Fare hedefleri yeni satırlarla eşleşiyor. Doom ILI9341 MADCTL kaydı OS yönüyle eşleştirildi (0xB8). ILI9488 yönü korunuyor. Yerel C/C++ sürücü, geometri, fare ve Wi-Fi sayfa testleri geçti. Bu son düzeltme için ESP-IDF firmware/Windows EXE derlemesi henüz yapılmadı: GitHub kaynak yüklemesi otomatik onay denetiminde engellendi. Aşağıdaki eski EXE/CI bilgileri önceki sürüme aittir. Fiziksel ekran testi yapılmadı.


Kurulum akışı: Başlangıç → **1. Ekranlar** → **2. COM portu** → **3. Firmware yükle**.

| Seçim | Fiziksel ekran | OS görüntüsü | Doom görüntüsü |
|---|---|---|---|
| ILI9488 (varsayılan) | 480×320 | Mevcut boyut | Mevcut ölçekli / ortalanmış mod |
| ILI9341 | 320×240 | Menüler ve ayarlar yerel 320×240; diğer uygulamalarda uyumluluk ölçeklemesi | 320×240 doğrudan RGB565 |

Ekran seçimi geri gidildiğinde ve dil değiştirildiğinde korunur. Yükleme onayında seçilen model gösterilir. Kurucu, seçilen modelin **hem OS hem Doom** dosyasını ve yedi flash bölümünün boyut/hash değerlerini kontrol eder. Derlenmiş OS/Doom içindeki model etiketi uyuşmazsa veya seçilen profil eksikse flash yazılmaz.

## Bağlantılar

İki ekran aynı SPI pinlerini kullanır: MOSI/SDI=11, SCK=12, CS=10, DC=9, RST=8. MISO kullanılmaz. Mevcut MOSFET üzerinden arka ışık kontrolü GPIO41'dir. Ekran modülünün güç bağlantısını kendi modülünün özelliklerine göre yap.

## Hazır EXE hakkında

Güncel kurucu **Freslest-Installer-LCD.exe** adıyla üretildi. ILI9488 ve ILI9341 için OS/Doom firmware'leri ESP-IDF v5.4.4 ile derlendi, bölüm boyutları ve hash değerleri doğrulandı. Windows'taki kurucu/Viewer testleri ve paketlenmiş EXE'lerin Türkçe/İngilizce açılış testleri geçti. Fiziksel LCD üzerinde test yapılmadı.

Firmware derleme kanıtı: https://github.com/yunusaay211-create/Freslest-OS-ESP32S3/actions/runs/40377333282
Windows paketleme ve açılış testi: https://github.com/yunusaay211-create/Freslest-OS-ESP32S3/actions/runs/40378905818

Arşivdeki eski `firmware-payload` ILI9488 temel sürümüdür. Yeni EXE her iki modele ait yeniden derlenmiş dosyaları içerir. Kaynaktan tekrar üretmek için aşağıdaki adımları kullan.

## İki ekranı içeren kurucuyu derleme

ESP-IDF **v5.4.4**, ESP32-S3 araç zinciri ve Python bulunan ESP-IDF komut isteminde, proje kökünde:

```powershell
python tools/build_lcd_profiles.py
```

Bu komut her ekran için OS ve Doom'u derler. Ortak bağımlılıkları tekrar derlememek için her projenin `build-lcd` dizinini kullanır; tamamlanan ilk profil, model değiştirilmeden önce ayrı klasöre kopyalanır. WAD/LittleFS verilerini orijinal payload'dan alır; profil dosyalarını `firmware/ili9488` ve `firmware/ili9341` altında hazırlar. İşlem, derleme başarısızsa veya binary bölüm sınırını aşıyorsa hata verir.

Ardından Windows'ta 64-bit Python **3.11** ile:

```powershell
py -3.11 -m pip install -r requirements-build.txt
py -3.11 build_windows.py
```

Yeni kurucu `dist/Freslest Installer.exe` olur. `build_windows.py`, iki profil de hazır değilse EXE paketlemesini durdurur.

GitHub Actions dosyası da aynı sırayla önce iki ekranın firmware'ini, ardından Windows EXE'lerini oluşturacak şekilde güncellenmiştir. İki LCD için derleme ve ayrı Windows paketleme workflow çalışmaları başarıyla tamamlanmıştır.

## Yalnız firmware derlemek

```powershell
idf.py -C OS -B build-ili9341 -DFRESLEST_LCD_MODEL=9341 build
idf.py -C Doom -B build-ili9341 -DFRESLEST_LCD_MODEL=9341 build
```

ILI9488 için hem `build-ili9341` hem `9341` değerlerini sırasıyla `build-ili9488` ve `9488` yap. OS ve Doom aynı model için derlenmelidir. Derleme modeli cihazdan otomatik tespit edilmez; kurulumda kullanıcı seçer.

ILI9341 ana menü, uygulama/oyun/ayar listeleri, görev değiştirici ve ayar sayfaları doğrudan 320×240 pikselde çizilir. Font pikselleri küçültülmez. Başlık/alt bilgi ve fare hedefleri aynı yerleşimi kullanır. Diğer uygulamalar 480×320 uyumluluk alanını kullanır. USB Viewer etkin tuvalin gerçek boyutunu alır. Doom ILI9341 yön kaydı OS ile aynı 0xB8 değerindedir; ILI9488 0xE8 değerini korur. Ekran menüsü derlenen LCD modelini gösterir.

ILI9341 firmware'i, ILI9488 hedefli USB güncelleme paketini farklı kart modeli olarak reddeder. Bu paket oluşturucusu iki profilli **ilk kurulum** içindir; ILI9341'e özel USB güncelleme ZIP'i üretmez.

## Doğrulama

Yeni ekran testleri gerçek OS `gfxPresent` gövdesini ve Doom SPI LCD sürücüsünü host ortamında derleyip çalıştırır. Piksel sınırları, renk baytları, tüm ekranın ölçeklenmesi, değişen satırlar, fare hareketi/geri yükleme ve mevcut AV yoluna yönlendirme kontrol edilir. Kurucu testleri model seçimi, eksik profil, yanlış model ve flash komutundaki dosya yollarını kontrol eder.

Firmware ve EXE derlemeleri GitHub Actions sunucularında tamamlandı. Fiziksel cihaz testi yapılmadı. Ayrıntılar `validation/lcd-support-checks.txt` içindedir.


## LCD/VGA geçişi

Kompozit AV kaldırıldı. VGA ayrı arayüz çizmez; seçilen LCD’nin aynı RGB565 tuvali RGB332’ye dönüştürülür. `VGA_DISPLAY_TR.md` ve `RELEASE_LCD_VGA.md` geçerli bağlantı ve değişiklik bilgilerini içerir.
