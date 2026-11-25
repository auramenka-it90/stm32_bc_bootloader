#ifndef DS1621_H
#define DS1621_H

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os2.h"
#include "stm32f4xx_hal.h"
#include "board_support_package.h"

/* --- Configuration --- */
/* Operating mode select: 1 - Continuous (default), 0 - One-Shot */
#define DS1621_MODE_CONTINUOUS    1

/* Poll the DONE bit instead of a fixed 800ms delay (One-Shot mode only) */
#define DS1621_USE_DONE_POLLING   1 

/* --- I2C Hardware Pins (from main.h via gpio_fallbacks.h) --- */
#include "gpio_fallbacks.h"

#define DS1621_BIT_1SHOT        0x01
#define DS1621_ADDR             (0x48 << 1)

/* Commands & Registers */
#define DS1621_REG_TEMP         0xAA
#define DS1621_REG_CONFIG       0xAC
#define DS1621_REG_COUNT        0xA8
#define DS1621_REG_SLOPE        0xA9
#define DS1621_CMD_START        0xEE

/* Bitmasks */
#define DS1621_BIT_DONE         0x80

typedef enum {
    DS1621_OK = 0,
    DS1621_ERROR_I2C,
    DS1621_ERROR_INIT,
    DS1621_ERROR_BUSY,
    DS1621_TIMEOUT
} DS1621_Status;

/* Public API */
/**
 * @brief Initialize the library, create mutex and configure sensor.
 * @param hi2c_ptr Pointer to the HAL I2C handle (e.g., &hi2c1)
 * @return true if successful
 */
bool          DS1621_Init(I2C_HandleTypeDef *hi2c_ptr);

/**
 * @brief Perform one measurement cycle or read instantly based on mode.
 * @return Status code
 */
DS1621_Status DS1621_Update(void);

/**
 * @brief Calculate high-resolution temperature from registers.
 * @param out_temp Pointer to float to store the result
 * @return Status code
 */
DS1621_Status DS1621_GetTemperatureHighRes(float *out_temp);

/**
 * @brief Returns the last successfully measured temperature.
 */
float         DS1621_GetLastTemperature(void);

/**
 * @brief Returns the mutex handle for sharing the bus with other devices like AM1805.
 */
osMutexId_t   DS1621_GetI2CMutex(void);

/**
 * @brief Creates the RTOS task for periodic sensor polling.
 * @return osThreadId_t Handle of the created task (NULL on error).
 */
osThreadId_t  DS1621_CreateTask(void);

#endif /* DS1621_H */
