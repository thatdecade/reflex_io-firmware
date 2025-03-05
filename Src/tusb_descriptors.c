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
 
 #define USBD_VID 1155
 #define USBD_PID_FS 22352
 
 
 //--------------------------------------------------------------------+
 // Device Descriptors
 //--------------------------------------------------------------------+
 tusb_desc_device_t const desc_device = {
     .bLength            = sizeof(tusb_desc_device_t),
     .bDescriptorType    = TUSB_DESC_DEVICE,
     .bcdUSB             = 0x0200,
     .bDeviceClass       = 0x00,
     .bDeviceSubClass    = 0x00,
     .bDeviceProtocol    = 0x00,
     .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
 
     .idVendor           = USBD_VID,
     .idProduct          = USBD_PID_FS,
     .bcdDevice          = 0x0200,
 
     .iManufacturer      = 0x01,
     .iProduct           = 0x02,
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
     0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
     0x09, 0x01,        // Usage (0x01)
     0xA1, 0x01,        // Collection (Application)
     0x19, 0x01,
     0x29, 0x40,
     0x15, 0x00,        // Logical Minimum (0)
     0x26, 0xFF, 0x00,  // Logical Maximum (255)
     0x75, 0x08,        // Report Size (8)
     0x95, 0x40,        //   Report Count (64)
     0x81, 0x02,        //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,
                        //   No Null Position)
     0x19, 0x01,
     0x29, 0x40,
     0x75, 0x08,
     0x95, 0x40,        //   Report Count (64)
     0x91, 0x02,        //   Output (Data,Array,Abs,No Wrap,Linear,Preferred State,
                        //   No Null Position,Non-volatile)
     0xC0,              // End Collection
 };
 
 // Keyboard report descriptor (boot keyboard)
 uint8_t const desc_keyboard_report[] = {
     0x05, 0x01,       // Usage Page (Generic Desktop)
     0x09, 0x06,       // Usage (Keyboard)
     0xA1, 0x01,       // Collection (Application)
     0x05, 0x07,       //   Usage Page (Keyboard/Keypad)
     0x19, 0xE0,       //   Usage Minimum (224)
     0x29, 0xE7,       //   Usage Maximum (231)
     0x15, 0x00,       //   Logical Minimum (0)
     0x25, 0x01,       //   Logical Maximum (1)
     0x75, 0x01,       //   Report Size (1)
     0x95, 0x08,       //   Report Count (8)
     0x81, 0x02,       //   Input (Data,Variable,Absolute) ; Modifier byte
     0x95, 0x01,       //   Report Count (1)
     0x75, 0x08,       //   Report Size (8)
     0x81, 0x03,       //   Input (Constant)                 ; Reserved byte
     0x95, 0x05,       //   Report Count (5)
     0x75, 0x01,       //   Report Size (1)
     0x05, 0x08,       //   Usage Page (LEDs)
     0x19, 0x01,       //   Usage Minimum (1)
     0x29, 0x05,       //   Usage Maximum (5)
     0x91, 0x02,       //   Output (Data,Variable,Absolute) ; LED report
     0x95, 0x01,       //   Report Count (1)
     0x75, 0x03,       //   Report Size (3)
     0x91, 0x03,       //   Output (Constant)                ; LED padding
     0x95, 0x06,       //   Report Count (6)
     0x75, 0x08,       //   Report Size (8)
     0x15, 0x00,       //   Logical Minimum (0)
     0x25, 0x65,       //   Logical Maximum (101)
     0x05, 0x07,       //   Usage Page (Keyboard/Keypad)
     0x19, 0x00,       //   Usage Minimum (0)
     0x29, 0x65,       //   Usage Maximum (101)
     0x81, 0x00,       //   Input (Data,Array)               ; Key array (6 bytes)
     0xC0              // End Collection
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
 
 // - Interface 0: Generic HID (2 endpoints: IN and OUT)
 //        ENDPOINTS
 //         1. Sensor data: device->host
 //         2. LED data: host->device
 // - Interface 1: Keyboard HID (boot keyboard, 1 endpoint: IN)
 //        ENDPOINTS
 //         1. Boot Keyaboard

 uint8_t const desc_configuration[] = {
     // ---------------------------
     // Configuration Descriptor (9 bytes)
     // ---------------------------
     
    //bLength, bDescriptorType, wTotalLength hibyte, wTotalLength lobyte, bConfigurationValue, iConfiguration, bmAttributes, bMaxPower
    9, TUSB_DESC_CONFIGURATION, 66, 0, 2, 1, 0, 0xC0, 250, // Total size = 9 (config) + 32 (interface 0) + 25 (interface 1) = 66 bytes.
    
     // ---------------------------
     // Interface 0: Generic HID
     // ---------------------------
     
    //bLength, bDescriptorType, bInterfaceNumber, bAlternateSetting, bNumEndpoints, bInterfaceClass, bInterfaceSubClass, bInterfaceProtocol, iInterface
    9, TUSB_DESC_INTERFACE, 0, 0, 2, 0x03, 0x00, 0x00, 0,

    //bLength, bDescriptorType, bcdHID lobyte, bcdHID hibyte, bCountryCode, bNumDescriptors, bDescriptionType
    9, TUSB_DESC_CS_DEVICE, 0x11, 0x01, 0x00, 0x01, TUSB_DESC_CS_CONFIGURATION,
       //wDescriptorLength, wDescriptorLength
       sizeof(desc_hid_report) / sizeof(desc_hid_report[0]), 0x00,

    //bLength, bDescriptorType, bEndpointAddress, bEndpointAddress, bmAttributes, wMaxPacketSize lobyte, wMaxPacketSize hibyte, bInterval
    7, TUSB_DESC_ENDPOINT, 0x81, 0x03, 64, 0, 1,

    //bLength, bDesccriptorType, bEndpointAddress, bmAttributes, wMaxPacketSize lobyte, wMaxPacketSize hibyte, bInterval
    7, TUSB_DESC_ENDPOINT, 0x01, 0x03, 64, 0, 1,
    

     // ---------------------------
     // Interface 1: Keyboard HID
     // ---------------------------
     

    //bLength, bDescriptorTypebInterfaceNumber, bAlternateSetting, bNumEndpoints, bInterfaceClass, bInterfaceSubClass, bInterfaceProtocol, iInterface, 
    9, TUSB_DESC_INTERFACE, 1, 0, 1, 0x03, HID_SUBCLASS_BOOT, HID_PROTOCOL_KEYBOARD, 4,


    // HID Descriptor for Interface 1 (9 bytes)
    //bLength, bDescriptorType, bcdHID, bCountryCode, bNumDescriptors, bDescriptorType, 
    9, HID_DESC_TYPE_HID, 0x11, 0x01, 0x00, 0x01, HID_DESC_TYPE_REPORT,
       //wDescriptorLength, wDescriptorLength
       sizeof(desc_keyboard_report) / sizeof(desc_keyboard_report[0]), 0x00,

    // Endpoint Descriptor for Interface 1 - IN
    //bLength, bDescriptorType, bEndpointAddress, bmAttributes, wMaxPacketSize, bInterval
    7, TUSB_DESC_ENDPOINT, 0x82, 0x03, 64, 0, 10
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
 
 char const* string_desc_arr [] = {
     (const char[]) { 0x09, 0x04 }, // 0: Supported language is English (0x0409)
     "Impulse Creations, Ltd.",     // 1: Manufacturer
     "RE:Flex Dance Pad",           // 2: Product
     "123456",                      // 3: Serials, should use chip ID
                                    //    Instead derived at runtime in
                                    //    tud_descriptor_string_cb
    "RE:Flex Keyboard"              // 4: Keyboard interface string
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
 
 