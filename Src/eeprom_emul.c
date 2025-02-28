/* eeprom_emul.c - EEPROM emulation for configuration data on STM32F303 (single flash bank)
 *
 * This file is designed for STM32F303 devices (e.g., STM32F303CCT6) which have only one flash bank.
 * It reserves one 2-KB flash page for storing configuration data.
 *
 * NOTE:
 *  - The reserved flash area (CONFIG_START_ADDR to CONFIG_END_ADDR) must be excluded from your application code.
 *  - Flash erase operations work on an entire page. Use sparingly!!
 */

#include "eeprom_emul.h"
#include <string.h>  // for memcpy and memcmp

/* 
 * Set up the erase structure for one page in FLASH_BANK_1.
 * The page number is calculated relative to the start of flash (0x08000000).
 */
FLASH_EraseInitTypeDef erase = {
    .TypeErase = FLASH_TYPEERASE_PAGES,
    .Banks = FLASH_BANK_1,
    .Page = (CONFIG_START_ADDR - 0x08000000) / CONFIG_PAGE_SIZE,
    .NbPages = 1
};

uint32_t fault = 0;

/* 
 * Erase the configuration flash page.
 * Returns HAL_OK if successful, or an error status otherwise.
 */
HAL_StatusTypeDef epemul_erase_config_page(void)
{
    HAL_FLASH_Unlock();
    uint32_t PageError = 0;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &PageError);
    HAL_FLASH_Lock();
    return status;
}

/* 
 * Write a double word (8 bytes) to flash at the specified address.
 * Returns HAL_OK if the programming operation is successful.
 */
HAL_StatusTypeDef epemul_write_doubleword(uint32_t address, uint64_t data)
{
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, data);
    HAL_FLASH_Lock();
    return status;
}

/*
 * Write configuration data to a specified slot (index) within the reserved flash page.
 * 'size' must be a multiple of 8.
 *
 * The function reads the entire page into a temporary buffer and compares the current
 * data in the selected slot with the new data. If they differ, the slot in the buffer
 * is updated, the entire page is erased, and then the whole page is reprogrammed.
 *
 * Returns HAL_OK if all operations are successful.
 */
HAL_StatusTypeDef epemul_write_config_data(uint8_t *data, uint32_t index, uint32_t size)
{
    // Check that size is a multiple of 8
    if (size % 8 != 0) {
        return HAL_ERROR;
    }

    // Calculate the slot's offset within the page
    uint32_t offset = index * size;
    if (offset + size > CONFIG_PAGE_SIZE) {
        return HAL_ERROR; // Slot out of range
    }

    uint8_t page_buffer[CONFIG_PAGE_SIZE];
    // Read the entire current page from flash
    memcpy(page_buffer, (uint8_t*)CONFIG_START_ADDR, CONFIG_PAGE_SIZE);

    // If the slot already contains the new data, no update is necessary.
    if (memcmp(&page_buffer[offset], data, size) == 0) {
        return HAL_OK;
    }

    // Update the slot in the buffer with the new data.
    memcpy(&page_buffer[offset], data, size);

    // Erase the entire configuration page.
    HAL_StatusTypeDef status = epemul_erase_config_page();
    if (status != HAL_OK) {
        return status;
    }

    // Write the whole page buffer back to flash, 8 bytes at a time.
    uint32_t addr = CONFIG_START_ADDR;
    for (uint32_t i = 0; i < CONFIG_PAGE_SIZE; i += 8)
    {
        uint64_t d = ((uint64_t)page_buffer[i])       |
                     ((uint64_t)page_buffer[i+1] << 8)  |
                     ((uint64_t)page_buffer[i+2] << 16) |
                     ((uint64_t)page_buffer[i+3] << 24) |
                     ((uint64_t)page_buffer[i+4] << 32) |
                     ((uint64_t)page_buffer[i+5] << 40) |
                     ((uint64_t)page_buffer[i+6] << 48) |
                     ((uint64_t)page_buffer[i+7] << 56);
        status = epemul_write_doubleword(addr, d);
        if (status != HAL_OK) {
            return status;
        }
        addr += 8;
    }
    return HAL_OK;
}

/*
 * Read configuration data from a specified slot (index) within the reserved flash page.
 * 'size' bytes are read from offset = index * size.
 *
 * Note: This function uses memcpy since the flash is memory-mapped.
 */
void epemul_read_config_data(uint8_t *data, uint32_t index, uint32_t size)
{
    uint32_t offset = index * size;
    if (offset + size > CONFIG_PAGE_SIZE) {
        // Out of range – handle error as needed.
        return;
    }

    memcpy(data, (uint8_t*)(CONFIG_START_ADDR + offset), size);
}

/*
 * Search for the next free (erased) double word (8 bytes) address within the config flash area.
 * Returns the address if free space is found, or 0 if no free space is available.
 */
uint32_t epemul_search_valid_addr(void)
{
    uint32_t addr;
    for (addr = CONFIG_START_ADDR; addr <= CONFIG_END_ADDR - 8; addr += 8)
    {
        // Check both 32-bit words of the double word
        if ((*(__IO uint32_t*)addr == 0xFFFFFFFF) && (*(__IO uint32_t*)(addr + 4) == 0xFFFFFFFF))
        {
            return addr;
        }
    }
    return 0; // No free space found
}
