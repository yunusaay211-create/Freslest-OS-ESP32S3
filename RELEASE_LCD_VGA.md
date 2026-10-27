# Freslest OS — LCD & VGA Update

Bu güncelleme ILI9341 uygulama ekranlarını düzenler, VGA kaymasına karşı önlemler ekler, Doom VGA renk dönüşümünü iyileştirir ve Freslest ses modülünün yeniden bağlantıdan sonra toparlanmasını sağlar. Kompozit AV kaldırılmıştır.

## Ekranlar ve kurulum

- Kurulumdaki **1. Ekranlar** adımından **ILI9488 (480×320)** veya **ILI9341 (320×240)** seçilebilir. Kurucu seçilen ekranın OS ve Doom firmware’lerini birlikte kullanır.
- ILI9341 ana menüsü, uygulama/oyun/ayar listeleri ve görev değiştirici 320×240’a uygun düzenlendi. Menü yazıları artık büyük arayüzden küçültülmüyor; başlıklar, alt bilgiler ve fare seçim alanları yeni yerleşime uyarlandı.
- Calculator, Note, Clock, Paint, Camera, Files, Browser, ekran klavyesi ve Settings, ILI9341’de doğrudan 320×240 tuvale çizilir. Yazılar küçültülmez; giriş/fare alanları ve metin kaydırma yeni yerleşime uyarlandı. Oyunların mevcut uyumluluk düzenleri korunur.
- Doom’un ILI9341 yön ayarı OS ile eşleştirildi; ters/aynalanmış yazılara neden olan yön biti düzeltildi.

## AV’den VGA’ya geçiş

- Kompozit AV motoru, AV test modu ve TV renk/hizalama sayfaları kaldırıldı.
- **Settings → Display** üzerinden LCD veya **VGA RGB332** seçilebilir; değişiklik onay ve yeniden başlatma ile uygulanır. BACK basılıyken RESET, LCD’ye dönüş sağlar.
- VGA ayrı bir masaüstü çizmez. **LCD için çizilen aynı RGB565 görüntü RGB332’ye dönüştürülür**; menüler, uygulamalar, oyunlar ve fare imleci aynı içerikle VGA’da gösterilir.
- VGA hedefi **640×480, yaklaşık 60 Hz, RGB332/256 renk**. RGB565 kaynak kullanılması, fiziksel VGA çıkışının RGB565 olduğu anlamına gelmez.
- Önceki AV seçimi otomatik VGA’ya dönüştürülmez; güncelleme sonrasında LCD ile açılır.

## VGA kayması, Doom renkleri ve ses

- Sabit OS görüntüsü her döngüde yeniden VGA belleğine yazılmaz. PSRAM veri yolu yükü azaltılır; OS ve Doom için VBlank DMA yeniden hizalama etkinleştirilir. Çalışan renk çubuğu çekirdeğinin saat, senkronizasyon ve pin değerleri korunur.
- Doom’un indeksli VGA paletinde 4×4 sıralı renk yuvarlaması uygulanır. RGB332’nin koyu tonlarda maviyi tamamen kaybetmesi azaltılır; kırmızı/yeşil/mavi uç değerleri korunur. Doom yönü ve LCD paleti değişmez.
- Freslest DFPlayer sesinde SD dosya sorgusu düzeltildi. SD kart yeniden takıldığında veya modül yeniden başlama bildirimi verdiğinde kaynak, DAC ve kullanıcının ses düzeyi geri yüklenir. Sessiz ayarı korunur; eski efektler yeniden oynatılmaz.
- Bu düzeltmeler yazılım testleriyle doğrulanır; bildirilen VGA kayması, kırmızı ton ve aralıklı ses kesilmesinin cihazda tamamen giderildiği henüz doğrulanmadı.

**Doom renk düzeltmesi için tam installer kullanın.** Freslest Update ZIP’leri yalnızca OS bölümünü günceller; Doom firmware’ini değiştirmez. Tam kurulumdan önce verilerinizi yedekleyin.

## Bağlantı ve bilinen sınırlar

VGA, V1.0 direnç DAC çekirdeğinin pinlerini kullanır. GPIO8/41 LCD, GPIO13 ise SD bağlantısıyla ortaktır; PCB veya mod anahtarlaması bu uçları ayırmalıdır. LCD ve VGA eşzamanlı sürülmez. USB PC Viewer korunur.

**VGA desteği deneyseldir.** Önceki donanım denemelerindeki sinyal kesilmesi sorununun kesin giderildiği iddia edilmez. Hareketli görüntülerde tearing görülebilir. Bağlantı tablosu ve teknik ayrıntılar için `VGA_DISPLAY_TR.md` dosyasına bakın.

Kurulumdan önce dosya, not ve ROM’larınızı yedekleyin. Ekran modelini kendi modülünüze göre seçin.

## Derleme doğrulaması

İki LCD için OS/Doom firmware derlemeleri, bölüm/hash kontrolleri, Windows kurucu/Viewer testleri ve EXE açılış testleri geçti: https://github.com/yunusaay211-create/Freslest-OS-ESP32S3/actions/runs/40525403029

Bu ortamda fiziksel ESP32-S3/LCD/VGA testi yapılmadı.
