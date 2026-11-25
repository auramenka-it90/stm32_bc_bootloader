#include "application.h"
#include <string.h>

/*
 * Read from internal Flash memory.
 * Executes at the maximum speed of the system bus.
 */
void eeprom_memory_read_buffer(uint8_t *dst, const uint8_t *src, uint32_t len) {
    memcpy(dst, src, len);
}

/*
 * Erase the whole application Flash region (application sectors).
 * Uses APPL_SECTOR_START and FLASH_CPU_SECTORS_COUNT from application.h.
 * Warning: the erase process can take up to 2-4 seconds.
 * During erase, code execution from internal Flash is hardware-suspended.
 */
bool eeprom_memory_erase_app(void) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;
    bool result = true;

    /* Configure erase parameters for the range of sectors */
    EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3; /* Requires 2.7V - 3.6V for fast erase */

    /* Pass the starting sector and the sector count (sectors 5, 6 and 7) */
    EraseInitStruct.Sector        = APPL_SECTOR_START;
    EraseInitStruct.NbSectors     = FLASH_CPU_SECTORS_COUNT;

    /* Unlock the Flash controller */
    if (HAL_FLASH_Unlock() != HAL_OK) return false;

    /* Clear possible previous error flags */
    CLEAR_FLAG;

    /* Execute the erase of all application sectors at once */
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        result = false;
    }

    /* Lock the Flash controller back */
    HAL_FLASH_Lock();
    
    return result;
}

/*
 * Fast 32-bit word programming into internal Flash memory.
 * Uses FLASH_TYPEPROGRAM_WORD, which is 4 times faster than byte programming.
 * IMPORTANT:
 * 1. The address `addr` must be 4-byte aligned.
 * 2. The memory area must be erased (contain 0xFFFFFFFF) before writing.
 */
bool eeprom_memory_write_words(const uint32_t *p_data, uint32_t addr, uint32_t words_count) {
    bool result = true;

    /* Guard: address must be 4-byte aligned */
    if (addr % 4 != 0) return false;

    if (HAL_FLASH_Unlock() != HAL_OK) return false;

    CLEAR_FLAG;

    /* Program data word by word (4 bytes per write) */
    for (uint32_t i = 0; i < words_count; ++i) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, p_data[i]) == HAL_OK) {
            addr += 4; /* Advance the write pointer by 4 bytes */
        } else {
            result = false;
            break; /* Halt programming on the first failure */
        }
    }

    HAL_FLASH_Lock();
    
    return result;
}

/*
 * Validates the application binary using hardware CRC32.
 */
bool check_application(void) {
    fw_header_t *fw_info = NULL;
    uint32_t *search_ptr = (uint32_t *)APPLICATION_ADDRESS;

    /* Invalidate flash caches: freshly programmed data must not be shadowed
     * by cache lines fetched before the erase/program operation */
    __HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
    __HAL_FLASH_DATA_CACHE_DISABLE();
    __HAL_FLASH_INSTRUCTION_CACHE_RESET();
    __HAL_FLASH_DATA_CACHE_RESET();
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    __HAL_FLASH_DATA_CACHE_ENABLE();

    /* 1. Search for the magic word (scan the first 1024 bytes / 256 words) */
    for (int i = 0; i < 256; i++) {
        if (search_ptr[i] == 0x55AA55AAU) {
            fw_info = (fw_header_t *)&search_ptr[i];
            break;
        }
    }

    /* 2. Basic safety checks for missing or corrupted firmware */
    if (fw_info == NULL || fw_info->fw_size == 0xFFFFFFFFU || fw_info->fw_size == 0) {
        return false;
    }

    /* The declared image must fit within the allocated application region */
    if (fw_info->fw_size > FLASH_CPU_SIZE) {
        return false;
    }

    /* 3. Prepare data offsets for CRC32 calculation */
    uint32_t *payload_ptr = (uint32_t *)(fw_info + 1);
    uint32_t header_offset_bytes = (uint32_t)payload_ptr - APPLICATION_ADDRESS;

    /* Safety guard: if size is corrupt (smaller than the header itself), prevent HardFault */
    if (fw_info->fw_size <= header_offset_bytes) {
        return false;
    }

    uint32_t payload_bytes = fw_info->fw_size - header_offset_bytes;
    uint32_t payload_words = payload_bytes / 4; /* Convert byte size into 32-bit words */

    /* 4. Calculate hardware-accelerated CRC32 */
    uint32_t calc_crc = HAL_CRC_Calculate(&hcrc, payload_ptr, payload_words);

    /* 5. Return comparison result */
    return (calc_crc == fw_info->crc32);
}
