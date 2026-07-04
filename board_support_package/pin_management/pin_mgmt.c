/**
 * @file    pin_mgmt.c
 * @brief   Hardware Abstraction Layer for Pin Management (BV PCB Rev02) - Bootloader Version
 * @encoding UTF-8
 */

#include "pin_mgmt.h"

static osMutexId_t pin_mutex = NULL;

/* --- Descriptor Initialization --- */
const Pin_Descriptor_t pin_fpga_done     = {PMGT_DONE_PORT, PMGT_DONE_PIN, "DONE"};
const Pin_Descriptor_t pin_reset_xs6     = {PMGT_RESET_PORT, PMGT_RESET_PIN, "RESET_XS6"};
const Pin_Descriptor_t pin_sel_mem_stm32 = {PMGT_SEL_MEM_PORT, PMGT_SEL_MEM_PIN, "SEL_MEM"};
const Pin_Descriptor_t pin_nss_spi2      = {PMGT_NSS_SPI2_PORT, PMGT_NSS_SPI2_PIN, "NSS_SPI2"};
const Pin_Descriptor_t pin_nss_xc6       = {PMGT_NSS_XC6_PORT, PMGT_NSS_XC6_PIN, "NSS_XC6"};
const Pin_Descriptor_t pin_stlink_gnd    = {PMGT_STLINK_GND_PORT, PMGT_STLINK_GND_PIN, "STLINK_GND"};

/* Fixed typo in GPIO_0 mapping */
const Pin_Descriptor_t pin_w_up_uart     = {W_UP_GPIO_Port, W_UP_Pin, "W_UP_UART"};
const Pin_Descriptor_t pin_gpio_0        = {GPIO_0_GPIO_Port, GPIO_0_Pin, "GPIO_0"};
const Pin_Descriptor_t pin_gpio_1        = {GPIO_1_GPIO_Port, GPIO_1_Pin, "GPIO_1"};
const Pin_Descriptor_t pin_tp0           = {TP0_GPIO_Port, TP0_Pin, "TP0"};
const Pin_Descriptor_t pin_tp1           = {TP1_GPIO_Port, TP1_Pin, "TP1"};
const Pin_Descriptor_t pin_ten			 = {PMGT_TEN_KPA_PORT, PMGT_TEN_KPA_PIN, "TEN"};

/* --- INTERNAL STATIC FUNCTIONS (NOT EXPOSED TO INTERFACE) --- */

/**
 * @brief  Provide delay safe for execution before OS scheduler start (Critical for bootloaders)
 * @param  ms Delay in milliseconds
 */
static void safe_delay_ms(uint32_t ms) {
    if (osKernelGetState() == osKernelRunning) {
        osDelay(ms);
    } else {
        HAL_Delay(ms);
    }
}

/* --- DRIVER INITIALIZATION --- */

osStatus_t PIN_Mgmt_Init(void) {
    const osMutexAttr_t attr = {"pin_mutex", osMutexRecursive | osMutexPrioInherit, NULL, 0};
    pin_mutex = osMutexNew(&attr);

    /* Set Initial Hardware States using fast atomic register writes */
    PIN_Set(&pin_sel_mem_stm32);
    PIN_Set(&pin_reset_xs6);
    PIN_Set(&pin_nss_xc6);
    PIN_Set(&pin_nss_spi2);

    return (pin_mutex != NULL) ? osOK : osError;
}

/* --- SYSTEM FUNCTIONS --- */

osStatus_t SPI_Bus_Acquire_For_STM32(void) {
    PIN_Reset(&pin_sel_mem_stm32);
    
    /* Reliable assembly delay loop (approx 300-500ns) independent of compiler optimization */
    volatile uint32_t cycles = 80;
    __asm__ volatile (
        "1: subs %0, #1 \n"
        "bne 1b \n"
        : "+r" (cycles)
    );
    
    return osOK;
}

osStatus_t SPI_Bus_Release_To_FPGA(void) {
    PIN_Set(&pin_sel_mem_stm32);
    return osOK;
}

osStatus_t FPGA_System_Restart(uint32_t timeout_ms) {
    PIN_Set(&pin_sel_mem_stm32);
    PIN_Reset(&pin_reset_xs6);
    safe_delay_ms(2);
    PIN_Set(&pin_reset_xs6);

    uint32_t start = (osKernelGetState() == osKernelRunning) ? osKernelGetTickCount() : HAL_GetTick();
    while (1) {
        if (PIN_Read(&pin_fpga_done) == 1U) {
            return osOK;
        }
        
        uint32_t elapsed = (osKernelGetState() == osKernelRunning) ? 
                           (osKernelGetTickCount() - start) : (HAL_GetTick() - start);
                           
        if (elapsed >= timeout_ms) {
            break;
        }
        safe_delay_ms(1);
    }
    return osErrorTimeout;
}

void	FPGA_Reset(void){
	PIN_Reset(&pin_reset_xs6);
	safe_delay_ms(2);
	PIN_Set(&pin_reset_xs6);
}

/* --- THREAD-SAFE GPIO (_S) WITH NESTED EXCEPTION SAFETY --- */

void PIN_Set_S(const Pin_Descriptor_t* desc) {
    /* Bypass mutex acquisition if inside an ISR, scheduler is inactive, or initialization failed */
    if (__get_IPSR() != 0 || osKernelGetState() != osKernelRunning || pin_mutex == NULL) {
        PIN_Set(desc);
        return;
    }
    if (osMutexAcquire(pin_mutex, osWaitForever) == osOK) {
        PIN_Set(desc);
        osMutexRelease(pin_mutex);
    }
}

void PIN_Reset_S(const Pin_Descriptor_t* desc) {
    if (__get_IPSR() != 0 || osKernelGetState() != osKernelRunning || pin_mutex == NULL) {
        PIN_Reset(desc);
        return;
    }
    if (osMutexAcquire(pin_mutex, osWaitForever) == osOK) {
        PIN_Reset(desc);
        osMutexRelease(pin_mutex);
    }
}

void PIN_Toggle_S(const Pin_Descriptor_t* desc) {
    if (__get_IPSR() != 0 || osKernelGetState() != osKernelRunning || pin_mutex == NULL) {
        PIN_Toggle(desc);
        return;
    }
    if (osMutexAcquire(pin_mutex, osWaitForever) == osOK) {
        PIN_Toggle(desc);
        osMutexRelease(pin_mutex);
    }
}

uint8_t PIN_Read_S(const Pin_Descriptor_t* desc) {
    uint8_t s = 0;
    if (__get_IPSR() != 0 || osKernelGetState() != osKernelRunning || pin_mutex == NULL) {
        return PIN_Read(desc);
    }
    if (osMutexAcquire(pin_mutex, osWaitForever) == osOK) {
        s = PIN_Read(desc);
        osMutexRelease(pin_mutex);
    }
    return s;
}


/**
  * @brief  Check if ST-Link is connected
  */
uint8_t STLINK_Is_Connected(void)
{
    return (PIN_Read(&pin_w_up_uart) == 0) ? 1 : 0;
}


/* --- DIRECT LOW-LATENCY ACCESS (SINGLE TRANS-ACTION AHB WRITES) --- */

void PIN_Set(const Pin_Descriptor_t* desc) { 
    /* Atomic bit set via BSRR register */
    desc->port->BSRR = desc->pin; 
}

void PIN_Reset(const Pin_Descriptor_t* desc) { 
    /* Atomic bit reset via upper 16 bits of BSRR register */
    desc->port->BSRR = (uint32_t)desc->pin << 16U; 
}

void PIN_Toggle(const Pin_Descriptor_t* desc) { 
    /* Guard non-atomic Read-Modify-Write using nested interrupt disable */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    
    uint32_t odr = desc->port->ODR;
    desc->port->BSRR = ((odr & desc->pin) << 16U) | (~odr & desc->pin);
    
    __set_PRIMASK(primask);
}

uint8_t PIN_Read(const Pin_Descriptor_t* desc) { 
    /* Single read cycle of IDR register */
    return (uint8_t)((desc->port->IDR & desc->pin) != 0U ? 1U : 0U); 
}