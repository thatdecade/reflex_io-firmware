#ifndef PROFILE_CONFIG_H
#define PROFILE_CONFIG_H

#include "eeprom_emul.h"
#include <stdint.h>
#include <stdbool.h>

#define DEFAULT_SENSOR_THRESHOLD   500  // the amount above idle needed to consider the pad pressed
#define DEFAULT_SENSOR_HYSTERESIS   50  // to avoid rapid toggling, once active the pad will remain active until sensor reading falls below (THRESHOLD - HYSTERESIS)
#define DEFAULT_SENSOR_COOLDOWN     60  // minimum time (ms) between state transitions

typedef struct {
    uint8_t manual_flag;                 // Bitmask: bit0 for pad0, bit1 for pad1, etc.
    uint16_t sensor_threshold[4];        // Per-pad threshold
    uint16_t sensor_hysteresis[4];       // Per-pad hysteresis
    uint16_t sensor_cooldown[4];         // Per-pad cooldown in ms
} ProfileConfig;

void profile_config_init(void);
bool get_config_mode(void);
bool check_LED_packet_for_config(uint8_t * packet);

HAL_StatusTypeDef profile_config_save(void);
void profile_config_get(ProfileConfig *data);
void profile_config_read(void);


#endif /* PROFILE_CONFIG_H */
