
#ifndef TERMINAL_SIGNALS_H_
#define TERMINAL_SIGNALS_H_

#include "w25q16.h"
#include "board_support_package.h"
#include "rgb_led.h"
#include "am1805.h"

#define		DSPA_SIGNALS_NAME		dspa

//	test hardware
extern	uint32_t	test_hardware_result;

// flash w25q16
extern	W25Q16_Handle_t 	DD15;

// check USB uart
extern	bool	w_up_uart;

// RTC AM1805
extern 	AM1805_Diag_t rtc_diag;
extern	char data_time_string[];

//	ds1621+
extern	float current_temp;
/*





extern 	char time_str[];
extern 	AM1805_Diag_t rtc_diag;

// RGB leds
extern LED_Handler_t led_main;

// debug section
extern float current_temp;
extern uint16_t test_fpga;
extern uint32_t cnt_1kHz_interrupt;


*/

int	init_terminal_signals(void);

#endif /* TERMINAL_SIGNALS_H_ */
