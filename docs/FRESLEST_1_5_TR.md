# Freslest OS 1.5 · ESP32-S3 N16R8

Bu sürüm `codex/audio-sd-recovery` dalındaki `42af5a3` kaynağını temel alır. 16 MB flash yerleşimi ve iki OS OTA yuvası korunur. ILI9488 klasik arayüzü 480×320, ILI9341 klasik menüleri 320×240 tuval kullanır. VGA'da yeni masaüstü 640×480, pencere içindeki mevcut uygulama tuvali 480×320'dir.

## VGA ve USB

- Hedef VGA zamanlaması 640×480, 800×525 toplam, 25.175 MHz ve yaklaşık 59.94 Hz. Çift PSRAM kare ve 20 satırlık dahili tamponlar sürer. OS hazır kareyi beklerken arayüzü bloklamaz. Uzayan aktarımda sürücüye VBlank yeniden eşitleme isteği gönderir. DMA tamamlanana kadar ilgili kareyi tekrar kullanmaz.
- VGA modunda LCD SPI ve PWM başlatılmaz. RGB332 hattındaki GPIO41 ve GPIO8, LCD arka ışık ve reset hatlarıyla ortaktır. Fiziksel olarak LCD'yi kapalı tutmak için bu ortak hatlar kartta ayrılmalıdır. VGA bağlantı tablosu `VGA_DISPLAY_TR.md` dosyasındadır. Fiziksel BACK ile RESET, derlenmiş LCD profiline döner.
- USB Host, takılı aygıtları HID sürücüsü hazır olduktan sonra işler. Ayrılma ve aktarım hatası basılı girişleri temizler. Geçici bir endpoint hatasında aktarım bir kez yeniden denenir. Boot HID klavye/fare, 3 baytlık standart fare ve tekerleği dördüncü baytta gönderen uyumlu fareler hedeflenir.
- Native USB Host, Mass Storage ve PC Viewer rolleri mevcut yeniden başlatmalı seçimle kullanılır. Host rolü uygun OTG bağlantısı ve 5 V VBUS gerektirir. USB Host'ta aynı anda klavye, fare ve SD okuyucu için yeterli beslemeli bir hub kullanılır.

## Desktop

VGA Desktop İngilizcedir. Start menüsü, mavi başlık çubukları, görev çubuğu ve durum göstergeleri XP döneminin görünümünden esinlenir. Files'ta Explorer tarzı görev paneli, Settings'te iki sütunlu Control Panel vardır. Calculator, Notepad, Paint, Clock ve Browser mevcut araç düzenini kullanır. Pencerelerde uygulamaya ait araç ve durum çubukları görünür. Start menüsünden uygulamalar doğrudan açılır.

Masaüstü kısayolları çift tıklamayla açılır. Başlık çubuğu sürüklenir, X pencereyi kapatır, Tab açık pencereler arasında dolaşır. En fazla dört pencere açılır. Ön plandaki uygulama çalışır. Odak değişince seçilen sayfa yeniden açılır. Oyunlar ve güncelleme araçları tam ekran çalışır. Terminalde `help`, `info`, `usb` ve `vga` tanı komutları vardır.

## FreStore

FreStore, VGA Desktop ve klasik **Apps** menüsünden açılır. Wi-Fi’ye bağlanıp saat eşitlendikten sonra **Install** ile **Internal flash**, **SD card** veya **USB drive** seç. **Installed Apps** ekranı kurulum yerini gösterir. Kartları buradan çevrimdışı açabilir veya kaldırabilirsin. Hazır katalog adresi ve doğrulama anahtarı firmware’e dahildir.

İmzalı kart üstverisi 4 KiB’ye kadar olabilir. Bir kartın imzalı ek veri dosyası HTTPS üzerinden parça parça yazılır. Dahili flash için paket başına üst sınır 500 KiB’dir. Bu kartta ROM’larla paylaşılan LittleFS bölümü toplam 320 KiB olduğundan dahili kurulum gerçek boş alanla sınırlıdır. SD ve USB için boş alan ve FAT dosya boyutu geçerlidir. Katalog başına 16 kart listelenir.

Doğrulanan kart ve veri dosyası, seçili diskte yayıncıya ait `FRESTORE` dizinine yazılır. Yeni sürüm tamamlanana kadar önceki doğrulanmış dosya açılabilir. Eski SD ve USB paketleri ilgili kayıtlı sunucu seçildiğinde görünür. [SD ve ekran bağlantıları](../README_TR.md#ekran-ve-sd-bağlantıları)

### Raspberry Pi 4B kurulumu

1. `frestore-server/` dizininde `go build -o frestore .` çalıştır. Dağıtım paketindeki Pi ikilisi de kullanılabilir. ARM64 için `aarch64`, 32 bit ARM için `armv7l` ikilisini seç.
2. Özel yayıncı anahtarını güvenilir bir makinede oluştur. HTTPS sunucusuna yalnızca `published/` içeriğini aktar.

```sh
./frestore keygen -private publisher.pem
./frestore publish -private publisher.pem -input examples/starter -out published
base64 -d published/publisher.pub | sha256sum
```

3. Pi'de DNS adıyla eşleşen ve ESP-IDF sertifika paketinin güvendiği TLS sertifikasıyla sunucuyu çalıştır. Katalog dosyaları başlangıçta yüklenir. Yeni yayın sonrası servisi yeniden başlat.

```sh
./frestore serve -dir published -addr :8443 -cert fullchain.pem -tls-key privkey.pem
```

4. Cihazı Wi-Fi’ye bağla ve saatin eşitlenmesini bekle. **FreStore → Server → Add HTTPS server** yolunda `https://store.example.org:8443` biçiminde adresi gir. FreStore açık anahtarı HTTPS üzerinden alır. Ekrandaki adresi ve SHA-256 parmak izinin ilk 16 karakterini yayıncı makinesindeki değerle karşılaştırıp **Confirm**’e bas. Fare olmadan sunucu menüsü için **LEFT + FIRE**, arama için **RIGHT + FIRE** kullanılır.
5. **Install** ile bir kart indir ve **Installed Apps** ekranından aç. **Server → Starter catalog** hazır kataloğa döner. **Saved server** ile kayıtlı Pi’ye geçebilirsin. Önceki SD ayar dosyasını kullanıyorsan sunucu menüsünde **Import previous SD settings** seçeneği bulunur. Yeni içerik yayımlarken sürüm numarasını artır.

Katalog ve paketler ayrı ECDSA P-256 imzası taşır. ESP32 paket boyutunu, SHA-256 özetini, yayın imzasını, kimliği, sürümü ve ABI'yi doğrular. Sunucu Go ile yazıldı ve katalog imzalı bir dosyada tutuluyor. Gönüllü P2P dağıtımı ayrı fazdır.

Bir karta veri dosyası eklemek için JSON’a `"asset": "sample.bin"` alanı koy ve dosyayı JSON ile aynı klasöre yerleştir. `publish` dosyanın boyutunu ve SHA-256 özetini imzalar. `serve` yayımlanan `.fsp.dat` dosyasını diskten okur. Kart görünümü metni gösterir ve ek veri, seçilen diskte kartla birlikte saklanır.

## Doom kayıtlarını dışa aktarma

**Games → Doom** menüsünde **Start Doom** altında üç seçenek bulunur.

| Seçenek | Dosyaların konumu |
| --- | --- |
| **Export saves to SD card** | FAT microSD’de `DOOM/BACKUP001` ve sonraki numaralı klasörler |
| **Export saves to USB drive** | USB/OTG’ye takılan FAT bellekte aynı klasör yapısı |
| **Export saves to PC** | Yeniden başlatma sonrası native USB/OTG üzerinden **FRESLEST** adlı geçici, salt okunur disk |

PC’ye aktarırken `.dsg` dosyalarını kopyala ve diski çıkar. Sonra cihazda **OK** ile Doom menüsüne dön. Dışa aktarma, dahili kayıtların bütünlüğünü denetler ve kopyalarını oluşturur.

## Wi-Fi OTA ve iki router arasında kopyalama

Bu 1.5 derlemesini ilk kez USB ZIP veya Windows Installer ile yükle. Ardından Desktop'ta **Start → Wi-Fi Update**, LCD'de **Apps → Wi-Fi Update** üzerinden daha yeni sürümleri alabilirsin. Ağ OTA'sı OS içindir. Doom güncellemeleri mevcut USB menüsünden yapılır.

Yayıncı Pi'de kendi FreStore anahtarınla, kurulu profil için daha yeni bir `freslest_os_s3.bin` yayımla. Desteklenen profil adları `ili9488`, `ili9341` ve `vga`. Cihaz ayarlardan VGA'ya geçse bile imajın derleme profili kullanılır. Sürümü `1.5.1` gibi mevcut sürümden yüksek bir sayısal değer yap.

```sh
./frestore publish-os -private publisher.pem -profile ili9488 -image freslest_os_s3.bin -out published
```

İki ağ arasında HTTPS erişimi varsa ESP32, diğer router arkasındaki yayıncı Pi'yi doğrudan kullanabilir. İkinci ağda yerel bir kopya için `published/` başlangıç içeriğini ve doğrulanmış `publisher.pub` dosyasını ikinci Pi'ye aktar. Özel yayın anahtarını yayıncı makinede tut. İkinci Pi'de:

```sh
./frestore mirror-os -from https://updates-a.example.org:8443 -profile ili9488 -out published
./frestore serve -dir published -addr :8443 -cert fullchain.pem -tls-key privkey.pem
```

`mirror-os`, ilk Pi'nin HTTPS sertifikasını ve imzayı kontrol eder, dosyayı doğrular ve yalnızca tamamlanan kopyayı yayımlar. Her yeni yayın veya kopyadan sonra çalışan `serve` işlemini yeniden başlat. Alıcı Pi’yi **Add HTTPS server** ile eklerken sunulan açık anahtarı ilk yayıncının parmak iziyle karşılaştır. Ağlar arası yönlendirme ve DNS, kullanılan Pi adresine erişim sağlamalıdır. Router'lar trafiği taşır. İmzalı kopyalamayı Pi'ler çalıştırır.

**Wi-Fi Update → Check for updates** ekranı yeni sürümü, sunucu adresini ve yayıncı parmak izini gösterir. **Install and restart** ile açık onay ver. İmaj PSRAM'e alınır, imzalı SHA-256 değeri ve ESP imajı doğrulanır, etkin olmayan OS yuvasına yazılır ve geri okunur. İlk açılış sağlık kontrolü mevcut A/B geri alma mekanizmasını kullanır. Yalnız kendi OS yayınlarını onayladığın FreStore anahtarını bu işlem için kullan.

## Derleme

```sh
# ESP-IDF v5.4.4 ortami
python tools/build_lcd_profiles.py
python tests/test_vga_transition.py
python tests/test_lcd_profiles.py
python tests/run_desktop_tests.py
python tests/run_frestore_tests.py
```

Windows yükleyicisi üç kaynak derlemesinin SHA-256 değerlerini ve OS 1.5 tanımlayıcısını kontrol eder. Windows x64, Python 3.11 ve `requirements-build.txt` ile `python build_windows.py` çalışır.

Donanımda VGA sinyali, hub ve SD okuyucu uyumluluğu, tak-çıkar, gerçek güç kesintisi ve Pi kurulumu cihaz denemesi gerektirir.
