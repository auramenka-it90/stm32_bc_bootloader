#include "version_control.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h> // Необходим для snprintf

// Product information strings
const char _VC_PRODUCT_NUMBER_[] =      "Ballistic computer board\n";
const char _VC_BOARD_NUMBER_[] =        "Number: 7111.00.00.000\n"; // <--- Заменили Board на Number
const char _VC_CHIP_DESIGNATION_[] =    "Chip: DD11\n";
const char _VC_SOFT_NAME_[] =           "STM32F415ZGT6(bootloader)\n";
const char _VC_SOFT_NOTE_[] =           "SW:  produced by mr. Andrew Auramenka\n";

// Build timestamp (compilation date and time)
static const char* build = __DATE__ "  " __TIME__ "\n";

// Pointer to device info string
static char* device_info = NULL;

/**
 * Convert uint32_t to hex string in format "0xABCDEF12"
 *
 * @param value  32-bit value to convert
 * @param buffer Output buffer (must be at least 11 bytes)
 */
static void uint32_to_hex_str(uint32_t value, char* buffer) {
    if (buffer == NULL) return;

    const char hex_digits[] = "0123456789ABCDEF";

    // Start with "0x" prefix
    buffer[0] = '0';
    buffer[1] = 'x';

    // Fill 8 hex digits (most significant first)
    for (int i = 0; i < 8; i++) {
        // Get hex digit (from most significant to least)
        int digit = (value >> (28 - i * 4)) & 0xF;
        buffer[i + 2] = hex_digits[digit];
    }

    // Null-terminate the string
    buffer[10] = '\0';
}

/**
 * Create device information string
 * @return Pointer to the created string, or NULL if allocation failed
 */
char* device_info_create(uint32_t password) {
    // Free previous string if exists
    device_info_destroy();
    
    // Allocate memory for device info string
    device_info = malloc(_VC_SIZE_DEVICE_INFO_STR_);
    
    // Append password in hex format (e.g., "0xABCDEF12")
	char pass_hex[11]; // "0x" + 8 digits + '\0' = 11 characters
	uint32_to_hex_str(password, pass_hex);

	if (device_info != NULL) {
	    // Формируем итоговую строку с добавлением версии IDE и пароля
	    snprintf(device_info, _VC_SIZE_DEVICE_INFO_STR_,
	             "%s%s%s%sIDE: %s\nBuild: %s%sVer. %d.%d\n%s\n",
	             _VC_PRODUCT_NUMBER_,
	             _VC_BOARD_NUMBER_,
	             _VC_CHIP_DESIGNATION_,
	             _VC_SOFT_NAME_,
	             _IDE_VERSION_,
	             build,
	             _VC_SOFT_NOTE_,
	             (int)_SOFT_VERSION_,      // Приведение к int для безопасности
	             (int)_SOFT_SUBVERSION_,   // Приведение к int для безопасности
	             pass_hex);
	}
    
    return device_info;
}

/**
 * Destroy device info string and free allocated memory
 */
void device_info_destroy(void) {
    if (device_info != NULL) {
        free(device_info);
        device_info = NULL;
    }
}
