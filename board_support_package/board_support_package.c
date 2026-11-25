
#include "board_support_package.h"
#include "pin_mgmt.h"
#include "rgb_led.h"
#include "led_blink.h"
#include "w25q16.h"
#include "fsmc_memory.h"
#include "fm22l16.h"
#include "ds1621.h"
#include "am1805.h"
#include "fram_locker.h"

#include "terminal.h"
#include "terminal_signals.h"

// Global hardware test result variable
uint32_t test_hardware_result = _B_TEST_HARDWARE_SUCCESS_;
//	true - software reset/ false - hardware
volatile bool	soft_reset;
// true - suspend usb-uart
bool	w_up_uart;
/* rgb led handle */
LED_Handler_t led_main;
// FPGA external flash memory handle
W25Q16_Handle_t DD15;
// diagnostic AM1805
AM1805_Diag_t rtc_diag;


// Callback function when power-on threshold is reached
static void Locker_ThresholdCallback(void) {
    // Disable all interrupts
    __disable_irq();

    // Stop RTOS scheduler if running
    osKernelLock();

    // Lock device - infinite loop
    while (1) {
        // Hardware is locked
        // Add NOP instructions to prevent optimization
        __NOP();
        __NOP();
        __NOP();
    }

    // Code never reaches here
}


// Initialize all hardware components
uint32_t init_hardware(void) {

	DWT_Init();
	w_up_uart=	PIN_Read(&pin_w_up_uart);

	// ********** Initialize GPIO ***********
	if (PIN_Mgmt_Init() != osOK) {
		test_hardware_result |= _B_FAULT_GPIO_;
	} else {
#ifdef _BHV_FPGA_START_
		SPI_Bus_Release_To_FPGA();
		if (FPGA_System_Restart(1000) != osOK) {
			PIN_Reset(&pin_reset_xs6);
			test_hardware_result |= _B_FAULT_FPGA_;
		}
#endif
	}
#ifdef	_BHV_RGB_LED_
	// *********** Initialize RGB LED ***********
	if (RGB_LED_Init(&led_main, &htim3, TIM_CHANNEL_1) != true) {
		test_hardware_result |= _B_FAULT_RGB_;
	} else {
		led_blink_init(&led_main);
	}
#endif


	//********** W25Q16 FLASH CONFIGURATION **********
	// Configure after pins are initialized
	if (test_status_hardware(_B_FAULT_GPIO_)) {
		if (W25Q16_Init_With_PinMgmt(&DD15, &hspi1) != osOK) {
			test_hardware_result |= _B_FAULT_W25Q16_;
		} else {
			SPI_Bus_Acquire_For_STM32();
			// Read flash identification
			W25Q16_ReadJEDECID(&DD15);
			W25Q16_ReadUID(&DD15);
			// Release SPI bus and FPGA reset
			SPI_Bus_Release_To_FPGA();

		}
	}

	//*********** Initialize fsmc **********
	if (fsmc_init() != FSMC_OK) {
		test_hardware_result |= _B_FAULT_FSMC_;
	} else {
		// FSMC bus is ready for use
		if (fm22_test_quick() != FSMC_OK) {
			test_hardware_result |= _B_FAULT_FRAM_;
		}
	}

	//********** I2C bus initialization ***********
	//********** DS1621 ************
	if (DS1621_Init(&hi2c1)) {
		if (DS1621_CreateTask() == NULL) {
			test_hardware_result |= _B_FAULT_DS1621_;
		}
		osMutexId_t shared_i2c_mutex = DS1621_GetI2CMutex();

		if (AM1805_Init_Smart(&hi2c1, shared_i2c_mutex)) {
			if (AM1805_GetDiagnostics(&rtc_diag) == AM1805_OK) {
				(void) rtc_diag;
			}
			AM1805_Time_t now;

			// Read RTC time. If it is uninitialized (year <= 2000), stamp the build date
			if (AM1805_GetTime(&now) == AM1805_OK) {
				if (now.year <= 2000) {
					AM1805_Time_t build = AM1805_ParseBuildTime(__DATE__,__TIME__);
					AM1805_SetTime(&build);
				}
			}

		} else {
			test_hardware_result |= _B_FAULT_AM1805_;
		}

	} else {
		test_hardware_result |= _B_FAULT_DS1621_;
	}

#ifdef	_BHV_LOCKER_CHECK_
	//********** LOCKER INITIALIZATION **********
	// Threshold value for this specific board
	const uint32_t BOARD_LOCKER_THRESHOLD = 1000;

	if (FramLocker_Init(BOARD_LOCKER_THRESHOLD, Locker_ThresholdCallback)!= osOK){
		test_hardware_result |= _B_FAULT_AM1805_;
	}
#endif
	//********** TERMINAL (eAssist) INITIALIZATION **********
	if (!terminal_init()) {
		test_hardware_result |= _B_FAULT_TERMINAL_;
	}


	return test_hardware_result;
}


// Test if specific hardware module is functional
inline bool test_status_hardware(uint32_t module) {
    return !(get_status_hardware() & module);
}

// Get current hardware status
inline uint32_t get_status_hardware(void) {
    return test_hardware_result;
}


// Perform system reset
void bsp_system_reset(void) {
    HAL_NVIC_SystemReset();
}

/**
 * @brief  Checks if a Software Reset occurred.
 *         Clears the sticky RCC reset flags only on a soft reset, so hard
 *         reset causes (POR/PIN/IWDG) stay available for the application.
 *         Call before HAL_Init (see main.c).
 */
inline bool get_rcc_csr(void) {
	// Check the sticky software reset flag (SET or RESET)
	bool is_soft_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET);

	if (is_soft_reset) {
		/* Clear sticky reset flags, otherwise SFTRST remains 'true' on subsequent power-on resets */
		__HAL_RCC_CLEAR_RESET_FLAGS();
	}

	return is_soft_reset;
}


/**
 * @brief  Full bootloader hardware teardown before jumping to the application.
 *         Explicitly shuts down all initialized peripherals and DMA streams,
 *         then force-resets every peripheral bus via RCC reset registers to
 *         return the MCU to its raw, power-on hardware defaults.
 */
void bsp_deinit(void) {
    /* 1. Deinitialize all HAL peripheral handles initialized in main() */
    HAL_UART_DeInit(&huart1);
    HAL_UART_DeInit(&huart2);
    HAL_SPI_DeInit(&hspi1);
    HAL_SPI_DeInit(&hspi2);
    HAL_I2C_DeInit(&hi2c1);
    HAL_CAN_DeInit(&hcan2);
    HAL_TIM_Base_DeInit(&htim3);
    HAL_CRC_DeInit(&hcrc);
    HAL_RNG_DeInit(&hrng);

    /* 2. Explicitly stop and deinitialize all DMA streams used by UARTs/TIM3 */
    HAL_DMA_DeInit(&hdma_usart1_rx);
    HAL_DMA_DeInit(&hdma_usart1_tx);
    HAL_DMA_DeInit(&hdma_usart2_rx);
    HAL_DMA_DeInit(&hdma_usart2_tx);
    HAL_DMA_DeInit(&hdma_tim3_ch1_trig);

    /* 3. Force hardware-level reset on all internal buses using RCC reset registers
          (AHB3 covers the FSMC used for the FRAM bus) */
    RCC->AHB1RSTR = 0xFFFFFFFFU;
    RCC->AHB2RSTR = 0xFFFFFFFFU;
    RCC->AHB3RSTR = 0xFFFFFFFFU;
    RCC->APB1RSTR = 0xFFFFFFFFU;
    RCC->APB2RSTR = 0xFFFFFFFFU;

    /* Release all peripheral resets so the application can initialize them */
    RCC->AHB1RSTR = 0x00000000U;
    RCC->AHB2RSTR = 0x00000000U;
    RCC->AHB3RSTR = 0x00000000U;
    RCC->APB1RSTR = 0x00000000U;
    RCC->APB2RSTR = 0x00000000U;

    /* 4. Final core-level HAL teardown, restore RCC clock configuration to HSI */
    HAL_RCC_DeInit();
    HAL_DeInit();
}

/**
 * @brief  Performs a safe, clean jump to the main application.
 *         Completely deinitializes the RTOS, SysTick and HAL peripherals
 *         before jumping.
 * @param  addr: target application start address (sector 5: 0x08020000)
 */
void jump_main_application(uint32_t addr) {
    void (*Jump_To_Application)(void);
    uint32_t JumpAddress;

    /* 1. Read the initial Stack Pointer value from the target firmware image */
    uint32_t stack_pointer = *(__IO uint32_t*)addr;

    /* 2. Validate that the stack pointer lies in this MCU's RAM:
          SRAM 0x20000000..0x20020000 (128 KB) or CCMRAM 0x10000000..0x10010000 (64 KB) */
    if (((stack_pointer >= 0x20000000U) && (stack_pointer <= 0x20020000U)) ||
        ((stack_pointer >= 0x10000000U) && (stack_pointer <= 0x10010000U))) {

        /* VALID FIRMWARE DETECTED! Proceed to safe bootloader teardown */

        /* 3. CRITICAL: disable all CPU interrupts globally FIRST to prevent
              ISR execution while hardware is being deinitialized */
        __disable_irq();

        /* Lock the RTOS kernel scheduler to stop task switching */
        osKernelLock();

        /* Hard-disable the SysTick timer and clear its registers */
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL  = 0;

        /* Deep-clean and deinitialize all hardware peripherals to raw reset states */
        bsp_deinit();

        /* 4. CRITICAL: disable and clear all pending interrupts in the NVIC so
              the application cannot take a stale bootloader interrupt */
        for (uint8_t i = 0; i < 8; i++) {
            NVIC->ICER[i] = 0xFFFFFFFFU; /* Disable all interrupts */
            NVIC->ICPR[i] = 0xFFFFFFFFU; /* Clear all pending flags */
        }

        /* 5. Disable and invalidate the ART instruction/data caches: the
              application starts with an empty cache and re-enables them */
        __HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
        __HAL_FLASH_DATA_CACHE_DISABLE();
        __HAL_FLASH_INSTRUCTION_CACHE_RESET();
        __HAL_FLASH_DATA_CACHE_RESET();

        /* 6. Switch Thread mode stack from PSP (used by FreeRTOS task) back to MSP */
        __set_CONTROL(0);

        /* Set the Main Stack Pointer (MSP) for the new application */
        __set_MSP(stack_pointer);

        /* 7. Data and instruction synchronization barriers */
        __DSB();
        __ISB();

        /* Relocate the vector table to the target application base address */
        SCB->VTOR = addr;

        /* Extract the reset handler address (stored at App base + 4) */
        JumpAddress = *(__IO uint32_t*)(addr + 4);
        Jump_To_Application = (void (*)(void))JumpAddress;

        /* Execute the jump (no return from this point) */
        Jump_To_Application();
    }
    else {
        /* NO VALID FIRMWARE DETECTED! (or flash memory is completely erased)
           Nothing was broken: the RTOS, interrupts and HAL remain active.
           Error handling placeholder (LED warning pattern, "No App!" on UART, etc.) */
    }
}


/* ========================================================================= */
/*  DWT DELAY HARDWARE CONTROL                                               */
/* ========================================================================= */

/**
 * @brief  Initializes the Data Watchpoint and Trace (DWT) cycle counter.
 */
void DWT_Init(void) {
    /* Trace block must be clocked before its registers accept writes */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* DWT->LAR (0xE0001FB0): already unlocked on STM32F4 (Cortex-M4, same
       core as F411), the write is ignored here but keeps the code safe for
       M7 parts where the unlock is mandatory. */
    volatile uint32_t *dwt_lar = (volatile uint32_t *)0xE0001FB0U;
    *dwt_lar = 0xC5ACCE55U;

    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief  Deinitializes the DWT cycle counter.
 */
void DWT_DeInit(void) {
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0;
    CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk;
}

/**
 * @brief  Performs microsecond blocking delay.
 *         Uses SystemCoreClock, so it is correct at any F415 frequency.
 */
void delay_us(const uint32_t us) {
    const uint32_t ticks_needed = us * (SystemCoreClock / 1000000U);
    const uint32_t tick_start = DWT->CYCCNT;

    /* Wrap-safe subtraction: uint32_t overflow is handled by the math itself */
    while ((DWT->CYCCNT - tick_start) < ticks_needed) {
        /* Busy wait */
    }
}



/* overwrite ten wrapper.c/h */
void	ten(bool par){
	if (par){
		PIN_Set(&pin_ten);
	}else{
		PIN_Reset(&pin_ten);
	}
}

