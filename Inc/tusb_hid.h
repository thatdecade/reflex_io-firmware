#ifndef __TUSB_HID_H
#define __TUSB_HID_H

#define USB_SEND_REPORT_ID (0)

#define USB_GENERIC_HID_INTERFACE (0)
#define USB_KEYBOARD_INTERFACE    (1)

uint8_t * usb_get_packet();

#endif