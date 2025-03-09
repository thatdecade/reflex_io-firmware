#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>
#include <stdbool.h>
#include "msgbus.h"
#include "stm32f3xx_hal.h"

#define SENSOR_RESPONSE_LEN  8U
#define USB_HID_PACKET_SIZE_BYTES (64U)

void send_request_sensors(void);
bool auto_calibrate_sensors(void);
void process_sensor_data_with_config(Response * resp, uint8_t * usb_buffer);

#endif // SENSORS_H
