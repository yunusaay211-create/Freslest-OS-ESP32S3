// SPDX-License-Identifier: Apache-2.0
// Seven active-low buttons. OK + direction cycles owned weapons in gameplay.
// Every 5 s, resend releases for physically released keys to repair lost KEYUP.
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "board_config.h"
#undef false
#undef true
#include "doomdef.h"
#include "doomtype.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_main.h"
#include "g_game.h"
#include "m_menu.h"
#include "gamepad.h"
#include "usb_doom.h"

#define DEBOUNCE_US 25000LL
#define RESET_HOLD_US 1500000LL
#define AUTO_RESET_US 5000000LL
#define USE_PULSE_US 80000LL

int usejoystick = 0;
int joyleft, joyright, joyup, joydown;

typedef struct {
    int pin;
    int *key;
    int stable;
    int raw;
    int posted;
    int64_t changed;
} Button;

static Button buttons[] = {
    {BTN_UP, &key_up, 0, 0, 0, 0}, {BTN_DOWN, &key_down, 0, 0, 0, 0},
    {BTN_LEFT, &key_left, 0, 0, 0, 0}, {BTN_RIGHT, &key_right, 0, 0, 0, 0},
    {BTN_OK, &key_use, 0, 0, 0, 0}, {BTN_BACK, &key_escape, 0, 0, 0, 0},
    {BTN_FIRE, &key_fire, 0, 0, 0, 0}
};
#define BUTTON_COUNT (sizeof(buttons) / sizeof(buttons[0]))
#define OK_INDEX 4
#define BACK_INDEX 5

static int64_t combo_since = -1;
static int combo_blocked;
static int combo_active;
static int enter_posted;
static int ok_deferred;
static int ok_modifier_used;
static int weapon_mode_prev;
static int weapon_queued = -1;
static int64_t use_pulse_until;
static int64_t next_auto_reset;

static void post(int key, int down)
{
    if (!key) return;
    if (!down && usbDoomKeyHeld(key)) return;
    event_t ev = {0};
    ev.type = down ? ev_keydown : ev_keyup;
    ev.data1 = key;
    D_PostEvent(&ev);
}

static void sync_button(Button *b, int desired_key)
{
    if (b->posted == desired_key) return;
    if (b->posted) post(b->posted, 0);
    b->posted = desired_key;
    if (desired_key) post(desired_key, 1);
}

static int in_gameplay(void)
{
    return gamestate == GS_LEVEL && !menuactive && !paused &&
           players[consoleplayer].playerstate == PST_LIVE;
}

static int selectable_weapon(const player_t *p, int weapon)
{
    if (weapon < 0 || weapon >= NUMWEAPONS || !p->weaponowned[weapon]) return 0;
    if (gamemode == shareware && (weapon == wp_plasma || weapon == wp_bfg)) return 0;
    if (gamemode != commercial && weapon == wp_supershotgun) return 0;
    return 1;
}

static void cycle_weapon(int direction)
{
    static const int order[] = {
        wp_fist, wp_chainsaw, wp_pistol, wp_shotgun, wp_supershotgun,
        wp_chaingun, wp_missile, wp_plasma, wp_bfg
    };
    const int count = (int)(sizeof(order) / sizeof(order[0]));
    const player_t *p = &players[consoleplayer];
    int current = weapon_queued >= 0 ? weapon_queued :
                  (int)(p->pendingweapon != wp_nochange ? p->pendingweapon : p->readyweapon);
    int position = 0;
    for (int i = 0; i < count; ++i)
        if (order[i] == current) { position = i; break; }
    for (int n = 1; n <= count; ++n) {
        int i = (position + direction * n + count) % count;
        if (selectable_weapon(p, order[i])) {
            if (order[i] != current) weapon_queued = order[i];
            return;
        }
    }
}

/* G_BuildTiccmd consumes this once, in the normal Doom weapon-change command.
 * A direct weapon ID also distinguishes fists from chainsaw, unlike key '1'. */
int gamepadTakeWeapon(void)
{
    int selected = weapon_queued;
    weapon_queued = -1;
    if (!in_gameplay() || !selectable_weapon(&players[consoleplayer], selected)) return -1;
    return selected;
}

static void release_all(int64_t now)
{
    for (unsigned i = 0; i < BUTTON_COUNT; ++i) {
        Button *b = &buttons[i];
        sync_button(b, 0);
        post(*b->key, 0);
        b->stable = b->raw = 0;
        b->changed = now;
    }
    post(key_menu_enter, 0);
    post(key_menu_backspace, 0);
    if (use_pulse_until) post(key_use, 0);
    enter_posted = ok_deferred = ok_modifier_used = weapon_mode_prev = 0;
    weapon_queued = -1;
    use_pulse_until = 0;
}

static void repair_released_keys(int64_t now)
{
    if (now < next_auto_reset) return;
    next_auto_reset = now + AUTO_RESET_US;
    for (unsigned i = 0; i < BUTTON_COUNT; ++i) {
        Button *b = &buttons[i];
        /* Held FIRE/movement continues without interruption or new presses. */
        if (!gpio_get_level((gpio_num_t)b->pin)) continue;
        if (b->pin == BTN_OK && use_pulse_until > now) continue;
        post(*b->key, 0);
        if (b->pin == BTN_OK) post(key_menu_enter, 0);
        if (b->pin == BTN_BACK) post(key_menu_backspace, 0);
    }
}

void gamepadInit(void)
{
    uint64_t mask = 0;
    for (unsigned i = 0; i < BUTTON_COUNT; ++i) mask |= 1ULL << buttons[i].pin;
    gpio_config_t cfg = {
        .pin_bit_mask = mask, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    int64_t now = esp_timer_get_time();
    for (unsigned i = 0; i < BUTTON_COUNT; ++i) {
        buttons[i].stable = buttons[i].raw = buttons[i].posted = 0;
        buttons[i].changed = now;
    }
    combo_since = -1;
    combo_blocked = combo_active = enter_posted = ok_deferred = 0;
    ok_modifier_used = weapon_mode_prev = 0;
    weapon_queued = -1;
    use_pulse_until = 0;
    next_auto_reset = now + AUTO_RESET_US;
}

bool gamepadKeyHeld(int key){
    for(unsigned i=0;i<BUTTON_COUNT;++i)if(buttons[i].posted==key)return true;
    return (enter_posted&&key==key_menu_enter)||(use_pulse_until&&key==key_use);
}
void gamepadPoll(void)
{
    usbDoomPoll();
    int64_t now = esp_timer_get_time();
    if (use_pulse_until && (now >= use_pulse_until || !in_gameplay())) {
        post(key_use, 0);
        use_pulse_until = 0;
    }
    repair_released_keys(now);
    int back = !gpio_get_level(BTN_BACK);
    int ok = !gpio_get_level(BTN_OK);
    if (back && ok) {
        if (combo_since < 0) combo_since = now;
        if (!combo_active) { release_all(now); combo_active = 1; }
        if (!combo_blocked && now - combo_since >= RESET_HOLD_US) {
            release_all(now);
            combo_blocked = 1;
        }
        return;
    }
    if (combo_active) {
        // Require BOTH combo keys to be released, including after a short chord.
        if (back || ok) return;
        release_all(now);
        combo_active = combo_blocked = 0;
        combo_since = -1;
        return;
    }
    combo_since = -1;

    int old[BUTTON_COUNT], pressed[BUTTON_COUNT];
    for (unsigned i = 0; i < BUTTON_COUNT; ++i) {
        Button *b = &buttons[i];
        old[i] = b->stable;
        int raw = !gpio_get_level(b->pin);
        if (raw != b->raw) { b->raw = raw; b->changed = now; }
        if (raw != b->stable && now - b->changed >= DEBOUNCE_US) b->stable = raw;
        pressed[i] = b->stable;
    }
    int gameplay = in_gameplay();
    int ok_down = pressed[OK_INDEX];
    int weapon_mode = gameplay && ok_down && !pressed[BACK_INDEX];
    if (!gameplay) { weapon_queued = -1; ok_deferred = 0; }
    if (ok_down && !old[OK_INDEX] && gameplay) {
        ok_deferred = 1;
        ok_modifier_used = 0;
    }
    if (weapon_mode) {
        for (int i = 0; i < 4; ++i) {
            if (pressed[i]) {
                ok_modifier_used = 1;
                if (!old[i] || !weapon_mode_prev) {
                    // UP/RIGHT next; DOWN/LEFT previous. One step per press.
                    cycle_weapon(i == 0 || i == 3 ? 1 : -1);
                    break;
                }
            }
        }
    }
    if (!ok_down && old[OK_INDEX]) {
        if (ok_deferred && !ok_modifier_used && gameplay) {
            // OK alone activates a door on release, after chord intent is known.
            post(key_use, 1);
            use_pulse_until = now + USE_PULSE_US;
        }
        ok_deferred = ok_modifier_used = 0;
    }
    for (unsigned i = 0; i < BUTTON_COUNT; ++i) {
        Button *b = &buttons[i];
        int desired = pressed[i] ? *b->key : 0;
        if (i < 4 && weapon_mode) desired = 0;
        if (b->pin == BTN_OK && (gameplay || ok_modifier_used)) desired = 0;
        if (b->pin == BTN_BACK && pressed[i]) {
            // Keep the key chosen on press, so its KEYUP always matches.
            desired = b->posted ? b->posted :
                      (Freslest_MenuHasParent() ? key_menu_backspace : *b->key);
        }
        sync_button(b, desired);
    }
    int enter = !gameplay && ok_down && !ok_modifier_used;
    if (enter != enter_posted) { post(key_menu_enter, enter); enter_posted = enter; }
    weapon_mode_prev = weapon_mode;
}
