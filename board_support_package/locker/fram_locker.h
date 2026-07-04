/* --- START OF FILE fram_locker.h --- */

/**
 * @file    fram_locker.h
 * @brief   Unauthorized access protection using non-volatile FRAM (FM22L16).
 * 
 * Boot counter is stored securely in the last memory slots of FRAM.
 * Session password is dynamically generated on each boot and resides strictly in RAM.
 */

#ifndef __FRAM_LOCKER_H_
#define __FRAM_LOCKER_H_

#include <stdint.h>
#include "cmsis_os2.h"
#include "fm22l16.h"

/* ------------------------------------------------------------------------- */
/*                    CONFIGURATION                                          */
/* ------------------------------------------------------------------------- */

/** Data starting address in FRAM (last 8 words of the memory array) */
#define FRAM_LOCKER_ADDR        (FM22_MAX_ADDR - 7)  // 0x3FFF8

/** Structure size in 16-bit words */
#define FRAM_LOCKER_SIZE        8

/** Polynomial for CRC-32 calculation */
#define FRAM_LOCKER_CRC_POLY    0x04C11DB7

/** Magic number for data validation */
#define FRAM_LOCKER_MAGIC       0x4C4F434BUL  // ASCII representation of "LOCK"

/** Session password range parameters */
#define FRAM_LOCKER_MIN_PASS    0x00001000UL
#define FRAM_LOCKER_MAX_PASS    0xFFFFFFF0UL

/* ------------------------------------------------------------------------- */
/*                    DATA TYPES                                             */
/* ------------------------------------------------------------------------- */

/** Callback pointer triggered on threshold breach */
typedef void (*FramLocker_Callback_t)(void);

/** Data layout stored inside FRAM (8 words = 16 bytes) */
#pragma pack(push, 1)
typedef struct {
    uint32_t counter;       // Boot counter
    uint32_t threshold;     // Lockout threshold limit
    uint32_t magic;         // Validation magic key
    uint32_t crc;           // Struct checksum excluding crc field
} FramLocker_Data_t;
#pragma pack(pop)

/** Handle instance structure */
typedef struct {
    FramLocker_Data_t   data;               // Copy of the data in FRAM
    uint32_t            runtime_password;   // Active session password in RAM
    FramLocker_Callback_t callback;         // Threshold breach callback
    bool                initialized;        // Initialization state flag
} FramLocker_Handle_t;

/* ------------------------------------------------------------------------- */
/*                    GLOBAL INSTANCE EXPORT                                 */
/* ------------------------------------------------------------------------- */

extern FramLocker_Handle_t fram_locker;

/* ------------------------------------------------------------------------- */
/*                    QUICK ACCESS ALIASES                                   */
/* ------------------------------------------------------------------------- */

#define FRAM_LOCKER_COUNTER       (fram_locker.data.counter)
#define FRAM_LOCKER_THRESHOLD     (fram_locker.data.threshold)
#define FRAM_LOCKER_PASSWORD      (fram_locker.runtime_password)
#define FRAM_LOCKER_INITIALIZED   (fram_locker.initialized)

/* ========================================================================= */
/* PUBLIC API                                                                */
/* ========================================================================= */

/**
 * @brief Initializes the module, verifies checksum, and auto-increments the boot counter.
 * @param threshold         Threshold limit for system lock.
 * @param callback          Breach handler callback (can be NULL).
 * @return osStatus_t Status of the initialization process.
 */
osStatus_t FramLocker_Init(uint32_t threshold, FramLocker_Callback_t callback);

/**
 * @brief Deinitializes the locker module and commits memory states to FRAM.
 * @return osStatus_t Status of the FRAM write transactions.
 */
osStatus_t FramLocker_DeInit(void);

/**
 * @brief Manually increments the session counter.
 * @return osStatus_t Status of the write transaction.
 */
osStatus_t FramLocker_Increment(void);

/**
 * @brief Forces a write-back of active state into FRAM.
 * @return osStatus_t Status of the write transaction.
 */
osStatus_t FramLocker_Save(void);

/**
 * @brief Saves data back into FRAM with runtime password validation.
 * @param password Passkey to authenticate the transaction.
 * @return osStatus_t Status of the write transaction.
 */
osStatus_t FramLocker_SaveWithPassword(uint32_t password);

/**
 * @brief Updates the active session password (RAM only).
 * @param new_password      New target password.
 * @param current_password  Active session password to verify transaction.
 * @return osStatus_t Status of the transaction.
 */
osStatus_t FramLocker_SetPassword(uint32_t new_password, uint32_t current_password);

/**
 * @brief Check if the current counter reached or exceeded the threshold.
 * @return true if counter >= threshold.
 */
bool FramLocker_IsThresholdReached(void);

/**
 * @brief Get the active session password.
 * @return Active password.
 */
uint32_t FramLocker_GetPassword(void);

/**
 * @brief Generates a pseudo-random unique password utilizing hardware TRNG.
 * @return Configured range-constrained password.
 */
uint32_t FramLocker_GeneratePassword(void);

#endif /* __FRAM_LOCKER_H_ */

/* --- END OF FILE fram_locker.h --- */