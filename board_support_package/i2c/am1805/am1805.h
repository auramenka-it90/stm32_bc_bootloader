/**
 * @file am1805.h
 * @brief Потокобезопасный драйвер для RTC AM1805AQ.
 */

#ifndef AM1805_H
#define AM1805_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"

/* --- Параметры I2C --- */
#define AM1805_ADDR              (0x69 << 1)
#define AM1805_I2C_TIMEOUT       100
/* Finite mutex timeout: a stuck bus must not freeze the caller forever.
   Kept equal to DS1621's mutex timeout (150 ms). */
#define AM1805_MUTEX_TIMEOUT     150

/* --- Пароли и защити --- */
#define AM1805_INIT_PASSWORD     0x55AA3C0F   // Парола за принудителна инициализация

/* --- Адреса регистров --- */
#define AM1805_REG_HUNDREDTHS    0x00
#define AM1805_REG_SECONDS       0x01
#define AM1805_REG_MINUTES       0x02
#define AM1805_REG_HOURS         0x03
#define AM1805_REG_DATE          0x04
#define AM1805_REG_MONTH         0x05
#define AM1805_REG_YEAR          0x06
#define AM1805_REG_WEEKDAY       0x07

#define AM1805_REG_STATUS        0x0F
#define AM1805_REG_CTRL1         0x10
#define AM1805_REG_CAL_XT        0x14
#define AM1805_REG_OSC_CTRL      0x1C
#define AM1805_REG_OSC_STAT      0x1D
#define AM1805_REG_KEY           0x1F
#define AM1805_REG_BREF          0x21
#define AM1805_REG_BATMODE_IO    0x27

/* --- Командные ключи и маски --- */
#define AM1805_KEY_OSC           0xA1 /* unlocks write access to OSC_CTRL (0x1C) */
#define AM1805_KEY_CONF          0x9D /* unlocks Trickle/BREF/AFCTRL/Batmode/OutCtrl */
#define AM1805_KEY_RESET         0x3C /* write to Configuration Key = software reset */

/* --- CTRL1 (0x10) bit masks, per datasheet tables 6-39/6-40 --- */
#define AM1805_CTRL1_WRTC        0x01 /* Write RTC: must be set to write counters */
#define AM1805_CTRL1_12H         0x40 /* 12/24 select: 0 = 24h mode, 1 = 12h mode */

/* --- OSC_CTRL (0x1C) values, per datasheet tables 6-70/6-71 --- */
/* OSEL=0 (XT oscillator), ACAL=0 (no autocal), no AOS/FOS auto-switch to RC */
#define AM1805_OSCCTRL_XT        0x00

/* --- BREF (0x21) values, per datasheet tables 6-80/6-81 --- */
/* BREF field is bits 7:4; 1011 = VBAT falling 2.1 V / rising 2.5 V */
#define AM1805_BREF_2V1          0xB0

/* --- BATMODE_IO (0x27), per datasheet section 6.10.4 --- */
#define AM1805_BATMODE_IOBM      0x80 /* IOBM=1: disable I/O on brownout */

/* --- CAL_XT (0x14) bit masks, per datasheet tables 6-50/6-51 --- */
#define AM1805_CALXT_CMDX        0x80 /* 0 = normal step 1.907 ppm, 1 = coarse 3.814 ppm */
#define AM1805_CALXT_OFFSETX_MSK 0x7F /* two's complement, -64..+63 steps */

/* --- OSC_STAT (0x1D) bit masks, per datasheet tables 6-72/6-73 --- */
#define AM1805_OSCSTAT_ACF       0x01 /* Autocalibration Failure, write 0 to clear */
#define AM1805_OSCSTAT_OF        0x02 /* Oscillator Failure, write 0 to clear */
#define AM1805_OSCSTAT_OMODE     0x10 /* read-only: 1 = RC drives clocks, 0 = XT */
#define AM1805_OSCSTAT_LKO2      0x20
#define AM1805_OSCSTAT_XTCAL_MSK 0xC0 /* Extended crystal calibration, 0/-122/-244/-366 ppm */
#define AM1805_OSCSTAT_XTCAL_POS 6

typedef enum {
    AM1805_OK = 0,
    AM1805_ERROR,
    AM1805_BUSY
} AM1805_Status;

typedef struct {
    uint8_t hours, minutes, seconds, hundredths;
    uint8_t day, month, weekday;
    uint16_t year;
    bool is_xt_active;
} AM1805_Time_t;

typedef struct {
    uint8_t status;      // 0x0F
    uint8_t ctrl1;       // 0x10
    uint8_t cal_xt;      // 0x14
    uint8_t osc_ctrl;    // 0x1C
    uint8_t osc_stat;    // 0x1D
    uint8_t bref;        // 0x21
    uint8_t batmode_io;  // 0x27
} AM1805_Diag_t;

/* --- Основной интерфейс --- */
bool          AM1805_Init_Smart(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id);
AM1805_Status AM1805_SoftwareReset(void);
AM1805_Status AM1805_GetTime(AM1805_Time_t *time);
AM1805_Status AM1805_SetTime(AM1805_Time_t *time);
AM1805_Status AM1805_SetCalibrationByDrift(float drift_sec_per_hour);
AM1805_Status AM1805_GetDiagnostics(AM1805_Diag_t *diag);
void          AM1805_FormatFullDateTime(char* buf, uint16_t len, AM1805_Time_t* time);
AM1805_Time_t AM1805_ParseBuildTime(const char* date, const char* time);
bool          AM1805_ForceInit(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id, uint32_t password);
bool          AM1805_ResetAndForceInit(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id, uint32_t password);

#endif /* AM1805_H */
