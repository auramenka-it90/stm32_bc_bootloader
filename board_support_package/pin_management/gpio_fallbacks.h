/**
 * ******************************************************************************
 * @file    gpio_fallbacks.h
 * @brief   Fallback definitions for all GPIO pins of the BC bootloader project.
 *          Ensures each pin macro has a safe default value based on main.h.
 *          Values mirror the current CubeMX-generated main.h (BV PCB Rev02).
 *          All comments in English.
 * ******************************************************************************
 */

#ifndef GPIO_FALLBACKS_H
#define GPIO_FALLBACKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Terminal USB wake detect */
#ifndef W_UP_Pin
#define W_UP_Pin                    GPIO_PIN_4
#endif
#ifndef W_UP_GPIO_Port
#define W_UP_GPIO_Port              GPIOE
#endif

/* FPGA DONE */
#ifndef DONE_Pin
#define DONE_Pin                    GPIO_PIN_6
#endif
#ifndef DONE_GPIO_Port
#define DONE_GPIO_Port              GPIOF
#endif

/* FPGA reset line */
#ifndef RESET_XS6_Pin
#define RESET_XS6_Pin               GPIO_PIN_7
#endif
#ifndef RESET_XS6_GPIO_Port
#define RESET_XS6_GPIO_Port         GPIOF
#endif

/* SPI bus multiplexer to FPGA flash */
#ifndef SEL_MEM_STM32_Pin
#define SEL_MEM_STM32_Pin           GPIO_PIN_8
#endif
#ifndef SEL_MEM_STM32_GPIO_Port
#define SEL_MEM_STM32_GPIO_Port     GPIOF
#endif

/* SPI2 slave select */
#ifndef SPI2_NSS_Pin
#define SPI2_NSS_Pin                GPIO_PIN_1
#endif
#ifndef SPI2_NSS_GPIO_Port
#define SPI2_NSS_GPIO_Port          GPIOC
#endif

/* USART2 KPA bus */
#ifndef USART2_KPA_TX_Pin
#define USART2_KPA_TX_Pin           GPIO_PIN_2
#endif
#ifndef USART2_KPA_TX_GPIO_Port
#define USART2_KPA_TX_GPIO_Port     GPIOA
#endif

#ifndef USART2_KPA_RX_Pin
#define USART2_KPA_RX_Pin           GPIO_PIN_3
#endif
#ifndef USART2_KPA_RX_GPIO_Port
#define USART2_KPA_RX_GPIO_Port     GPIOA
#endif

/* SPI1 to XC6 FPGA flash */
#ifndef SCK_STM32_XC6_Pin
#define SCK_STM32_XC6_Pin           GPIO_PIN_5
#endif
#ifndef SCK_STM32_XC6_GPIO_Port
#define SCK_STM32_XC6_GPIO_Port     GPIOA
#endif

#ifndef MISO_XC6_STM32_Pin
#define MISO_XC6_STM32_Pin          GPIO_PIN_6
#endif
#ifndef MISO_XC6_STM32_GPIO_Port
#define MISO_XC6_STM32_GPIO_Port    GPIOA
#endif

#ifndef MOSI_STM32_XC6_Pin
#define MOSI_STM32_XC6_Pin          GPIO_PIN_7
#endif
#ifndef MOSI_STM32_XC6_GPIO_Port
#define MOSI_STM32_XC6_GPIO_Port    GPIOA
#endif

#ifndef NSS_STM32_XC6_Pin
#define NSS_STM32_XC6_Pin           GPIO_PIN_4
#endif
#ifndef NSS_STM32_XC6_GPIO_Port
#define NSS_STM32_XC6_GPIO_Port     GPIOC
#endif

/* General purpose lines */
#ifndef GPIO_0_Pin
#define GPIO_0_Pin                  GPIO_PIN_0
#endif
#ifndef GPIO_0_GPIO_Port
#define GPIO_0_GPIO_Port            GPIOB
#endif

#ifndef GPIO_1_Pin
#define GPIO_1_Pin                  GPIO_PIN_1
#endif
#ifndef GPIO_1_GPIO_Port
#define GPIO_1_GPIO_Port            GPIOB
#endif

/* CAN2 */
#ifndef CAN2_TX_Pin
#define CAN2_TX_Pin                 GPIO_PIN_13
#endif
#ifndef CAN2_TX_GPIO_Port
#define CAN2_TX_GPIO_Port           GPIOB
#endif

/* RGB LED PWM channel */
#ifndef TMR_DMA_LED_Pin
#define TMR_DMA_LED_Pin             GPIO_PIN_6
#endif
#ifndef TMR_DMA_LED_GPIO_Port
#define TMR_DMA_LED_GPIO_Port       GPIOC
#endif

/* USART1 KPA path (CP2102 side) */
#ifndef UART_TX_KPA_Pin
#define UART_TX_KPA_Pin             GPIO_PIN_9
#endif
#ifndef UART_TX_KPA_GPIO_Port
#define UART_TX_KPA_GPIO_Port       GPIOA
#endif

#ifndef UART_RX_KPA_Pin
#define UART_RX_KPA_Pin             GPIO_PIN_10
#endif
#ifndef UART_RX_KPA_GPIO_Port
#define UART_RX_KPA_GPIO_Port       GPIOA
#endif

/* STLINK presence detect */
#ifndef STLINK_GND_Pin
#define STLINK_GND_Pin              GPIO_PIN_7
#endif
#ifndef STLINK_GND_GPIO_Port
#define STLINK_GND_GPIO_Port        GPIOB
#endif

/* I2C1 bus (DS1621 / AM1805) */
#ifndef I2C1_SCL_DS1621_Pin
#define I2C1_SCL_DS1621_Pin         GPIO_PIN_8
#endif
#ifndef I2C1_SCL_DS1621_GPIO_Port
#define I2C1_SCL_DS1621_GPIO_Port   GPIOB
#endif

#ifndef I2C1_SDA_DS1621_Pin
#define I2C1_SDA_DS1621_Pin         GPIO_PIN_9
#endif
#ifndef I2C1_SDA_DS1621_GPIO_Port
#define I2C1_SDA_DS1621_GPIO_Port   GPIOB
#endif

/* Test points */
#ifndef TP0_Pin
#define TP0_Pin                     GPIO_PIN_0
#endif
#ifndef TP0_GPIO_Port
#define TP0_GPIO_Port               GPIOE
#endif

#ifndef TP1_Pin
#define TP1_Pin                     GPIO_PIN_1
#endif
#ifndef TP1_GPIO_Port
#define TP1_GPIO_Port               GPIOE
#endif

#ifdef __cplusplus
}
#endif

#endif /* GPIO_FALLBACKS_H */
