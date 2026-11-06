# Freslest OS 1.5 · ESP32-S3 N16R8

Bu sürüm `codex/audio-sd-recovery` dalındaki `42af5a3` kaynağını temel alır. 16 MB flash yerleşimi ve iki OS OTA yuvası korunur. ILI9488 klasik arayüzü 480×320, ILI9341 klasik menüleri 320×240 tuval kullanır. VGA'da yeni masaüstü 640×480, pencere içindeki mevcut uygulama tuvali 480×320'dir.

## VGA ve USB

- Hedef VGA zamanlaması 640×480, 800×525 toplam, 25.175 MHz ve yaklaşık 59.94 Hz. Çift PSRAM kare ve 20 satırlık dahili tamponlar sürer. OS hazır kareyi beklerken arayüzü bloklamaz. Uzayan aktarımda sürücüye VBlank yeniden eşitleme isteği gönderir. DMA tamamlanana kadar ilgili kareyi tekrar kullanmaz.
- VGA modunda LCD SPI ve PWM başlatılmaz. RGB332 hattındaki GPIO41 ve GPIO8, LCD arka ışık ve reset hatlarıyla ortaktır. Fiziksel olarak LCD'yi kapalı tutmak için bu ortak hatlar kartta ayrılmalıdır. VGA bağlantı tablosu `VGA_DISPLAY_TR.md` dosyasındadır. Fiziksel BACK ile RESET, derlenmiş LCD profiline döner.
- USB Host, takılı aygıtları HID sürücüsü hazır olduktan sonra işler. Ayrılma ve aktarım hatası basılı girişleri temizler. Geçici bir endpoint hatasında aktarım bir kez yeniden denenir. Boot HID klavye/fare, 3 baytlık standart fare ve tekerleği dördüncü baytta gönderen uyumlu fareler hedeflenir.
- Native USB Host, Mass Storage ve PC Viewer rolleri mevcut yeniden başlatmalı seçimle kullanılır. Host rolü uygun OTG bağlantısı ve 5 V VBUS gerektirir. USB Host'ta aynı anda klavye, fare ve SD okuyucu için yeterli beslemeli bir hub kullanılır.

## Desktop

Başlat menüsü, görev çubuğu ve saat/ağ/ses/USB göstergeleri VGA'ya özeldir. Kısayollar çift tıklamayla açılır. Başlık çubuğu sürüklenir, X pencereyi kapatır, Tab açık pencereler arasında dolaşır. En fazla dört pencere açılır. Ön plandaki mevcut uygulama çalışır. Odak değişince seçilen sayfa yeniden açılır. Oyunlar ve Freslest Update mevcut tam ekran yolunu kullanır. Terminalde `help`, `info`, `usb` ve `vga` tanı komutları vardır.

## FreStore

FreStore, VGA Desktop ve klasik Apps menüsünden açılır. Ara, kategori, detay, yükleme, güncelleme ve kaldırma akışları vardır. Yüklü Uygulamalar listesi karttaki doğrulanmış paketleri açar. Bu ilk sürüm ESP32-S3 ABI 1 imzalı metin kartlarını destekler. Paket başına 4 KiB, katalogda ve kartta en fazla 16 uygulama kullanılır. İlk sürümde bağımlılık listesi boş olur.

FreStore, mevcut USB FAT depolama servisini kullanır. FAT16/FAT32 microSD kartı USB kart okuyucuyla bağla. Doğrudan SD-SPI hattındaki GPIO13, VGA RGB332 ile ortaktır. İndirilen paketler kartta `FRESTORE` dizinine gider. Her uygulamada iki imzalı dosya konumu vardır. Kurulum tamamlanana kadar önceki doğrulanmış dosya durur.

### Raspberry Pi 4B kurulumu

1. `frestore-server/` dizininde `go build -o frestore .` çalıştır. Dağıtım paketindeki Pi ikilisi de kullanılabilir. ARM64 için `aarch64`, 32 bit ARM için `armv7l` ikilisini seç.
2. Özel yayıncı anahtarını güvenilir bir makinede oluştur. HTTPS sunucusuna yalnızca `published/` içeriğini aktar.

```sh
./frestore keygen -private publisher.pem
./frestore publish -private publisher.pem -input examples -out published
base64 -d published/publisher.pub | sha256sum
```

3. Pi'de DNS adıyla eşleşen ve ESP-IDF sertifika paketinin güvendiği TLS sertifikasıyla sunucuyu çalıştır. Katalog dosyaları başlangıçta yüklenir. Yeni yayın sonrası servisi yeniden başlat.

```sh
./frestore serve -dir published -addr :8443 -cert fullchain.pem -tls-key privkey.pem
```

4. FAT kartın kökünde `FRESTORE.CFG` oluştur. İlk satıra kendi HTTPS sunucu adresini, ikinci satıra `published/publisher.pub` içeriğini yaz. Adreste yol ve sonda eğik çizgi kullanma.

```text
https://store.example.org:8443
<tek satir Base64 yayin acik anahtari>
```

5. Cihazı Wi-Fi'ye bağla ve saatin eşitlenmesini bekle. FreStore → Sunucu'yu aç. Ekrandaki adresi ve SHA-256 parmak izinin ilk 16 karakterini yayıncı makinesindeki değerle karşılaştır. Onayladıktan sonra Yenile'yi seç. Fare olmadan Sunucu için SOL+FIRE, Ara için SAĞ+FIRE kullanılır.
6. Örnek **Merhaba Freslest** kartını yükle ve Yüklü Uygulamalar'dan aç. Yeni içerik yayımlarken sürüm numarasını artır.

Katalog ve paketler ayrı ECDSA P-256 imzası taşır. ESP32 paket boyutunu, SHA-256 özetini, yayın imzasını, kimliği, sürümü ve ABI'yi doğrular. Sunucu Go ile yazıldı ve katalog imzalı bir dosyada tutuluyor. Gönüllü P2P dağıtımı ayrı fazdır.

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
