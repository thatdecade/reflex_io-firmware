#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>
#include <stdbool.h>

#define SENSOR_RESPONSE_LEN  8U

void send_request_sensors(void);
void process_sensor_data(Response * resp, uint8_t * usb_buffer);
bool auto_calibrate_sensors(void);
bool sensor_pad_is_active(uint8_t pad);

#endif // SENSORS_H
