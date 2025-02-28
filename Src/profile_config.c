#include "profile_config.h"
#include <string.h>   // For memcpy

/* Default profile configuration (63 bytes) for use on checksum failure. */
#define PROFILE_CONFIG_DEFAULT { \
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
    0, 0, 0 \
}

HAL_StatusTypeDef profile_config_save(const uint8_t *data) {
    uint8_t config[64];
    uint8_t checksum = 0;

    // Copy the 63 data bytes into the temporary buffer.
    memcpy(config, data, 63);

    // Compute the checksum as an 8-bit sum over the 63 bytes.
    for (int i = 0; i < 63; i++) {
        checksum += config[i];
    }
    // Store the checksum as the 64th byte.
    config[63] = checksum;

    // Write the complete 64-byte block to slot 0
    return epemul_write_config_data(config, 0, 64);
}

void profile_config_read(uint8_t *data) {
    uint8_t config[64];
    uint8_t checksum = 0;

    // Read the 64-byte profile from slot 0.
    epemul_read_config_data(config, 0, 64);

    // Compute the checksum over the first 63 bytes.
    for (int i = 0; i < 63; i++) {
        checksum += config[i];
    }
    
    // If the checksum does not match, use the default profile.
    if (checksum != config[63]) {
        static const uint8_t default_profile[63] = PROFILE_CONFIG_DEFAULT;
        memcpy(data, default_profile, 63);
    } else {
        memcpy(data, config, 63);
    }
}
