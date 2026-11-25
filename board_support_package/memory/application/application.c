#include "application.h"
#include <string.h>

/*
 * Чтение из внутренней Flash-памяти
 * Выполняется с максимальной скоростью системной шины
 */
void eeprom_memory_read_buffer(uint8_t *dst, const uint8_t *src, uint32_t len) {
    memcpy(dst, src, len);
}

/*
 * Стирание всей пользовательской области Flash-памяти (Сектора приложения).
 * Берет настройки APPL_SECTOR_START и FLASH_CPU_SECTORS_COUNT из application.h.
 * Внимание: Процесс стирания может занимать до 2-4 секунд.
 * Во время стирания выполнение кода из Flash аппаратно приостанавливается.
 */
bool eeprom_memory_erase_app(void) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;
    bool result = true;

    // Настройка параметров стирания на диапазон секторов
    EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3; // Требует питания 2.7-3.6V для быстрой работы

    // Передаем стартовый сектор и общее количество (в нашем случае Сектор 5 и 2 штуки)
    EraseInitStruct.Sector        = APPL_SECTOR_START;
    EraseInitStruct.NbSectors     = FLASH_CPU_SECTORS_COUNT;

    // Снимаем защиту с контроллера Flash
    if (HAL_FLASH_Unlock() != HAL_OK) return false;

    // Очищаем возможные предыдущие ошибки
    CLEAR_FLAG;

    // Запускаем процесс стирания сразу всех секторов приложения
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        result = false;
    }

    // Возвращаем защиту
    HAL_FLASH_Lock();
    
    return result;
}

/*
 * Быстрая запись 32-битных слов во внутреннюю Flash-память.
 * Используется FLASH_TYPEPROGRAM_WORD, что в 4 раза быстрее побайтовой записи.
 * * ВАЖНО: 
 * 1. Адрес `addr` должен быть кратен 4 (выровнен).
 * 2. Область памяти перед записью должна быть стерта (содержать 0xFFFFFFFF).
 */
bool eeprom_memory_write_words(const uint32_t *p_data, uint32_t addr, uint32_t words_count) {
    bool result = true;

    // Проверка выравнивания адреса 
    if (addr % 4 != 0) return false;

    if (HAL_FLASH_Unlock() != HAL_OK) return false;

    CLEAR_FLAG;

    // Запись данных циклом по 4 байта
    for (uint32_t i = 0; i < words_count; ++i) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, p_data[i]) == HAL_OK) {
            addr += 4; // Сдвигаем адрес на 4 байта вперед
        } else {
            result = false;
            break; // Прерываем запись при первой же ошибке
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
