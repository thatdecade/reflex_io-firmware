#ifndef PROFILE_CONFIG_H
#define PROFILE_CONFIG_H

#include "eeprom_emul.h"
#include <stdint.h>

/**
  * @brief  Saves the profile configuration.
  *         The caller provides a pointer to 63 bytes (e.g. usb_buffer + 1).
  *         A checksum is computed and appended as the 64th byte.
  *
  * @param  data: Pointer to 63 bytes of configuration data.
  * @retval HAL status.
  */
HAL_StatusTypeDef profile_config_save(const uint8_t *data);

/**
  * @brief  Reads the profile configuration.
  *         The stored 64-byte slot is read; the checksum is recalculated and verified.
  *         If the checksum is valid, the first 63 bytes are returned.
  *         Otherwise, a default 63-byte packet is copied.
  *
  * @param  data: Pointer to a 63-byte buffer where the configuration will be stored.
  */
void profile_config_read(uint8_t *data);

#endif /* PROFILE_CONFIG_H */
