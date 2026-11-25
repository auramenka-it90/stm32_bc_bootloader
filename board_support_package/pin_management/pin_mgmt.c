/**
  ******************************************************************************
  * @file    pin_mgmt.c
  * @brief   Pin Management Library Implementation (BV PCB Rev02, Bootloader).
  *          Features Standard (Mutex Protected) and Fast (Register Direct) I/O.
  *          All comments in English.
  ******************************************************************************
  */

#include "pin_mgmt.h"

/* Private variables ---------------------------------------------------------*/
static osMutexId_t gpio_mutex = NULL;

/* Pin descriptors */
const Pin_Descriptor_t pin_w_up_uart     = PIN_DESC(PMGT_W_UP_PORT, PMGT_W_UP_PIN, "W_UP_UART");
const Pin_Descriptor_t pin_fpga_done     = PIN_DESC(PMGT_DONE_PORT, PMGT_DONE_PIN, "DONE");
const Pin_Descriptor_t pin_reset_xs6     = PIN_DESC(PMGT_RESET_PORT, PMGT_RESET_PIN, "RESET_XS6");
const Pin_Descriptor_t pin_sel_mem_stm32 = PIN_DESC(PMGT_SEL_MEM_PORT, PMGT_SEL_MEM_PIN, "SEL_MEM");
const Pin_Descriptor_t pin_nss_spi2      = PIN_DESC(PMGT_NSS_SPI2_PORT, PMGT_NSS_SPI2_PIN, "NSS_SPI2");
const Pin_Descriptor_t pin_nss_xc6       = PIN_DESC(PMGT_NSS_XC6_PORT, PMGT_NSS_XC6_PIN, "NSS_XC6");
const Pin_Descriptor_t pin_gpio_0        = PIN_DESC(GPIO_0_GPIO_Port, GPIO_0_Pin, "GPIO_0");
const Pin_Descriptor_t pin_gpio_1        = PIN_DESC(GPIO_1_GPIO_Port, GPIO_1_Pin, "GPIO_1");
const Pin_Descriptor_t pin_stlink_gnd    = PIN_DESC(PMGT_STLINK_GND_PORT, PMGT_STLINK_GND_PIN, "STLINK_GND");
const Pin_Descriptor_t pin_tp0           = PIN_DESC(TP0_GPIO_Port, TP0_Pin, "TP0");
const Pin_Descriptor_t pin_tp1           = PIN_DESC(TP1_GPIO_Port, TP1_Pin, "TP1");
const Pin_Descriptor_t pin_ten           = PIN_DESC(PMGT_TEN_KPA_PORT, PMGT_TEN_KPA_PIN, "TEN");

/* Private function prototypes -----------------------------------------------*/
static osStatus_t _create_gpio_mutex(void);
static osStatus_t _delete_gpio_mutex(void);

/* Private functions implementation ------------------------------------------*/

static osStatus_t _create_gpio_mutex(void) {
    if (gpio_mutex == NULL) {
        const osMutexAttr_t mutex_attr = {
            .name = "PIN_MGMT_GPIO_Mutex",
            .attr_bits = osMutexRecursive | osMutexPrioInherit,
            .cb_mem = NULL,
            .cb_size = 0
        };
        gpio_mutex = osMutexNew(&mutex_attr);
        if (gpio_mutex == NULL) {
            return osErrorResource;
        }
    }
    return osOK;
}

static osStatus_t _delete_gpio_mutex(void) {
    if (gpio_mutex != NULL) {
        osMutexDelete(gpio_mutex);
        gpio_mutex = NULL;
    }
    return osOK;
}

/* Public API Functions ------------------------------------------------------*/

osStatus_t PIN_MGMT_Init(void) {
    osStatus_t status = _create_gpio_mutex();
    if (status != osOK) {
        return status;
    }

    /* Set Initial Hardware States using fast atomic register writes.
       SEL_MEM_STM32 is active low (0 = flash routed to STM32), so a HIGH
       keeps the config flash on the FPGA side. RESET_XS6 is active low,
       so a LOW holds the FPGA in reset (same as CubeMX .ioc default) to
       prevent SPI bus contention while the bootloader owns the flash. */
    PIN_Set(&pin_sel_mem_stm32);
    PIN_Reset(&pin_reset_xs6);
    PIN_Set(&pin_nss_xc6);
    PIN_Set(&pin_nss_spi2);

    return osOK;
}

osStatus_t PIN_MGMT_DeInit(void) {
    _delete_gpio_mutex();
    return osOK;
}

/* ========================================================================= */
/*  PIN OPERATIONS (ATOMIC BSRR/IDR ACCESS, ISR-CALLABLE)                    */
/*  Single-pin write via BSRR is atomic in hardware; no mutex is required.   */
/*  Use PIN_GPIO_Mutex_Acquire/Release to protect multi-pin sequences.       */
/* ========================================================================= */

osStatus_t PIN_Set(const Pin_Descriptor_t* pin) {
    if (!pin || !pin->port) return osErrorParameter;
    pin->port->BSRR = pin->pin;
    return osOK;
}

osStatus_t PIN_Reset(const Pin_Descriptor_t* pin) {
    if (!pin || !pin->port) return osErrorParameter;
    pin->port->BSRR = (uint32_t)pin->pin << 16U;
    return osOK;
}

osStatus_t PIN_Toggle(const Pin_Descriptor_t* pin) {
    if (!pin || !pin->port) return osErrorParameter;

    /* Atomic BSRR-based toggle to prevent RMW corruption of adjacent pins on the same port */
    uint32_t odr = pin->port->ODR;
    if (odr & pin->pin) {
        pin->port->BSRR = (uint32_t)pin->pin << 16U; /* Reset pin */
    } else {
        pin->port->BSRR = pin->pin;                   /* Set pin */
    }
    return osOK;
}

uint8_t PIN_Read(const Pin_Descriptor_t* pin) {
    if (!pin || !pin->port) return 0;
    return (pin->port->IDR & pin->pin) ? 1 : 0;
}

/* ========================================================================= */
/*  FAST ALIASES (kept for source compatibility, same atomic implementation) */
/* ========================================================================= */

osStatus_t PIN_Set_F(const Pin_Descriptor_t* pin)    { return PIN_Set(pin); }
osStatus_t PIN_Reset_F(const Pin_Descriptor_t* pin)  { return PIN_Reset(pin); }
osStatus_t PIN_Toggle_F(const Pin_Descriptor_t* pin) { return PIN_Toggle(pin); }
uint8_t    PIN_Read_F(const Pin_Descriptor_t* pin)   { return PIN_Read(pin); }

/* ========================================================================= */
/*  MUTEX OPERATIONS                                                         */
/* ========================================================================= */

osStatus_t PIN_GPIO_Mutex_Acquire(uint32_t timeout) {
    if (gpio_mutex == NULL) {
        osStatus_t status = _create_gpio_mutex();
        if (status != osOK) {
            return status;
        }
    }
    return osMutexAcquire(gpio_mutex, timeout);
}

osStatus_t PIN_GPIO_Mutex_Release(void) {
    if (gpio_mutex == NULL) {
        return osErrorResource;
    }
    return osMutexRelease(gpio_mutex);
}

uint8_t PIN_GPIO_Mutex_Is_Locked(void) {
    if (gpio_mutex == NULL) {
        return 0;
    }

    osStatus_t status = osMutexAcquire(gpio_mutex, 0);
    if (status == osOK) {
        osMutexRelease(gpio_mutex);
        return 0;
    }
    return 1;
}

/* ========================================================================= */
/*  INTERFACE PRESENCE DETECTION                                             */
/* ========================================================================= */

/**
  * @brief  Check if the isolated USB terminal interface is active.
  *         Reads PE4 = CP2102 SUSPEND output routed through ADuM1201.
  *         CP2102 SUSPEND is active-high (1 = USB suspended / no host),
  *         so a LOW level means USB host is present and awake.
  * @retval 1 - USB host active (terminal on USART1/CP2102 path),
  *         0 - no USB host (terminal path defaults to USART2/ADM3491 bus).
  */
uint8_t CP2102_Is_Active(void) {
    return (PIN_Read_F(&pin_w_up_uart) == 0) ? 1 : 0;
}

/**
  * @brief  Check if STLINK debugger is attached.
  *         Reads PB7 = STLINK_GND, input pull-up: STLINK clips the line
  *         to GND, so a LOW level means the debugger is connected.
  * @retval 1 - STLINK attached, 0 - not attached.
  */
uint8_t STLINK_Is_Connected(void) {
    return (PIN_Read_F(&pin_stlink_gnd) == 0) ? 1 : 0;
}

/* ========================================================================= */
/*  UTILITIES                                                                */
/* ========================================================================= */

/**
  * @brief  Kernel-aware delay: osDelay when scheduler runs, HAL_Delay before
  *         start (critical for bootloader execution before OS launch).
  */
osStatus_t PIN_Delay(uint32_t ms) {
    if (osKernelGetState() == osKernelRunning) {
        osDelay(ms);
    } else {
        HAL_Delay(ms);
    }
    return osOK;
}

osStatus_t PIN_Blink(const Pin_Descriptor_t* pin, uint8_t count, uint32_t delay_ms) {
    if (!pin) return osErrorParameter;

    for (uint8_t i = 0; i < count; i++) {
        PIN_Set(pin);
        PIN_Delay(delay_ms);
        PIN_Reset(pin);
        PIN_Delay(delay_ms);
    }

    return osOK;
}

/* ========================================================================= */
/*  SYSTEM FUNCTIONS (BV PCB SPECIFIC)                                       */
/* ========================================================================= */

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
    PIN_Delay(2);
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
        PIN_Delay(1);
    }
    return osErrorTimeout;
}

void FPGA_Reset(void) {
    PIN_Reset(&pin_reset_xs6);
    PIN_Delay(2);
    PIN_Set(&pin_reset_xs6);
}
