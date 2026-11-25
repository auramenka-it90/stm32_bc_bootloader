/**
 ******************************************************************************
 * @file    version_control.h
 * @brief   Software/Hardware version information module for Bootloader.
 *          MSB/PSPM-style compile-time string parts in Flash (.rodata).
 *          All comments in ASCII English.
 ******************************************************************************
 */

#ifndef VERSION_CONTROL_H_
#define VERSION_CONTROL_H_

#include "board_support_package.h"

/* ========================================================================= */
/*  STATIC STRING DEFINITIONS (BOOTLOADER)                                   */
/* ========================================================================= */

/* --- Settings for the BC board --- */
#define _VC_PRODUCT_NUMBER_     "Ballistic computer board (BC)\n"
#define _VC_BOARD_NUMBER_       "Board:  7111.00.00.000\n"
#define _VC_CHIP_DESIGNATION_   "Chip:   DD11 - STM32F415ZGT6\n"
#define _VC_SOFT_NAME_          "Software: bootloader\n"
#define _VC_SOFT_NOTE_          "Produced by mr. Andrew Auramenka \n"
#define _VC_SOFT_VERSION_       "Ver. 2.0\n"

/*
 * Comment out if you need to change the compilation date
 */
//#define _VC_MACRO_ENA_DATE_COMPILE_
#define _VC_SIZE_DEVICE_INFO_STR_ (4*256)

/* Compile-time concatenated prefix, session password is appended by create */
#define _VC_INFO_PREFIX_ \
    _VC_PRODUCT_NUMBER_ \
    _VC_BOARD_NUMBER_ \
    _VC_CHIP_DESIGNATION_ \
    _VC_SOFT_NAME_ \
    "Build:   " __DATE__ "  " __TIME__ "\n" \
    _VC_SOFT_NOTE_ \
    _VC_SOFT_VERSION_

/* ========================================================================= */
/*  PUBLIC API                                                               */
/* ========================================================================= */

// Function to create device info string
char* device_info_create(uint32_t password);

// Function to destroy device info string and free memory
void device_info_destroy(void);

#endif /* VERSION_CONTROL_H_ */
