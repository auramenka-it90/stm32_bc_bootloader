/**
  ******************************************************************************
  * @file    pin_mgmt.h
  * @brief   Pin Management Library for STM32 (BV PCB Rev02, Bootloader)
  *          with Standard and Fast Pin Access.
  *          All comments in English.
  ******************************************************************************
  */

#ifndef __PIN_MGMT_H
#define __PIN_MGMT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "board_support_package.h"  /* Bring in BSP configuration (main.h, cmsis_os2.h, stdbool) */
#include "gpio_fallbacks.h"         /* Safe default values for pin macros from main.h */

/* Exported types ------------------------------------------------------------*/

/**
  * @brief  Pin descriptor structure
  */
typedef struct {
    GPIO_TypeDef* port;      /*!< GPIO port (GPIOA, GPIOB, etc.) */
    uint16_t pin;            /*!< Pin mask (GPIO_PIN_0, GPIO_PIN_1, etc.) */
    const char* name;        /*!< Pin name for debugging */
} Pin_Descriptor_t;

/* Macros for creating pin descriptors */
#define PIN_DESC(_port, _pin, _name) \
    { .port = _port, .pin = _pin, .name = _name }

/* --- Pin Aliases from main.h (BV PCB Rev02) --- */

#define PMGT_DONE_PIN         DONE_Pin
#define PMGT_DONE_PORT        DONE_GPIO_Port

#define PMGT_RESET_PIN        RESET_XS6_Pin
#define PMGT_RESET_PORT       RESET_XS6_GPIO_Port

#define PMGT_SEL_MEM_PIN      SEL_MEM_STM32_Pin
#define PMGT_SEL_MEM_PORT     SEL_MEM_STM32_GPIO_Port

#define PMGT_NSS_SPI2_PIN     SPI2_NSS_Pin
#define PMGT_NSS_SPI2_PORT    SPI2_NSS_GPIO_Port

#define PMGT_NSS_XC6_PIN      NSS_STM32_XC6_Pin
#define PMGT_NSS_XC6_PORT     NSS_STM32_XC6_GPIO_Port

#define PMGT_STLINK_GND_PIN   STLINK_GND_Pin
#define PMGT_STLINK_GND_PORT  STLINK_GND_GPIO_Port

#define PMGT_W_UP_PIN         W_UP_Pin
#define PMGT_W_UP_PORT        W_UP_GPIO_Port

#define PMGT_TEN_KPA_PIN      GPIO_0_Pin
#define PMGT_TEN_KPA_PORT     GPIO_0_GPIO_Port

/* Exported functions --------------------------------------------------------*/

/* ==================== INITIALIZATION ==================== */
osStatus_t PIN_MGMT_Init(void);
osStatus_t PIN_MGMT_DeInit(void);

/* Compatibility with legacy CubeMX-style spelling */
#define PIN_Mgmt_Init PIN_MGMT_Init

/* ==================== PIN OPERATIONS (ATOMIC BSRR/IDR, ISR-CALLABLE) ==================== */
osStatus_t PIN_Set(const Pin_Descriptor_t* pin);
osStatus_t PIN_Reset(const Pin_Descriptor_t* pin);
osStatus_t PIN_Toggle(const Pin_Descriptor_t* pin);
uint8_t PIN_Read(const Pin_Descriptor_t* pin);

/* ==================== FAST ALIASES (SOURCE COMPATIBILITY) ==================== */
osStatus_t PIN_Set_F(const Pin_Descriptor_t* pin);
osStatus_t PIN_Reset_F(const Pin_Descriptor_t* pin);
osStatus_t PIN_Toggle_F(const Pin_Descriptor_t* pin);
uint8_t PIN_Read_F(const Pin_Descriptor_t* pin);

/* ==================== GPIO MUTEX (MANUAL, FOR MULTI-PIN SEQUENCES) ==================== */
osStatus_t PIN_GPIO_Mutex_Acquire(uint32_t timeout);
osStatus_t PIN_GPIO_Mutex_Release(void);
uint8_t PIN_GPIO_Mutex_Is_Locked(void);

/* ==================== INTERFACE PRESENCE DETECTION ==================== */

/* Reads PE4 = CP2102 SUSPEND (via ADuM1201, active-high): 1 = USB host active */
uint8_t CP2102_Is_Active(void);

/* Reads PB7 = STLINK_GND (input pull-up): 1 = STLINK debugger attached */
uint8_t STLINK_Is_Connected(void);

/* ==================== UTILITIES ==================== */
osStatus_t PIN_Delay(uint32_t ms);
osStatus_t PIN_Blink(const Pin_Descriptor_t* pin, uint8_t count, uint32_t delay_ms);

/* ==================== CONVENIENCE MACROS ==================== */

/* Standard Macros */
#define PIN_ON(pin)          PIN_Set(pin)
#define PIN_OFF(pin)         PIN_Reset(pin)
#define PIN_TOGGLE(pin)      PIN_Toggle(pin)
#define PIN_STATE(pin)       PIN_Read(pin)

/* Fast Macros */
#define PIN_ON_F(pin)        PIN_Set_F(pin)
#define PIN_OFF_F(pin)       PIN_Reset_F(pin)
#define PIN_TOGGLE_F(pin)    PIN_Toggle_F(pin)
#define PIN_STATE_F(pin)     PIN_Read_F(pin)

/* ==================== SYSTEM FUNCTIONS (BV PCB SPECIFIC) ==================== */
osStatus_t SPI_Bus_Acquire_For_STM32(void);
osStatus_t SPI_Bus_Release_To_FPGA(void);
osStatus_t FPGA_System_Restart(uint32_t timeout_ms);
void FPGA_Reset(void);

/* Exported Pin descriptors */
extern const Pin_Descriptor_t pin_w_up_uart;
extern const Pin_Descriptor_t pin_fpga_done;
extern const Pin_Descriptor_t pin_reset_xs6;
extern const Pin_Descriptor_t pin_sel_mem_stm32;
extern const Pin_Descriptor_t pin_nss_spi2;
extern const Pin_Descriptor_t pin_nss_xc6;
extern const Pin_Descriptor_t pin_gpio_0;
extern const Pin_Descriptor_t pin_gpio_1;
extern const Pin_Descriptor_t pin_stlink_gnd;
extern const Pin_Descriptor_t pin_tp0;
extern const Pin_Descriptor_t pin_tp1;
extern const Pin_Descriptor_t pin_ten;

#ifdef __cplusplus
}
#endif

#endif /* __PIN_MGMT_H */
