/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "board_support_package.h"
#include "pin_mgmt.h"
#include "rgb_led.h"
#include "terminal.h"
#include "ds1621.h"
#include "am1805.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Synchronization flag to start tasks after hardware initialization */
#define TASK_START_FLAG 0x01U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
/* Low level sensor data cache, read by terminal signals */
float current_temp;
AM1805_Time_t current_time;
char data_time_string[32];
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for LowLevelTask */
osThreadId_t LowLevelTaskHandle;
const osThreadAttr_t LowLevelTask_attributes = {
  .name = "LowLevelTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityLow1,
};
/* Definitions for TerminalTask */
osThreadId_t TerminalTaskHandle;
const osThreadAttr_t TerminalTask_attributes = {
  .name = "TerminalTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityLow7,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartLowLevelTask(void *argument);
void StartTerminalTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of LowLevelTask */
  LowLevelTaskHandle = osThreadNew(StartLowLevelTask, NULL, &LowLevelTask_attributes);

  /* creation of TerminalTask */
  TerminalTaskHandle = osThreadNew(StartTerminalTask, NULL, &TerminalTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument) {
  /* USER CODE BEGIN StartDefaultTask */

    /* Run board-level POST and hardware initialization */
    /* Any delays (osDelay) inside this function are now completely safe */
    init_hardware();

    /* Cascade start tasks based on POST diagnostic results using Thread Flags */
    if (test_status_hardware(_B_FAULT_TERMINAL_)) {
        osThreadFlagsSet(TerminalTaskHandle, TASK_START_FLAG);
    }

    if (test_status_hardware(_B_FAULT_AM1805_ | _B_FAULT_DS1621_)) {
        osThreadFlagsSet(LowLevelTaskHandle, TASK_START_FLAG);
    }

    /* Monitor heartbeat: direct fast port toggle on Test Point 0 (no mutex overhead) */
    for (;;) {
        PIN_Toggle_F(&pin_tp0);
        osDelay(10);
    }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartLowLevelTask */
/**
* @brief Function implementing the LowLevelTask thread.
* @param argument: Not used
* @retval None
*/

/* USER CODE END Header_StartLowLevelTask */
void StartLowLevelTask(void *argument) {
  /* USER CODE BEGIN StartLowLevelTask */

  /* Wait for the flag from the monitor task. The task sleeps and consumes no CPU. */
  osThreadFlagsWait(TASK_START_FLAG, osFlagsWaitAny, osWaitForever);

  /* Infinite loop */
  for (;;) {
    /* Update current temperature (DS1621 driver keeps its own cache) */
    current_temp = DS1621_GetLastTemperature();

    /* Update RTC date/time string */
    if (AM1805_GetTime(&current_time) == AM1805_OK) {
        AM1805_FormatFullDateTime(data_time_string, sizeof(data_time_string), &current_time);
    }

    osDelay(100);
  }
  /* USER CODE END StartLowLevelTask */
}

/* USER CODE BEGIN Header_StartTerminalTask */
/**
* @brief Function implementing the TerminalTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTerminalTask */
void StartTerminalTask(void *argument) {
  /* USER CODE BEGIN StartTerminalTask */

  /* Wait for the flag from the monitor task. The task sleeps and consumes no CPU. */
  osThreadFlagsWait(TASK_START_FLAG, osFlagsWaitAny, osWaitForever);

  /* Infinite loop */
  for (;;) {
    terminal_task();
  }
  /* USER CODE END StartTerminalTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

