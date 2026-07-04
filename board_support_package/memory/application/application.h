#ifndef MEMORY_APPLICATION_H_
#define MEMORY_APPLICATION_H_

#include "board_support_package.h"

/* application marker */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t fw_size;
    uint32_t crc32;
} fw_header_t;


// Адрес 5-го сектора Flash-памяти (Начало пользовательского приложения)
#define APPLICATION_ADDRESS         0x08020000

// 128 kB (Размер одного сектора: с 5 по 11 на STM32F4)
#define FLASH_CPU_SECTOR_SIZE       (128 * 1024)
#define FLASH_CPU_START_ADDRESS     APPLICATION_ADDRESS 
#define FLASH_CPU_ADR               FLASH_CPU_START_ADDRESS

// --- НАСТРОЙКА РАЗМЕРА ПРОГРАММЫ ---
// Выделяем 2 сектора под боевую программу (Сектор 5 и Сектор 6) = 256 КБ
#define FLASH_CPU_SECTORS_COUNT     2

// Размер страницы обмена DSPA (1024 байта)
#define FLASH_CPU_PAGE_SIZE         1024

// Вычисляемый общий размер выделенной памяти (262144 байт)
#define FLASH_CPU_SIZE              (FLASH_CPU_SECTOR_SIZE * FLASH_CPU_SECTORS_COUNT) 

// Вычисляемый конечный адрес: 0x08020000 + 0x40000 - 1 = 0x0805FFFF
#define FLASH_CPU_ADR_HIGH          (FLASH_CPU_START_ADDRESS + FLASH_CPU_SIZE - 1)

#define CLEAR_FLAG                  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR)

// Стартовый и конечный сектора для драйвера очистки (Erase)
#define APPL_SECTOR_START           FLASH_SECTOR_5
#define APPL_SECTOR_END             FLASH_SECTOR_6

// Прототипы функций драйвера
void eeprom_memory_read_buffer(uint8_t *dst, const uint8_t *src, uint32_t len);

// Функция стирания теперь не принимает номер сектора, а стирает всю выделенную область приложения
bool eeprom_memory_erase_app(void);
bool eeprom_memory_write_words(const uint32_t *p_data, uint32_t addr, uint32_t words_count);

/**
 * @brief Проверяет наличие прошивки и валидность её CRC32.
 * @return true, если прошивка валидна и можно делать jump.
 * @return false, если прошивки нет, или CRC не совпадает.
 */
bool check_application(void);

#endif /* MEMORY_APPLICATION_H_ */
