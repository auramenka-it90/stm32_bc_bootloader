/**
 ******************************************************************************
 * @file    version_control.c
 * @brief   Software and Hardware Version Information Implementation (Bootloader).
 *          MSB-style: prefix is a compile-time Flash (.rodata) string,
 *          session password is appended into a static RAM buffer.
 *          No malloc, no snprintf. Output text is byte-identical to old code.
 *          All comments in ASCII English.
 ******************************************************************************
 */

#include "version_control.h"

/* Compile-time concatenated device info prefix, stored in Flash (.rodata) */
static const char device_info_string[] = _VC_INFO_PREFIX_;

/* Static output buffer, lives for the whole program lifetime */
static char device_info[_VC_SIZE_DEVICE_INFO_STR_];

/**
 * Create device information string
 * @param password Session password appended in "0xABCDEF12" hex format
 * @return Pointer to the static device info string
 */
char* device_info_create(uint32_t password) {
    static const char hex_digits[] = "0123456789ABCDEF";
    const char *src = device_info_string;
    char *p = device_info;
    char *const end = &device_info[_VC_SIZE_DEVICE_INFO_STR_ - 12];

    /* Clear buffer before assembling the string */
    device_info_destroy();

    /* Copy the Flash prefix into the static buffer (bounded) */
    while ((*src != '\0') && (p < end))
    {
        *p++ = *src++;
    }

    /* Append session password in "0xABCDEF12" hex format */
    *p++ = '0';
    *p++ = 'x';
    for (int i = 7; i >= 0; i--)
    {
        *p++ = hex_digits[(password >> (i * 4)) & 0xF];
    }
    *p++ = '\n';
    *p = '\0';

    return device_info;
}

/**
 * Destroy device info string (clears the static buffer)
 */
void device_info_destroy(void) {
    device_info[0] = '\0';
}
