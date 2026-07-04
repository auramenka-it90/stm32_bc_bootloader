/* --- START OF FILE fram_locker.c --- */

/**
 * @file    fram_locker.c
 * @brief   FramLocker implementation - protection via boot counter in FRAM.
 */

#include "fram_locker.h"
#include "board_support_package.h"
#include <string.h>

/* ------------------------------------------------------------------------- */
/*                    GLOBAL INSTANCE DEFINITION                             */
/* ------------------------------------------------------------------------- */

FramLocker_Handle_t fram_locker;

/* ------------------------------------------------------------------------- */
/*                    PRIVATE FUNCTIONS                                      */
/* ------------------------------------------------------------------------- */

/** 
 * Calculate CRC32 over the data structure (excluding the crc field itself)
 */
static uint32_t calc_crc(const FramLocker_Data_t *data)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    size_t len = sizeof(FramLocker_Data_t) - sizeof(uint32_t);

    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint32_t)bytes[i] << 24);
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x80000000) ? (crc << 1) ^ FRAM_LOCKER_CRC_POLY : (crc << 1);
        }
    }
    return crc;
}

/** 
 * Read 16 bytes from FRAM into RAM buffer, verify checksum integrity
 */
static osStatus_t read_fram(void)
{
    t_float_uint buf[4];  // 4 unions * 32-bit width = 8 words total

    for (int i = 0; i < 4; i++) {
        if (fm22_read_union(FRAM_LOCKER_ADDR + i * 2, &buf[i]) != FSMC_OK)
            return osError;
    }
    memcpy(&fram_locker.data, buf, sizeof(FramLocker_Data_t));
    return (calc_crc(&fram_locker.data) == fram_locker.data.crc) ? osOK : osError;
}

/** 
 * Save RAM buffer state (16 bytes) into FRAM with active CRC checksum update
 */
static osStatus_t write_fram(void)
{
    t_float_uint buf[4];

    fram_locker.data.crc = calc_crc(&fram_locker.data);
    memcpy(buf, &fram_locker.data, sizeof(FramLocker_Data_t));

    for (int i = 0; i < 4; i++) {
        if (fm22_write_union(FRAM_LOCKER_ADDR + i * 2, buf[i]) != FSMC_OK)
            return osError;
    }
    return osOK;
}

/* ------------------------------------------------------------------------- */
/*                    PUBLIC API IMPLEMENTATION                              */
/* ------------------------------------------------------------------------- */

/** 
 * Generates a pseudo-random unique password utilizing hardware TRNG.
 * Falls back to software LCG/Xorshift mixing if 48MHz clock is unconfigured.
 */
uint32_t FramLocker_GeneratePassword(void)
{
    uint32_t random_val = 0;
    uint32_t timeout = 20000;

    // 1. Enable RNG peripheral clock in RCC (AHB2 bus)
    __HAL_RCC_RNG_CLK_ENABLE();

    // 2. Enable True Random Number Generator (TRNG) peripheral
    RNG->CR |= RNG_CR_RNGEN;

    // 3. Wait for Data Ready (DRDY) flag with a safe guard timeout
    while (((RNG->SR & RNG_SR_DRDY) == 0) && (timeout > 0)) {
        timeout--;
    }

    // 4. Retrieve hardware value if successful, or execute fallback algorithm
    if (timeout > 0) {
        random_val = RNG->DR;
    } else {
        // Fallback: Software-based pseudo-random mixing if PLL48CLK is not ready
        uint32_t seed = fram_locker.data.counter * 1664525UL;
        seed ^= HAL_GetTick();
        seed ^= (uint32_t)&fram_locker;
        seed ^= FRAM_LOCKER_ADDR;

        #if defined(__GNUC__)
        register uint32_t sp asm ("sp");
        seed ^= sp;
        #endif

        seed ^= seed << 13;  
        seed ^= seed >> 17;  
        seed ^= seed << 5;
        seed ^= HAL_GetTick() * 1103515245UL;
        seed  = seed * 0x85EBCA6BUL;
        seed ^= seed >> 16;
        seed ^= HAL_GetTick();

        random_val = seed;
    }

    // 5. Disable RNG peripheral to conserve active power
    RNG->CR &= ~RNG_CR_RNGEN;
    __HAL_RCC_RNG_CLK_DISABLE();

    // 6. Scale and constrain password within configured physical boundaries
    uint32_t range = FRAM_LOCKER_MAX_PASS - FRAM_LOCKER_MIN_PASS;
    uint32_t pass  = FRAM_LOCKER_MIN_PASS + (random_val % (range + 1));

    if (pass < FRAM_LOCKER_MIN_PASS) pass = FRAM_LOCKER_MIN_PASS;
    if (pass > FRAM_LOCKER_MAX_PASS) pass = FRAM_LOCKER_MAX_PASS;

    return pass;
}

/** 
 * Initialization: read FRAM, check magic key, execute auto-increment and generate passkey
 */
osStatus_t FramLocker_Init(uint32_t threshold, FramLocker_Callback_t callback)
{
    memset(&fram_locker, 0, sizeof(FramLocker_Handle_t));
    fram_locker.callback = callback;

    // Try reading saved state from FRAM
    if (read_fram() == osOK && fram_locker.data.magic == FRAM_LOCKER_MAGIC) {
        fram_locker.initialized = true;
    } else {
        // First boot initialization routine
        fram_locker.data.counter   = 0;
        fram_locker.data.threshold = threshold;
        fram_locker.data.magic     = FRAM_LOCKER_MAGIC;
        fram_locker.initialized    = true;
        if (write_fram() != osOK) { 
            fram_locker.initialized = false; 
            return osError; 
        }
    }

    // Process auto-increment if limit is not yet hit
    if (!FramLocker_IsThresholdReached()) {
        fram_locker.data.counter++;
        osStatus_t s = write_fram();
        if (s == osOK && FramLocker_IsThresholdReached() && callback) {
            callback();
        }
    }

    // Generate active session password
    fram_locker.runtime_password = FramLocker_GeneratePassword();
    return osOK;
}

/** 
 * Deinitialization: commit active memory state and clear RAM handle
 */
osStatus_t FramLocker_DeInit(void)
{
    if (!fram_locker.initialized) return osErrorParameter;
    osStatus_t s = write_fram();
    memset(&fram_locker, 0, sizeof(FramLocker_Handle_t));
    return s;
}

/** 
 * Manually increments the active session counter
 */
osStatus_t FramLocker_Increment(void)
{
    if (!fram_locker.initialized || FramLocker_IsThresholdReached()) return osOK;
    fram_locker.data.counter++;
    osStatus_t s = write_fram();
    if (s == osOK && FramLocker_IsThresholdReached() && fram_locker.callback) {
        fram_locker.callback();
    }
    return s;
}

/** 
 * Commits RAM handle state back into FRAM memory
 */
osStatus_t FramLocker_Save(void)
{
    return fram_locker.initialized ? write_fram() : osErrorParameter;
}

/** 
 * Commits RAM handle state with active session password validation
 */
osStatus_t FramLocker_SaveWithPassword(uint32_t password)
{
    if (!fram_locker.initialized) return osErrorParameter;
    if (fram_locker.runtime_password != password) return osErrorParameter;
    return write_fram();
}

/** 
 * Change password context (RAM only)
 */
osStatus_t FramLocker_SetPassword(uint32_t new_password, uint32_t current)
{
    if (!fram_locker.initialized)                    return osErrorParameter;
    if (fram_locker.runtime_password != current)     return osErrorParameter;
    if (new_password < FRAM_LOCKER_MIN_PASS || new_password > FRAM_LOCKER_MAX_PASS)
        return osErrorParameter;

    fram_locker.runtime_password = new_password;
    return osOK;
}

/** 
 * Checks if the counter reached or exceeded the threshold
 */
bool FramLocker_IsThresholdReached(void)
{
    return fram_locker.initialized && fram_locker.data.counter >= fram_locker.data.threshold;
}

/** 
 * Retrieve the active session password
 */
uint32_t FramLocker_GetPassword(void)
{
    return fram_locker.runtime_password;
}

/* --- END OF FILE fram_locker.c --- */