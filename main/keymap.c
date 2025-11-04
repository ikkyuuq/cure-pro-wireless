#include "keymap.h"
#include "config.h"

// Define keymaps for each layer
static const key_def_t keymaps[MAX_LAYERS][MATRIX_ROW][MATRIX_COL] = {
#if !IS_MASTER
    // Layer 0 - Base layer Left Side
    // =    1  2  3  4  5
    // TAB  Q  W  E  R  T
    // CTRL A  S  D  F  G
    // ALT  Z  X  C  V  B
    //                  L1/TAB  GUI/SPC
    [0] = {{NORM_KEY(KC_EQUAL), NORM_KEY(KC_1), NORM_KEY(KC_2), NORM_KEY(KC_3),
            NORM_KEY(KC_4), NORM_KEY(KC_5)},
           {NORM_KEY(KC_ESC), NORM_KEY(KC_Q), NORM_KEY(KC_W), NORM_KEY(KC_E),
            NORM_KEY(KC_R), NORM_KEY(KC_T)},
           {MOD_KEY(KC_LCTRL), NORM_KEY(KC_A), NORM_KEY(KC_S), NORM_KEY(KC_D),
            NORM_KEY(KC_F), NORM_KEY(KC_G)},
           {MOD_KEY(KC_LALT), NORM_KEY(KC_Z), NORM_KEY(KC_X), NORM_KEY(KC_C),
            NORM_KEY(KC_V), NORM_KEY(KC_B)},
           {NORM_KEY(KC_NO), NORM_KEY(KC_NO), NORM_KEY(KC_NO), NORM_KEY(KC_NO),
            LT_TO(1, KC_TAB, 100), MT_TO(KC_LGUI, KC_SPACE, 100)}},
#else
    // Layer 0 - Base layer Right Side
    //        6    7    8    9    0    -
    //        Y    U    I    O    P    bslash
    //        H    J    K    L    ;    L1/'
    //        N    M    ,    .    /    RALT
    // SHIFT/ENT   L2/BS
    [0] = {{NORM_KEY(KC_6), NORM_KEY(KC_7), NORM_KEY(KC_8), NORM_KEY(KC_9),
            NORM_KEY(KC_0), NORM_KEY(KC_MINUS)},
           {NORM_KEY(KC_Y), NORM_KEY(KC_U), NORM_KEY(KC_I), NORM_KEY(KC_O),
            NORM_KEY(KC_P), NORM_KEY(KC_BSLASH)},
           {NORM_KEY(KC_H), NORM_KEY(KC_J), NORM_KEY(KC_K), NORM_KEY(KC_L),
            NORM_KEY(KC_SEMICOLON), LT(1, KC_QUOT)},
           {NORM_KEY(KC_N), NORM_KEY(KC_M), NORM_KEY(KC_COMMA),
            NORM_KEY(KC_DOT), NORM_KEY(KC_SLASH), MOD_KEY(KC_LGUI)},
           {MT_TO(KC_RSHIFT, KC_ENTER, 100), LT_TO(2, KC_BSPC, 100),
            NORM_KEY(KC_NO), NORM_KEY(KC_NO), NORM_KEY(KC_NO),
            NORM_KEY(KC_NO)}},
#endif

#if !IS_MASTER
    // Layer 1 - Function Symbol layer Left side
    // ESC  F2    F3    F4    F5    F6
    // TAB  `     <     >     -     |
    // CTRL !     *     /     =     &
    // ALT  ~     +     [     ]     %
    //                        NO  GUI/SPC
    [1] =
        {
            {TRANS_KEY(), NORM_KEY(KC_F2), NORM_KEY(KC_F3), NORM_KEY(KC_F4),
             NORM_KEY(KC_F5), NORM_KEY(KC_F6)},
            {TRANS_KEY(), NORM_KEY(KC_GRAVE), SHIFT_KEY(KC_DOT),
             SHIFT_KEY(KC_COMMA), NORM_KEY(KC_MINUS), SHIFT_KEY(KC_BSLASH)},
            {TRANS_KEY(), SHIFT_KEY(KC_1), SHIFT_KEY(KC_8), NORM_KEY(KC_SLASH),
             NORM_KEY(KC_EQUAL), SHIFT_KEY(KC_7)},
            {TRANS_KEY(), SHIFT_KEY(KC_GRAVE), SHIFT_KEY(KC_EQUAL),
             NORM_KEY(KC_LBRC), NORM_KEY(KC_RBRC), SHIFT_KEY(KC_5)},
            {NORM_KEY(KC_NO), NORM_KEY(KC_NO), NORM_KEY(KC_NO), NORM_KEY(KC_NO),
             TRANS_KEY(), TRANS_KEY()},
        },
#else
    // Layer 1 - Function Symbol layer Right side
    //          F7   F8   F9   F10  F11  F12
    //          ^    "    :    ;    _    bslash
    //          $    (    {    [    @    L1/'
    //          #    )    }    ]    NO   NO
    // SHIFT/ENT     0
    [1] =
        {
            {NORM_KEY(KC_F7), NORM_KEY(KC_F8), NORM_KEY(KC_F9),
             NORM_KEY(KC_F10), NORM_KEY(KC_F11), NORM_KEY(KC_F12)},
            {SHIFT_KEY(KC_6), SHIFT_KEY(KC_QUOT), SHIFT_KEY(KC_SEMICOLON),
             NORM_KEY(KC_SEMICOLON), SHIFT_KEY(KC_MINUS), TRANS_KEY()},
            {SHIFT_KEY(KC_4), SHIFT_KEY(KC_9), SHIFT_KEY(KC_LBRC),
             NORM_KEY(KC_LBRC), SHIFT_KEY(KC_2), TRANS_KEY()},
            {SHIFT_KEY(KC_3), SHIFT_KEY(KC_0), SHIFT_KEY(KC_RBRC),
             NORM_KEY(KC_RBRC), NORM_KEY(KC_NONE), NORM_KEY(KC_NONE)},
            {TRANS_KEY(), NORM_KEY(KC_0), NORM_KEY(KC_NO), NORM_KEY(KC_NO),
             NORM_KEY(KC_NO), NORM_KEY(KC_NO)},
        },
#endif

#if !IS_MASTER
    // Layer 2 - Media Navigation layer Left side
    // ESC      F2      F3      F4      F5      F6
    // TAB      NO      MUTE    VOL_D   VOL_U   NO
    // CTRL     NO      PREV    NEXT    PLAY    STOP
    // ALT      NO      NO      NO      NO      NO
    //                                  L1/TAB  GUI/SPC
    [2] =
        {
            {TRANS_KEY(), NORM_KEY(KC_F2), NORM_KEY(KC_F3), NORM_KEY(KC_F4),
             NORM_KEY(KC_F5), NORM_KEY(KC_F6)},
            {TRANS_KEY(), CONS_KEY(KC_BRIGHTNESS_UP), CONS_KEY(KC_AUDIO_MUTE),
             CONS_KEY(KC_AUDIO_VOL_DOWN), CONS_KEY(KC_AUDIO_VOL_UP),
             NORM_KEY(KC_NO)},
            {TRANS_KEY(), CONS_KEY(KC_BRIGHTNESS_DOWN),
             CONS_KEY(KC_MEDIA_PREV_TRACK), CONS_KEY(KC_MEDIA_NEXT_TRACK),
             CONS_KEY(KC_MEDIA_PLAY_PAUSE), CONS_KEY(KC_MEDIA_STOP)},
            {TRANS_KEY(), NORM_KEY(KC_NO), NORM_KEY(KC_NO), NORM_KEY(KC_NO),
             NORM_KEY(KC_NO), NORM_KEY(KC_NO)},
            {NORM_KEY(KC_NO), NORM_KEY(KC_NO), NORM_KEY(KC_NO), TRANS_KEY(),
             TRANS_KEY(), TRANS_KEY()},
        },
#else
    // Layer 2 - Media Navigation layer Right side
    //      F7       F8       F9       F10      F11      F12
    //      PGUP     HOME     UP       END      NO       DEL
    //      PGDOWN   LEFT     DOWN     RIGHT    NO       INS
    //      NO       NO       NO       NO       NO       NO
    // SHIFT/ENT     NO
    [2] =
        {
            {NORM_KEY(KC_F7), NORM_KEY(KC_F8), NORM_KEY(KC_F9),
             NORM_KEY(KC_F10), NORM_KEY(KC_F11), NORM_KEY(KC_F12)},
            {NORM_KEY(KC_PGUP), NORM_KEY(KC_HOME), NORM_KEY(KC_UP),
             NORM_KEY(KC_END), NORM_KEY(KC_NO), NORM_KEY(KC_DEL)},
            {NORM_KEY(KC_PGDN), NORM_KEY(KC_LEFT), NORM_KEY(KC_DOWN),
             NORM_KEY(KC_RIGHT), NORM_KEY(KC_NO), NORM_KEY(KC_INS)},
            {NORM_KEY(KC_NO), NORM_KEY(KC_NO), NORM_KEY(KC_NO), NORM_KEY(KC_NO),
             NORM_KEY(KC_NO), NORM_KEY(KC_NO)},
            {TRANS_KEY(), TRANS_KEY(), NORM_KEY(KC_NO), NORM_KEY(KC_NO),
             NORM_KEY(KC_NO), NORM_KEY(KC_NO)},
        },
#endif
};

key_def_t keymap_get_key(uint8_t layer, uint8_t row, uint8_t col)
{
  if (layer >= MAX_LAYERS || row >= MATRIX_ROW || col >= MATRIX_COL)
  {
    return NORM_KEY(KC_NO);
  }
  return keymaps[layer][row][col];
}

static const char *key_to_string(key_def_t key)
{
  switch (key.type)
  {
  case KEY_TYPE_NORMAL:
    switch (key.keycode)
    {
    case KC_A:
      return "A";
    case KC_B:
      return "B";
    case KC_C:
      return "C";
    case KC_D:
      return "D";
    case KC_E:
      return "E";
    case KC_F:
      return "F";
    case KC_G:
      return "G";
    case KC_H:
      return "H";
    case KC_I:
      return "I";
    case KC_J:
      return "J";
    case KC_K:
      return "K";
    case KC_L:
      return "L";
    case KC_M:
      return "M";
    case KC_N:
      return "N";
    case KC_O:
      return "O";
    case KC_P:
      return "P";
    case KC_Q:
      return "Q";
    case KC_R:
      return "R";
    case KC_S:
      return "S";
    case KC_T:
      return "T";
    case KC_U:
      return "U";
    case KC_V:
      return "V";
    case KC_W:
      return "W";
    case KC_X:
      return "X";
    case KC_Y:
      return "Y";
    case KC_Z:
      return "Z";
    case KC_1:
      return "1";
    case KC_2:
      return "2";
    case KC_3:
      return "3";
    case KC_4:
      return "4";
    case KC_5:
      return "5";
    case KC_6:
      return "6";
    case KC_7:
      return "7";
    case KC_8:
      return "8";
    case KC_9:
      return "9";
    case KC_0:
      return "0";
    case KC_ENTER:
      return "Enter";
    case KC_ESC:
      return "Esc";
    case KC_BSPC:
      return "Backspace";
    case KC_TAB:
      return "Tab";
    case KC_SPACE:
      return "Space";
    case KC_NO:
      return "None";
    case KC_TRNS:
      return "Transparent";
    default:
      return "Unknown";
    }
  case KEY_TYPE_MODIFIER:
    switch (key.modifier)
    {
    case KC_LCTRL:
      return "LCtrl";
    case KC_LSHIFT:
      return "LShift";
    case KC_LALT:
      return "LAlt";
    case KC_LGUI:
      return "LGui";
    case KC_RCTRL:
      return "RCtrl";
    case KC_RSHIFT:
      return "RShift";
    case KC_RALT:
      return "RAlt";
    case KC_RGUI:
      return "RGui";
    default:
      return "Mod";
    }
  case KEY_TYPE_LAYER_TAP:
    return "LayerTap";
  case KEY_TYPE_MOD_TAP:
    return "ModTap";
  case KEY_TYPE_LAYER_TOGGLE:
    return "LayerToggle";
  case KEY_TYPE_LAYER_MOMENTARY:
    return "LayerMomentary";
  case KEY_TYPE_CONSUMER:
    return "Media";
  case KEY_TYPE_MACRO:
    return "Macro";
  case KEY_TYPE_TRANSPARENT:
    return "Transparent";
  case KEY_TYPE_SHIFTED:
    switch (key.keycode)
    {
    case KC_1:
      return "!";
    case KC_2:
      return "@";
    case KC_3:
      return "#";
    case KC_4:
      return "$";
    case KC_5:
      return "%";
    case KC_6:
      return "^";
    case KC_7:
      return "&";
    case KC_8:
      return "*";
    case KC_9:
      return "(";
    case KC_0:
      return ")";
    case KC_MINUS:
      return "_";
    case KC_EQUAL:
      return "+";
    case KC_LBRC:
      return "{";
    case KC_RBRC:
      return "}";
    case KC_BSLASH:
      return "|";
    case KC_SEMICOLON:
      return ":";
    case KC_QUOT:
      return "\"";
    case KC_GRAVE:
      return "~";
    case KC_COMMA:
      return "<";
    case KC_DOT:
      return ">";
    case KC_SLASH:
      return "?";
    default:
      return "Shift+?";
    }
  default:
    return "Unknown";
  }
}
