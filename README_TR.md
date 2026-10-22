# Freslest OS 1.0 — Windows kaynak ve derleme paketi

**Bu klasör, Windows'ta derlenip açılış testleri geçen Freslest 1.0 uygulamalarının kaynaklarını ve firmware dosyalarını içerir. Doğrulama sonucu DOGRULAMA.txt içindedir.**

## Kullanım

- **Freslest Installer.exe**: Açılış ekranında **Dil / Language** listesinden **Türkçe** veya **English** seç; ikinci sayfada ESP32-S3 COM portunu seç. Kurulum adımları, uyarılar, sonuç ekranı ve installer günlükleri seçilen dilde gösterilir. Firmware yüklemesinden sonra ayrı tamamlandı ekranı açılır. Python veya ESP-IDF kurulumu gerekmez.
- **Freslest USB Viewer.exe**: Doğrudan açılır ve Freslest cihazını otomatik arar. Ekrana tıklayınca PC klavyesi ve faresi cihazı kontrol eder. Native USB CDC ve USB-UART desteklenir; Windows ikisini de COM portu olarak gösterebilir.

Viewer için cihazda Settings > Display > USB PC Viewer açık olmalıdır. Installer kullanırken Viewer ve Arduino seri monitörünü kapat. Port görünmüyorsa BOOT basılıyken RESET yapıp port listesini yenile. Yükleme hatasında 115200 hızını deneyebilirsin. Native USB ile ekran aktarımı genellikle UART bağlantısından daha hızlıdır. USB kablosu veri aktarımını desteklemelidir.

Kurulum OS, Doom, WAD, OTA seçim verisi ve başlangıç LittleFS içeriğini yazar. LittleFS dosyalarını önceden yedekle. Başka bir bölüme toplu flash silme uygulanmaz. Firmware yazılırken programın kapanması engellenir.

## Kaynaklar ve derleme

`installer/`, `viewer/` ve `tests/` kaynakları korunur. OS firmware'in uygulama sürüm alanı **6.16 → 1.0** olarak damgalanmış, XOR sağlama toplamı ve SHA256 özeti yeniden hesaplanmıştır. System Info bu alanı okuduğu için yeni firmware yüklendiğinde **Freslest OS: 1.0** gösterir. Kod yeniden derlenmemiştir; sürüm ve bütünlük alanları dışında OS baytları aynıdır. Doom ve diğer altı firmware dosyası özgün halleriyle korunur. Ayrıntılı kanıt `firmware/VERSION_REPORT.json`, tekrarlanabilir işlem `firmware_release.py` içindedir. Tam ESP32-S3 kaynak paketindeki `OS/CMakeLists.txt` dosyasında da `PROJECT_VER` değeri 1.0 yapılmıştır.

64 bit Windows üzerinde Python 3.11 kuruluysa `build_windows.bat` iki gerçek Windows EXE üretir ve paketli uygulamaların görünür pencere açıp normal kapanmasını test eder. Bağımlılıklar `requirements-build.txt` içinde sabitlenmiştir. GitHub Actions akışı `.github/workflows/windows-exes.yml` içindedir.

## Değişiklikler

- İstenen iki EXE adı: `Freslest Installer.exe`, `Freslest USB Viewer.exe`.
- Installer açılışında Türkçe ve English seçimi; geri gidip dil değiştirirken COM portu ve hız korunur.
- Gerçek OS firmware sürümü 1.0; System Info, açılış ve kurtarma ekranları aynı sürüm alanını kullanır.
- Kurulumda bağımsız tamamlandı ekranı, hata sonrası tekrar deneme ve günlük kaydetme.
- Flaşlama çıktısında stdout/stderr ve satır sonlarının korunması.
- Yükleme hızını seçme; yazma sürerken pencerenin yanlışlıkla kapatılmasını engelleme.
- Viewer otomatik bağlantı, en son kareyi gösterme, klavye/fare ve yeniden bağlanma davranışları korunmuştur.
- Viewer başlangıç penceresi 960 × 680 yapılmıştır.

## Doğrulama sınırı

Windows derleme ve açılış testi tamamlandı. Sonuç raporları hazır EXE paketindeki Test_Raporlari klasöründe bulunur. Bu test ortamına gerçek ESP32-S3 bağlı değildir; fiziksel flaşlama, ekrandan aktarım ve cihaz kontrolü bu ortamda test edilemez. Viewer ayrı Doom veya updater firmware'i açıkken OS ekranını alamaz.

## Tanılama

Bir sorun araştırılırken `"Freslest Installer.exe" --diagnostics rapor.json` komutu cihazla iletişim kurmadan flaşlama bileşenlerini ve gömülü firmware dosyalarını kontrol eder.
