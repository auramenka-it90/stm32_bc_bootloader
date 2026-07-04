/*
 * board_support_packahe.h
 *
 *  Created on: Apr 11, 2026
 *      Author: aao19
 */

#ifndef BOARD_SUPPORT_PACKAGE_H_
#define BOARD_SUPPORT_PACKAGE_H_

#include	<math.h>
#include	<stdlib.h>
#include	<stdbool.h>
#include	<stddef.h>
#include 	<string.h>
#include 	<stdio.h>
#include 	<stdint.h>
#include 	"main.h"
#include 	"cmsis_os2.h"
#include 	"cmsis_os.h"
#include 	"limits.h"
#include 	"float.h"

#include	"usefull_define.h"


/**********************************************************
 *   system behavior configuration
 *   comment if we don't use it
 */
#define	_BHV_FPGA_START_		1
#define	_BHV_RGB_LED_			1
#define	_BHV_CHECK_APPL_		1
#define	_BHV_JUMP_APPL_			1
#define _BHV_LOCKER_CHECK_		1

/**********************************************************
 * 	STM32 hardware modules
 */
/* RGB led pwm timer */
extern TIM_HandleTypeDef htim3;
extern DMA_HandleTypeDef hdma_tim3_ch1_trig;

// W25Q16 spi
extern	SPI_HandleTypeDef 	hspi1;

//  I2C handle ds1621s+ and AM1805 rtc
extern I2C_HandleTypeDef hi2c1;

// HW CRC
extern CRC_HandleTypeDef hcrc;

// eAssist UART terminal ST-LINK
extern	UART_HandleTypeDef 	huart1;
extern	DMA_HandleTypeDef 	hdma_usart1_rx;
extern	DMA_HandleTypeDef 	hdma_usart1_tx;

// eAssist UART terminal connector
extern	UART_HandleTypeDef 	huart2;
extern	DMA_HandleTypeDef 	hdma_usart2_rx;
extern	DMA_HandleTypeDef 	hdma_usart2_tx;

/* ********************************************************
 * test	fault bits  */
#define		_B_TEST_HARDWARE_SUCCESS_	0
#define		_B_FAULT_OS_				B0
#define		_B_FAULT_RGB_				B1
#define		_B_FAULT_DS1621_			B2
#define		_B_FAULT_GPIO_				B3
#define		_B_FAULT_FPGA_				B4
#define		_B_FAULT_AM1805_			B5
#define		_B_FAULT_W25Q16_			B6
#define		_B_FAULT_FSMC_				B7
#define		_B_FAULT_FRAM_				B8
//#define		_B_FAULT_EMI_FPGA_			B9
//#define		_B_FAULT_SPI_FPGA_			B10
#define		_B_FAULT_LOCKER_			B11
#define		_B_FAULT_TERMINAL_			B15

/*	global functions */
uint32_t 	init_hardware(void);
bool 		test_status_hardware(uint32_t module);
uint32_t 	get_status_hardware(void);

bool	get_rcc_csr(void);
void	bsp_system_reset(void);
void 	jump_main_application(uint32_t addr);

void	DWT_Init(void);
void	DWT_DeInit(void);
void	delay_us(uint32_t us);

/* overwrite ten wrapper.c/h */
void	ten(bool par);


#endif /* BOARD_SUPPORT_PACKAGE_H_ */
