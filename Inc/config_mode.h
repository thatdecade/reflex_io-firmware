#ifndef CONFIG_MODE_H
#define CONFIG_MODE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Global functions for configuration mode control.

typedef enum {
    CONFIG_MODE_NORMAL = 0,
    CONFIG_MODE_ENTER  = 1,
    CONFIG_MODE_EXIT   = 2,
} config_modes_t;

typedef enum {
    PROFILE_PUSH_PACKET = 0xF0,
    PROFILE_READ_PACKET = 0xF1,
} config_packets_t;

bool is_config_mode(void);
void set_config_mode(config_modes_t mode);

config_modes_t packet_filter_for_config_mode(const uint8_t *packet);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MODE_H
