/**
 * @file keyboard.c
 * @brief High-level Keyboard Interface using TinyUSB HID.
 *
 * This source is derived from the work of cyborg5 (Chris Young)
 *     https://github.com/cyborg5/TinyUSB_Mouse_and_Keyboard/
 *
 * Description:
 *   High-level keyboard API for TinyUSB, managing an local key report.
 *
 * Usage example:
 *   keyboard_begin();              // Initialize the keyboard module.
 *   keyboard_press_and_release('A');           // Send the 'A' key (press and release).
 *   keyboard_press_and_hold('A');  // Send the 'A' key (press and hold).
 *   keyboard_release('A');         // Release 'A' key only
 *   keyboard_release_all();         // Release all keys
 *
 * Author: Dustin Westaby
 * Date: March 5, 2025
 */

#include "keyboard.h"
#include "tusb_hid.h"
#include <string.h>

static KeyReport _keyReport;

static void sendReport(void)
{
    tud_hid_keyboard_report(USB_KEYBOARD_INTERFACE, 0, _keyReport.modifiers, _keyReport.keys);
}

typedef struct {
    uint8_t keycode;
    uint8_t modifier;
} AsciiMapping;

static AsciiMapping map_ascii(uint8_t c)
{
    AsciiMapping mapping = {0, 0};
    if (c >= 'a' && c <= 'z') {
        mapping.keycode = (c - 'a') + 0x04;  // 'a' corresponds to 0x04
    } else if (c >= 'A' && c <= 'Z') {
        mapping.keycode = (c - 'A') + 0x04;
        mapping.modifier = 0x02;  // Left Shift
    } else if (c >= '1' && c <= '9') {
        mapping.keycode = (c - '1') + 0x1e;  // '1' = 0x1e
    } else if (c == '0') {
        mapping.keycode = 0x27;  // '0'
    } else if (c == ' ') {
        mapping.keycode = 0x2c;  // Space
    } else if (c == '\n' || c == '\r') {
        mapping.keycode = 0x28;  // Enter
    } else if (c == '\b') {
        mapping.keycode = 0x2a;  // Backspace
    }
    return mapping;
}

static bool isKeyInReport(uint8_t keycode)
{
    for (int i = 0; i < 6; i++) {
        if (_keyReport.keys[i] == keycode)
            return true;
    }
    return false;
}

static bool addKeyToReport(uint8_t keycode)
{
    if (isKeyInReport(keycode))
        return true; // already present
    for (int i = 0; i < 6; i++) {
        if (_keyReport.keys[i] == 0) {
            _keyReport.keys[i] = keycode;
            return true;
        }
    }
    return false; // no free slot available
}

static void removeKeyFromReport(uint8_t keycode)
{
    for (int i = 0; i < 6; i++) {
        if (_keyReport.keys[i] == keycode)
            _keyReport.keys[i] = 0;
    }
}

void keyboard_begin(void)
{
    memset(&_keyReport, 0, sizeof(_keyReport));
    sendReport();
}

void keyboard_end(void)
{
    keyboard_release_all();
}

size_t keyboard_press_and_hold(uint8_t key)
{
    if (key >= 0x80 && key <= 0x87) {
        uint8_t modifier_bit = 1 << (key - 0x80);
        _keyReport.modifiers |= modifier_bit;
        sendReport();
        return 1;
    } else {
        // Otherwise, assume key is an ASCII character.
        AsciiMapping mapping = map_ascii(key);
        if (mapping.keycode == 0)
            return 0; // no mapping available
        // Add any required modifier (for uppercase, etc.)
        _keyReport.modifiers |= mapping.modifier;
        if (!addKeyToReport(mapping.keycode))
            return 0; // no free slot
        sendReport();
        return 1;
    }
}

size_t keyboard_release(uint8_t key)
{
    if (key >= 0x80 && key <= 0x87) {
        uint8_t modifier_bit = 1 << (key - 0x80);
        _keyReport.modifiers &= ~modifier_bit;
        sendReport();
        return 1;
    } else {
        AsciiMapping mapping = map_ascii(key);
        if (mapping.keycode == 0)
            return 0;
        removeKeyFromReport(mapping.keycode);
        // Clear the modifier if it was set by this key.
        _keyReport.modifiers &= ~(mapping.modifier);
        sendReport();
        return 1;
    }
}

void keyboard_release_all(void)
{
    memset(&_keyReport, 0, sizeof(_keyReport));
    sendReport();
}

size_t keyboard_press_and_release(uint8_t key)
{
    size_t result = keyboard_press_and_hold(key);
    keyboard_release(key);
    return result;
}

size_t keyboard_string_to_keypresses(const uint8_t *buffer, size_t size)
{
    size_t n = 0;
    for (size_t i = 0; i < size; i++) {
        if (keyboard_press_and_release(buffer[i]))
            n++;
    }
    return n;
}
