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


/* Application start: flash sector 5 */
#define APPLICATION_ADDRESS         0x08020000

#define FLASH_CPU_START_ADDRESS     APPLICATION_ADDRESS 
#define FLASH_CPU_ADR               FLASH_CPU_START_ADDRESS

/* --- APPLICATION REGION SIZE --- */
/* Individual flash sector sizes (STM32F415: S0..S3 16K, S4 64K, S5 128K, S6..S11 64K).
   Sectors 5, 6 and 7 form the application region */
#define FLASH_CPU_SECTOR5_SIZE      (128 * 1024)
#define FLASH_CPU_SECTOR6_SIZE      (64 * 1024)
#define FLASH_CPU_SECTOR7_SIZE      (64 * 1024)

#define FLASH_CPU_SECTORS_COUNT     3

/* DSPA exchange page size (1024 bytes) */
#define FLASH_CPU_PAGE_SIZE         1024

/* Total application region, summed from the real (unequal) sector sizes */
#define FLASH_CPU_SIZE              (FLASH_CPU_SECTOR5_SIZE + FLASH_CPU_SECTOR6_SIZE + FLASH_CPU_SECTOR7_SIZE)

/* End address: 0x08020000 + 0x40000 - 1 = 0x0805FFFF */
#define FLASH_CPU_ADR_HIGH          (FLASH_CPU_START_ADDRESS + FLASH_CPU_SIZE - 1)

#define CLEAR_FLAG                  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR)

/* Start and end sectors for the erase driver */
#define APPL_SECTOR_START           FLASH_SECTOR_5
#define APPL_SECTOR_END             FLASH_SECTOR_7

/* Driver function prototypes */
void eeprom_memory_read_buffer(uint8_t *dst, const uint8_t *src, uint32_t len);

/* Erase function takes no sector number: it erases the whole application region */
bool eeprom_memory_erase_app(void);
bool eeprom_memory_write_words(const uint32_t *p_data, uint32_t addr, uint32_t words_count);

/**
 * @brief Checks that the firmware image is present and its CRC32 is valid.
 * @return true if the firmware is valid and jump can be performed.
 * @return false if there is no firmware or the CRC does not match.
 */
bool check_application(void);

#endif /* MEMORY_APPLICATION_H_ */
