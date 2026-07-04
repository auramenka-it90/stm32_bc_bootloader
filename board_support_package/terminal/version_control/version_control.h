#ifndef VERSION_CONTROL_H_
#define VERSION_CONTROL_H_

#include "board_support_package.h"

/*
 * Comment out if you need to change the compilation date
 */
//#define _VC_MACRO_ENA_DATE_COMPILE_
#define _VC_SIZE_DEVICE_INFO_STR_ (4*256)

/*
 * Software version
 */
#define _SOFT_VERSION_      2
#define _SOFT_SUBVERSION_   0

/*
 * IDE Version
 */
#define _IDE_VERSION_       "STM32CubeIDE 1.19.0" // Укажи здесь свою текущую версию

// Function to create device info string
char* device_info_create(uint32_t password);

// Function to destroy device info string and free memory
void device_info_destroy(void);

#endif /* VERSION_CONTROL_H_ */
