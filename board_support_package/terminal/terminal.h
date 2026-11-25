#ifndef TERMINAL_H_
#define TERMINAL_H_

#include	"board_support_package.h"


// terminal.h
#define 	_TERMINAL_TIMEOUT_MS_       		5000        // 5 second timeout
#define		_TERMINAL_PERMISSION_JUMP_APPL_		1

#define		_TERMINAL_PERMISSION_WRAPPER_

#ifdef		_TERMINAL_PERMISSION_WRAPPER_
	#define		_TERMINAL_WRAPPER_ADDRESS_		0x0B
	#define		_TERMINAL_WRAPPER_ALT_ADDRESS_	_TERMINAL_WRAPPER_ADDRESS_
#else

#endif

#define		_TERMINAL_RX_BUFF_SIZE_				1024*2
#define		_TERMINAL_TX_BUFF_SIZE_				1024*8
#define		_TERMINAL_TASK_TICK_	        	1U		    //	mSec
#define   	_TERMINAL_RECEIVE_TIMEOUT_US_		20000U		//	uSec
#define		_TERMINAL_BOOTLOADER_TIMEOUT_		15000U
#define		_TERMINAL_FPGA_MAX_NUM_BLOCK_		6


//	memory identifiers
#define		_ID_FPGA_FLASH			0x00
#define		_ID_CPU_MEMORY			0x01		// ADDED: CPU internal memory ID
#define		_ID_FRAM_RECORDER		0x02		// FRAM recorder ID in FRAM (TERMINAL_ID_FRAM_RECORDER)
#define		_ID_DEV_NUM				3			// CHANGED: now we have 3 devices

/*
 *		eAssist max = 1024 !!!!!
 */
#define	DSPA_CFG_WRITE_PAGE_SIZE		1024
#define	DSPA_CFG_READ_PAGE_SIZE			1024
/*
 *
 */

bool	terminal_init(void);
void	terminal_task(void);
void	terminal_telemetry_handler(void);

#endif /* TERMINAL_H_ */
