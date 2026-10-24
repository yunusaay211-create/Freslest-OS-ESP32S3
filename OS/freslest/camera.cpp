#include <WiFi.h>
#include <TJpg_Decoder.h>
#include "gfx.h"
#include "src/freslest/apps.h"
#include "input.h"
// ================= WIFI =================
static const char* ssid = "FRESLEST_CAM";
static const char* password = "12345678";
static const char* CAM_IP = "192.168.4.1";
static const uint16_t CAM_PORT = 80;

static WiFiClient client;

// ================= BUFFER =================
static uint8_t jpgBuf[60000];

// ================= DRAW =================
static int drawX = 0;
static int drawY = 0;

static int ledLevel = 0; // 0-255
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  static bool centered = false;

  if (!centered)
  {
    drawX = (480 - w) / 2;
    drawY = (320 - h) / 2;
    centered = true;
  }

  tft.pushImage(drawX + x, drawY + y, w, h, bitmap);
  return true;
}

// ================= STATE =================
enum CamState
{
  CAM_IDLE,
  CAM_WIFI_CONNECTING,
  CAM_TCP_CONNECTING,
  CAM_READING_LEN,
  CAM_READING_JPG,
  CAM_READY
};

static CamState camState = CAM_IDLE;
static uint32_t camStateMs = 0;

static uint32_t frameLen = 0;
static uint8_t lenBuf[4];
static uint8_t lenRead = 0;
static uint32_t jpgRead = 0;

static bool uiDrawn = false;

// ================= UI =================
static void cameraDrawStatus(const char* line1, const char* line2 = nullptr)
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 10);
  tft.print(line1);
  if (line2)
  {
    tft.setCursor(10, 30);
    tft.print(line2);
  }
}

static void cameraDrawBar()
{
  tft.fillRect(0, 295, 480, 25, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setCursor(10, 300);
  tft.print("WIFI CAMERA");
}

// ================= RESET HELPERS =================
static void cameraResetFrameRead()
{
  frameLen = 0;
  lenRead = 0;
  jpgRead = 0;
}

static void cameraRestartTcp()
{
  client.stop();
  cameraResetFrameRead();
  camState = CAM_TCP_CONNECTING;
  camStateMs = millis();
}
void cameraExit()
{
  Serial.println("📴 CAMERA FULL EXIT");

  // 🔌 TCP kapat
  client.stop();

  // 📡 WiFi kapat
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);

  // 🧠 state sıfırla
  camState = CAM_IDLE;
  frameLen = 0;
  lenRead = 0;
  jpgRead = 0;

  // 🧼 buffer temizle
  memset(jpgBuf, 0, sizeof(jpgBuf));

  // 🎥 decoder reset (opsiyonel ama iyi)
  TJpgDec.setCallback(nullptr);

  // 🖥 ekranı tamamen sil
  tft.fillScreen(TFT_BLACK);

  // 🧊 küçük delay (stabilite)
  delay(10);
}
// ================= ENTER =================
void cameraEnter()
{
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(50);
  WiFi.begin(ssid, password);

  TJpgDec.setCallback(tft_output);
  TJpgDec.setSwapBytes(false);

  camState = CAM_WIFI_CONNECTING;
}

// ================= LOOP =================
void cameraLoop(uint32_t now)
{
  static uint32_t lastDebug = 0;
  // 🔆 LED KONTROL (gönderilecek)
  if (bUp.pressed())
  {
    ledLevel += 20;
    if (ledLevel > 255) ledLevel = 255;
  }

  if (bDown.pressed())
  {
    ledLevel -= 20;
    if (ledLevel < 0) ledLevel = 0;
  }

  // 📡 LED DATA GÖNDER
  static uint32_t lastSend = 0;
  if (millis() - lastSend > 80 && client.connected())
  {
    lastSend = millis();

    uint8_t buf[2] = {0xAA, (uint8_t)ledLevel};
    client.write(buf, 2);
  }

  switch (camState)
  {
    case CAM_WIFI_CONNECTING:
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.print("🟢 WiFi OK, IP: ");
        Serial.println(WiFi.localIP());

        camState = CAM_TCP_CONNECTING;
        camStateMs = now;
        uiDrawn = false;
        return;
      }

      if (millis() - lastDebug > 2000)
      {
        Serial.println("🟡 WAITING WIFI...");
        lastDebug = millis();
      }

      if (!uiDrawn || (now - camStateMs > 1000))
      {
        cameraDrawStatus("Connecting WiFi...", "Please wait");
        camStateMs = now;
        uiDrawn = true;
      }

      return;
    }

    case CAM_TCP_CONNECTING:
    {
      if (!uiDrawn || (now - camStateMs > 1000))
      {
        cameraDrawStatus("WiFi OK", "Connecting camera...");
        camStateMs = now;
        uiDrawn = true;
      }  

      if (client.connected() || client.connect(CAM_IP, CAM_PORT))
      {
        client.setNoDelay(true);
        Serial.println("🟢 TCP CONNECTED");

        camState = CAM_READING_LEN;
        cameraResetFrameRead();
        cameraDrawStatus("CAM READY");
        cameraDrawBar();
        return;
      }

      if (millis() - lastDebug > 2000)
      {
        Serial.println("🟡 WAITING TCP...");
        lastDebug = millis();
      }

      return;
    }

    case CAM_READING_LEN:
    {
      if (!client.connected())
      {
        Serial.println("❌ TCP LOST (LEN)");
        cameraRestartTcp();
        return;
      }

      int avail = client.available();

      if (avail > 0 && millis() - lastDebug > 500)
      {
        Serial.print("📥 LEN BYTES: ");
        Serial.println(avail);
        lastDebug = millis();
      }

      if (avail <= 0) return;

      while (avail > 0 && lenRead < 4)
      {
        int b = client.read();
        if (b < 0) break;
        lenBuf[lenRead++] = (uint8_t)b;
        avail--;
      }

      if (lenRead < 4) return;

      frameLen =
        ((uint32_t)lenBuf[0]) |
        ((uint32_t)lenBuf[1] << 8) |
        ((uint32_t)lenBuf[2] << 16) |
        ((uint32_t)lenBuf[3] << 24);

      Serial.print("📦 FRAME LEN: ");
      Serial.println(frameLen);

      if (frameLen < 1000 || frameLen > sizeof(jpgBuf))
      {
        Serial.print("❌ BAD LEN: ");
        Serial.println(frameLen);

        cameraRestartTcp();
        return;
      }

      jpgRead = 0;
      camState = CAM_READING_JPG;
      return;
    }

    case CAM_READING_JPG:
    {
      if (!client.connected())
      {
        Serial.println("❌ TCP LOST (JPG)");
        cameraRestartTcp();
        return;
      }

      int avail = client.available();

      if (avail > 0 && millis() - lastDebug > 500)
      {
        Serial.print("📥 JPG BYTES: ");
        Serial.println(avail);
        lastDebug = millis();
      }

      if (avail <= 0) return;

      int need = (int)(frameLen - jpgRead);
      int take = (avail < need) ? avail : need;

      if (take > 0)
      {
        int got = client.read(jpgBuf + jpgRead, take);
        if (got > 0)
        {
          jpgRead += got;

          Serial.print("📊 PROGRESS: ");
          Serial.print(jpgRead);
          Serial.print("/");
          Serial.println(frameLen);
        }
      }

      if (jpgRead < frameLen) return;

      Serial.println("🎥 FRAME COMPLETE");

      tft.startWrite();
      TJpgDec.drawJpg(drawX, drawY, jpgBuf, frameLen);
      tft.endWrite();

      cameraDrawBar();

      camState = CAM_READING_LEN;
      cameraResetFrameRead();
      return;
    }

    default:
      return;
  }
}