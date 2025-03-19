#ifndef PROFILE_CONFIG_H
#define PROFILE_CONFIG_H

#include "eeprom_emul.h"
#include <stdint.h>
#include <stdbool.h>

#define DEFAULT_SENSOR_THRESHOLD   500  // the amount above idle needed to consider the pad pressed
#define DEFAULT_SENSOR_HYSTERESIS   50  // to avoid rapid toggling, once active the pad will remain active until sensor reading falls below (THRESHOLD - HYSTERESIS)
#define DEFAULT_SENSOR_COOLDOWN     60  // minimum time (ms) between state transitions
#define DEFAULT_PANEL0_KEYPRESS  '0'
#define DEFAULT_PANEL1_KEYPRESS  '1'
#define DEFAULT_PANEL2_KEYPRESS  '3'
#define DEFAULT_PANEL3_KEYPRESS  '4'

#define CURRENT_PROFILE_VERSION 1

typedef struct {
    uint8_t profile_version;       //change CURRENT_PROFILE_VERSION whenever this structure changes!!
    uint8_t calibration_type;      // 2 bit mask: bit01 for pad0, bit23 for pad1, etc. 
    //types are 0b00 = No Calibration, 0b01 Manual Calibrated, 0b10 Auto Calibrated, 0b11 Reserved
    uint16_t sensor_threshold[4];  // Per-pad threshold
    uint16_t sensor_hysteresis[4]; // Hysteresis
    uint16_t sensor_cooldown[4];   // Coldown in ms
    uint8_t keypress_key[4];       // Per-pad key to press
} ProfileConfig;

void profile_config_init(void);
bool get_config_mode(void);
bool check_LED_packet_for_config(uint8_t * packet);

HAL_StatusTypeDef profile_config_save(void);
void profile_config_get(ProfileConfig *data);
void profile_config_read(void);


#endif /* PROFILE_CONFIG_H */
