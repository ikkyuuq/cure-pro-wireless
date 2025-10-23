#ifndef HID_SVC_H
#define HID_SVC_H

#include "common.h"
#include "config.h"

// HID Report IDs
#define HID_KEYBOARD_REPORT_ID 0x01
#define HID_CONSUMER_REPORT_ID 0x02
#define HID_MOUSE_REPORT_ID    0x03
#define HID_SYSTEM_REPORT_ID   0x04

// HID Modifier keys
#define HID_MOD_LEFT_CTRL   0x01
#define HID_MOD_LEFT_SHIFT  0x02
#define HID_MOD_LEFT_ALT    0x04
#define HID_MOD_LEFT_GUI    0x08
#define HID_MOD_RIGHT_CTRL  0x10
#define HID_MOD_RIGHT_SHIFT 0x20
#define HID_MOD_RIGHT_ALT   0x40
#define HID_MOD_RIGHT_GUI   0x80

// Special keys
#define HID_KEY_NONE    0x00
#define HID_KEY_ERR_OVF 0x01

// Letters
#define HID_KEY_A 0x04
#define HID_KEY_B 0x05
#define HID_KEY_C 0x06
#define HID_KEY_D 0x07
#define HID_KEY_E 0x08
#define HID_KEY_F 0x09
#define HID_KEY_G 0x0A
#define HID_KEY_H 0x0B
#define HID_KEY_I 0x0C
#define HID_KEY_J 0x0D
#define HID_KEY_K 0x0E
#define HID_KEY_L 0x0F
#define HID_KEY_M 0x10
#define HID_KEY_N 0x11
#define HID_KEY_O 0x12
#define HID_KEY_P 0x13
#define HID_KEY_Q 0x14
#define HID_KEY_R 0x15
#define HID_KEY_S 0x16
#define HID_KEY_T 0x17
#define HID_KEY_U 0x18
#define HID_KEY_V 0x19
#define HID_KEY_W 0x1A
#define HID_KEY_X 0x1B
#define HID_KEY_Y 0x1C
#define HID_KEY_Z 0x1D

// Numbers
#define HID_KEY_1 0x1E
#define HID_KEY_2 0x1F
#define HID_KEY_3 0x20
#define HID_KEY_4 0x21
#define HID_KEY_5 0x22
#define HID_KEY_6 0x23
#define HID_KEY_7 0x24
#define HID_KEY_8 0x25
#define HID_KEY_9 0x26
#define HID_KEY_0 0x27

// Special characters and symbols
#define HID_KEY_ENTER      0x28
#define HID_KEY_ESC        0x29
#define HID_KEY_BACKSPACE  0x2A
#define HID_KEY_TAB        0x2B
#define HID_KEY_SPACE      0x2C
#define HID_KEY_MINUS      0x2D
#define HID_KEY_EQUAL      0x2E
#define HID_KEY_LEFTBRACE  0x2F
#define HID_KEY_RIGHTBRACE 0x30
#define HID_KEY_BACKSLASH  0x31
#define HID_KEY_HASHTILDE  0x32
#define HID_KEY_SEMICOLON  0x33
#define HID_KEY_APOSTROPHE 0x34
#define HID_KEY_GRAVE      0x35
#define HID_KEY_COMMA      0x36
#define HID_KEY_DOT        0x37
#define HID_KEY_SLASH      0x38
#define HID_KEY_CAPSLOCK   0x39

// Function keys
#define HID_KEY_F1  0x3A
#define HID_KEY_F2  0x3B
#define HID_KEY_F3  0x3C
#define HID_KEY_F4  0x3D
#define HID_KEY_F5  0x3E
#define HID_KEY_F6  0x3F
#define HID_KEY_F7  0x40
#define HID_KEY_F8  0x41
#define HID_KEY_F9  0x42
#define HID_KEY_F10 0x43
#define HID_KEY_F11 0x44
#define HID_KEY_F12 0x45

// System keys
#define HID_KEY_PRINTSCREEN 0x46
#define HID_KEY_SCROLLLOCK  0x47
#define HID_KEY_PAUSE       0x48
#define HID_KEY_INSERT      0x49
#define HID_KEY_HOME        0x4A
#define HID_KEY_PAGEUP      0x4B
#define HID_KEY_DELETE      0x4C
#define HID_KEY_END         0x4D
#define HID_KEY_PAGEDOWN    0x4E

// Arrow keys
#define HID_KEY_RIGHT 0x4F
#define HID_KEY_LEFT  0x50
#define HID_KEY_DOWN  0x51
#define HID_KEY_UP    0x52

// Numpad
#define HID_KEY_NUMLOCK    0x53
#define HID_KEY_KPSLASH    0x54
#define HID_KEY_KPASTERISK 0x55
#define HID_KEY_KPMINUS    0x56
#define HID_KEY_KPPLUS     0x57
#define HID_KEY_KPENTER    0x58
#define HID_KEY_KP1        0x59
#define HID_KEY_KP2        0x5A
#define HID_KEY_KP3        0x5B
#define HID_KEY_KP4        0x5C
#define HID_KEY_KP5        0x5D
#define HID_KEY_KP6        0x5E
#define HID_KEY_KP7        0x5F
#define HID_KEY_KP8        0x60
#define HID_KEY_KP9        0x61
#define HID_KEY_KP0        0x62
#define HID_KEY_KPDOT      0x63

// Additional keys
#define HID_KEY_102ND   0x64
#define HID_KEY_COMPOSE 0x65
#define HID_KEY_POWER   0x66
#define HID_KEY_KPEQUAL 0x67

// Extended Function keys
#define HID_KEY_F13 0x68
#define HID_KEY_F14 0x69
#define HID_KEY_F15 0x6A
#define HID_KEY_F16 0x6B
#define HID_KEY_F17 0x6C
#define HID_KEY_F18 0x6D
#define HID_KEY_F19 0x6E
#define HID_KEY_F20 0x6F
#define HID_KEY_F21 0x70
#define HID_KEY_F22 0x71
#define HID_KEY_F23 0x72
#define HID_KEY_F24 0x73

// Additional system keys
#define HID_KEY_MENU       0x76
#define HID_KEY_SELECT     0x77
#define HID_KEY_STOP       0x78
#define HID_KEY_AGAIN      0x79
#define HID_KEY_UNDO       0x7A
#define HID_KEY_CUT        0x7B
#define HID_KEY_COPY       0x7C
#define HID_KEY_PASTE      0x7D
#define HID_KEY_FIND       0x7E
#define HID_KEY_MUTE       0x7F
#define HID_KEY_VOLUMEUP   0x80
#define HID_KEY_VOLUMEDOWN 0x81

// Media keys (Consumer usage page)
#define HID_CONSUMER_BRIGHTNESS_UP   0x6F
#define HID_CONSUMER_BRIGHTNESS_DOWN 0x70

#define HID_CONSUMER_PLAY         0xB0
#define HID_CONSUMER_PAUSE        0xB1
#define HID_CONSUMER_RECORD       0xB2
#define HID_CONSUMER_FAST_FORWARD 0xB3
#define HID_CONSUMER_REWIND       0xB4
#define HID_CONSUMER_SCAN_NEXT    0xB5
#define HID_CONSUMER_SCAN_PREV    0xB6
#define HID_CONSUMER_STOP         0xB7
#define HID_CONSUMER_EJECT        0xB8

#define HID_CONSUMER_MUTE        0xE2
#define HID_CONSUMER_BASS_BOOST  0xE5
#define HID_CONSUMER_LOUDNESS    0xE7
#define HID_CONSUMER_VOLUME_UP   0xE9
#define HID_CONSUMER_VOLUME_DOWN 0xEA

#define HID_CONSUMER_PLAY_PAUSE 0xCD

typedef struct
{
  esp_hidd_dev_t *hid_dev;
  uint8_t         protocol_mode;
  uint8_t        *buffer;
} hid_param_t;

esp_err_t hid_svc_init(void);

#endif
