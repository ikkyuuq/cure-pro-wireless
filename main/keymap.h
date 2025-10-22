#ifndef KEYMAP_H
#define KEYMAP_H

#include "hid_gatt_svr_svc.h"

// Key type definitions
typedef enum {
    KEY_TYPE_NORMAL,
    KEY_TYPE_MODIFIER,
    KEY_TYPE_SHIFTED,
    KEY_TYPE_LAYER_TAP,
    KEY_TYPE_MOD_TAP,
    KEY_TYPE_LAYER_MOMENTARY,
    KEY_TYPE_LAYER_TOGGLE,
    KEY_TYPE_CONSUMER,
    KEY_TYPE_MACRO,
    KEY_TYPE_TRANSPARENT
} key_type_t;

// Key definition structure
typedef struct {
    key_type_t type;

    union {
        uint8_t keycode;        // For normal keys
        uint8_t modifier;       // For modifier keys
        uint16_t consumer;      // For consumer keys
        struct {
            uint8_t tap_key;
            uint8_t hold_key;
            uint16_t tap_timeout_ms;  // 0 = use default TAP_TIMEOUT_MS
        } mod_tap;
        struct {
            uint8_t tap_key;
            uint8_t layer;
            uint16_t tap_timeout_ms;  // 0 = use default TAP_TIMEOUT_MS
        } layer_tap;
        uint8_t layer;          // For layer keys
        uint8_t macro_id;       // For macros
    };
} key_definition_t;

// Letter keys
#define KC_A    HID_KEY_A
#define KC_B    HID_KEY_B
#define KC_C    HID_KEY_C
#define KC_D    HID_KEY_D
#define KC_E    HID_KEY_E
#define KC_F    HID_KEY_F
#define KC_G    HID_KEY_G
#define KC_H    HID_KEY_H
#define KC_I    HID_KEY_I
#define KC_J    HID_KEY_J
#define KC_K    HID_KEY_K
#define KC_L    HID_KEY_L
#define KC_M    HID_KEY_M
#define KC_N    HID_KEY_N
#define KC_O    HID_KEY_O
#define KC_P    HID_KEY_P
#define KC_Q    HID_KEY_Q
#define KC_R    HID_KEY_R
#define KC_S    HID_KEY_S
#define KC_T    HID_KEY_T
#define KC_U    HID_KEY_U
#define KC_V    HID_KEY_V
#define KC_W    HID_KEY_W
#define KC_X    HID_KEY_X
#define KC_Y    HID_KEY_Y
#define KC_Z    HID_KEY_Z

// Number keys
#define KC_1    HID_KEY_1
#define KC_2    HID_KEY_2
#define KC_3    HID_KEY_3
#define KC_4    HID_KEY_4
#define KC_5    HID_KEY_5
#define KC_6    HID_KEY_6
#define KC_7    HID_KEY_7
#define KC_8    HID_KEY_8
#define KC_9    HID_KEY_9
#define KC_0    HID_KEY_0

// Special characters
#define KC_ENTER        HID_KEY_ENTER
#define KC_ENT          HID_KEY_ENTER
#define KC_RET          HID_KEY_ENTER
#define KC_ESC          HID_KEY_ESC
#define KC_ESCAPE       HID_KEY_ESC
#define KC_BSPACE       HID_KEY_BACKSPACE
#define KC_BSPC         HID_KEY_BACKSPACE
#define KC_TAB          HID_KEY_TAB
#define KC_SPACE        HID_KEY_SPACE
#define KC_SPC          HID_KEY_SPACE
#define KC_MINUS        HID_KEY_MINUS
#define KC_MINS         HID_KEY_MINUS
#define KC_EQUAL        HID_KEY_EQUAL
#define KC_EQL          HID_KEY_EQUAL
#define KC_LEFTBRACE    HID_KEY_LEFTBRACE
#define KC_LBRC         HID_KEY_LEFTBRACE
#define KC_RIGHTBRACE   HID_KEY_RIGHTBRACE
#define KC_RBRC         HID_KEY_RIGHTBRACE
#define KC_BACKSLASH    HID_KEY_BACKSLASH
#define KC_BSLASH       HID_KEY_BACKSLASH
#define KC_SEMICOLON    HID_KEY_SEMICOLON
#define KC_SCLN         HID_KEY_SEMICOLON
#define KC_APOSTROPHE   HID_KEY_APOSTROPHE
#define KC_QUOT         HID_KEY_APOSTROPHE
#define KC_SINGLEQUOTE  HID_KEY_APOSTROPHE
#define KC_GRAVE        HID_KEY_GRAVE
#define KC_GRV          HID_KEY_GRAVE
#define KC_COMMA        HID_KEY_COMMA
#define KC_COMM         HID_KEY_COMMA
#define KC_DOT          HID_KEY_DOT
#define KC_SLASH        HID_KEY_SLASH
#define KC_SLSH         HID_KEY_SLASH
#define KC_CAPSLOCK     HID_KEY_CAPSLOCK
#define KC_CAPS         HID_KEY_CAPSLOCK

// Function keys
#define KC_F1           HID_KEY_F1
#define KC_F2           HID_KEY_F2
#define KC_F3           HID_KEY_F3
#define KC_F4           HID_KEY_F4
#define KC_F5           HID_KEY_F5
#define KC_F6           HID_KEY_F6
#define KC_F7           HID_KEY_F7
#define KC_F8           HID_KEY_F8
#define KC_F9           HID_KEY_F9
#define KC_F10          HID_KEY_F10
#define KC_F11          HID_KEY_F11
#define KC_F12          HID_KEY_F12

// Arrow keys
#define KC_RIGHT        HID_KEY_RIGHT
#define KC_RGHT         HID_KEY_RIGHT
#define KC_LEFT         HID_KEY_LEFT
#define KC_DOWN         HID_KEY_DOWN
#define KC_UP           HID_KEY_UP

// System keys
#define KC_PRINTSCREEN  HID_KEY_PRINTSCREEN
#define KC_PSCR         HID_KEY_PRINTSCREEN
#define KC_SCROLLLOCK   HID_KEY_SCROLLLOCK
#define KC_SLCK         HID_KEY_SCROLLLOCK
#define KC_PAUSE        HID_KEY_PAUSE
#define KC_PAUS         HID_KEY_PAUSE
#define KC_INSERT       HID_KEY_INSERT
#define KC_INS          HID_KEY_INSERT
#define KC_HOME         HID_KEY_HOME
#define KC_PAGEUP       HID_KEY_PAGEUP
#define KC_PGUP         HID_KEY_PAGEUP
#define KC_DELETE       HID_KEY_DELETE
#define KC_DEL          HID_KEY_DELETE
#define KC_END          HID_KEY_END
#define KC_PAGEDOWN     HID_KEY_PAGEDOWN
#define KC_PGDN         HID_KEY_PAGEDOWN

// Modifiers
#define KC_LCTRL        HID_MOD_LEFT_CTRL
#define KC_LSHIFT       HID_MOD_LEFT_SHIFT
#define KC_LSFT         HID_MOD_LEFT_SHIFT
#define KC_LALT         HID_MOD_LEFT_ALT
#define KC_LGUI         HID_MOD_LEFT_GUI
#define KC_LCMD         HID_MOD_LEFT_GUI
#define KC_LWIN         HID_MOD_LEFT_GUI
#define KC_RCTRL        HID_MOD_RIGHT_CTRL
#define KC_RSHIFT       HID_MOD_RIGHT_SHIFT
#define KC_RSFT         HID_MOD_RIGHT_SHIFT
#define KC_RALT         HID_MOD_RIGHT_ALT
#define KC_RGUI         HID_MOD_RIGHT_GUI
#define KC_RCMD         HID_MOD_RIGHT_GUI
#define KC_RWIN         HID_MOD_RIGHT_GUI

// Special values
#define KC_NO           HID_KEY_NONE
#define KC_NONE         HID_KEY_NONE
#define KC_TRNS         0xFF  // Transparent key (use lower layer)

// Numpad keys
#define KC_KP_SLASH     HID_KEY_KPSLASH
#define KC_KP_ASTERISK  HID_KEY_KPASTERISK
#define KC_KP_MINUS     HID_KEY_KPMINUS
#define KC_KP_PLUS      HID_KEY_KPPLUS
#define KC_KP_ENTER     HID_KEY_KPENTER
#define KC_KP_1         HID_KEY_KP1
#define KC_KP_2         HID_KEY_KP2
#define KC_KP_3         HID_KEY_KP3
#define KC_KP_4         HID_KEY_KP4
#define KC_KP_5         HID_KEY_KP5
#define KC_KP_6         HID_KEY_KP6
#define KC_KP_7         HID_KEY_KP7
#define KC_KP_8         HID_KEY_KP8
#define KC_KP_9         HID_KEY_KP9
#define KC_KP_0         HID_KEY_KP0
#define KC_KP_DOT       HID_KEY_KPDOT

// Media keys
#define KC_BRIGHTNESS_UP        HID_CONSUMER_BRIGHTNESS_UP
#define KC_BRIGHTNESS_DOWN      HID_CONSUMER_BRIGHTNESS_DOWN

#define KC_MEDIA_PLAY           HID_CONSUMER_PLAY
#define KC_MEDIA_PAUSE          HID_CONSUMER_PAUSE
#define KC_MEDIA_PLAY_PAUSE     HID_CONSUMER_PLAY_PAUSE
#define KC_MEDIA_RECORD         HID_CONSUMER_RECORD
#define KC_MEDIA_FAST_FORWARD   HID_CONSUMER_FAST_FORWARD
#define KC_MEDIA_REWIND         HID_CONSUMER_REWIND
#define KC_MEDIA_NEXT_TRACK     HID_CONSUMER_SCAN_NEXT
#define KC_MEDIA_PREV_TRACK     HID_CONSUMER_SCAN_PREV
#define KC_MEDIA_STOP           HID_CONSUMER_STOP
#define KC_MEDIA_EJECT          HID_CONSUMER_EJECT

#define KC_AUDIO_MUTE           HID_CONSUMER_MUTE
#define KC_AUDIO_BASS_BOOST     HID_CONSUMER_BASS_BOOST
#define KC_AUDIO_LOUDNESS       HID_CONSUMER_LOUDNESS
#define KC_AUDIO_VOL_UP         HID_CONSUMER_VOLUME_UP
#define KC_AUDIO_VOL_DOWN       HID_CONSUMER_VOLUME_DOWN

// Macro functions for creating complex key definitions
#define NORM_KEY(k)             ((key_definition_t){.type = KEY_TYPE_NORMAL,            .keycode = (k)})
#define MOD_KEY(m)              ((key_definition_t){.type = KEY_TYPE_MODIFIER,          .modifier = (m)})
#define LAYER_TAP(t, l)         ((key_definition_t){.type = KEY_TYPE_LAYER_TAP,         .layer_tap = {(t), (l), 0}})
#define MOD_TAP(t, m)           ((key_definition_t){.type = KEY_TYPE_MOD_TAP,           .mod_tap = {(t), (m), 0}})
#define LAYER_TAP_TO(t, l, to)  ((key_definition_t){.type = KEY_TYPE_LAYER_TAP,         .layer_tap = {(t), (l), (to)}})
#define MOD_TAP_TO(t, m, to)    ((key_definition_t){.type = KEY_TYPE_MOD_TAP,           .mod_tap = {(t), (m), (to)}})
#define LAYER_TOG(l)            ((key_definition_t){.type = KEY_TYPE_LAYER_TOGGLE,      .layer = (l)})
#define LAYER_MOM(l)            ((key_definition_t){.type = KEY_TYPE_LAYER_MOMENTARY,   .layer = (l)})
#define CONS_KEY(k)             ((key_definition_t){.type = KEY_TYPE_CONSUMER,          .consumer = (k)})
#define MACRO_KEY(id)           ((key_definition_t){.type = KEY_TYPE_MACRO,             .macro_id = (id)})
#define TRANS_KEY()             ((key_definition_t){.type = KEY_TYPE_TRANSPARENT,       .keycode = KC_TRNS})
#define SHIFT_KEY(k)            ((key_definition_t){.type = KEY_TYPE_SHIFTED,           .keycode = (k)})

// Convenient shortcuts
#define LT(layer, tap)          LAYER_TAP(tap, layer)
#define MT(mod, tap)            MOD_TAP(tap, mod)
#define LT_TO(layer, tap, to)   LAYER_TAP_TO(tap, layer, to)
#define MT_TO(mod, tap, to)     MOD_TAP_TO(tap, mod, to)
#define TO(layer)               LAYER_TOG(layer)
#define MO(layer)               LAYER_MOM(layer)

// Function declarations
key_definition_t keymap_get_key(uint8_t layer, uint8_t row, uint8_t col);
void keymap_init(void);
const char* keymap_key_to_string(key_definition_t key);

#endif
