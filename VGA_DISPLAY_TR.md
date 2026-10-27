# Freslest LCD RGB565 → VGA RGB332

VGA, LCD için çizilen aynı framebuffer’ı kullanır. Ayrı bir VGA arayüzü yoktur: menüler, uygulamalar, oyunlar ve fare imleci aynı kaynaktan gelir. ILI9341 menüler ve Apps listesindeki uygulamalar için 320×240 tuval kullanır; ILI9488 ve mevcut oyun uyumluluk düzenleri 480×320’dir. Görüntünün tamamı varsayılan olarak 640×480 sinyalin ortasındaki 576×432 alana ölçeklenir; TV kenar kırpılmasına karşı siyah pay bırakılır. Doom’un 320×240 indeksli karesi özgün PLAYPAL RGB888 paletinden ayrı bir RGB332 çizim yoluyla aktarılır; LCD RGB565 ara dönüşümünü kullanmaz.

Fiziksel çıkış **RGB332 / 8 bit / 256 renk**; kaynak görüntü RGB565’tir. VGA RGB565 veya 65.536 renk çıkışı değildir. Kaynak `Freslest_VGA_RGB332_1_0_Core(1).zip` V1.0 çekirdeği, hedef zamanlama 25.175 MHz PCLK, 640×480 yaklaşık 59.94 Hz. Bu değerler yazılım hedefidir; osiloskop ölçümü değildir.

## Seçim

Settings → Display → LCD / VGA RGB332 / USB PC Viewer. LCD/VGA değişimi iki aşamalı onaydan sonra yeniden başlatılır. VGA modunda SPI LCD ve GPIO41 backlight PWM başlatılmaz. Eski AV seçimi yeni sürümde LCD’ye döner; VGA kendiliğinden etkinleşmez. BACK basılıyken RESET, LCD’yi seçer.

## TV boyutu ve LCD profili

Settings → Display → VGA size: LEFT/RIGHT ile %80, %85, %90, %95, %100. OK sonraki değeri seçer; ayar hemen kaydedilir. OS ve Doom aynı ayarı okur. Sinyal her zaman 640×480 kalır; boyut ayarı porch veya saat değerlerini değiştirmez.

Display menüsü ILI9488 ve ILI9341’i ayrı satırlarda gösterir. `[loaded]` derlenip yüklenmiş LCD profilidir. Diğer model seçildiğinde installer’daki gerekli ekran profili gösterilir; yalnızca NVS değeri değiştirerek farklı SPI sürücüsü uygulanmış gibi davranılmaz. Model değişimi için installer > Screens adımından eşleşen OS+Doom firmware’ini yükleyin.

## V1.0 bağlantıları

| GPIO / bit | Direnç | VGA DB15 |
|---|---|---|
| 39 / R2 | 510 Ω + 47 Ω seri | 1 RED |
| 40 / R1 | 1 kΩ | 1 RED |
| 42 / R0 | 2.2 kΩ | 1 RED |
| 2 / G2 | 510 Ω + 47 Ω seri | 2 GREEN |
| 14 / G1 | 1 kΩ | 2 GREEN |
| 13 / G0 | 2.2 kΩ | 2 GREEN |
| 8 / B1 | 680 Ω | 3 BLUE |
| 41 / B0 | 1.5 kΩ | 3 BLUE |
| 47 | Doğrudan | 13 HSYNC |
| 48 | Doğrudan | 14 VSYNC |
| GND | Doğrudan | 5, 6, 7, 8, 10 |

Aynı rengin direnç çıkışları birleşir; GPIO uçları birbirine doğrudan bağlanmaz. VGA pin 9 kullanılmaz. Renkler, VGA girişinin 75 Ω sonlandırmasını kullanır.

GPIO8 LCD RESET, GPIO41 LCD arka ışık MOSFET’i ve GPIO13 SD-MISO ile ortaktır. PCB/mod anahtarlaması bu uçları ayırmalıdır: yazılım PWM’i durdursa da VGA’nın GPIO41 verisi bağlı MOSFET’i fiziksel olarak sürebilir. Bu bağlantıyla LCD ve VGA eşzamanlı sürülmez; aynı framebuffer seçilen çıkışa gider. SD ve VGA da bu ortak hatla eşzamanlı kullanılamaz.

## Doom renkleri ve ses

Doom aynı RGB332 devresini kullanır. OS renkleri doğruyken yalnızca Doom kırmızı görünüyorsa bağlantıdaki R/B uçlarını değiştirmeyin. Son düzeltmede eklenmiş 4×4 renk noktalaması kaldırılmıştır; özgün palet doğrudan RGB332 bitlerine çevrilir ve düz renkler düz kalır; tam installer bu Doom firmware’ini de yükler. OS Update ZIP’i Doom’u güncellemez.

DFPlayer ses yolu VGA’dan ayrıdır: ESP GPIO18 TX → DFPlayer RX, DFPlayer TX → ESP GPIO17 RX, ortak GND. SD kart/modül geri geldiğini UART üzerinden bildirirse yazılım kaynak, DAC ve ses ayarını geri kurar. RX hattı bağlı değilse bu bildirim alınamaz. Kesilme sürerse Settings ses testi ve Viewer tanı komutuyla DFPLAYER DIAG satırındaki RX, error ve recovery değerleri kaydedilmelidir.

## Durum ve test sınırı

Bu geçiş deneysel VGA desteğidir. Önceki donanım denemelerinde sinyal gidip gelmesi görülmüştür; kesin giderildiği iddia edilmez. Yeni aktarım iki PSRAM kare ve iki dahili on satırlık DMA tamponu kullanır. CPU aktif kareye yazmaz; tamamlanma bildirimi gelmezse yeni kare atlanır ve OS tekrar dener. Koşulsuz her-VBlank DMA sıfırlaması kapatılmıştır. Flash yazımında önbelleğin erişilebilir kalması için OS ve Doom’da PSRAM XIP açıktır. Bu mimari değişikliğinin fiziksel sonucu cihazda doğrulanmalıdır. Yazılım VSYNC ve gönderilen kareleri izler; monitörün fiziksel bağlantısını veya analog sinyal kalitesini tespit edemez.

AV/kompozit motoru, test modu ve TV renk/hizalama menüleri kaldırıldı. USB Viewer bağımsız olarak korunur. Fiziksel ESP32-S3/LCD/VGA testi bu çalışma ortamında yapılamaz.
