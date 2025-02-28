#ifndef EEPROM_EMUML_H_
#define EEPROM_EMUML_H_

#include "stm32f3xx_hal.h"  // Ensure the HAL header is included

/* Flash memory mapping for configuration data:
 * Reserve one flash page.
 * For a 512KB device, use 0x0807F800.
 * For a 256KB device, use 0x0803F800.
 */
#define CONFIG_START_ADDR   0x0803F800      // Start address of reserved flash page
#define CONFIG_PAGE_SIZE    0x800U          // 2 KB per page
#define CONFIG_END_ADDR     (CONFIG_START_ADDR + CONFIG_PAGE_SIZE - 1)

/* API Functions */

HAL_StatusTypeDef epemul_erase_config_page(void);
HAL_StatusTypeDef epemul_write_doubleword(uint32_t address, uint64_t data);
HAL_StatusTypeDef epemul_write_config_data(uint8_t *data, uint32_t index, uint32_t size);
void epemul_read_config_data(uint8_t *data, uint32_t index, uint32_t size);
uint32_t epemul_search_valid_addr(void);

#endif /* EEPROM_EMUML_H_ */
