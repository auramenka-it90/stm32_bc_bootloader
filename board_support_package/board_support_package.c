
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
bool	soft_reset;
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
	soft_reset=	get_rcc_csr();
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

			// 3. Читаем время. Если оно сброшено (год <= 2000), ставим дату сборки
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

// check reset
inline bool get_rcc_csr(void) {
	// Проверяем флаг (макрос возвращает SET или RESET)
	bool is_soft_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET);

	// ОЧИЩАЕМ все флаги причин сброса!
	// Это гарантирует, что следующая перезагрузка покажет только свежую причину.
	__HAL_RCC_CLEAR_RESET_FLAGS();

	return is_soft_reset;
}


void jump_main_application(uint32_t addr) {
    void (*Jump_To_Application)(void);
    uint32_t JumpAddress;

    // Читаем значение Stack Pointer из начала прошивки
        uint32_t stack_pointer = *(__IO uint32_t*)addr;

        // Проверяем, что стек лежит в SRAM (0x20xxxxxx) или в CCMRAM (0x10xxxxxx)
        if (((stack_pointer & 0xFF000000) == 0x20000000) ||
            ((stack_pointer & 0xFF000000) == 0x10000000)) {

        // ПРОШИВКА ЕСТЬ! Теперь можно безопасно "ломать" среду бутлоадера.

        // 2. Останавливаем планировщик RTOS
        osKernelLock();

        // 3. Сбрасываем всю периферию STM32
        HAL_RCC_DeInit();
        HAL_DeInit();

        // 4. ЖЕСТКО выключаем SysTick
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL  = 0;

        // 5. Запрещаем все прерывания ядра перед прыжком
        __disable_irq();

        // 6. Вычисляем адрес прыжка
        JumpAddress = *(__IO uint32_t*) (addr + 4);
        Jump_To_Application = (void (*)(void)) JumpAddress;

        // 7. Устанавливаем Main Stack Pointer (MSP) для новой прошивки
        __set_MSP(*(__IO uint32_t*) addr);

        // 8. Прыжок веры! (Отсюда возврата нет)
        Jump_To_Application();
    }
    else {
        // ПРОШИВКИ НЕТ! (или память стерта)
        // Мы попадаем сюда, и при этом мы ничего не сломали.
        // RTOS работает, прерывания включены, HAL активен.

        // Здесь можно безопасно обработать ошибку:
        // - Запустить мигание красным светодиодом ошибки
        // - Выдать строку "No App!" в UART
        // - Запустить USB/UART загрузчик (XMODEM/YMODEM) для приема файла
    }
}


/********************************* Точная us задержка *********************************************/
// Инициализация DWT
void DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    // Разблокировка доступа к DWT (актуально для старших ядер Cortex-M)
    // Разкомментируйте, если компилятор знает регистр LAR:
    // DWT->LAR = 0xC5ACCE55;

    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

// Деинициализация DWT
void DWT_DeInit(void) {
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0;
    CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk;
}

// Микросекундная блокирующая задержка
void delay_us(uint32_t us) {
    // Вычисляем, сколько тактов процессора нужно прождать
    uint32_t ticks_needed = us * (SystemCoreClock / 1000000U);

    // Фиксируем стартовое значение счетчика
    uint32_t tick_start = DWT->CYCCNT;

    // Ждем, пока разница не достигнет нужного количества тактов.
    // Переполнение uint32_t обрабатывается аппаратно и безопасно!
    while ((DWT->CYCCNT - tick_start) < ticks_needed) {
        __NOP(); // Опционально: пустая инструкция, чтобы не сводить с ума отладчик
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

