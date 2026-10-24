/* Freslest-Doom-S3 - split DFPlayer music + ESP32-S3 PWM Doom SFX backend
 *
 * Wiring:
 *   DFPlayer TX   -> ESP32-S3 GPIO17 (RX)
 *   ESP32 GPIO18  -> 1k resistor -> DFPlayer RX
 *   ESP32 GPIO21 -> 1k -> RC filter -> 1k -> RC filter -> 10k -> MIX
 *   DFPlayer DAC_R -> 10k -> MIX; MIX -> 1uF coupling -> amp R_IN
 *   GPIO42 -> optional amp enable ONLY if verified compatible; disabled by default
 *   DFPlayer VCC  -> 5V
 *   DFPlayer GND  -> GND
 *
 * SD card:
 *   /MP3/0001.mp3 ... /MP3/0104.mp3
 *
 * Track layout:
 *   /MP3/0068..0099 = Doom music; other MP3 tracks are unused here.
 *
 * DFPlayer only plays music. Doom SFX are 8-bit PCM from WAD via LEDC PWM.
 * 8 kHz timer mixing is experimental; check FPS and audio quality on hardware.
 * Reset-time silence REQUIRES an amp with confirmed active-high enable/SHDN
 * controlled by GPIO42 with an EXTERNAL 100k pulldown; STOP on boot is not
 * immediate silence when the DFPlayer is powered independently.
 */

#include "config.h"
#include <math.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_timer.h"

/* ESP-IDF/stdbool compatibility with Doom's own boolean enum */
#ifdef false
#undef false
#endif

#ifdef true
#undef true
#endif

#include "z_zone.h"
#include "m_swap.h"
#include "i_sound.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"
#include "lprintf.h"
#include "s_sound.h"
#include "sounds.h"

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "d_main.h"

#define DF_UART         UART_NUM_1
#define DF_TX_GPIO      18
#define DF_RX_GPIO      17
#define DF_BAUD         9600
#define DF_VOLUME       24   /* safe fallback before reading saved preference */

#define PWM_SFX_GPIO    21
#define PWM_SAMPLE_HZ   8000
#define PWM_CARRIER_HZ  30000 /* safer for S3 10-bit LEDC clocks */
#define PWM_CENTER      512
#define PWM_CHANNEL     LEDC_CHANNEL_0
#define PWM_TIMER       LEDC_TIMER_0
#define PWM_VOICES      8

/* -1 disables amp-enable control when the module has no verified pin.
 * Active-HIGH amp enable ONLY; never connect this to amp R_IN/R_OUT! */
#define AMP_ENABLE_GPIO -1 /* no confirmed amp EN pin; safe default */


int snd_card = 1;
int mus_card = 1;
int snd_samplerate = 0;

static int df_ready = 0;
static TaskHandle_t df_task = NULL;
static SemaphoreHandle_t df_task_exited = NULL;
static portMUX_TYPE df_lock = portMUX_INITIALIZER_UNLOCKED;
typedef struct {
    uint16_t track;
    uint8_t volume;
    int looping;
    int paused;
    uint32_t revision;
    int shutdown;
} df_request_t;
static df_request_t df_request;

/* Only the music worker writes UART after startup. Doom just updates the
 * desired state, so a level change / volume slider never waits for SD I/O.
 * Some MP3-TF-16P firmwares latch the OLD file when 0x19 (single loop) is
 * enabled immediately after 0x12. Repeat via the SD-finished event instead.
 */
#define DF_COMMAND_US 100000LL
#define DF_START_SETTLE_US 600000LL
static int64_t df_next_command_us;
static int df_card_present = -1; /* -1: RX not confirmed, 0: removed, 1: online */
static uint8_t df_rx_packet[10];
static unsigned df_rx_used;
static int df_end_pending;
static int df_status_value;
static uint32_t df_status_serial;
static uint32_t df_media_serial;
static int df_rx_errors;

/* The WAD stores uncompressed Doom PCM: 8-byte header + unsigned 8-bit samples.
 * Source data stays mapped for the lifetime of this game.
 * Voice state is shared by Doom core and the esp_timer task.
 */
typedef struct {
    const uint8_t *pcm;
    uint32_t count;
    uint64_t phase;           /* fixed point; long sounds cannot wrap at 65536 */
    uint32_t step;
    uint8_t gain;
    int active;
} pwm_voice_t;

static pwm_voice_t sfx_voices[PWM_VOICES];
static portMUX_TYPE sfx_lock = portMUX_INITIALIZER_UNLOCKED;
static esp_timer_handle_t sfx_timer = NULL;
static int sfx_ready = 0;
static int sfx_diag_events = 0;
static volatile unsigned sfx_pwm_errors;
static int sfx_timer_running;

/* No fade service/semaphore is needed for PCM. At runtime the esp_timer
 * callback is the SOLE writer of this LEDC channel. Initialization runs
 * before the sample timer starts, and shutdown does not write duty.
 */
static esp_err_t pwm_write_duty(uint32_t duty)
{
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, duty);
    return err == ESP_OK ? ledc_update_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL) : err;
}

static void amp_set_enabled(int enabled)
{
#if AMP_ENABLE_GPIO >= 0
    gpio_set_level((gpio_num_t)AMP_ENABLE_GPIO, enabled ? 1 : 0);
#else
    (void)enabled;
#endif
}

/* Must be called before sound init; external pulldown protects during RESET. */
void freslest_audio_mute_early(void)
{
#if AMP_ENABLE_GPIO >= 0
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << AMP_ENABLE_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_set_level((gpio_num_t)AMP_ENABLE_GPIO, 0);
    gpio_config(&cfg);
    gpio_set_level((gpio_num_t)AMP_ENABLE_GPIO, 0);
#endif
}

static void pwm_mix_callback(void *arg)
{
    (void)arg;
    if (!sfx_ready) return;
    int mixed = 0;
    portENTER_CRITICAL(&sfx_lock);
    for (int i = 0; i < PWM_VOICES; ++i) {
        pwm_voice_t *v = &sfx_voices[i];
        if (!v->active)
            continue;
        uint32_t index = v->phase >> 16;
        if (index >= v->count) {
            v->active = 0;
            continue;
        }
        mixed += ((int)v->pcm[index] - 128) * (int)v->gain / 255;
        v->phase += v->step;
        if ((v->phase >> 16) >= v->count)
            v->active = 0;
    }
    portEXIT_CRITICAL(&sfx_lock);

    /* +3.5 dB versus previous output; keep single voices unclipped.
     * Soft-limit overlapping effects near the PWM rails. */
    mixed *= 3;
    if (mixed > 448)
        mixed = 448 + ((mixed - 448) * 63) / (mixed - 448 + 63);
    else if (mixed < -448) {
        int excess = -mixed - 448;
        mixed = -(448 + (excess * 63) / (excess + 63));
    }
    int duty = PWM_CENTER + mixed;
    if (duty < 0) duty = 0;
    if (duty > 1023) duty = 1023;
    if (pwm_write_duty((uint32_t)duty) != ESP_OK) ++sfx_pwm_errors;
}

static int pwm_sfx_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = PWM_TIMER,
        .freq_hz = PWM_CARRIER_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_channel_config_t channel = {
        .gpio_num = PWM_SFX_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = PWM_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER,
        .duty = PWM_CENTER,
        .hpoint = 0,
    };
    if (ledc_timer_config(&timer) != ESP_OK ||
        ledc_channel_config(&channel) != ESP_OK)
        return 0;

    esp_err_t duty_err = pwm_write_duty(PWM_CENTER);
    printf("PWM_SFX: DIRECT_DUTY_READY rc=%d duty=%lu\n", (int)duty_err,
           (unsigned long)ledc_get_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL));
    if (duty_err != ESP_OK)
        return 0;

    esp_timer_create_args_t cb = {
        .callback = pwm_mix_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "doom_pwm_sfx",
        .skip_unhandled_events = 1,
    };
    if (esp_timer_create(&cb, &sfx_timer) != ESP_OK)
        return 0;
    sfx_pwm_errors = 0;
    sfx_timer_running = 0; /* initialization configures LEDC before callbacks exist */
    sfx_ready = 1;
    return 1;
}

static void pwm_sfx_shutdown(void)
{
    sfx_ready = 0;
    if (sfx_timer) {
        if (sfx_timer_running) esp_timer_stop(sfx_timer);
        esp_timer_delete(sfx_timer);
        sfx_timer = NULL;
        sfx_timer_running = 0;
    }
    portENTER_CRITICAL(&sfx_lock);
    memset(sfx_voices, 0, sizeof(sfx_voices));
    portEXIT_CRITICAL(&sfx_lock);
    /* Do not race the last in-flight callback by writing from the Doom task.
     * Its final sample may finish; the amp is already muted if EN is wired. */
}

/* Start at silent midpoint: no beep or frequency change. */
static void pwm_sfx_start(void)
{
    if (!sfx_ready || !sfx_timer) return;
    esp_err_t err = esp_timer_start_periodic(sfx_timer, 1000000 / PWM_SAMPLE_HZ);
    if (err != ESP_OK) {
        printf("PWM_SFX: START_ERROR rc=%d\n", (int)err);
        sfx_ready = 0;
        return;
    }
    sfx_timer_running = 1;
    printf("PWM_SFX: PCM_READY GPIO21 carrier=%luHz sample=%dHz; no boot beep\n",
           (unsigned long)ledc_get_freq(LEDC_LOW_SPEED_MODE, PWM_TIMER), PWM_SAMPLE_HZ);
}

/* /MP3 track numbers for Doom 1 music. Zero = no track in this pack. */
static const uint16_t df_track_for_music[NUMMUSIC] = {
    [mus_e1m1]  = 70,
    [mus_e1m2]  = 71,
    [mus_e1m3]  = 72,
    [mus_e1m4]  = 73,
    [mus_e1m5]  = 74,
    [mus_e1m6]  = 75,
    [mus_e1m7]  = 76,
    [mus_e1m8]  = 77,
    [mus_e1m9]  = 78,
    [mus_e2m1]  = 79,
    [mus_e2m2]  = 80,
    [mus_e2m3]  = 81,
    [mus_e2m4]  = 82,
    [mus_e2m5]  = 83,
    [mus_e2m6]  = 84,
    [mus_e2m7]  = 85,
    [mus_e2m8]  = 86,
    [mus_e2m9]  = 87,
    [mus_e3m1]  = 88,
    [mus_e3m2]  = 89,
    [mus_e3m3]  = 90,
    [mus_e3m4]  = 91,
    [mus_e3m5]  = 92,
    [mus_e3m6]  = 93,
    [mus_e3m7]  = 94,
    [mus_e3m8]  = 95,
    [mus_e3m9]  = 96,
    [mus_inter] = 97,
    [mus_intro] = 68,
    [mus_bunny] = 99,
    [mus_victor]= 98,
    [mus_introa]= 69
};

static int df_write_packet(uint8_t cmd, uint16_t param, int request_ack)
{
    uint8_t packet[10];
    uint16_t checksum;

    packet[0] = 0x7E;
    packet[1] = 0xFF;
    packet[2] = 0x06;
    packet[3] = cmd;
    packet[4] = request_ack ? 0x01 : 0x00;
    packet[5] = (uint8_t)(param >> 8);
    packet[6] = (uint8_t)(param & 0xFF);

    checksum = (uint16_t)(0 - (
        packet[1] + packet[2] + packet[3] +
        packet[4] + packet[5] + packet[6]
    ));

    packet[7] = (uint8_t)(checksum >> 8);
    packet[8] = (uint8_t)(checksum & 0xFF);
    packet[9] = 0xEF;

    int sent = uart_write_bytes(DF_UART, (const char *)packet, sizeof(packet));
    df_next_command_us = esp_timer_get_time() + DF_COMMAND_US;
    return sent == (int)sizeof(packet);
}

static void df_card_changed(int present)
{
    if (df_card_present != present) {
        df_card_present = present;
        ++df_media_serial;
    }
}

static void df_receive_packet(const uint8_t *p)
{
    uint16_t param = ((uint16_t)p[5] << 8) | p[6];
    switch (p[3]) {
    case 0x3A: /* inserted */
    case 0x3F: /* online sources */
        if (param & 2) df_card_changed(1);
        break;
    case 0x3B: /* removed */
        if (param & 2) df_card_changed(0);
        break;
    case 0x48: /* number of files on SD */
        if (param) df_card_changed(1);
        break;
    case 0x3D: /* SD playback completed (param may be a FAT index, NOT 0070) */
        df_end_pending = 1;
        break;
    case 0x42:
        df_status_value = param & 0xFF;
        ++df_status_serial;
        break;
    case 0x40:
        if (df_rx_errors++ < 8)
            printf("DFPLAYER: module error=%u (check SD and /MP3 files)\n", param);
        break;
    default:
        break;
    }
}

static void df_poll_rx(void)
{
    uint8_t bytes[64];
    int count;
    /* Bound work per tick, including a noisy/disconnected UART RX line. */
    for (int batch = 0; batch < 4; ++batch) {
        count = uart_read_bytes(DF_UART, bytes, sizeof(bytes), 0);
        if (count <= 0) break;
        for (int i = 0; i < count; ++i) {
            if (!df_rx_used && bytes[i] != 0x7E) continue;
            df_rx_packet[df_rx_used++] = bytes[i];
            if (df_rx_used != sizeof(df_rx_packet)) continue;
            uint16_t sum = 0;
            for (int j = 1; j <= 6; ++j) sum += df_rx_packet[j];
            uint16_t received = ((uint16_t)df_rx_packet[7] << 8) | df_rx_packet[8];
            if (df_rx_packet[1] == 0xFF && df_rx_packet[2] == 6 &&
                df_rx_packet[9] == 0xEF && received == (uint16_t)(0u - sum)) {
                df_receive_packet(df_rx_packet);
                df_rx_used = 0;
            } else {
                /* Recover even if a partial/broken packet preceded a good one. */
                unsigned start = 1;
                while (start < 10 && df_rx_packet[start] != 0x7E) ++start;
                df_rx_used = 10 - start;
                memmove(df_rx_packet, df_rx_packet + start, df_rx_used);
            }
        }
    }
}

typedef struct {
    uint32_t revision;
    uint32_t media_serial;
    unsigned phase; /* 0 loop-off, 1 stop, 2 play, 3 settle, 4 query, 5 ready */
    int volume;
    int paused;
    int retries;
    int64_t started_us;
    int64_t deadline_us;
    uint32_t query_serial;
} df_worker_state_t;

static df_worker_state_t df_state;

static df_request_t df_snapshot(void)
{
    df_request_t r;
    portENTER_CRITICAL(&df_lock);
    r = df_request;
    portEXIT_CRITICAL(&df_lock);
    return r;
}

/* One bounded step, called every 20 ms by the UART owner task. A newer music
 * request cancels an unfinished transition, including an intermission request
 * immediately followed by the next map. Never replay a saved old track. */
static void df_worker_step(int64_t now)
{
    df_poll_rx();
    df_request_t r = df_snapshot();
    if (df_state.revision != r.revision || df_state.media_serial != df_media_serial) {
        df_state.revision = r.revision;
        df_state.media_serial = df_media_serial;
        df_state.phase = 0;
        df_state.paused = 0;
        df_state.retries = 0;
        df_end_pending = 0;
    }
    if (now < df_next_command_us) return;

    if (df_state.volume != r.volume) {
        if (df_write_packet(0x06, r.volume, 0)) df_state.volume = r.volume;
        return;
    }
    switch (df_state.phase) {
    case 0:
        if (df_write_packet(0x19, 1, 0)) df_state.phase = 1; /* old native loop OFF */
        return;
    case 1:
        if (df_write_packet(0x16, 0, 0)) df_state.phase = r.track ? 2 : 5;
        return;
    case 6: /* track ended: one second of silence before repeating */
        if (r.paused) { df_state.deadline_us = now + 1000000; return; }
        if (now < df_state.deadline_us) return;
        df_state.phase = 2;
        /* fall through */
    case 2:
        if (!r.track || df_card_present == 0) return;
        df_end_pending = 0;
        if (df_write_packet(0x12, r.track, 0)) {
            df_state.started_us = now;
            df_state.deadline_us = now + DF_START_SETTLE_US;
            df_state.paused = 0;
            df_state.phase = 3;
        }
        return;
    case 3:
        df_end_pending = 0; /* discard delayed completion of the old file */
        if (now < df_state.deadline_us) return;
        if (r.paused) {
            if (df_write_packet(0x0E, 0, 0)) {
                df_state.paused = 1;
                df_state.phase = 5;
            }
        } else {
            df_state.query_serial = df_status_serial;
            if (df_write_packet(0x42, 0, 0)) {
                df_state.deadline_us = now + 400000;
                df_state.phase = 4;
            }
        }
        return;
    case 4:
        if (df_status_serial != df_state.query_serial) {
            if (df_status_value == 0 && df_state.retries < 2) {
                ++df_state.retries;
                df_state.phase = 1; /* stopped/busy: bounded STOP -> PLAY retry */
                return;
            }
            df_state.phase = 5;
        } else if (now >= df_state.deadline_us) {
            /* Missing RX feedback must not disable an otherwise working module. */
            df_state.phase = 5;
        }
        return;
    default:
        break;
    }
    if (!r.track) { df_end_pending = 0; return; }
    if (df_state.paused != r.paused) {
        if (df_write_packet(r.paused ? 0x0E : 0x0D, 0, 0)) {
            df_state.paused = r.paused;
            df_end_pending = 0;
        }
        return;
    }
    if (df_end_pending) {
        df_end_pending = 0;
        if (r.looping && !r.paused && now - df_state.started_us >= DF_START_SETTLE_US) {
            df_state.retries = 0;
            df_state.deadline_us = now + 1000000;
            df_state.phase = 6; /* delayed repeat of the CURRENT desired file */
        }
    }
}

static void df_music_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (df_snapshot().shutdown) break;
        df_worker_step(esp_timer_get_time());
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    int64_t remaining = df_next_command_us - esp_timer_get_time();
    if (remaining > 0) vTaskDelay(pdMS_TO_TICKS((remaining + 999) / 1000));
    df_write_packet(0x16, 0, 0);
    uart_wait_tx_done(DF_UART, pdMS_TO_TICKS(100));
    xSemaphoreGive(df_task_exited);
    vTaskDelete(NULL);
}

void I_UpdateSoundParams(int handle, int volume, int seperation, int pitch)
{
    (void)seperation;
    (void)pitch;
    if (handle < 0 || handle >= PWM_VOICES) return;
    if (volume < 0) volume = 0;
    if (volume > 127) volume = 127;
    portENTER_CRITICAL(&sfx_lock);
    if (sfx_voices[handle].active)
        sfx_voices[handle].gain = (uint8_t)((volume * 255) / 127);
    portEXIT_CRITICAL(&sfx_lock);
}

void I_SetChannels(void)
{
    extern int numChannels, default_numChannels;
    if (numChannels < 1) numChannels = 1;
    if (numChannels > PWM_VOICES) numChannels = PWM_VOICES;
    default_numChannels = numChannels;
}

int I_GetSfxLumpNum(sfxinfo_t *sfx)
{
    if (!sfx || !sfx->name) return -1;
    /* Linked sounds (e.g. chaingun) use the parent sample. */
    const char *name = sfx->link ? sfx->link->name : sfx->name;
    char lump[9];
    snprintf(lump, sizeof(lump), "ds%.6s", name);
    return W_CheckNumForName(lump);
}

int I_StartSound(int id, int channel, int vol, int sep, int pitch, int priority)
{
    (void)sep;
    (void)priority;
    if (!sfx_ready || channel < 0 || channel >= PWM_VOICES || id <= sfx_None || id >= NUMSFX) {
        if (sfx_diag_events++ < 8)
            printf("PWM_SFX: start rejected (ready=%d id=%d channel=%d)\n",
                   sfx_ready, id, channel);
        return -1;
    }

    /* S_Init has already populated this lump index through I_GetSfxLumpNum. */
    int lump = S_sfx[id].lumpnum;
    if (lump < 0) {
        if (sfx_diag_events++ < 8)
            printf("PWM_SFX: no WAD lump for sfx id=%d\n", id);
        return -1;
    }
    int length = W_LumpLength(lump);
    if (length <= 8) return -1;
    const uint8_t *raw = (const uint8_t *)W_CacheLumpNum(lump);
    if (!raw || raw[0] != 3 || raw[1] != 0) {
        if (sfx_diag_events++ < 8)
            printf("PWM_SFX: bad WAD sound for sfx id=%d lump=%d\n", id, lump);
        return -1;
    }

    uint32_t hz = (uint32_t)raw[2] | ((uint32_t)raw[3] << 8);
    uint32_t samples = (uint32_t)raw[4] | ((uint32_t)raw[5] << 8) |
                       ((uint32_t)raw[6] << 16) | ((uint32_t)raw[7] << 24);
    if (samples > (uint32_t)(length - 8)) samples = (uint32_t)(length - 8);
    if (!samples || hz < 1000 || hz > 48000) {
        if (sfx_diag_events++ < 8)
            printf("PWM_SFX: bad rate/length id=%d hz=%lu samples=%lu\n",
                   id, (unsigned long)hz, (unsigned long)samples);
        return -1;
    }

    /* Doom pitch 128 = native speed. PCM is resampled to 8 kHz. */
    if (pitch < 32) pitch = 32;
    if (pitch > 255) pitch = 255;
    uint32_t step = (uint32_t)(((uint64_t)hz * (uint32_t)pitch * 65536ULL) /
                               (128ULL * PWM_SAMPLE_HZ));
    if (!step) step = 1;
    if (vol < 0) vol = 0;
    if (vol > 127) vol = 127;

    int voice_idx = channel;
    portENTER_CRITICAL(&sfx_lock);
    sfx_voices[voice_idx] = (pwm_voice_t){
        .pcm = raw + 8,
        .count = samples,
        .phase = 0,
        .step = step,
        .gain = (uint8_t)((vol * 255) / 127),
        .active = 1,
    };
    portEXIT_CRITICAL(&sfx_lock);
    if (sfx_diag_events++ < 8)
        printf("PWM_SFX: START id=%d channel=%d vol=%d hz=%lu samples=%lu\n",
               id, channel, vol, (unsigned long)hz, (unsigned long)samples);
    return channel;
}

void I_StopSound(int handle)
{
    if (handle < 0 || handle >= PWM_VOICES) return;
    portENTER_CRITICAL(&sfx_lock);
    sfx_voices[handle].active = 0;
    portEXIT_CRITICAL(&sfx_lock);
}

int I_SoundIsPlaying(int handle)
{
    if (handle < 0 || handle >= PWM_VOICES) return 0;
    portENTER_CRITICAL(&sfx_lock);
    int playing = sfx_voices[handle].active;
    portEXIT_CRITICAL(&sfx_lock);
    return playing;
}

int I_AnySoundStillPlaying(void)
{
    int playing = 0;
    portENTER_CRITICAL(&sfx_lock);
    for (int i = 0; i < PWM_VOICES; ++i)
        if (sfx_voices[i].active) { playing = 1; break; }
    portEXIT_CRITICAL(&sfx_lock);
    return playing;
}

void I_ShutdownSound(void)
{
    amp_set_enabled(0);
    pwm_sfx_shutdown();
    if (df_ready) {
        portENTER_CRITICAL(&df_lock);
        df_request.shutdown = 1;
        portEXIT_CRITICAL(&df_lock);
        if (xSemaphoreTake(df_task_exited, pdMS_TO_TICKS(1500))) {
            /* The worker has flushed STOP; there is no concurrent UART writer. */
            uart_driver_delete(DF_UART);
            vSemaphoreDelete(df_task_exited);
            df_task_exited = NULL;
            df_task = NULL;
        } else {
            printf("DFPLAYER: shutdown timeout; UART left owned by music task\n");
        }
    }
    df_ready = 0;
}

void I_InitSound(void)
{
    if (df_ready || sfx_ready) return;
    freslest_audio_mute_early();

    if (!pwm_sfx_init())
        printf("PWM_SFX: FAILED to initialize; GPIO21 effects DISABLED\n");
    else
        printf("PWM_SFX: READY GPIO%d carrier=%d Hz sample=%d Hz voices=%d; "
               "silent PCM start after DFPlayer initialization\n",
               PWM_SFX_GPIO, PWM_CARRIER_HZ, PWM_SAMPLE_HZ, PWM_VOICES);

    uart_config_t uart_cfg = {
        .baud_rate = DF_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };
    if (uart_param_config(DF_UART, &uart_cfg) == ESP_OK &&
        uart_set_pin(DF_UART, DF_TX_GPIO, DF_RX_GPIO,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) == ESP_OK &&
        uart_driver_install(DF_UART, 1024, 256, 0, NULL, 0) == ESP_OK) {
        df_card_present = -1;
        df_media_serial = df_status_serial = 0;
        df_rx_used = df_rx_errors = df_end_pending = 0;
        uart_flush_input(DF_UART);
        /* RESET recovery: stop old DFPlayer music at the first possible UART moment. */
        df_write_packet(0x16, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(300));
        df_write_packet(0x0C, 0, 0); /* reset DFPlayer */
        vTaskDelay(pdMS_TO_TICKS(1200));
        df_write_packet(0x09, 2, 0); /* TF card */
        vTaskDelay(pdMS_TO_TICKS(300));
        // The Doom configuration / NVS has been loaded before I_InitSound.
        extern int snd_MusicVolume;
        int boot_volume = snd_MusicVolume;
        if (boot_volume < 0 || boot_volume > 15) boot_volume = DF_VOLUME / 2;
        df_write_packet(0x06, (uint16_t)(boot_volume * 2), 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        df_write_packet(0x1A, 0, 0); /* enable DAC */
        vTaskDelay(pdMS_TO_TICKS(100));
        df_poll_rx();
        if (df_card_present != 1) {
            df_write_packet(0x48, 0, 0); /* request SD file count */
            for (int i = 0; i < 15 && df_card_present != 1; ++i) {
                vTaskDelay(pdMS_TO_TICKS(20));
                df_poll_rx();
            }
        }
        memset(&df_request, 0, sizeof(df_request));
        df_request.volume = (uint8_t)(boot_volume * 2);
        df_request.revision = 1;
        memset(&df_state, 0, sizeof(df_state));
        df_state.revision = UINT32_MAX;
        df_state.volume = -1;
        df_task_exited = xSemaphoreCreateBinary();
        if (df_task_exited &&
            xTaskCreatePinnedToCore(df_music_task, "df_music", 4096, NULL,
                                    4, &df_task, 0) == pdPASS) {
            df_ready = 1;
            printf("DFPLAYER: music worker READY; SD=%s, /MP3 by filename\n",
                   df_card_present == 1 ? "online" : "RX not confirmed");
        } else {
            if (df_task_exited) vSemaphoreDelete(df_task_exited);
            df_task_exited = NULL;
            df_task = NULL;
            uart_driver_delete(DF_UART);
            printf("DFPLAYER: cannot allocate music task; music disabled\n");
        }
    } else {
        printf("DFPLAYER: UART initialization failed; music disabled\n");
    }
    snd_card = sfx_ready ? 1 : 0;
    mus_card = df_ready ? 1 : 0;
    /* Unmute only after both sources have initialized. GPIO42 cannot
     * silence anything unless physically wired to the verified amp EN pin. */
    amp_set_enabled(1);
    pwm_sfx_start();
}

void I_ShutdownMusic(void)
{
    I_StopSong(0);
}

void I_InitMusic(void)
{
}

void I_PlaySong(int handle, int looping)
{
    uint16_t track;
    (void)looping; /* User preference: repeat menu AND level music. */

    if (!df_ready || handle <= mus_None || handle >= NUMMUSIC)
        return;

    track = df_track_for_music[handle];
    portENTER_CRITICAL(&df_lock);
    df_request.track = track;
    df_request.looping = 1;
    df_request.paused = 0;
    ++df_request.revision;
    portEXIT_CRITICAL(&df_lock);
    /* One line per song change; never print from the 8 kHz sample callback. */
    if (track)
        printf("DFPLAYER: MUSIC %s -> /MP3/%04u.mp3 loop=%d\n",
               S_music[handle].name, track, 1);
}

extern int mus_pause_opt;

void I_PauseSong(int handle)
{
    (void)handle;
    if (!df_ready) return;
    portENTER_CRITICAL(&df_lock);
    if (df_request.track) df_request.paused = 1;
    portEXIT_CRITICAL(&df_lock);
}

void I_ResumeSong(int handle)
{
    (void)handle;
    if (!df_ready) return;
    portENTER_CRITICAL(&df_lock);
    df_request.paused = 0;
    portEXIT_CRITICAL(&df_lock);
}

void I_StopSong(int handle)
{
    (void)handle;

    if (!df_ready) return;
    portENTER_CRITICAL(&df_lock);
    df_request.track = 0;
    df_request.looping = 0;
    df_request.paused = 0;
    ++df_request.revision;
    portEXIT_CRITICAL(&df_lock);
}

void I_UnRegisterSong(int handle)
{
    (void)handle;
}

int I_RegisterSong(const void *data, size_t len)
{
    int i;

    if (!data || !len) return 0;

    /*
     * S_ChangeMusic stores the just-cached WAD pointer in S_music[i].data
     * immediately before calling us. Return the Doom music enum as handle.
     */
    for (i = 1; i < NUMMUSIC; ++i) {
        if (S_music[i].data == data)
            return i;
    }

    return 0;
}

int I_RegisterMusic(const char *filename, musicinfo_t *song)
{
    (void)filename;
    (void)song;
    return 1;
}

void I_SetMusicVolume(int volume)
{
    // Doom's Options slider is 0..15; DFPlayer UART volume is 0..30.
    if (volume < 0) volume = 0;
    if (volume > 15) volume = 15;
    if (!df_ready) return;
    portENTER_CRITICAL(&df_lock);
    df_request.volume = (uint8_t)(volume * 2);
    portEXIT_CRITICAL(&df_lock);
}
