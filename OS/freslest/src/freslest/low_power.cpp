#include "low_power.h"
#include "../../gfx.h"
#include "apps.h"
#include "settings.h"

static uint32_t lastInput = 0;
static bool active = false;

static int currentBrightness = 0;
static uint32_t dimTimer = 0;
static bool sleepScreenDrawn = false;

#define LP_TIMEOUT 30000       // 30 saniye
#define DIM_STEP_DELAY 8       // dim step süresi
#define MIN_SLEEP_BRIGHTNESS 5

// Dışarıdan kontrol edilecek
bool allowSleep = true;

void LowPower_init()
{
    lastInput = millis();
    active = false;
    currentBrightness = brightnessUI;
    dimTimer = 0;
    sleepScreenDrawn = false;
}

void LowPower_onUserInput()
{
    lastInput = millis();

    if (active)
    {
        active = false;
        sleepScreenDrawn = false;
        currentBrightness = brightnessUI;

        // Parlaklığı geri aç
        backlightSet(brightnessUI);

        // Ekranı tekrar çiz
        if (app == APP_MENU) drawMenuFull();
        else enterApp(app);
    }
}

void LowPower_update()
{
    uint32_t now = millis();

    // Bu uygulamalarda uyku yok
    if (app == APP_DINO ||
        app == APP_TETRIS ||
        app == APP_SNAKE ||
        app == APP_MINES ||
        app == APP_2048 ||
        app == APP_DOOM ||
        app == APP_CAMERA)
    {
        return;
    }

    // Genel sleep kapalıysa direkt çık
    // DİKKAT: lastInput burada resetlenmiyor
    if (!allowSleep)
    {
        return;
    }

    // Zaten aktif sleep modundaysa dim ve ekran işini yap
    if (active)
    {
        if (currentBrightness > MIN_SLEEP_BRIGHTNESS)
        {
            if (now - dimTimer >= DIM_STEP_DELAY)
            {
                dimTimer = now;
                currentBrightness -= 5;
                if (currentBrightness < MIN_SLEEP_BRIGHTNESS)
                    currentBrightness = MIN_SLEEP_BRIGHTNESS;

                backlightSet(currentBrightness);
            }
        }
        else if (!sleepScreenDrawn)
        {
            backlightSet(MIN_SLEEP_BRIGHTNESS);

            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_DARKGREY);
            tft.setTextSize(2);
            tft.setCursor(120, 140);
            tft.print("Sleeping...");

            sleepScreenDrawn = true;
        }

        return;
    }

    // Süre dolduysa sleep başlat
    if (now - lastInput >= LP_TIMEOUT)
    {
        active = true;
        dimTimer = now;
        currentBrightness = brightnessUI;
        sleepScreenDrawn = false;
    }
}

bool LowPower_isActive()
{
    return active;
}