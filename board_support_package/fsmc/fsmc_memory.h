#ifndef FSMC_MEMORY_H
#define FSMC_MEMORY_H

#include <stdint.h>
#include "cmsis_os2.h"

/* FSMC bank base addresses */
#define FSMC_ADDR_NE1    ((uint32_t)0x60000000) // FRAM
#define FSMC_ADDR_NE2    ((uint32_t)0x64000000) // Spartan 6
#define FSMC_ADDR_NE3    ((uint32_t)0x68000000) // Expansion

typedef enum {
    FSMC_DEV_FRAM = 0,
    FSMC_DEV_SPARTAN,
    FSMC_DEV_EXT,
    FSMC_DEV_COUNT
} FSMC_Device_t;

typedef enum {
    FSMC_OK = 0,
    FSMC_ERROR,
    FSMC_TIMEOUT
} FSMC_Status_t;

extern const uint32_t fsmc_base_addrs[];

/* Initialization and system control */
FSMC_Status_t fsmc_init(void);    // create the recursive mutex
FSMC_Status_t fsmc_lock(void);    // acquire the bus
void          fsmc_unlock(void);  // release the bus

/* Fast inline access (no mutex, caller must hold the lock) */
static inline void fsmc_write_fast(FSMC_Device_t dev, uint32_t addr, uint16_t data) {
    *((volatile uint16_t*)(fsmc_base_addrs[dev]) + addr) = data;
}

static inline uint16_t fsmc_read_fast(FSMC_Device_t dev, uint32_t addr) {
    return *((volatile uint16_t*)(fsmc_base_addrs[dev]) + addr);
}

/* Standard access functions (mutex protected) */
FSMC_Status_t fsmc_write(FSMC_Device_t dev, uint32_t addr, uint16_t data);
FSMC_Status_t fsmc_read(FSMC_Device_t dev, uint32_t addr, uint16_t *data);
FSMC_Status_t fsmc_write_buffer(FSMC_Device_t dev, uint32_t addr, const uint16_t *p_data, uint32_t count);
FSMC_Status_t fsmc_read_buffer(FSMC_Device_t dev, uint32_t addr, uint16_t *p_data, uint32_t count);

/* Bit operations (Read-Modify-Write) */
FSMC_Status_t fsmc_set_bit(FSMC_Device_t dev, uint32_t addr, uint16_t mask);
FSMC_Status_t fsmc_clr_bit(FSMC_Device_t dev, uint32_t addr, uint16_t mask);
FSMC_Status_t fsmc_tgl_bit(FSMC_Device_t dev, uint32_t addr, uint16_t mask);
uint8_t       fsmc_test_bit(FSMC_Device_t dev, uint32_t addr, uint16_t mask);

#endif
