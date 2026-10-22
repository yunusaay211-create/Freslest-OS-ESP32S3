# Freslest OS 1.0 — Windows kaynak ve derleme paketi

**Bu paket hazır EXE içermez. Windows derlemesi için gerekli kaynaklar ve firmware dosyalarıdır. Durum ve çalıştırılan testler DOGRULAMA.txt içindedir.**

## Kullanım

- **Freslest Installer.exe**: Açılış ekranında Devam et; ikinci sayfada ESP32-S3 COM portunu seç. Firmware yüklemesinden sonra ayrı tamamlandı ekranı açılır. Python veya ESP-IDF kurulumu gerekmez.
- **Freslest USB Viewer.exe**: Doğrudan açılır ve Freslest cihazını otomatik arar. Ekrana tıklayınca PC klavyesi ve faresi cihazı kontrol eder. Native USB CDC ve USB-UART desteklenir; Windows ikisini de COM portu olarak gösterebilir.

Viewer için cihazda Settings > Display > USB PC Viewer açık olmalıdır. Installer kullanırken Viewer ve Arduino seri monitörünü kapat. Port görünmüyorsa BOOT basılıyken RESET yapıp port listesini yenile. Yükleme hatasında 115200 hızını deneyebilirsin. Native USB ile ekran aktarımı genellikle UART bağlantısından daha hızlıdır. USB kablosu veri aktarımını desteklemelidir.

Kurulum OS, Doom, WAD, OTA seçim verisi ve başlangıç LittleFS içeriğini yazar. LittleFS dosyalarını önceden yedekle. Başka bir bölüme toplu flash silme uygulanmaz. Firmware yazılırken programın kapanması engellenir.

## Kaynaklar ve derleme

`installer/`, `viewer/` ve `tests/` kaynakları korunur. Firmware, önceki tam proje ZIP'indeki dosyalardan hashleri doğrulanarak alınmıştır; bu çalışmada firmware yeniden derlenmemiş ve değiştirilmemiştir. Windows uygulamaları Freslest OS 1.0 adıyla paketlenmiştir. Cihaz yazılımının iç sürüm metinleri önceki firmware ile aynıdır.

64 bit Windows üzerinde Python 3.11 kuruluysa `build_windows.bat` iki gerçek Windows EXE üretir ve paketli uygulamaların görünür pencere açıp normal kapanmasını test eder. Bağımlılıklar `requirements-build.txt` içinde sabitlenmiştir. GitHub Actions akışı `.github/workflows/windows-exes.yml` içindedir.

## Değişiklikler

- İstenen iki EXE adı: `Freslest Installer.exe`, `Freslest USB Viewer.exe`.
- Kurulumda bağımsız tamamlandı ekranı, hata sonrası tekrar deneme ve günlük kaydetme.
- Flaşlama çıktısında stdout/stderr ve satır sonlarının korunması.
- Yükleme hızını seçme; yazma sürerken pencerenin yanlışlıkla kapatılmasını engelleme.
- Viewer otomatik bağlantı, en son kareyi gösterme, klavye/fare ve yeniden bağlanma davranışları korunmuştur.
- Viewer başlangıç penceresi 960 × 680 yapılmıştır.

## Doğrulama sınırı

EXE dosyaları üretildiğinde Windows derleme ve açılış testinin sonuçları dist içindeki JSON dosyalarına yazılır. Bu test ortamına gerçek ESP32-S3 bağlı değildir; fiziksel flaşlama, ekrandan aktarım ve cihaz kontrolü bu ortamda test edilemez. Viewer ayrı Doom veya updater firmware'i açıkken OS ekranını alamaz.
