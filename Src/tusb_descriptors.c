/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * HID map:
 *   - Interface 0: Generic HID
 *   - Interface 1: Keyboard HID
 */

#include "tusb.h"
#include "debug_leds.h"

#define USBD_VID    1155
#define USBD_PID_FS 22352

//--------------------------------------------------------------------+
// Macros
//--------------------------------------------------------------------+

// Descriptor Encoding
#define TUSB_CONFIGURATION_DESCRIPTOR(total_length, num_interfaces, configuration_value, iConfiguration, bmAttributes, bMaxPower) \
    9, TUSB_DESC_CONFIGURATION, ((total_length) & 0xFF), (((total_length) >> 8) & 0xFF), (num_interfaces), (configuration_value), (iConfiguration), (bmAttributes), (bMaxPower)
#define TUSB_INTERFACE_DESCRIPTOR(interface_number, alternate_setting, num_endpoints, interface_class, interface_subclass, interface_protocol, iInterface) \
    9, TUSB_DESC_INTERFACE, (interface_number), (alternate_setting), (num_endpoints), (interface_class), (interface_subclass), (interface_protocol), (iInterface)
#define TUSB_HID_CLASS_DESCRIPTOR(bcdHID, country_code, num_descriptors, report_descriptor_type, report_descriptor_length) \
    9, TUSB_DESC_CS_DEVICE, ((bcdHID) & 0xFF), (((bcdHID) >> 8) & 0xFF), (country_code), (num_descriptors), (report_descriptor_type), ((report_descriptor_length) & 0xFF), (((report_descriptor_length) >> 8) & 0xFF)
#define TUSB_ENDPOINT_DESCRIPTOR(endpoint_address, bmAttributes, max_packet_size, interval) \
    7, TUSB_DESC_ENDPOINT, (endpoint_address), (bmAttributes), ((max_packet_size) & 0xFF), (((max_packet_size) >> 8) & 0xFF), (interval)

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
// Device descriptor for the Dance Pad
tusb_desc_device_t const desc_device_dance_pad = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USBD_VID,
    .idProduct          = USBD_PID_FS, // same or different PID as desired
    .bcdDevice          = 0x0200,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,        // Points to "RE:Flex Dance Pad"
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

// Generic HID report descriptor
uint8_t const desc_hid_report[] = {
    HID_USAGE_PAGE_N(0xFF00, 2),
    HID_USAGE(0x01),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
      HID_USAGE_MIN(1),
      HID_USAGE_MAX(64),
      HID_LOGICAL_MIN(0),
      HID_LOGICAL_MAX_N(255, 2),
      HID_REPORT_SIZE(8),
      HID_REPORT_COUNT(64),
      HID_INPUT(0x02),
      HID_USAGE_MIN(1),
      HID_USAGE_MAX(64),
      HID_REPORT_SIZE(8),
      HID_REPORT_COUNT(64),
      HID_OUTPUT(0x02),
    HID_COLLECTION_END
};

// Keyboard report descriptor (boot keyboard)
uint8_t const desc_keyboard_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

// HID report descriptor callback to dispatch the requested interface instance.
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
    if (instance == 0) {
        return desc_hid_report;
    } else if (instance == 1) {
        return desc_keyboard_report;
    }
    return NULL;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

/*
 * USB Configuration Descriptor for a Composite Device
 *
 * This descriptor defines the overall configuration for a USB device that supports two interfaces:
 *   - Interface 0: Generic HID for vendor-specific communications.
 *   - Interface 1: Standard Boot Keyboard HID.
 *
 * The total descriptor length is 66 bytes, calculated as follows:
 *    9 bytes (Configuration Descriptor)
 * + 32 bytes (Interface 0 block: 9 + 9 + 7 + 7)
 * + 25 bytes (Interface 1 block: 9 + 9 + 7)
 * = 66 bytes.
 *
 * Reference Links:
 *   - USB in a Nutshell: https://www.beyondlogic.org/usbnutshell/usb1.shtml
 *   - USB 2.0 Specification: https://www.usb.org/document-library/usb-20-specification
 *   - TinyUSB Documentation: https://github.com/hathach/tinyusb
 */

#define CONFIG_TOTAL_LENGTH 66

uint8_t const desc_configuration[] = {
    TUSB_CONFIGURATION_DESCRIPTOR(CONFIG_TOTAL_LENGTH, 2, 1, 0, 0xC0, 250),

    // Interface 0: Generic HID
    TUSB_INTERFACE_DESCRIPTOR(0, 0, 2, 0x03, 0x00, 0x00, 0),
    TUSB_HID_CLASS_DESCRIPTOR(0x0111, 0x00, 1, TUSB_DESC_CS_CONFIGURATION, sizeof(desc_hid_report) / sizeof(desc_hid_report[0])),
    TUSB_ENDPOINT_DESCRIPTOR(0x81, 0x03, 64, 1), // IN
    TUSB_ENDPOINT_DESCRIPTOR(0x01, 0x03, 64, 1), // OUT

    // Interface 1: Keyboard HID
    TUSB_INTERFACE_DESCRIPTOR(1, 0, 1, 0x03, HID_SUBCLASS_BOOT, HID_PROTOCOL_KEYBOARD, 4),
    TUSB_HID_CLASS_DESCRIPTOR(0x0111, 0x00, 1, HID_DESC_TYPE_REPORT, sizeof(desc_keyboard_report) / sizeof(desc_keyboard_report[0])),
    TUSB_ENDPOINT_DESCRIPTOR(0x82, 0x03, 64, 10) // IN
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void) index; // for multiple configurations
    return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

static const char* string_desc_arr [] = {
    (const char[]) { 0x09, 0x04 },           // 0: Supported Language (English 0x0409)
    "Impulse Creations, Ltd.",               // 1: Manufacturer
    "RE:Flex Dance Pad",                     // 2: Product
    "123456",                                // 3: Serial Number (runtime generated)
    "RE:Flex Keyboard"                       // 4: Keyboard interface (internal use)
};

// Converts a uint32_t into 8 characters representing the hexadecimal
// digits the nibbles (4 bits) it's composed of represent
// but points to an array to copy the resulting characters to.
// This is a uint16_t array because that's tinyusb wants for unicode.
// len specifies the number of nibbles to process. 8 is the maximum.
// Starts reading at the most significant 4 nibbles.
static void uint32_t_to_chars(uint32_t val, uint16_t * buf, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        // Get most significant nibble
        uint32_t char_val = (val >> 28);

        buf[i] = char_val < 0xA 
          ? char_val + '0' // If nibble < 10, char = numeric digit
          : char_val + 'A' - 10; // Otherwise, A-F

        // Shift to next nibble
        val = val << 4;
    }
}

// Storage for string being requested, populated by tud_descriptor_string_cb
static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long
// enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;

    uint8_t chr_count;

    if (index == 0) {

        // Copy supported language bytes
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
      
    } else if (index == 3) {

        // Derive serial number
        uint32_t serial0, serial1, serial2;

        serial0 = *(uint32_t *) UID_BASE;
        serial1 = *(uint32_t *) (UID_BASE + 4);
        serial2 = *(uint32_t *) (UID_BASE + 8);

        serial0 += serial2;
        uint32_t_to_chars(serial0, _desc_str + 1, 8);
        uint32_t_to_chars(serial1, _desc_str + 9, 4);
        chr_count = 12;

    } else {

        // Convert ASCII string into UTF-16
        // Prevent index out of bounds access
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
            return NULL;
        }

        const char* str = string_desc_arr[index];

        // Cap at max char
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;

        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1+i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2 * chr_count + 2);

    return _desc_str;
}
 
 