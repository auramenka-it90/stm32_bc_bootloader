#include "ds1621.h"

static osMutexId_t i2c_mutex = NULL;
static I2C_HandleTypeDef *p_hi2c = NULL;
static float last_temp = -999.0f;
static const uint32_t I2C_TIMEOUT = 100U;

/* --- RTOS Task Variables --- */
static osThreadId_t ds1621_task_handle = NULL;

static const osThreadAttr_t ds1621_task_attributes = {
  .name = "ds1621_task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/**
 * @brief Returns the mutex handle for sharing the bus with other devices like AM1805.
 */
osMutexId_t DS1621_GetI2CMutex(void) {
    return i2c_mutex;
}

/**
 * @brief Internal helper for thread-safe I2C memory operations.
 */
static DS1621_Status i2c_transfer(uint8_t reg, uint8_t *pData, uint16_t size, bool is_read) {
    if (p_hi2c == NULL || i2c_mutex == NULL) return DS1621_ERROR_INIT;

    if (osMutexAcquire(i2c_mutex, 150) != osOK) return DS1621_ERROR_BUSY;

    HAL_StatusTypeDef res;
    if (is_read) {
        res = HAL_I2C_Mem_Read(p_hi2c, DS1621_ADDR, reg, I2C_MEMADD_SIZE_8BIT, pData, size, I2C_TIMEOUT);
    } else {
        res = HAL_I2C_Mem_Write(p_hi2c, DS1621_ADDR, reg, I2C_MEMADD_SIZE_8BIT, pData, size, I2C_TIMEOUT);
    }

    osMutexRelease(i2c_mutex);
    return (res == HAL_OK) ? DS1621_OK : DS1621_ERROR_I2C;
}

/**
 * @brief Software unlock of a hung I2C bus (clear BUSY flag).
 * Pin macros come from main.h via gpio_fallbacks.h.
 */
static void I2C_RecoverBus(I2C_HandleTypeDef *hi2c) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Disable the hardware I2C peripheral */
    HAL_I2C_DeInit(hi2c);

    /* Configure the pins as open-drain outputs */
    GPIO_InitStruct.Pin = I2C1_SCL_DS1621_Pin | I2C1_SDA_DS1621_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    /* Init the ports (both assumed on GPIOB; split-port case handled too) */
    HAL_GPIO_Init(I2C1_SCL_DS1621_GPIO_Port, &GPIO_InitStruct);
    if (I2C1_SCL_DS1621_GPIO_Port != I2C1_SDA_DS1621_GPIO_Port) {
        HAL_GPIO_Init(I2C1_SDA_DS1621_GPIO_Port, &GPIO_InitStruct);
    }

    /* Release both lines high */
    HAL_GPIO_WritePin(I2C1_SDA_DS1621_GPIO_Port, I2C1_SDA_DS1621_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(I2C1_SCL_DS1621_GPIO_Port, I2C1_SCL_DS1621_Pin, GPIO_PIN_SET);

    /* Short delay */
    for(volatile int i=0; i<1000; i++) {}

    /* Clock 9 pulses on SCL to flush the stuck bit out of the slave */
    for (int i = 0; i < 9; i++) {
        /* SCL low */
        HAL_GPIO_WritePin(I2C1_SCL_DS1621_GPIO_Port, I2C1_SCL_DS1621_Pin, GPIO_PIN_RESET);
        for(volatile int j=0; j<1000; j++) {}

        /* SCL high */
        HAL_GPIO_WritePin(I2C1_SCL_DS1621_GPIO_Port, I2C1_SCL_DS1621_Pin, GPIO_PIN_SET);
        for(volatile int j=0; j<1000; j++) {}

        /* If the slave released SDA (line is high now), stop early */
        if (HAL_GPIO_ReadPin(I2C1_SDA_DS1621_GPIO_Port, I2C1_SDA_DS1621_Pin) == GPIO_PIN_SET) {
            break;
        }
    }

    /* Generate a STOP condition manually (SCL high, then SDA: low -> high) */
    HAL_GPIO_WritePin(I2C1_SCL_DS1621_GPIO_Port, I2C1_SCL_DS1621_Pin, GPIO_PIN_RESET);
    for(volatile int j=0; j<1000; j++) {}
    HAL_GPIO_WritePin(I2C1_SDA_DS1621_GPIO_Port, I2C1_SDA_DS1621_Pin, GPIO_PIN_RESET);
    for(volatile int j=0; j<1000; j++) {}

    HAL_GPIO_WritePin(I2C1_SCL_DS1621_GPIO_Port, I2C1_SCL_DS1621_Pin, GPIO_PIN_SET); /* SCL high */
    for(volatile int j=0; j<1000; j++) {}
    HAL_GPIO_WritePin(I2C1_SDA_DS1621_GPIO_Port, I2C1_SDA_DS1621_Pin, GPIO_PIN_SET); /* SDA high (STOP) */
    for(volatile int j=0; j<1000; j++) {}

    /* Hardware reset of the I2C1 peripheral */
    __HAL_RCC_I2C1_FORCE_RESET();
    for(volatile int j=0; j<1000; j++) {}
    __HAL_RCC_I2C1_RELEASE_RESET();

    /* Return the pins to I2C alternate-function mode */
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1; /* I2C1 on STM32F4 */

    HAL_GPIO_Init(I2C1_SCL_DS1621_GPIO_Port, &GPIO_InitStruct);
    if (I2C1_SCL_DS1621_GPIO_Port != I2C1_SDA_DS1621_GPIO_Port) {
        HAL_GPIO_Init(I2C1_SDA_DS1621_GPIO_Port, &GPIO_InitStruct);
    }

    /* Re-initialize the HAL driver */
    HAL_I2C_Init(hi2c);
}

/**
 * @brief Initialize the library, create mutex and configure sensor.
 */
bool DS1621_Init(I2C_HandleTypeDef *hi2c_ptr) {
    if (hi2c_ptr == NULL) return false;
    p_hi2c = hi2c_ptr;

    if (i2c_mutex == NULL) {
        i2c_mutex = osMutexNew(NULL);
        if (i2c_mutex == NULL) return false;
    }

    /* Check the hardware state of the I2C layer: if it is not ready */
    /* (busy, error, timeout) while the peripheral is enabled, recover the bus. */
    HAL_I2C_StateTypeDef i2c_state = HAL_I2C_GetState(p_hi2c);

    if (i2c_state != HAL_I2C_STATE_READY && i2c_state != HAL_I2C_STATE_RESET) {
        I2C_RecoverBus(p_hi2c);
    }

    /* Check that the sensor is present on the bus */
    if (HAL_I2C_IsDeviceReady(p_hi2c, DS1621_ADDR, 3, I2C_TIMEOUT) != HAL_OK) {
        /* Sensor still silent: try bus recovery as a last chance */
        I2C_RecoverBus(p_hi2c);
        if (HAL_I2C_IsDeviceReady(p_hi2c, DS1621_ADDR, 3, I2C_TIMEOUT) != HAL_OK) {
            return false;
        }
    }

    uint8_t cfg_status;
    if (i2c_transfer(DS1621_REG_CONFIG, &cfg_status, 1, true) != DS1621_OK) return false;

#if DS1621_MODE_CONTINUOUS == 1
    /* Clear the 1SHOT bit to enable continuous conversion mode */
    cfg_status &= ~DS1621_BIT_1SHOT;
#else
    /* Set the 1SHOT bit to enable one-shot conversion mode */
    cfg_status |= DS1621_BIT_1SHOT;
#endif

    if (i2c_transfer(DS1621_REG_CONFIG, &cfg_status, 1, false) != DS1621_OK) return false;

#if DS1621_MODE_CONTINUOUS == 1
    /* In continuous mode the START command is sent only once, at init */
    if (osMutexAcquire(i2c_mutex, 150) == osOK) {
        uint8_t cmd_start = DS1621_CMD_START;
        HAL_I2C_Master_Transmit(p_hi2c, DS1621_ADDR, &cmd_start, 1, I2C_TIMEOUT);
        osMutexRelease(i2c_mutex);
    }
#endif

    return true;
}

/**
 * @brief Perform measurement based on selected mode (Continuous or One-Shot).
 */
DS1621_Status DS1621_Update(void) {
    float current = 0.0f;
    DS1621_Status res;

#if DS1621_MODE_CONTINUOUS == 0
    /* --- ONE-SHOT MODE --- */
    /* 1. Start conversion */
    if (osMutexAcquire(i2c_mutex, 150) == osOK) {
        uint8_t cmd_start = DS1621_CMD_START;
        HAL_I2C_Master_Transmit(p_hi2c, DS1621_ADDR, &cmd_start, 1, I2C_TIMEOUT);
        osMutexRelease(i2c_mutex);
    }

#ifdef DS1621_USE_DONE_POLLING
    /* 2a. Poll the DONE bit with 1 second total timeout */
    bool is_done = false;
    uint8_t cfg_status;
    for (int i = 0; i < 10; i++) {
        osDelay(100);
        if (i2c_transfer(DS1621_REG_CONFIG, &cfg_status, 1, true) == DS1621_OK) {
            if (cfg_status & DS1621_BIT_DONE) {
                is_done = true;
                break;
            }
        }
    }
    if (!is_done) return DS1621_TIMEOUT;
#else
    /* 2b. Fixed delay based on maximum conversion time (750ms) */
    osDelay(800);
#endif
#endif /* DS1621_MODE_CONTINUOUS == 0 */

    /* In continuous mode the block above is skipped and we read immediately */
    /* 3. Retrieve and store calculated data */
    res = DS1621_GetTemperatureHighRes(&current);
    if (res == DS1621_OK) {
        last_temp = current;
    }
    return res;
}

/**
 * @brief Calculate high-resolution temperature from registers.
 */
DS1621_Status DS1621_GetTemperatureHighRes(float *out_temp) {
    if (out_temp == NULL) return DS1621_ERROR_INIT;

    uint8_t temp_data[2] = {0};
    uint8_t count_remain = 0;
    uint8_t count_per_c = 0;

    if (i2c_transfer(DS1621_REG_TEMP, temp_data, 2, true) != DS1621_OK) return DS1621_ERROR_I2C;
    if (i2c_transfer(DS1621_REG_COUNT, &count_remain, 1, true) != DS1621_OK) return DS1621_ERROR_I2C;
    if (i2c_transfer(DS1621_REG_SLOPE, &count_per_c, 1, true) != DS1621_OK) return DS1621_ERROR_I2C;

    if (count_per_c == 0) return DS1621_ERROR_I2C; /* Prevent division by zero */

    /* Read raw temperature (truncate the 0.5C bit, keep only integer part) */
    int8_t temp_read = (int8_t)temp_data[0];

    /* High resolution formula: Temperature = temp_read - 0.25 + (count_per_c - count_remain) / count_per_c */
    *out_temp = (float)temp_read - 0.25f + ((float)(count_per_c - count_remain) / (float)count_per_c);

    return DS1621_OK;
}

/**
 * @brief Returns the last successfully measured temperature.
 */
float DS1621_GetLastTemperature(void) {
    return last_temp;
}

/**
 * @brief FreeRTOS task for periodic sensor handling.
 */
static void DS1621_Task(void *argument) {
    for (;;) {
        /* The function behaves according to DS1621_MODE_CONTINUOUS */
        DS1621_Update();

        /* Delay between temperature updates */
        osDelay(1000);
    }
}

/**
 * @brief Creates the RTOS task for polling the sensor.
 */
osThreadId_t DS1621_CreateTask(void) {
    /* If the task already exists, just return its handle */
    if (ds1621_task_handle != NULL) {
        return ds1621_task_handle;
    }

    ds1621_task_handle = osThreadNew(DS1621_Task, NULL, &ds1621_task_attributes);
    return ds1621_task_handle;
}
