#ifndef FM22L16_H
#define FM22L16_H

#include "fsmc_memory.h"
#include "board_support_package.h" /* for t_float_uint */

/* Last word address: 256K words, 18 address lines (A0..A17) */
#define FM22_MAX_ADDR      0x3FFFF

/* Software sector protection (8 sectors, 32 kWords each) */
#define FM22_PROT_SEC_0    (1 << 0) // 00000h..07FFFh
#define FM22_PROT_SEC_1    (1 << 1) // 08000h..0FFFFh
#define FM22_PROT_SEC_2    (1 << 2) // 10000h..17FFFh
#define FM22_PROT_SEC_3    (1 << 3) // 18000h..1FFFFh
#define FM22_PROT_SEC_4    (1 << 4) // 20000h..27FFFh
#define FM22_PROT_SEC_5    (1 << 5) // 28000h..2FFFFh
#define FM22_PROT_SEC_6    (1 << 6) // 30000h..37FFFh
#define FM22_PROT_SEC_7    (1 << 7) // 38000h..3FFFFh
#define FM22_PROT_NONE     (0x00)   // protection disabled
#define FM22_PROT_ALL      (0xFF)   // all sectors protected

/* 32-bit data access */
FSMC_Status_t fm22_write_union(uint32_t addr, t_float_uint data);
FSMC_Status_t fm22_read_union(uint32_t addr, t_float_uint *data);

/* Sector protection control */
FSMC_Status_t fm22_set_protection(uint8_t sector_mask);

/* Memory tests */
FSMC_Status_t fm22_test_quick(void); // 3-point sample test with restore
FSMC_Status_t fm22_test_full(void);  // full array test (destroys user data)

#endif
