# Freslest OS 1.5

**ESP32-S3 N16R8** için Freslest OS. ILI9488 ve ILI9341 ekranlarda klasik arayüz, VGA’da Freslest Desktop açılır. Tüm cihaz menüleri İngilizcedir. [English README](README.md)

## 1.5 yenilikleri

- **Freslest Desktop.** 640×480 VGA’da XP tarzı Start menüsü, görev çubuğu ve sürüklenebilir pencereler. Files, Explorer tarzı görev paneliyle açılır. Settings, Control Panel düzenini kullanır. Calculator, Notepad, Paint, Clock ve Browser mevcut araç düzenini kullanır.
- **FreStore.** İmzalı kartları ve ek verilerini **Internal flash**, **SD card** veya **USB drive** seçenekleriyle kurabilirsin. Kendi Raspberry Pi sunucunu **Server → Add HTTPS server** ile ekleyebilirsin.
- **Bluetooth.** HID eşlemleri ve eşleşme listesi tamponları PSRAM kullanır. BLE açılışta 2.500 KiB boş PSRAM bekler, bağlantıdan önce dahili RAM’i kontrol eder ve kayıtlı cihazlara yeniden bağlanır.
- **Doom kayıtları.** Başlatma menüsünde SD, USB bellek ve native USB üzerinden PC’ye `.dsg` kopyalama seçenekleri var.
- **Wi-Fi Update.** İmzalı, daha yeni OS imajı erişilebilir Pi sunucusundan alınır. İkinci router arkasındaki Pi, `mirror-os` ile imzalı yayını kopyalayabilir. Kurulum onayla başlar ve A/B deneme açılışını kullanır.
- **VGA ve USB.** Geciken VGA kareleri yeniden eşitlenir. USB klavye veya fare ayrılınca basılı girişler bırakılır. NES, Doom, ses ve Recovery yolları korunur.

## İndir ve kur

- [Freslest 1.5 test paketi](https://github.com/yunusaay211-create/Freslest-OS-ESP32S3/actions/runs/42897905512/artifacts/13802621454)
- [Windows x64 Installer ve USB Viewer](https://github.com/yunusaay211-create/Freslest-OS-ESP32S3/actions/runs/42897905512/artifacts/13802406945)
- [Derleme ve test sonuçları](https://github.com/yunusaay211-create/Freslest-OS-ESP32S3/actions/runs/42897905512)

Dış arşivi aç. Kurulu firmware profiline uygun iç ZIP’i **açmadan** FAT16/FAT32 USB belleğin köküne kopyala.

| Kurulu profil | OS Update dosyası |
| --- | --- |
| ILI9488 | `UPD88.ZIP` |
| ILI9341 | `UPD41.ZIP` |
| Özel VGA derlemesi | `UPDVGA.ZIP` |

**Freslest Update → OS Update** ile yükle ve sağlıklı açılışı bekle. Ayarlardan VGA’ya alınan bir LCD kurulumunda OS Update için kurulu LCD profilinin ZIP’i kullanılır. Doom güncellemesi için **Doom Update** menüsünde LCD’de aynı LCD ZIP’ini, VGA’da `DOOMVGA.ZIP` seç. [USB güncelleme biçimi](UPDATE_FORMAT.md)

## Pi ve ağ güncellemesi

Bu 1.5 derlemesini önce USB veya Installer ile yükle. FreStore’da Pi adresini ve güvenilir yayın anahtarını tanıt. Sonraki daha yeni OS sürümlerini **Start → Wi-Fi Update** ile denetle. LCD’de yol **Apps → Wi-Fi Update**. [Pi kurulumu, publish-os ve mirror-os komutları](docs/FRESLEST_1_5_TR.md)

## Ekran ve SD bağlantıları

VGA hedefi 640×480, 59,94 Hz ve 25.175 MHz RGB332. [VGA pin ve direnç tablosu](VGA_DISPLAY_TR.md)

FreStore ve Doom kayıt aktarımı için FAT16/FAT32 microSD’yi ESP32-S3’e güç vermeden önce tak. FreStore eski paketleri SD-SPI ve USB’den de okur.

| SD-SPI hattı | ESP32-S3 | Not |
| --- | --- | --- |
| MOSI / DI | GPIO11 | TFT MOSI ile ortak |
| SCK / CLK | GPIO12 | TFT SCK ile ortak |
| MISO / DO | GPIO46 | Açılış modu pini |
| CS | GPIO0 | Açık drenç çıkışı. Normal açılış için reset sırasında yüksek |

SD modülü reset sırasında GPIO46’yı yüksek tutarsa ESP32-S3 ROM yükleme moduna giremeyebilir. [ESP32-S3 açılış modu](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html) · [SD pull-up gereksinimleri](https://docs.espressif.com/projects/esp-idf/en/release-v5.4/esp32s3/api-reference/peripherals/sd_pullup_requirements.html)

## Derleme

ESP-IDF **5.4.4** ortamında depo kökünden `python tools/build_lcd_profiles.py` çalıştır. Üç ekran profili için OS 1.5 ve Doom firmware’leri derlenir ve flash bölümlerine göre doğrulanır. Windows x64 kurucu ve Viewer, [derleme iş akışında](.github/workflows/windows-exes.yml) paketlenir. Fiziksel ESP32-S3, ekran ve iki router arası kurulum cihaz üzerinde ayrıca doğrulanır.
