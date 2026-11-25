#include	"dspa.h"
#include	"dspa_defs.h"
#include 	"dspa_sigdefs.h"

#include	"board_support_package.h"
#include 	"pin_mgmt.h"
#include 	"rgb_led.h"
#include 	"led_blink.h"
#include	"std_com.h"
#include	"wrapper.h"
#include 	"version_control.h"
#include 	"w25q16.h"
#include 	"terminal.h"
#include	"terminal_signals.h"
#include    "application.h"
#include 	"fm22l16.h"
#include 	"fram_recorder.h"
#include 	"fram_locker.h"


/* main terminal uart */
Uart_ctrl_t	uctrl;

/* dspa  main func. */
static	PROG_Config_t	PROG_Config;
static	SYS_Config_t  	SYS_Config;

static 	bool reset_request=	false;
static 	bool fl_stay_here=		false;
static 	int	handle;

static	char* uint32_to_string(uint32_t n, char* buffer, size_t buffer_size) ;



//	terminal communication functions
static int	Send(du8 *const pbuf, const unsigned int num);
static int	InBufUsed(void);
static int	GetData(du8 *const pbuf, const unsigned int num);
static void	FlushInBuffer(void);

//	main device functions
static dboolean	sys_SaveSettings(void);
static void	sys_Reset(void);
static char*	sys_SelfTest(void);

//	main	programming functions
static du32		prg_ReadId(const du8 dev_num);
static dboolean prg_Read(const du8 dev_num, du8 *const pusData,	const du32 ulStartAddress);
static dboolean prg_Erase(const du8 dev_num);
static dboolean prg_Write(const du8 dev_num, du8 *const pusData, const du32 ulStartAddress);
static void		prg_Reset(const du8 dev_num);
static dboolean prg_Finish(const du8 dev_num);


/* device descriptor */
Dev_descriptor_t dev_descr_arr[_ID_DEV_NUM] =
{
	{
	//	FLASH FPGA
		"DD4-W25Q16JV/M_ID=",
		W25Q16_PAGE_SIZE,
		W25Q16_PAGE_SIZE,
		_TERMINAL_FPGA_MAX_NUM_BLOCK_*W25Q16_BLOCK_SIZE,
		W25Q16_TIMEOUT_CHIP_WRITE_S,
		W25Q16_TIMEOUT_CHIP_ERASE_S,
		//dtrue,
		dfalse, /* MUST HAVE otherwise the terminal link drops !!! */
		0,
		0
	},
	{
	//	CPU internal memory (ADDED)
		"DD11-STM32F415ZGTX/ID=",
		FLASH_CPU_PAGE_SIZE,     // 1024
		FLASH_CPU_PAGE_SIZE,     // 1024
		FLASH_CPU_SIZE,          // defined in application.h
		2,                       // Timeout write (sec)
		6,                       // Timeout erase (sec) - increased for sectors
		dfalse,
		FLASH_CPU_START_ADDRESS,
		FLASH_CPU_START_ADDRESS
	},
	{
//	[2] FRAM recorder (_ID_FRAM_RECORDER = 0x02)
		"DD17-FM22L16/ID=",
		FRAM_RECORDER_BLOCK_SIZE, // transfer 1024 bytes on write
		FRAM_RECORDER_BLOCK_SIZE, // transfer 1024 bytes on read
		FRAM_RECORDER_SIZE_BYTES, // 512 kB (256 kWords)
		2,
		2,
		dfalse,
		FRAM_RECORDER_START_ADDRESS, // word address 0x00000, start of the FRAM array
		FRAM_RECORDER_START_ADDRESS
	}
};


/*	Telemetry descriptor */
TEL_Descriptor_t TEL_dscr = {
		100/*us*/* (_TERMINAL_TASK_TICK_ * 1000),	// Period (lsb = 0.01 us)
		TEL_MAX_SIGNALS, 		// maximum number signals permitted to use in telemetry
		0,  									// Frame size (not supportes)
		TEL_ATTR_SUPPORT_BUFFER					//attributes
		};




/*	Initialization dspa  system */
bool terminal_init(void) {

	PROG_Config.descriptor = dev_descr_arr;
	PROG_Config.descriptor_size = _ID_DEV_NUM;

	PROG_Config.FuncPtr_ReadId = &prg_ReadId;
	PROG_Config.FuncPtr_Read = &prg_Read;
	PROG_Config.FuncPtr_Erase = &prg_Erase;
	PROG_Config.FuncPtr_Write = &prg_Write;
	PROG_Config.FuncPtr_Reset = &prg_Reset;
	PROG_Config.Func_Ptr_Finish = &prg_Finish;

	SYS_Config.FuncPtr_SaveSettings = &sys_SaveSettings;
	SYS_Config.FuncPtr_SelfTest = &sys_SelfTest;
	SYS_Config.FuncPtr_SysReset = &sys_Reset;

	SYS_Config.system_info = device_info_create(FRAM_LOCKER_PASSWORD);

	SYS_Init(&SYS_Config);

	PROG_Init(&PROG_Config);

	TEL_Set_descriptor(&TEL_dscr);

	dspa_dispatcher_init(Send, GetData, InBufUsed, FlushInBuffer);

	handle = init_terminal_signals();

	/*	 uart initialization	*/

	if 	(CP2102_Is_Active()){
		set_state_led(LED_STATE_BLINK_WHITE);
		uctrl.huart = &huart1;
		uctrl.dma_handle_rx = &hdma_usart1_rx;
		uctrl.dma_handle_tx = &hdma_usart1_tx;
	}else{
		set_state_led(LED_STATE_BLINK_BLUE);
		uctrl.huart = &huart2;
		uctrl.dma_handle_rx = &hdma_usart2_rx;
		uctrl.dma_handle_tx = &hdma_usart2_tx;
	}


	uctrl.rx_buf_size = _TERMINAL_RX_BUFF_SIZE_;
	uctrl.tx_buf_size = _TERMINAL_TX_BUFF_SIZE_;
	com_init(&uctrl);

	/* One-shot STLINK presence snapshot at startup (PB7) */
	stlink_attached = (STLINK_Is_Connected() == 1) ? true : false;

#ifdef	_TERMINAL_PERMISSION_WRAPPER_
	wrp_init(&uctrl, _TERMINAL_WRAPPER_ADDRESS_, _TERMINAL_WRAPPER_ALT_ADDRESS_,
			_TERMINAL_RECEIVE_TIMEOUT_US_);
#endif



	com_start_receive(&uctrl);
	return true;

}


/*
 *	Handling DSPAssist Task
 */
void terminal_task(void) {

	terminal_telemetry_handler();

	dspa_dispatcher(_TERMINAL_TASK_TICK_ * 1000);

#ifdef	_TERMINAL_PERMISSION_WRAPPER_
	com_check_timeout(&uctrl, _TERMINAL_TASK_TICK_ * 1000);
#endif

	if (reset_request) {
		osDelay(200);
		bsp_system_reset();
	}


	/* Handle bootloader timeout and conditional jump */
	static uint32_t tick_start = 0;
	if (tick_start == 0) {
		tick_start = osKernelGetTickCount();
	}

	/* Calculate dynamic timeout based on reset cause */
	uint32_t boot_timeout = _TERMINAL_TIMEOUT_MS_;

#if (_BHV_FAST_JUMP_ == 1)
	/* If this is a cold boot (Hard Reset), bypass the delay and jump immediately */
	if (!soft_reset) {
		boot_timeout = 0;
	}
#endif

	/* Timeout expired AND no "stay here" command -> jump to the application */
	if (((osKernelGetTickCount() - tick_start) >= boot_timeout) && !fl_stay_here) {
#ifdef	_TERMINAL_PERMISSION_JUMP_APPL_
		/* Latch the timeout expiry so this block is never entered again */
		fl_stay_here=	true;
		/* Check: if the firmware image is alive - jump */
		if (check_application() == true) {
			jump_main_application(APPLICATION_ADDRESS);
		}

#endif
	}

	led_blink();
	osDelay(_TERMINAL_TASK_TICK_);
}

/* telemetry */
void	terminal_telemetry_handler(void){
	TEL_Sample_Update(handle);			// Telemetry: update sample value
	TEL_Sample_Save();					// Telemetry: save sample to buffer
}

/**************************************************************************************************
 * terminal communication functionS
 *************************************************************************************************/
//	Translate data -> UART
static int Send(unsigned char *const buf, const unsigned int num) {

	if ((get_state_led()==LED_STATE_BLINK_BLUE)||(get_state_led()==LED_STATE_BLINK_WHITE)){
	//	set_state_led(LED_STATE_BREATH_WHITE);	//	LED_STATE_RG_OFF
		set_state_led(LED_STATE_RG_OFF);	//
		fl_stay_here=	true;

	}
	com_send(&uctrl, buf, num);
	return num;
}

//	Copy ingoing dataS -> buffer 'buf'
static int GetData(unsigned char *const buf, const unsigned int num) {

	unsigned int num_i = num;
	if (num_i == 0){
		num_i = com_inbuf_used(&uctrl);
	}
	if (num_i == 0){
		return 0;
	}
	com_inbuf_fetch(&uctrl, buf, num_i);

	return num_i;
}



//	How many data item
static int InBufUsed(void){
	return com_inbuf_used(&uctrl);
}

//	Reset input buffers
static void FlushInBuffer(void){
	com_flush_in_buffer(&uctrl);
}

/**************************************************************************************************
 *	Main device functionS
 *	Maintaining the basic settings
 **************************************************************************************************/
static dboolean sys_SaveSettings(void){
	bool	result=	dtrue;
	return	result;
}

//	Device Reset
static void sys_Reset(void){
    reset_request = true;
}

//	Self test
static char* sys_SelfTest(void){
	static char result[64];
	char str[16];
	uint32_to_string(test_hardware_result, str, sizeof(str));
	snprintf(result, sizeof(result), "Self-diagnosis: Device status(0=ok): %s", str);
	return result;
}


/**************************************************************************************************
 * main programming functions
 *************************************************************************************************/
//	read ID
static du32 prg_ReadId(const du8 dev_num) {
	uint32_t t=0;

	switch (dev_num) {
	case _ID_FPGA_FLASH:{
		t=	(uint32_t)DD15.manufacturer_id;
		break;
	}
	case _ID_CPU_MEMORY: {
		t = HAL_GetDEVID(); // read STM32 controller ID
		break;
	}
	case _ID_FRAM_RECORDER: {
		// Parallel FRAM has no simple ID register.
		// Return a fixed "magic" constant 0x2216 for the terminal.
		t = 0x2216;
		break;
	}
	default:	break;
	}

	return t;
}

static dboolean prg_Read(const du8 dev_num, du8 *const pusData,	const du32 ulStartAddress) {
	dboolean result = dfalse;

	switch (dev_num) {
	case _ID_FPGA_FLASH:{
		SPI_Bus_Acquire_For_STM32();
		result=	(W25Q16_ReadData(&DD15, ulStartAddress, pusData, W25Q16_PAGE_SIZE)==osOK);
		break;
	}
	case _ID_CPU_MEMORY: {
		eeprom_memory_read_buffer(pusData, (const uint8_t*)ulStartAddress, dev_descr_arr[_ID_CPU_MEMORY].cnt_rd_block);
		result = dtrue;
		break;
	}
	case _ID_FRAM_RECORDER: {
			uint32_t block_bytes = dev_descr_arr[_ID_FRAM_RECORDER].cnt_rd_block;
			uint32_t words_to_read = block_bytes / 2;

			// 1. Compute the offset in BYTES from the start of the block (0, 1024, 2048...)
			uint32_t byte_offset = ulStartAddress - FRAM_RECORDER_START_ADDRESS;

			// 2. Convert the byte offset to a word offset (/ 2) and add it to the FSMC start address
			uint32_t current_word_addr = FRAM_RECORDER_START_ADDRESS + (byte_offset / 2);

			// Guard against running past the end of the physical FRAM
			if ((current_word_addr + words_to_read) > (FRAM_RECORDER_END_ADDRESS + 1)) {
				words_to_read = (FRAM_RECORDER_END_ADDRESS + 1) - current_word_addr;
			}

			// Read the correct block at the correct address!
			if (fsmc_read_buffer(FSMC_DEV_FRAM, current_word_addr, (uint16_t*)pusData, words_to_read) == FSMC_OK) {
				result = dtrue;
			}
			break;
		}
	default:	break;
	}
	return result;
}


//	erase all memory
static dboolean prg_Erase(const du8 dev_num) {
	dboolean result=	false;

	set_state_led(LED_STATE_POLICE_FLASHER);

	switch (dev_num) {
	case _ID_FPGA_FLASH: {
		SPI_Bus_Acquire_For_STM32();
		for (uint32_t i = 0; i < _TERMINAL_FPGA_MAX_NUM_BLOCK_; i++) {
			if (W25Q16_BlockErase(&DD15, i * W25Q16_BLOCK_SIZE) != osOK) {
				SPI_Bus_Release_To_FPGA();
				return	dfalse;
			}
		}
		result=	dtrue;
		break;
	}
	case _ID_CPU_MEMORY: {
	    // Smart bulk erase of all application sectors in a single call
	    result = eeprom_memory_erase_app();
	    break;
	}
	case _ID_FRAM_RECORDER: {
		result = dfalse;
		break;
	}

	default:	break;
	}
	return result;
}

//	write page
static dboolean prg_Write(const du8 dev_num, du8 *const pusData,	const du32 ulStartAddress) {
	dboolean result = dfalse;

	switch (dev_num) {
	case _ID_FPGA_FLASH: {
		result =(W25Q16_PageProgram(&DD15, ulStartAddress, pusData, W25Q16_PAGE_SIZE) == osOK);
		break;
	}
	case _ID_CPU_MEMORY: {
		uint32_t bytes_to_write = dev_descr_arr[_ID_CPU_MEMORY].cnt_wr_block;
		uint32_t words_to_write = bytes_to_write >> 2; // bytes -> words
		result = eeprom_memory_write_words((uint32_t*)pusData, ulStartAddress, words_to_write);
		break;
	}
	case _ID_FRAM_RECORDER: {
		// Writing from the terminal side is blocked!
		// Return false so the PC tool knows the data was not written.
		result = dfalse;
		break;
	}
	default:	break;
	}
	return result;
}


//	reset
static void prg_Reset(const du8 dev_num) {
	switch (dev_num) {
	case _ID_FPGA_FLASH: {
		SPI_Bus_Release_To_FPGA();
		if (CP2102_Is_Active()){
			FPGA_Reset();
		}else{
			osDelay(10);
		}
		break;
	}
	case _ID_CPU_MEMORY: {
		// nothing specific to reset here
		break;
	}
	default:	break;
	}
}



//	Finish programming
static dboolean prg_Finish(const du8 dev_num) {
	dboolean result = dfalse;
	switch (dev_num) {
	case _ID_FPGA_FLASH: {
		set_state_led(LED_STATE_RG_OFF);
		SPI_Bus_Release_To_FPGA();
/*		FPGA_Reset() is mandatory !!!!	*/
		result=	true;
		break;
	}
	case _ID_CPU_MEMORY: {
		set_state_led(LED_STATE_RG_OFF);
		result = dtrue;
		break;
	}
	case _ID_FRAM_RECORDER:{
		result = dtrue;
		break;
	}

	default:
		break;
	}

	return result;
}

/**************************************************************************************************
 * auxiliary functions
 *************************************************************************************************/

/* uint32_t ->string */
static char* uint32_to_string(uint32_t n, char *buffer, size_t buffer_size) {
	if (buffer_size < 11)
		return NULL;
	if (n == 0) {
		buffer[0] = '0';
		buffer[1] = '\0';
		return buffer;
	}
	char temp[11];
	int i = 0;
	while (n > 0) {
		temp[i++] = '0' + (n % 10);
		n /= 10;
	}
	int j = 0;
	while (i > 0) {
		buffer[j++] = temp[--i];
	}
	buffer[j] = '\0';
	return buffer;
}
