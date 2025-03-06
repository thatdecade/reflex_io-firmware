#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// The HID keyboard report is 8 bytes:
//   Byte 0: Modifier keys (bit0 = Left Ctrl, bit1 = Left Shift, bit2 = Left Alt, etc.)
//   Byte 1: Reserved (always 0)
//   Bytes 2-7: Up to six keycodes (if 0 then no key)
typedef struct {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
} KeyReport;

// Initialize the keyboard (clear key report and send an empty report)
void keyboard_begin(void);

// End the keyboard (release all keys)
void keyboard_end(void);

// Press a key:
//  - For keys in the HID modifier range (e.g. 0x80–0x87) the corresponding modifier bit is set.
//  - For other values (assumed to be ASCII), a mapping is applied.
// Returns 1 on success, 0 on failure (for example if no free slot is available).
size_t keyboard_press_and_hold(uint8_t k);

// Release a key (using the same rules as keyboard_press_and_hold)
size_t keyboard_release(uint8_t k);

// Release all keys (clearing modifiers and key array)
void keyboard_release_all(void);

// Write a single character (press then release)
size_t keyboard_press_and_release(uint8_t c);

// Write an entire buffer/string (each character is written individually)
size_t keyboard_string_to_keypresses(const uint8_t *buffer, size_t size);

#endif // KEYBOARD_H
