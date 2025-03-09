#include "profile_config.h"
#include <string.h>

// Magic packets are precompiled from this python code:
/* 
import hashlib
def generate_config_magic_packet(string: str) -> bytes:
    return hashlib.sha512(string.encode('ascii')).digest()
enter_config_magic = generate_config_magic_packet("REFLEXENTERCONFIG")
exit_config_magic  = generate_config_magic_packet("REFLEXEXITCONFIG" )
*/
static const uint8_t enter_config_magic[64] = { 0xb6, 0xda, 0x3d, 0xc8, 0x90, 0x4a, 0xae, 0x15, 0x87, 0xf7, 0xee, 0x99, 0x13, 0xc8, 0xbc, 0x5f, 0x4e, 0x61, 0x6d, 0x7b, 0x75, 0x05, 0xc4, 0xb3, 0x62, 0x20, 0xc9, 0xa7, 0x84, 0x18, 0x66, 0xd1, 0x87, 0x27, 0x82, 0xb8, 0x7c, 0xaa, 0xe1, 0xbf, 0x41, 0xc0, 0x01, 0xc4, 0x57, 0xd4, 0xe1, 0xe3, 0xd5, 0x4b, 0x5d, 0xb6, 0xa6, 0xc1, 0x67, 0x68, 0xa6, 0x15, 0x73, 0x5f, 0x43, 0xc9, 0x5a, 0xb3 };
static const uint8_t exit_config_magic [64] = { 0x7f, 0x54, 0x55, 0xb2, 0x0d, 0x20, 0x11, 0x05, 0xe6, 0x4b, 0x98, 0x52, 0xcf, 0x49, 0x11, 0x47, 0x5c, 0xef, 0xae, 0x3d, 0x39, 0xbd, 0xe6, 0xba, 0xa1, 0x2d, 0x69, 0xb1, 0x4d, 0xf3, 0xc6, 0x1d, 0x71, 0xff, 0xbc, 0x33, 0x09, 0x1f, 0xd4, 0x10, 0x34, 0xe5, 0x45, 0xb0, 0xfa, 0xe1, 0x89, 0xda, 0xfc, 0x3a, 0x32, 0xdf, 0xe9, 0x7a, 0x8d, 0xd6, 0xb7, 0x23, 0x8b, 0x33, 0xbd, 0xd6, 0x5e, 0xa6 };

volatile bool config_mode = false;

bool get_config_mode(void)
{
    return config_mode;
}

bool check_LED_packet_for_config(uint8_t * packet) {

    if (!config_mode) {
        if (memcmp(packet, enter_config_magic, 64) == 0) {
            // Enter configuration mode.
            config_mode = true;
            return true;
        }
    } else {
        if (memcmp(packet, exit_config_magic, 64) == 0) {
            // Exit configuration mode.
            config_mode = false;
            return true;
        }
    }
    return false;
}

ProfileConfig g_profile_config = {
    .manual_flag = 0, // 0 = auto calibration; set bits for pads in manual mode.
    .sensor_threshold  = { DEFAULT_SENSOR_THRESHOLD,  DEFAULT_SENSOR_THRESHOLD,  DEFAULT_SENSOR_THRESHOLD,  DEFAULT_SENSOR_THRESHOLD },
    .sensor_hysteresis = { DEFAULT_SENSOR_HYSTERESIS, DEFAULT_SENSOR_HYSTERESIS, DEFAULT_SENSOR_HYSTERESIS, DEFAULT_SENSOR_HYSTERESIS },
    .sensor_cooldown   = { DEFAULT_SENSOR_COOLDOWN,   DEFAULT_SENSOR_COOLDOWN,   DEFAULT_SENSOR_COOLDOWN,   DEFAULT_SENSOR_COOLDOWN },
};

void profile_config_init() {
    profile_config_read();
}

void profile_config_get(ProfileConfig *data) {
    *data = g_profile_config;
}

HAL_StatusTypeDef profile_config_save() {
    uint8_t data[64];
    // Pack configuration data into 63 bytes (e.g., 1 byte manual_flag + 3*4*2 bytes = 25 bytes; pad rest with zeros).
    data[0] = g_profile_config.manual_flag;
    memcpy(&data[1], g_profile_config.sensor_threshold,                         4 * sizeof(uint16_t));
    memcpy(&data[1 + 4 * sizeof(uint16_t)], g_profile_config.sensor_hysteresis, 4 * sizeof(uint16_t));
    memcpy(&data[1 + 8 * sizeof(uint16_t)], g_profile_config.sensor_cooldown,   4 * sizeof(uint16_t));
    // Zero remaining bytes.
    memset(&data[1 + 12 * sizeof(uint16_t)], 0, 63 - (1 + 12 * sizeof(uint16_t)));
    
    uint8_t checksum = 0;
    for (int i = 0; i < 63; i++) {
        checksum += data[i];
    }
    data[63] = checksum;
    return epemul_write_config_data(data, 0, 64);
}

void profile_config_read() {
    uint8_t data[64];
    epemul_read_config_data(data, 0, 64);
    uint8_t checksum = 0;
    for (int i = 0; i < 63; i++) {
        checksum += data[i];
    }
    if (checksum != data[63]) {
        // On checksum error, load default values.
        g_profile_config = (ProfileConfig){
            .manual_flag = 0,
            .sensor_threshold  = { DEFAULT_SENSOR_THRESHOLD,  DEFAULT_SENSOR_THRESHOLD,  DEFAULT_SENSOR_THRESHOLD,  DEFAULT_SENSOR_THRESHOLD },
            .sensor_hysteresis = { DEFAULT_SENSOR_HYSTERESIS, DEFAULT_SENSOR_HYSTERESIS, DEFAULT_SENSOR_HYSTERESIS, DEFAULT_SENSOR_HYSTERESIS },
            .sensor_cooldown   = { DEFAULT_SENSOR_COOLDOWN,   DEFAULT_SENSOR_COOLDOWN,   DEFAULT_SENSOR_COOLDOWN,   DEFAULT_SENSOR_COOLDOWN }
        };
    } else {
        g_profile_config.manual_flag = data[0];
        memcpy(g_profile_config.sensor_threshold, &data[1], 4 * sizeof(uint16_t));
        memcpy(g_profile_config.sensor_hysteresis, &data[1 + 4 * sizeof(uint16_t)], 4 * sizeof(uint16_t));
        memcpy(g_profile_config.sensor_cooldown, &data[1 + 8 * sizeof(uint16_t)], 4 * sizeof(uint16_t));
    }
}
